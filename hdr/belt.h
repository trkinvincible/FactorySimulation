#pragma once

enum /*class*/ COMPONENT : uint8_t {

   EMPTY = 0,
   C_A = 1,
   C_B = 1 << 2,
   C_C = 1 << 3,
   C_P = 1 << 4,
   C_Q = C_P
};

struct SlotData {

    // No role for Component so not doing object visualization
    std::bitset<8> component;
    bool isUpdated{false};
    // must be alway free always

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

    inline bool AnyComponent(){
        return component.test(COMPONENT::C_A) || component.test(COMPONENT::C_B) || component.test(COMPONENT::C_C);
    }
};

template<class T, std::size_t NO_OF_SLOTS>
class ConveyorBelt {
public:
    T& operator[](std::size_t pos)
    {
        return *std::launder(reinterpret_cast<T*>(&m_Slots[pos]));
    }

    ~ConveyorBelt()
    {
        for(std::size_t pos = 0; pos < NO_OF_SLOTS; ++pos) {
            std::destroy_at(std::launder(reinterpret_cast<T*>(&m_Slots[pos])));
        }
    }

private:
    static constexpr std::size_t hardware_constructive_interference_size = 64;
    std::aligned_storage_t<sizeof(T), hardware_constructive_interference_size> m_Slots[NO_OF_SLOTS];

    template<std::size_t N>
    friend class Worker;
};
