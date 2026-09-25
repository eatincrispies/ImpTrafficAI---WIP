#pragma once
#include <cstdint>
#include "Addresses.h"

namespace Hook {

    bool Detour(const char* name, const Addr::CodeBytes& site, void* replacement, void** original);
    bool RedirectCall(const char* name, const Addr::CodeBytes& site, void* replacement, void** original);
    bool SwapSlot(const char* name, const Addr::Slot& slot, void* replacement, void** original);
    bool Replace(const char* name, const Addr::CodeBytes& site, const std::uint8_t* bytes);
    void RemoveAll();
    int  Failures();

}
