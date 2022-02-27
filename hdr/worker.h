#pragma once

class Production;

template<class... Ts> struct overloaded : Ts... { using Ts::operator()...; };
template<class... Ts> overloaded(Ts...) -> overloaded<Ts...>;
class StateChart {

    struct StateFetch { };
    struct StateGetBOrC { };
    struct StateGetA { };
    struct StateDecode { };
    struct StateFull { };

public:
    bool Process(SlotData& slot){

        m_PrevState = m_CurrState;
        bool isUpdated = false;

        if (m_Timeout > 0)
            --m_Timeout;

        std::visit(overloaded {
                       [&slot, this, &isUpdated](const StateFetch& arg) {

                           if (slot.testComponent<COMPONENT::C_A>()){
                               m_CurrState = StateGetBOrC{};
                               isUpdated = true;
                               slot.ClearComponent<COMPONENT::C_A>();
                           }else if (slot.testComponent<COMPONENT::C_B>() || slot.testComponent<COMPONENT::C_C>()){
                               m_CurrState = StateGetA{};
                               slot.ClearComponent<COMPONENT::C_B>();
                               slot.ClearComponent<COMPONENT::C_C>();
                               isUpdated = true;
                           }
                       },
                       [&slot, this, &isUpdated](const StateGetBOrC& arg) {
                           if (slot.testComponent<COMPONENT::C_A>()){
                               m_Timeout = 4;
                               m_CurrState = StateDecode{};
                               slot.ClearComponent<COMPONENT::C_A>();
                               isUpdated = true;
                           }
                       },
                       [&slot, this, &isUpdated](const StateGetA& arg) {
                           if (slot.testComponent<COMPONENT::C_B>() || slot.testComponent<COMPONENT::C_C>()){
                               m_Timeout = 4;
                               m_CurrState = StateDecode{};
                               slot.ClearComponent<COMPONENT::C_B>();
                               slot.ClearComponent<COMPONENT::C_C>();
                               isUpdated = true;
                           }                       },
                       [&slot, this, &isUpdated](const StateDecode& arg) {
                           if (slot.testComponent<COMPONENT::EMPTY>() && m_Timeout == 0){
                               m_CurrState = StateFetch{};
                               slot.SetComponent<COMPONENT::C_P>();
                           }else if (m_Timeout == 0 && slot.AnyComponent()){
                               if (slot.testComponent<COMPONENT::C_A>()){
                                   m_CurrState = StateFull{};
                                   slot.ClearComponent<COMPONENT::C_A>();
                                   m_ComponentInHand = COMPONENT::C_A;
                               }
                               if (slot.testComponent<COMPONENT::C_B>() || slot.testComponent<COMPONENT::C_C>()){
                                   m_CurrState = StateFull{};
                                   slot.ClearComponent<COMPONENT::C_B>();
                                   slot.ClearComponent<COMPONENT::C_C>();
                                   m_ComponentInHand = COMPONENT::C_B;
                               }
                           }
                       },
                       [&slot, this, &isUpdated](const StateFull& arg) {
                           if (slot.testComponent<COMPONENT::EMPTY>()){
                               slot.SetComponent<COMPONENT::C_P>();
                               isUpdated = true;

                               if (m_ComponentInHand == COMPONENT::C_A){
                                   m_CurrState = StateGetBOrC{};
                               }else if (m_ComponentInHand == COMPONENT::C_B ||
                                         m_ComponentInHand == COMPONENT::C_C){
                                   m_CurrState = StateGetA{};
                               }else{
                                   m_CurrState = StateFetch{};
                               }
                           }
                       },
                       [](auto) { }
                   }, m_CurrState);

        return isUpdated;
    }

    inline void Commit(){
        m_CurrState = m_CurrState;
    }

    inline void Rollback(){
        m_CurrState = m_PrevState;
    }

private:
    COMPONENT m_ComponentInHand;
    uint8_t m_Timeout{0};
    std::variant<StateFetch, StateGetBOrC, StateGetA, StateDecode, StateFull> m_PrevState = StateFetch{};
    std::variant<StateFetch, StateGetBOrC, StateGetA, StateDecode, StateFull> m_CurrState = StateFetch{};
};

template<std::size_t NO_OF_SLOTS>
class WorkerPair;

template<std::size_t NO_OF_SLOTS>
class Worker {

public:
    Worker(const std::uint8_t initialIndex, Production& prod, WorkerPair<NO_OF_SLOTS>& mngr)
         :m_Belt(prod.m_Belt),
          m_LastReadIndex(initialIndex),
          m_Mu(prod.m_Mu), m_CondVar(prod.m_CondVar),
          m_BeltOwner(prod),
          m_Manager(mngr)
    {}

    bool Work(){

        while(true){

            std::shared_lock lk(m_Mu);
            m_LastReadIndex = m_BeltOwner.getSlotIndexAfter(m_LastReadIndex);
            if (m_LastReadIndex >= NO_OF_SLOTS)
                return false;

            // Ok to copy
            SlotData s_cur = std::atomic_load_explicit(&m_Belt[m_LastReadIndex], std::memory_order_acquire);
            if (s_cur.isUpdated)
                continue;

            // Will update data and isUpdated flag as well.
            const bool isupdated = m_WorkFlow->Process(s_cur);

            // finish the work and check if can update. exactly once stratergy
            const SlotData& s_new = std::atomic_load_explicit(&m_Belt[m_LastReadIndex], std::memory_order_acquire);
            if (!s_new.isUpdated){
                if (m_Manager.TryLock()){
                    std::atomic_store_explicit(&m_Belt[m_LastReadIndex], s_cur, std::memory_order_release);
                    m_WorkFlow->Commit();
                    m_Manager.UnLock();
                    continue;
                }
            }
            m_WorkFlow->Rollback();

            if (m_BeltOwner.m_Exit.load(std::memory_order_relaxed))
                break;
        }

        return true;
    }

private:
    ConveyorBelt<std::atomic<SlotData>, NO_OF_SLOTS>& m_Belt;
    std::uint8_t m_LastReadIndex;
    std::shared_mutex& m_Mu;
    std::condition_variable_any& m_CondVar;
    Production& m_BeltOwner;
    WorkerPair<NO_OF_SLOTS>& m_Manager;
    std::unique_ptr<StateChart> m_WorkFlow;
};

template<std::size_t NO_OF_SLOTS>
class WorkerPair {
public:

    WorkerPair() = delete;
    WorkerPair(const std::uint8_t initialIndex, Production& prod){

        m_Workers.emplace_back(std::make_unique<Worker<NO_OF_SLOTS>>(initialIndex, prod, *this));
    }

    void Start() {
        auto fu1 = std::async(std::launch::async, &Worker<NO_OF_SLOTS>::Work, m_Workers[0].get());
        auto fu2 = std::async(std::launch::async, &Worker<NO_OF_SLOTS>::Work, m_Workers[1].get());
        m_Futures.push_back(std::move(fu1));
        m_Futures.push_back(std::move(fu2));
    }

    inline bool TryLock(){
        return m_Mu.try_lock();
    }

    inline void UnLock(){
        m_Mu.unlock();
        m_CondVar.notify_one();
    }

private:
    std::vector<std::unique_ptr<Worker<NO_OF_SLOTS>>> m_Workers;
    std::mutex m_Mu;
    std::condition_variable m_CondVar;
    std::deque<std::future<bool>> m_Futures;
};
