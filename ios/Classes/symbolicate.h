
#include <sys/types.h>
#include <dlfcn.h>

#ifndef symbolicate_h
#define symbolicate_h

bool findAppFrameworkSymbol(Dl_info *isolateSymbol, Dl_info *vmSymbol);

bool dladdrResolver(const uintptr_t address, Dl_info* const info);

#endif
