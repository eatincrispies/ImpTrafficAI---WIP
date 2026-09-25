#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cstring>
#include "Memory.h"

namespace Memory {

    namespace {

        bool GuardedCopy(void* destination, const void* source, std::size_t size) {
            __try {
                std::memcpy(destination, source, size);
                return true;
            }
            __except (EXCEPTION_EXECUTE_HANDLER) {
                return false;
            }
        }

    }

    bool Read(std::uintptr_t va, void* out, std::size_t size) {
        return GuardedCopy(out, reinterpret_cast<const void*>(va), size);
    }

    bool Matches(std::uintptr_t va, const std::uint8_t* bytes, std::size_t size) {
        std::uint8_t current[32] = {};
        if (size > sizeof(current) || !Read(va, current, size)) return false;
        return std::memcmp(current, bytes, size) == 0;
    }

    bool WriteCode(std::uintptr_t va, const void* bytes, std::size_t size) {
        const LPVOID target = reinterpret_cast<LPVOID>(va);
        DWORD previous = 0;
        if (!VirtualProtect(target, size, PAGE_EXECUTE_READWRITE, &previous)) return false;

        const bool written = GuardedCopy(target, bytes, size);

        DWORD ignored = 0;
        VirtualProtect(target, size, previous, &ignored);
        FlushInstructionCache(GetCurrentProcess(), target, size);
        return written;
    }

    void Hex(const std::uint8_t* bytes, std::size_t size, char* out, std::size_t outSize) {
        static const char digits[] = "0123456789ABCDEF";
        if (outSize == 0) return;

        std::size_t used = 0;
        for (std::size_t i = 0; i < size && used + 3 < outSize; ++i) {
            if (i != 0) out[used++] = ' ';
            out[used++] = digits[bytes[i] >> 4];
            out[used++] = digits[bytes[i] & 0x0F];
        }
        out[used] = '\0';
    }

}
