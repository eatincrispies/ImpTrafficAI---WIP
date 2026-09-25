#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cstdint>
#include <cstring>
#include "Hook.h"
#include "Log.h"
#include "Memory.h"

namespace Hook {

    namespace {

        constexpr int         kCapacity        = 16;
        constexpr std::size_t kMaxBytes        = sizeof(Addr::CodeBytes::bytes);
        constexpr std::size_t kTrampolineBytes = 32;

        struct Record {
            std::uintptr_t va;
            std::uint8_t   original[kMaxBytes];
            std::size_t    size;
        };

        Record gRecords[kCapacity] = {};
        int    gCount = 0;
        int    gFailures = 0;

        bool Fail(const char* name, const char* reason) {
            Log::Line("%s: not hooked, %s.", name, reason);
            ++gFailures;
            return false;
        }

        void EncodeRelative(std::uint8_t* instruction, std::uintptr_t instructionVa, std::uintptr_t destination) {
            const auto delta = static_cast<std::int32_t>(destination - (instructionVa + 5u));
            std::memcpy(instruction + 1, &delta, sizeof(delta));
        }

        bool Verify(const char* name, std::uintptr_t va, const std::uint8_t* expected, std::size_t size) {
            if (Memory::Matches(va, expected, size)) return true;

            std::uint8_t found[kMaxBytes] = {};
            char expectedHex[32] = "";
            char foundHex[32] = "unreadable";
            Memory::Hex(expected, size, expectedHex, sizeof(expectedHex));
            if (Memory::Read(va, found, size)) Memory::Hex(found, size, foundHex, sizeof(foundHex));
            Log::Line("%s: not hooked, 0x%08X should hold %s but holds %s; another mod probably changed it.",
                      name, static_cast<unsigned>(va), expectedHex, foundHex);
            ++gFailures;
            return false;
        }

        void Remember(std::uintptr_t va, const std::uint8_t* bytes, std::size_t size) {
            Record& record = gRecords[gCount++];
            record.va = va;
            record.size = size;
            std::memcpy(record.original, bytes, size);
        }

    }

    bool Detour(const char* name, const Addr::CodeBytes& site, void* replacement, void** original) {
        if (gCount == kCapacity) return Fail(name, "the hook table is full");
        if (site.size < 5 || site.size > kMaxBytes) return Fail(name, "the stolen byte count is invalid");
        if (!Verify(name, site.va, site.bytes, site.size)) return false;

        const auto trampoline = static_cast<std::uint8_t*>(
            VirtualAlloc(nullptr, kTrampolineBytes, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE));
        if (!trampoline) return Fail(name, "no memory was available for the trampoline");

        std::memset(trampoline, 0xCC, kTrampolineBytes);
        std::memcpy(trampoline, site.bytes, site.size);
        trampoline[site.size] = 0xE9;
        EncodeRelative(trampoline + site.size, reinterpret_cast<std::uintptr_t>(trampoline) + site.size, site.va + site.size);
        FlushInstructionCache(GetCurrentProcess(), trampoline, kTrampolineBytes);

        std::uint8_t jump[kMaxBytes];
        std::memset(jump, 0x90, sizeof(jump));
        jump[0] = 0xE9;
        EncodeRelative(jump, site.va, reinterpret_cast<std::uintptr_t>(replacement));

        *original = trampoline;
        if (!Memory::WriteCode(site.va, jump, site.size)) {
            *original = nullptr;
            return Fail(name, "the jump could not be written");
        }

        Remember(site.va, site.bytes, site.size);
        Log::Line("%s: hooked at 0x%08X.", name, static_cast<unsigned>(site.va));
        return true;
    }

    bool RedirectCall(const char* name, const Addr::CodeBytes& site, void* replacement, void** original) {
        if (gCount == kCapacity) return Fail(name, "the hook table is full");
        if (site.size != 5 || site.bytes[0] != 0xE8) return Fail(name, "the site is not a relative call");
        if (!Verify(name, site.va, site.bytes, site.size)) return false;

        std::int32_t delta = 0;
        std::memcpy(&delta, site.bytes + 1, sizeof(delta));
        *original = reinterpret_cast<void*>(site.va + 5u + static_cast<std::uintptr_t>(delta));

        std::uint8_t call[5] = { 0xE8, 0x00, 0x00, 0x00, 0x00 };
        EncodeRelative(call, site.va, reinterpret_cast<std::uintptr_t>(replacement));
        if (!Memory::WriteCode(site.va, call, sizeof(call))) {
            *original = nullptr;
            return Fail(name, "the call could not be written");
        }

        Remember(site.va, site.bytes, site.size);
        Log::Line("%s: call redirected at 0x%08X.", name, static_cast<unsigned>(site.va));
        return true;
    }

    bool SwapSlot(const char* name, const Addr::Slot& slot, void* replacement, void** original) {
        if (gCount == kCapacity) return Fail(name, "the hook table is full");

        const auto expected = static_cast<std::uint32_t>(slot.expected);
        std::uint8_t expectedBytes[sizeof(expected)];
        std::memcpy(expectedBytes, &expected, sizeof(expected));
        if (!Verify(name, slot.va, expectedBytes, sizeof(expectedBytes))) return false;

        *original = reinterpret_cast<void*>(slot.expected);
        const auto pointer = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(replacement));
        if (!Memory::WriteCode(slot.va, &pointer, sizeof(pointer))) {
            *original = nullptr;
            return Fail(name, "the vtable slot could not be written");
        }

        Remember(slot.va, expectedBytes, sizeof(expectedBytes));
        Log::Line("%s: vtable slot 0x%08X hooked.", name, static_cast<unsigned>(slot.va));
        return true;
    }

    bool Replace(const char* name, const Addr::CodeBytes& site, const std::uint8_t* bytes) {
        if (gCount == kCapacity) return Fail(name, "the hook table is full");
        if (site.size == 0 || site.size > kMaxBytes) return Fail(name, "the patch size is invalid");
        if (!Verify(name, site.va, site.bytes, site.size)) return false;
        if (!Memory::WriteCode(site.va, bytes, site.size)) return Fail(name, "the patch could not be written");

        Remember(site.va, site.bytes, site.size);
        Log::Line("%s: patched at 0x%08X.", name, static_cast<unsigned>(site.va));
        return true;
    }

    void RemoveAll() {
        while (gCount > 0) {
            const Record& record = gRecords[--gCount];
            Memory::WriteCode(record.va, record.original, record.size);
        }
    }

    int Failures() {
        return gFailures;
    }

}
