//
//  ui_thread_stacktrace.h
//  Pods
//
//  Created by jackyma on 2024/6/21.
//

#ifndef ui_thread_stacktrace_h
#define ui_thread_stacktrace_h

#include <mach/mach_types.h>
#include <dlfcn.h>

typedef struct StacktraceInfo {
    Dl_info info;
    uintptr_t address;
    uintptr_t isolate_start;
    uintptr_t vm_start;
} TraceInfo;


__attribute__((visibility("default"))) __attribute__((used)) void uiThreadStacktrace(TraceInfo *info, uint32_t length);

#endif /* ui_thread_stacktrace_h */
