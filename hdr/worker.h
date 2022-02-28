#pragma once

class Production;

/*
 * State chart for worker's work flow management
 * transaction based model. if worker is quick enough to act on the slot than its partner
 * transaction will commit if not rollback. Since every worker is a thread no penalty in
 * perform work and rollback if not needed.
*/
template<class... Ts> struct overloaded : Ts... { using Ts::operator()...; };
template<class... Ts> overloaded(Ts...) -> overloaded<Ts...>;
class StateChart {

    struct StateFetch { };
    struct StateGetBOrC { };
    struct StateGetA { };
    struct StateDecode { };
    struct StateFull { };

public:
    void Process(SlotData& slot) noexcept{

        static std::string classnames[] = {"StateFetch", "StateGetBOrC", "StateGetA", "StateDecode", "StateFull"};

        m_PrevState = m_CurrState;

        //std::cout << "m_PrevState: " << classnames[m_PrevState.index()] << std::endl;

        if (m_Timeout > 0)[[likely]]
            --m_Timeout;

        std::visit(overloaded {
                       [&slot, this](const StateFetch& arg) {

                           if (slot.testComponent<COMPONENT::COMPONENT_A>()){
                               m_CurrState = StateGetBOrC{};
                               slot.isUpdated = true;
                               slot.ClearComponent<COMPONENT::COMPONENT_A>();
                           }else if (slot.testComponent<COMPONENT::COMPONENT_B>() || slot.testComponent<COMPONENT::COMPONENT_C>()){
                               m_CurrState = StateGetA{};
                               slot.ClearComponent<COMPONENT::COMPONENT_B>();
                               slot.ClearComponent<COMPONENT::COMPONENT_C>();
                               slot.isUpdated = true;
                           }
                       },
                       [&slot, this](const StateGetBOrC& arg) {

                           if (slot.testComponent<COMPONENT::COMPONENT_B>() || slot.testComponent<COMPONENT::COMPONENT_C>()){
                               m_Timeout = 4;
                               m_CurrState = StateDecode{};
                               slot.ClearComponent<COMPONENT::COMPONENT_B>();
                               slot.ClearComponent<COMPONENT::COMPONENT_C>();
                               slot.isUpdated = true;
                           }
                       },
                       [&slot, this](const StateGetA& arg) {
                           if (slot.testComponent<COMPONENT::COMPONENT_A>()){
                               m_Timeout = 4;
                               m_CurrState = StateDecode{};
                               slot.ClearComponent<COMPONENT::COMPONENT_A>();
                               slot.isUpdated = true;
                           }
                       },
                       [&slot, this](const StateDecode& arg) {
                           if (slot.testIsEmpty() && m_Timeout == 0){
                               m_CurrState = StateFetch{};
                               slot.SetComponent<COMPONENT::COMPONENT_P>();
                           }else if (m_Timeout == 0 && slot.AnyComponent()){
                               if (slot.testComponent<COMPONENT::COMPONENT_A>()){
                                   m_CurrState = StateFull{};
                                   slot.ClearComponent<COMPONENT::COMPONENT_A>();
                                   m_ComponentInHand = COMPONENT::COMPONENT_A;
                               }
                               if (slot.testComponent<COMPONENT::COMPONENT_B>() || slot.testComponent<COMPONENT::COMPONENT_C>()){
                                   m_CurrState = StateFull{};
                                   slot.ClearComponent<COMPONENT::COMPONENT_B>();
                                   slot.ClearComponent<COMPONENT::COMPONENT_C>();
                                   m_ComponentInHand = COMPONENT::COMPONENT_B;
                               }
                           }
                       },
                       [&slot, this](const StateFull& arg) {
                           if (slot.testIsEmpty()){
                               slot.SetComponent<COMPONENT::COMPONENT_P>();
                               slot.isUpdated = true;

                               if (m_ComponentInHand == COMPONENT::COMPONENT_A){
                                   m_CurrState = StateGetBOrC{};
                               }else if (m_ComponentInHand == COMPONENT::COMPONENT_B ||
                                         m_ComponentInHand == COMPONENT::COMPONENT_C){
                                   m_CurrState = StateGetA{};
                               }else{
                                   m_CurrState = StateFetch{};
                               }
                           }
                       },
                       [](auto) { }
                   }, m_CurrState);

        //::cout << "m_CurrState: " << classnames[m_CurrState.index()] << std::endl;
    }

    inline void Commit(){
        m_CurrState = m_CurrState;
    }

    inline void Rollback(){
        m_CurrState = m_PrevState;
    }

    bool getIsWorkersWithUnfinishedProducts() noexcept{
        bool ret = false;
        std::visit([&ret, this](auto&& arg) {
            using T = std::decay_t<decltype(arg)>;
            if constexpr (std::is_same_v<T, StateGetBOrC> || std::is_same_v<T, StateGetA>)
               ret = true;
            else if constexpr (std::is_same_v<T, StateDecode> || std::is_same_v<T, StateFull>)
               ret = m_ComponentInHand != 0 ? true : false;
        }, m_CurrState);

        return ret;
    }

private:
    COMPONENT m_ComponentInHand{COMPONENT::EMPTY};
    std::uint8_t m_Timeout{0};
    std::variant<StateFetch, StateGetBOrC, StateGetA, StateDecode, StateFull> m_PrevState = StateFetch{};
    std::variant<StateFetch, StateGetBOrC, StateGetA, StateDecode, StateFull> m_CurrState = StateFetch{};

    GETTER(m_CurrState);
};

template<std::size_t NO_OF_SLOTS>
class WorkerPair;

/*
 * SOLID principle. Worker class will take only responsibility of filling up the slot.
 * Rule of 5. not defined compiler generated functions unless required.
*/
template<std::size_t NO_OF_SLOTS>
class Worker {

public:
    Worker(const std::uint8_t initialIndex, Production& prod, WorkerPair<NO_OF_SLOTS>& mngr)
         :m_Belt(prod.m_Belt),
          m_LastReadIndex(initialIndex),
          m_FutureFromProd(std::move(prod.getFuture())),
          m_PromiseToProd(std::move(prod.getPromise())),
          m_Mu(prod.m_Mu),
          m_BeltOwner(prod),
          m_Manager(mngr),
          m_WorkFlow(std::make_unique<StateChart>())
    {}


    bool Work(){

        while(true){

            // stand-by until Belt is fed and signaled.
            m_FutureFromProd.get();
            m_FutureFromProd = std::move(m_BeltOwner.getFuture());

            std::shared_lock lk(m_Mu);

            m_LastReadIndex = m_BeltOwner.getSlotIndexAfter(m_LastReadIndex);
            if (m_LastReadIndex >= NO_OF_SLOTS)
                return false;

            //std::cout << "reading in index: " << (int)m_LastReadIndex << std::endl;

            // Ok to copy
            SlotData s_cur = std::atomic_load_explicit(&m_Belt[m_LastReadIndex], std::memory_order_acquire);
            if (s_cur.isUpdated){
                m_PromiseToProd.set_value();
                m_PromiseToProd = std::move(m_BeltOwner.getPromise());
                continue;
            }

            // Will update data and isUpdated of s_cur.
            m_WorkFlow->Process(s_cur);

            if (s_cur.isUpdated){
                // finish the work and check if can update. exactly once stratergy
                const SlotData& s_new = std::atomic_load_explicit(&m_Belt[m_LastReadIndex], std::memory_order_acquire);
                if (!s_new.isUpdated){
                    // try lock is non-blocking. get ready for rollback if not lucky!!.
                    if (m_Manager.TryLock()){
                        std::atomic_store_explicit(&m_Belt[m_LastReadIndex], s_cur, std::memory_order_release);
                        m_WorkFlow->Commit();
                        m_Manager.UnLock();
                        m_PromiseToProd.set_value();
                        m_PromiseToProd = std::move(m_BeltOwner.getPromise());
                        continue;
                    }
                }

                // must have followed RAII style but to keep simple.
                m_WorkFlow->Rollback();
            }
            m_PromiseToProd.set_value();
            if (m_BeltOwner.m_Exit.load(std::memory_order_relaxed)){
                return true;
            }
            m_PromiseToProd = std::move(m_BeltOwner.getPromise());
        }

        m_PromiseToProd.set_value();
        return true;
    }

    bool getIsWorkersWithUnfinishedProducts(){
        return m_WorkFlow->getIsWorkersWithUnfinishedProducts();
    }
private:
    ConveyorBelt<std::atomic<SlotData>, NO_OF_SLOTS>& m_Belt;
    std::uint8_t m_LastReadIndex;
    std::shared_mutex& m_Mu;
    std::future<void> m_FutureFromProd;
    std::promise<void> m_PromiseToProd;
    Production& m_BeltOwner;
    WorkerPair<NO_OF_SLOTS>& m_Manager;
    std::unique_ptr<StateChart> m_WorkFlow;
};

template<std::size_t NO_OF_SLOTS>
class WorkerPair {
public:

    WorkerPair() = delete;
    WorkerPair(const std::uint8_t initialIndex, Production& prod){

        for (int i = 0; i < 2; ++i)
            m_Workers.emplace_back(std::make_unique<Worker<NO_OF_SLOTS>>(initialIndex, prod, *this));
    }

    void Start() {
        for (int i = 0; i < 2; ++i){
            auto fu = std::async(std::launch::async, &Worker<NO_OF_SLOTS>::Work, m_Workers[i].get());
            m_Futures.push_back(std::move(fu));
        }
    }

    inline bool TryLock(){
        return m_Mu.try_lock();
    }

    inline void UnLock(){
        m_Mu.unlock();
        m_CondVar.notify_one();
    }

    std::size_t getNoOfWorkersWithUnfinishedProducts(){
        return ((int)m_Workers[0]->getIsWorkersWithUnfinishedProducts() +
                (int)m_Workers[1]->getIsWorkersWithUnfinishedProducts());
    }

private:
    std::vector<std::unique_ptr<Worker<NO_OF_SLOTS>>> m_Workers;
    std::mutex m_Mu;
    std::condition_variable m_CondVar;
    std::deque<std::future<bool>> m_Futures;
};
