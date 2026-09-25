#pragma once
#include <cstddef>
#include <cstdint>

namespace Memory {

    bool Read(std::uintptr_t va, void* out, std::size_t size);
    bool Matches(std::uintptr_t va, const std::uint8_t* bytes, std::size_t size);
    bool WriteCode(std::uintptr_t va, const void* bytes, std::size_t size);
    void Hex(const std::uint8_t* bytes, std::size_t size, char* out, std::size_t outSize);

    template <typename T>
    T* At(const void* object, unsigned offset) {
        const auto base = reinterpret_cast<std::uintptr_t>(object);
        return reinterpret_cast<T*>(base + offset);
    }

    template <typename T>
    T* Global(std::uintptr_t va) {
        return reinterpret_cast<T*>(va);
    }

    template <typename Result, typename... Args>
    Result Invoke(void* object, unsigned slot, Args... args) {
        using Method = Result(__fastcall*)(void*, void*, Args...);
        void** const table = *static_cast<void***>(object);
        return reinterpret_cast<Method>(table[slot / sizeof(void*)])(object, nullptr, args...);
    }

    template <typename Result, typename... Args>
    Result Call(std::uintptr_t function, void* object, Args... args) {
        using Method = Result(__fastcall*)(void*, void*, Args...);
        return reinterpret_cast<Method>(function)(object, nullptr, args...);
    }

}
