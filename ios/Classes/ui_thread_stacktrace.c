#include "ui_thread_stacktrace.h"
#include <signal.h>
#include <mach/mach.h>
#include <limits.h>
#include <string.h>
#include <pthread/pthread.h>
#include <mach-o/arch.h>
#include <mach/machine/thread_status.h>
#include <mach/mach_types.h>
#include <stdbool.h>
#include <sys/ucontext.h>
#include <malloc/_malloc.h>

#include "symbolicate.h"


#ifdef __arm64__
    #define STRUCT_MCONTEXT_L _STRUCT_MCONTEXT64
    #define MY_EXCEPTION_STATE_COUNT ARM_EXCEPTION_STATE64_COUNT
    #define MY_EXCEPITON_STATE ARM_EXCEPTION_STATE64
    #define MY_THREAD_STATE_COUTE ARM_THREAD_STATE64_COUNT
    #define MY_THREAD_STATE ARM_THREAD_STATE64
    #define INSTRUCTION_ADDRESS(A) ((A) & ~(3UL))
#else /* x86_64 */
    #define STRUCT_MCONTEXT_L _STRUCT_MCONTEXT
    #define MY_EXCEPTION_STATE_COUNT x86_EXCEPTION_STATE64_COUNT
    #define MY_EXCEPITON_STATE x86_EXCEPTION_STATE64
    #define MY_THREAD_STATE_COUTE x86_THREAD_STATE64_COUNT
    #define MY_THREAD_STATE x86_THREAD_STATE64
    #define INSTRUCTION_ADDRESS(A) (A)
#endif

typedef struct FrameEntry
{
  struct FrameEntry* previous;
  
  uintptr_t return_address;
} FrameEntry;

thread_t _thread_self(void)
{
   thread_t thread_self = mach_thread_self();
   mach_port_deallocate(mach_task_self(), thread_self);
   return thread_self;
}

bool suspendUIThread(thread_t *UIThread)
{

   thread_act_array_t allThreads = NULL;
   mach_msg_type_number_t numThreads = 0;
   
   kern_return_t kr;
   const task_t thisTask = mach_task_self();
   const thread_t thisThread = (thread_t)_thread_self();
   
   if((kr = task_threads(thisTask, &allThreads, &numThreads)) != KERN_SUCCESS)
   {
       return false;
   }
   
   for(mach_msg_type_number_t i = 0; i < numThreads; i++)
   {
       thread_t thread = allThreads[i];
       
       char name[256];
       pthread_t pt = pthread_from_mach_thread_np(thread);
       int rc = pthread_getname_np(pt, name, sizeof name);
       if (rc != 0 || name[0] == 0) {
           continue;
       }
       
       if(thread != thisThread && strstr(name, "io.flutter") && strstr(name, "ui"))
       {
           if((kr = thread_suspend(thread)) == KERN_SUCCESS)
           {
               *UIThread = thread;
               return true;
           }
       
       }
   }
   return false;
}

void fillSSMContext(STRUCT_MCONTEXT_L *_mcontext, thread_t thread)
{
   mach_msg_type_number_t stateCount = MY_THREAD_STATE_COUTE;
   kern_return_t ret = thread_get_state(thread, MY_THREAD_STATE, (thread_state_t)&_mcontext->__ss, &stateCount);
   if (ret != KERN_SUCCESS) {
       return;
   }
}

uintptr_t normaliseInstructionPointer(uintptr_t ip)
{
    #ifdef __arm64__
        return ip & 0x0000000fffffffff;
    #else 
        return ip;
    #endif
}

void fillStackFrame(intptr_t *entry, int64_t maxDepth, STRUCT_MCONTEXT_L *_mcontext)
{
    if (entry == NULL || _mcontext == NULL) return;
    if (maxDepth <= 2) return;

    int64_t depth = 0;

    #ifdef __arm64__

    entry[depth++] = normaliseInstructionPointer(_mcontext->__ss.__pc);
    entry[depth++] = normaliseInstructionPointer(_mcontext->__ss.__lr);

    FrameEntry *f = (struct FrameEntry*)(_mcontext->__ss.__fp);

    #else 

    entry[depth++] = normaliseInstructionPointer(_mcontext->__ss.__rip);

    FrameEntry *f = (struct FrameEntry*)(_mcontext->__ss.__rbp);

    #endif

    while (depth < maxDepth && f != 0 && f->return_address != 0) {
        entry[depth++] = normaliseInstructionPointer(f->return_address);
        f = f->previous;
    }
}

void uiThreadStacktrace(TraceInfo *info, uint32_t length)
{
   thread_t ui_thread;
   bool res = suspendUIThread(&ui_thread);
   if (res == false) return;
   
   STRUCT_MCONTEXT_L context;
   fillSSMContext(&context, ui_thread);
   
   intptr_t *stackAddress = (intptr_t *)malloc(sizeof(intptr_t) * length);
   
   fillStackFrame(stackAddress, length, &context);
   
   thread_resume(ui_thread);
    
   Dl_info isolateInfo;
   Dl_info vmInfo;
    
   findAppFrameworkSymbol(&isolateInfo, &vmInfo);
    
   uint32_t i = 0;
   while (stackAddress[i] != 0 && i < length) {
       intptr_t address = INSTRUCTION_ADDRESS(stackAddress[i]);
       info[i].address = (intptr_t)address;
       info[i].isolate_start = isolateInfo.dli_saddr;
       info[i].vm_start = vmInfo.dli_saddr;
       dladdrResolver(address, &info[i++].info);
   }

   free(stackAddress);
}

