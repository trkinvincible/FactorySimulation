#pragma once

enum /*class*/ COMPONENT : uint8_t {

   EMPTY = 0,
   COMPONENT_A = 1,
   COMPONENT_B = 2,
   COMPONENT_C = 3,
   COMPONENT_P = 4,
   COMPONENT_Q = COMPONENT_P
};

// must be lock free always
struct SlotData {

    // No role for Component so not doing object visualization
    std::bitset<4> component;
    bool isUpdated{false};

    inline bool testIsEmpty() {
        return !component.any();
    }

    inline void SetComponentData(uint8_t c){
        component.set(c);
    }

    inline bool AnyComponent(){
        return component.test(COMPONENT::COMPONENT_A) || component.test(COMPONENT::COMPONENT_B) || component.test(COMPONENT::COMPONENT_C);
    }

    template<std::uint8_t C>
    inline bool testComponent() {
        return component.test(C);
    }

    template<std::uint8_t C>
    inline void ClearComponent(){
        component.reset(C);
    }

    template<std::uint8_t C>
    inline void SetComponent(){
        component.set(C);
    }
};

/*
 * sttaic vector with slots aligned to cache line to ensure false sharing
*/
template<class T, std::size_t NO_OF_SLOTS>
class ConveyorBelt {
public:
    T& operator[](std::size_t pos) {

        return *std::launder(reinterpret_cast<T*>(&m_Slots[pos]));
    }

    ~ConveyorBelt() {

        for(std::size_t pos = 0; pos < NO_OF_SLOTS; ++pos) {
            std::destroy_at(std::launder(reinterpret_cast<T*>(&m_Slots[pos])));
        }
    }

private:
    static constexpr std::size_t hardware_constructive_interference_size = 64;
    std::aligned_storage_t<sizeof(T), hardware_constructive_interference_size> m_Slots[NO_OF_SLOTS]{0};
};
