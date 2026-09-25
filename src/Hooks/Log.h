#pragma once

namespace Log {

    void Open(void* module);
    void Close();
    void Line(const char* format, ...);

}
