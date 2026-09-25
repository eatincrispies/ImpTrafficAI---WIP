#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <share.h>
#include "Log.h"

namespace Log {

    namespace {

        std::FILE* gFile = nullptr;
        SRWLOCK    gLock = SRWLOCK_INIT;

    }

    void Open(void* module) {
        AcquireSRWLockExclusive(&gLock);
        if (!gFile) {
            char path[MAX_PATH] = {};
            const DWORD length = GetModuleFileNameA(static_cast<HMODULE>(module), path, MAX_PATH);
            if (length != 0 && length < MAX_PATH) {
                char* slash = std::strrchr(path, '\\');
                const std::size_t folder = slash ? static_cast<std::size_t>(slash - path) + 1 : 0;
                const char name[] = "ImpTrafficAI.log";
                if (folder + sizeof(name) <= MAX_PATH) {
                    std::memcpy(path + folder, name, sizeof(name));
                    gFile = _fsopen(path, "w", _SH_DENYNO);
                }
            }
        }
        ReleaseSRWLockExclusive(&gLock);
    }

    void Close() {
        AcquireSRWLockExclusive(&gLock);
        if (gFile) {
            std::fclose(gFile);
            gFile = nullptr;
        }
        ReleaseSRWLockExclusive(&gLock);
    }

    void Line(const char* format, ...) {
        char text[768];
        va_list args;
        va_start(args, format);
        std::vsnprintf(text, sizeof(text), format, args);
        va_end(args);

        char line[800];
        std::snprintf(line, sizeof(line), "[ImpTrafficAI] %s\n", text);
        OutputDebugStringA(line);

        AcquireSRWLockExclusive(&gLock);
        if (gFile) {
            std::fputs(line, gFile);
            std::fflush(gFile);
        }
        ReleaseSRWLockExclusive(&gLock);
    }

}
