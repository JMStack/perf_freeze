
#include <limits.h>
#include <mach-o/dyld.h>
#include <mach-o/nlist.h>
#include <mach-o/stab.h>
#include <mach-o/getsect.h>
#include <string.h>
#include <dlfcn.h>
#include <stdbool.h>
#include <sys/types.h>

static uintptr_t firstCmdAfterHeader(const struct mach_header* const header)
{
   switch(header->magic)
   {
       case MH_MAGIC:
       case MH_CIGAM:
           return (uintptr_t)(header + 1);
       case MH_MAGIC_64:
       case MH_CIGAM_64:
           return (uintptr_t)(((struct mach_header_64*)header) + 1);
       default:
           // Header is corrupt
           return 0;
   }
}

static uint32_t imageIndexContainingAddress(const uintptr_t address)
{
   const uint32_t imageCount = _dyld_image_count();
   const struct mach_header* header = 0;
   
   for(uint32_t iImg = 0; iImg < imageCount; iImg++)
   {
       header = _dyld_get_image_header(iImg);
       if(header != NULL)
       {
           // Look for a segment command with this address within its range.
           uintptr_t addressWSlide = address - (uintptr_t)_dyld_get_image_vmaddr_slide(iImg);
           uintptr_t cmdPtr = firstCmdAfterHeader(header);
           if(cmdPtr == 0)
           {
               continue;
           }
           for(uint32_t iCmd = 0; iCmd < header->ncmds; iCmd++)
           {
               const struct load_command* loadCmd = (struct load_command*)cmdPtr;
               if(loadCmd->cmd == LC_SEGMENT)
               {
                   const struct segment_command* segCmd = (struct segment_command*)cmdPtr;
                   if(addressWSlide >= segCmd->vmaddr &&
                      addressWSlide < segCmd->vmaddr + segCmd->vmsize)
                   {
                       return iImg;
                   }
               }
               else if(loadCmd->cmd == LC_SEGMENT_64)
               {
                   const struct segment_command_64* segCmd = (struct segment_command_64*)cmdPtr;
                   if(addressWSlide >= segCmd->vmaddr &&
                      addressWSlide < segCmd->vmaddr + segCmd->vmsize)
                   {
                       return iImg;
                   }
               }
               cmdPtr += loadCmd->cmdsize;
           }
       }
   }
   return UINT_MAX;
}


static uintptr_t segmentBaseOfImageIndex(const uint32_t idx)
{
   const struct mach_header* header = _dyld_get_image_header(idx);
   
   // Look for a segment command and return the file image address.
   uintptr_t cmdPtr = firstCmdAfterHeader(header);
   if(cmdPtr == 0)
   {
       return 0;
   }
   for(uint32_t i = 0;i < header->ncmds; i++)
   {
       const struct load_command* loadCmd = (struct load_command*)cmdPtr;
       if(loadCmd->cmd == LC_SEGMENT)
       {
           const struct segment_command* segmentCmd = (struct segment_command*)cmdPtr;
           if(strcmp(segmentCmd->segname, SEG_LINKEDIT) == 0)
           {
               return segmentCmd->vmaddr - segmentCmd->fileoff;
           }
       }
       else if(loadCmd->cmd == LC_SEGMENT_64)
       {
           const struct segment_command_64* segmentCmd = (struct segment_command_64*)cmdPtr;
           if(strcmp(segmentCmd->segname, SEG_LINKEDIT) == 0)
           {
               return (uintptr_t)(segmentCmd->vmaddr - segmentCmd->fileoff);
           }
       }
       cmdPtr += loadCmd->cmdsize;
   }
   
   return 0;
}

int is_end_with(const char *str1, char *str2)
{
    if(str1 == NULL || str2 == NULL)
        return -1;
    unsigned long len1 = strlen(str1);
    unsigned long len2 = strlen(str2);
    if((len1 < len2) ||  (len1 == 0 || len2 == 0))
        return -1;
    while(len2 >= 1)
    {
        if(str2[len2 - 1] != str1[len1 - 1])
            return 0;
        len2--;
        len1--;
    }
    return 1;
}

bool findAppFrameworkSymbol(Dl_info *isolateSymbol, Dl_info *vmSymbol) {
    const uint32_t imageCount = _dyld_image_count();
    const char *imageName = NULL;
    uint32_t iImg = UINT_MAX;

    // 查找目标镜像
    for(iImg = 0; iImg < imageCount; iImg++)
    {
        imageName = _dyld_get_image_name(iImg);
        if (is_end_with(imageName, "App")) {
            break;
        }
    }
    if (iImg == -1 || imageName == NULL) {
        return false;
    }

    memset(isolateSymbol, 0, sizeof(Dl_info));

    const struct mach_header* header = _dyld_get_image_header(iImg);
    const uintptr_t appImageVMSlide = (uintptr_t)_dyld_get_image_vmaddr_slide(iImg);
    const uintptr_t segmentBase = segmentBaseOfImageIndex(iImg) + appImageVMSlide;
    if(segmentBase == 0)
    {
        return false;
    }

    isolateSymbol->dli_fname = imageName;
    isolateSymbol->dli_fbase = (void*)header;

    memcpy(vmSymbol, isolateSymbol, sizeof(Dl_info));

    uintptr_t cmdPtr = firstCmdAfterHeader(header);

    if(cmdPtr == 0)
    {
        return false;
    }

    // 遍历 Load Command 查找符号表
    for(uint32_t iCmd = 0; iCmd < header->ncmds; iCmd++)
    {
        const struct load_command* loadCmd = (struct load_command*)cmdPtr;
        if(loadCmd->cmd == LC_SYMTAB)
        {
            const struct symtab_command* symtabCmd = (struct symtab_command*)cmdPtr;
            const struct nlist_64* symbolTable = (struct nlist_64*)(segmentBase + symtabCmd->symoff);
            const uintptr_t stringTable = segmentBase + symtabCmd->stroff;

            // 遍历符号表找到目标符号
            for(uint32_t iSym = 0; iSym < symtabCmd->nsyms; iSym++)
            {
               const struct nlist_64* symbol = symbolTable + iSym;
               char *symbolName = (char*)((intptr_t)stringTable + (intptr_t)symbol->n_un.n_strx);

               if (strcmp(symbolName, "_kDartIsolateSnapshotInstructions") == 0) {
                    isolateSymbol->dli_sname = symbolName;
                    isolateSymbol->dli_saddr = (void *)(symbol->n_value + appImageVMSlide);
               }

               if (strcmp(symbolName, "_kDartVmSnapshotInstructions") == 0) {
                   vmSymbol->dli_sname = symbolName;
                   vmSymbol->dli_saddr = (void *)(symbol->n_value + appImageVMSlide);
               }
            }
            
            break;
        }
        cmdPtr += loadCmd->cmdsize;
    }

    return false;
}

bool dladdrResolver(const uintptr_t address, Dl_info* const info)
{
   info->dli_fname = NULL;
   info->dli_fbase = NULL;
   info->dli_sname = NULL;
   info->dli_saddr = NULL;

   const uint32_t idx = imageIndexContainingAddress(address);
   if(idx == UINT_MAX)
   {
       return false;
   }
   const struct mach_header* header = _dyld_get_image_header(idx);
   const uintptr_t imageVMAddrSlide = (uintptr_t)_dyld_get_image_vmaddr_slide(idx);
   const uintptr_t addressWithSlide = address - imageVMAddrSlide;
   const uintptr_t segmentBase = segmentBaseOfImageIndex(idx) + imageVMAddrSlide;
   if(segmentBase == 0)
   {
       return false;
   }

   info->dli_fname = _dyld_get_image_name(idx);
   info->dli_fbase = (void*)header;

   // Find symbol tables and get whichever symbol is closest to the address.
   const struct nlist_64* bestMatch = NULL;
   uintptr_t bestDistance = ULONG_MAX;
   uintptr_t cmdPtr = firstCmdAfterHeader(header);
   if(cmdPtr == 0)
   {
       return false;
   }
   for(uint32_t iCmd = 0; iCmd < header->ncmds; iCmd++)
   {
       const struct load_command* loadCmd = (struct load_command*)cmdPtr;
       if(loadCmd->cmd == LC_SYMTAB)
       {
           const struct symtab_command* symtabCmd = (struct symtab_command*)cmdPtr;
           const struct nlist_64* symbolTable = (struct nlist_64*)(segmentBase + symtabCmd->symoff);
           const uintptr_t stringTable = segmentBase + symtabCmd->stroff;

           for(uint32_t iSym = 0; iSym < symtabCmd->nsyms; iSym++)
           {
               // Skip all debug N_STAB symbols
               if ((symbolTable[iSym].n_type & N_STAB) != 0)
               {
                   continue;
               }

               // If n_value is 0, the symbol refers to an external object.
               if(symbolTable[iSym].n_value != 0)
               {
                   uintptr_t symbolBase = symbolTable[iSym].n_value;
                   uintptr_t currentDistance = addressWithSlide - symbolBase;
                   if((addressWithSlide >= symbolBase) &&
                      (currentDistance <= bestDistance))
                   {
                       bestMatch = symbolTable + iSym;
                       bestDistance = currentDistance;
                   }
               }
           }
           if(bestMatch != NULL)
           {
               info->dli_saddr = (void*)(bestMatch->n_value + imageVMAddrSlide);
               if(bestMatch->n_desc == 16)
               {
                   info->dli_sname = NULL;
               }
               else
               {
                   info->dli_sname = (char*)((intptr_t)stringTable + (intptr_t)bestMatch->n_un.n_strx);
                   if(*info->dli_sname == '_')
                   {
                       info->dli_sname++;
                   }
               }
               break;
           }
       }
       cmdPtr += loadCmd->cmdsize;
   }
   
   return true;
}


