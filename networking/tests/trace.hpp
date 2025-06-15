#include <iostream>
#include <cstdlib>
#include <exception>

#if defined(_WIN32)
  #define NOMINMAX
  #include <windows.h>
  #include <dbghelp.h>
  #pragma comment(lib, "dbghelp.lib")
#elif defined(__GNUC__) || defined(__clang__)
  #include <execinfo.h>
#endif

namespace {

void print_backtrace()
{
#if defined(_WIN32)

    void*  frames[62];
    USHORT n = ::CaptureStackBackTrace(0, std::size(frames), frames, nullptr);

    HANDLE proc = ::GetCurrentProcess();
    ::SymInitialize(proc, nullptr, TRUE);

    SYMBOL_INFO* sym = static_cast<SYMBOL_INFO*>(
        std::calloc(sizeof(SYMBOL_INFO) + 256, 1));
    sym->MaxNameLen   = 255;
    sym->SizeOfStruct = sizeof(SYMBOL_INFO);

    for (USHORT i = 0; i < n; ++i)
    {
        DWORD64 addr = reinterpret_cast<DWORD64>(frames[i]);
        ::SymFromAddr(proc, addr, nullptr, sym);
        std::cerr << i << ": " << sym->Name
                  << " 0x" << std::hex << sym->Address << std::dec << '\n';
    }
    std::free(sym);

#elif defined(__GNUC__) || defined(__clang__)

    void*  frames[32];
    int    n = ::backtrace(frames, std::size(frames));
    ::backtrace_symbols_fd(frames, n, 2);          // print to stderr (fd 2)

#else
    std::cerr << "(no back-trace available on this platform)\n";
#endif
}

} // unnamed namespace

// Call once at the start of main():
void install_custom_terminate_handler()
{
    std::set_terminate([] {
        std::cerr << "std::terminate() called!\n";
        print_backtrace();
        std::abort();          // keep default behaviour after dumping
    });
}