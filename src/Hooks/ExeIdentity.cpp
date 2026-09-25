#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cstdint>
#include "ExeIdentity.h"
#include "Addresses.h"
#include "Log.h"

namespace ExeIdentity {

    bool IsSupported() {
        const HMODULE exe = GetModuleHandleA(nullptr);
        if (!exe) return false;

        const auto image = reinterpret_cast<const std::uint8_t*>(exe);
        const auto dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(image);
        if (dos->e_magic != IMAGE_DOS_SIGNATURE) return false;

        const auto nt = reinterpret_cast<const IMAGE_NT_HEADERS32*>(image + dos->e_lfanew);
        if (nt->Signature != IMAGE_NT_SIGNATURE) return false;

        const bool supported = reinterpret_cast<std::uintptr_t>(exe) == Addr::ImageBase
                            && nt->FileHeader.Machine == IMAGE_FILE_MACHINE_I386
                            && nt->FileHeader.TimeDateStamp == Addr::TimeDateStamp
                            && nt->OptionalHeader.SizeOfImage == Addr::SizeOfImage;

        if (supported)
            Log::Line("NFSC.exe v1.4 detected.");
        else
            Log::Line("This NFSC.exe is not the supported v1.4 build (timestamp 0x%08X, image size 0x%08X); nothing was changed.",
                      static_cast<unsigned>(nt->FileHeader.TimeDateStamp),
                      static_cast<unsigned>(nt->OptionalHeader.SizeOfImage));
        return supported;
    }

}
