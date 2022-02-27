#pragma once

#include <type_traits>
#include <bitset>
#include <atomic>
#include <new>
#include <mutex>
#include <condition_variable>
#include <array>
#include <future>
#include <vector>
#include <shared_mutex>
#include <deque>
#include <variant>

#include "belt.h"
#include "worker.h"

class Production {

    static constexpr std::uint8_t NO_OF_SLOTS = 3;
public:
    Production(){

        // Assign the belt for the workers
        for (uint8_t i = 0; i <= NO_OF_SLOTS; ++i){
            m_WorkerPairs.emplace_back(std::make_unique<WorkerPair<NO_OF_SLOTS>>(i, *this));
        }
    }

    void Start(const std::size_t runTime){

        // Trigger up all workers
        for (const auto& w : m_WorkerPairs){
            w->Start();
        }

        auto componentFeeder = [](){
            SlotData data;
            return data;
        };

        for (int i = 0; i < runTime; ++i){
            auto component = componentFeeder();
            auto indexToFeed = getSlotIndexAfter(m_SlotIndexFed);
            std::unique_lock<std::shared_mutex> lk;
            m_CondVar.wait(lk);
            m_Belt[indexToFeed] = component;
            lk.unlock();
            m_CondVar.notify_all();
        }
        m_Exit = true;
    }

private:
    std::uint8_t getSlotIndexAfter(std::int8_t currIndex) const{
        return --currIndex < 0 ? NO_OF_SLOTS : currIndex;
    }

    std::int8_t m_SlotIndexFed{-1};
    ConveyorBelt<std::atomic<SlotData>, NO_OF_SLOTS> m_Belt;
    std::vector<std::unique_ptr<WorkerPair<NO_OF_SLOTS>>> m_WorkerPairs; // Not array intentionally
    std::shared_mutex m_Mu;
    std::condition_variable_any m_CondVar;

    std::atomic_bool m_Exit;

    // just to keep less verbose
    template<std::size_t N=NO_OF_SLOTS>
    friend class Worker;
};
