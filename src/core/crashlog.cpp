#include "crashlog.h"
#include "version.h"
#include <cstdio>
#include <cstring>
#include <ctime>
#include <csignal>
#include <exception>
#include <cstdlib>
#ifdef _WIN32
#include <windows.h>
#else
#include <execinfo.h>
#include <unistd.h>
#endif

namespace {
const int RING = 40;
char g_ring[RING][200];
int g_head = 0, g_count = 0;
char g_state[400] = "";
char g_dir[600] = "";

void writeReport(const char* what, void** frames, int nFrames) {
  char path[700];
  std::snprintf(path, sizeof path, "%s/crash_%lld.txt", g_dir, (long long)std::time(nullptr));
  FILE* f = std::fopen(path, "w");
  if (!f) f = std::fopen("crash.txt", "w");
  if (!f) return;
  std::fprintf(f, "%s\nError: %s\nState: %s\n\nCall stack (addresses):\n", HW_VERSION_FULL, what, g_state);
#ifdef _WIN32
  HMODULE mod = GetModuleHandleA(nullptr);
  for (int i = 0; i < nFrames; i++) std::fprintf(f, "  #%02d  exe+0x%llx\n", i, (unsigned long long)((char*)frames[i] - (char*)mod));
#else
  char** sym = backtrace_symbols(frames, nFrames);
  for (int i = 0; i < nFrames; i++) std::fprintf(f, "  #%02d  %s\n", i, sym ? sym[i] : "?");
#endif
  std::fprintf(f, "\nLast messages (oldest first):\n");
  for (int i = 0; i < g_count; i++) std::fprintf(f, "  %s\n", g_ring[(g_head - g_count + i + RING) % RING]);
  std::fclose(f);
}

#ifdef _WIN32
LONG WINAPI onCrash(EXCEPTION_POINTERS* ep) {
  void* frames[48];
  int n = CaptureStackBackTrace(0, 48, frames, nullptr);
  char what[96];
  std::snprintf(what, sizeof what, "exception 0x%08lx at exe+0x%llx", ep->ExceptionRecord->ExceptionCode,
                (unsigned long long)((char*)ep->ExceptionRecord->ExceptionAddress - (char*)GetModuleHandleA(nullptr)));
  writeReport(what, frames, n);
  return EXCEPTION_CONTINUE_SEARCH;
}
#else
void onSignal(int sig) {
  void* frames[48];
  int n = backtrace(frames, 48);
  writeReport(sig == SIGSEGV ? "segmentation fault" : sig == SIGABRT ? "abort" : sig == SIGFPE ? "floating point error" : "fatal signal", frames, n);
  std::signal(sig, SIG_DFL);
  std::raise(sig);
}
#endif
void onTerminate() {
  void* frames[1] = {nullptr};
  writeReport("unhandled C++ exception (std::terminate)", frames, 0);
  std::abort();
}
}  // namespace

namespace crashlog {
void init(const std::string& dir) {
  std::snprintf(g_dir, sizeof g_dir, "%s", dir.c_str());
#ifdef _WIN32
  SetUnhandledExceptionFilter(onCrash);
#else
  for (int s : {SIGSEGV, SIGABRT, SIGFPE, SIGILL, SIGBUS}) std::signal(s, onSignal);
#endif
  std::set_terminate(onTerminate);
}
void note(const std::string& line) {
  char stamp[16];
  std::time_t t = std::time(nullptr);
  std::strftime(stamp, sizeof stamp, "%H:%M:%S", std::localtime(&t));
  std::snprintf(g_ring[g_head], sizeof g_ring[0], "[%s] %s", stamp, line.c_str());
  g_head = (g_head + 1) % RING;
  if (g_count < RING) g_count++;
}
void setState(const std::string& s) { std::snprintf(g_state, sizeof g_state, "%s", s.c_str()); }
}  // namespace crashlog
