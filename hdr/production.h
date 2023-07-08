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
#include <random>
#include <chrono>
#include <iostream>

#define GETTER(OBJ) public:\
    const decltype(OBJ)& get##OBJ() const { return OBJ; }

#include "belt.h"
#include "worker.h"

class Production {

    static constexpr std::uint8_t NO_OF_SLOTS = 3;
public:
    Production(){

        // Assign the belt for the workers
        for (uint8_t i = 0; i < NO_OF_SLOTS; ++i){
            m_WorkerPairs.emplace_back(std::make_unique<WorkerPair<NO_OF_SLOTS>>(i, *this));
        }
    }

    std::future<void> getFuture() {

        std::lock_guard lk(m_MuFutGuard);
        std::promise<void> promise;
        std::future<void> future = promise.get_future();
        m_Promises.push_back(std::move(promise));

        return future;
    }

    std::promise<void> getPromise() {

        std::lock_guard lk(m_MuProGuard);
        std::cout << "m_MuProGuard: locked: " << ++takenCount << std::endl;
        std::promise<void> promise;
        std::future<void> future = promise.get_future();
        m_Futures.push_back(std::move(future));

        std::cout << "m_MuProGuard: locked: " << --takenCount << std::endl;
        return promise;
    }

    void Start(const std::size_t runTime){

        // Trigger all workers
        for (const auto& w : m_WorkerPairs){
            w->Start();
        }

        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> distrib(0, 4);
        std::array<uint8_t, 5> component_array{0, COMPONENT::COMPONENT_A, COMPONENT::COMPONENT_B, COMPONENT::COMPONENT_C, COMPONENT::EMPTY};

        auto componentFeeder = [component_array = std::move(component_array), &rd, &distrib, &gen](){
            SlotData data;
#ifdef RUN_CATCH
            data.SetComponentData(COMPONENT::COMPONENT_A);
#else
            data.SetComponentData(component_array[distrib(gen)]);
#endif
            return data;
        };

        /* 1. feed the belt
         * 2. notify all workers on stand-by to work on designated slots at different cache lines simultaneously for exactly once.
         * 3. once all workers raise done will continue loop
         * 4. workers will take new promise after every loop while production will clear expired promises.
         * like C++ 20 counting semaphores. life much easier.
        */

        static int c;

        auto WaitForWorkers = [this](int i){
            // Don't wait for workers until first feed
            if (i == 0)
                return;
            std::lock_guard lk(m_MuProGuard);
            std::cout << "m_MuProGuard: locked in prod: " << ++takenCount << std::endl;
            for (int i = 0; i < 2 * NO_OF_SLOTS && i < m_Futures.size(); ++i){
                m_Futures[i].get();
                m_Futures.pop_front();
            }
            std::cout << "m_MuProGuard: locked in prod: " << --takenCount << std::endl;
        };
        auto NotifyWorkers = [this]() {
            std::lock_guard lk(m_MuFutGuard);
            for (int i = 0; i < 2 * NO_OF_SLOTS && i < m_Promises.size(); ++i){
                m_Promises[i].set_value();
                m_Promises.pop_front();
            }
        };
        auto Feed = [this](SlotData& component){
            // simultaneous read  but exclusive write with shared_mutex.
            std::unique_lock lk(m_Mu);
            //std::cout << "fed in index: " << (int)m_SlotIndexFed << std::endl;
            if (component.testIsEmpty()) ++m_noOfEmptyFeed;
            // Atomic load/store of slots in concecutive cachelines for avoid false sharing.
            std::atomic_store_explicit(&m_Belt[m_SlotIndexFed], component, std::memory_order_release);
        };

        for (std::size_t i = 0; i < runTime; ++i){
            auto component = componentFeeder();
            m_SlotIndexFed = getSlotIndexAfter(m_SlotIndexFed, true);
            WaitForWorkers(i);
            Feed(component);
            NotifyWorkers();
        }

        m_Exit.store(true, std::memory_order_relaxed);
    }

    std::size_t getNoOfWorkersWithUnfinishedProducts(){

        std::size_t ret = 0;
        for (const auto& w : m_WorkerPairs){
            ret += w->getNoOfWorkersWithUnfinishedProducts();
        }

        return ret;
    }

private:
    std::uint8_t getSlotIndexAfter(std::int8_t currIndex, const bool isWrite=false) noexcept{

        /*
         * progress belt by rotating the buffer index. overridden buffer is checked for
         * unhandled component or completed product.
        */
        std::uint8_t ret_index;
        if (--currIndex < 0){
            ret_index = NO_OF_SLOTS - 1;
            if (isWrite){
                SlotData* ptr = std::launder(reinterpret_cast<SlotData*>(&m_Belt[ret_index]));
                if (ptr->AnyComponent()){
                    ++m_noOfComponentsUnHandled;
                }else if (!ptr->testIsEmpty()){
                    ++m_noOfProductsFormed;
                }
            }
        }else{
            ret_index = currIndex;
        }

        return ret_index;
    }

    std::int8_t m_SlotIndexFed{1};
    ConveyorBelt<std::atomic<SlotData>, NO_OF_SLOTS> m_Belt;
    std::vector<std::unique_ptr<WorkerPair<NO_OF_SLOTS>>> m_WorkerPairs; // Not array intentionally
    std::shared_mutex m_Mu;

    std::atomic_bool m_Exit;

    std::size_t m_noOfProductsFormed{0};
    std::size_t m_noOfComponentsUnHandled{0};
    std::size_t m_noOfEmptyFeed{0};

    // just to keep less verbose
    template<std::size_t N>
    friend class Worker;

    // If c++20 use counting semaphore
    std::mutex m_MuProGuard;
    std::deque<std::promise<void>> m_Promises;
    std::mutex m_MuFutGuard;
    std::deque<std::future<void>> m_Futures;

    static inline int takenCount{0};
    GETTER(m_noOfEmptyFeed);
    GETTER(m_noOfProductsFormed);
    GETTER(m_noOfComponentsUnHandled);
};
