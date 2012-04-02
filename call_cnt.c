#define _GNU_SOURCE
#include <dlfcn.h>
#include <link.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#ifdef DEBUG
#include <stdio.h>
#endif

#include <sys/mman.h>
#include "call_cnt.h"

#define LIB_FOUND_CODE 42

struct callback_data{
    char const * lib_name;
    struct dl_phdr_info * result;
    size_t size;
    ElfW(Addr)* rel;
    ElfW(Word) relsz;
    ElfW(Addr) base;
    ElfW(Sym)* symtab;
};

#pragma pack(push, 1)
struct cnt_code{
    char iinc[3];
    ElfW(Addr) cnt_addr;
    char imov;
    ElfW(Addr) jmp_addr;
    char ijmp[2];
};
#pragma pack(pop)

struct call_cnt_entry{
    ElfW(Addr)* gotent;
    int counter;
    struct cnt_code code;
    ElfW(Section) shndx;
};

struct call_cnt{
    size_t len;
    struct call_cnt_entry* entries;
};


int dl_phdr_callback(struct dl_phdr_info *info, size_t size, void *data){
    int i;
    ElfW(Dyn)* dyn;
    char* soname;
    struct callback_data* cb_data = (struct callback_data*)data;
    ElfW(Addr) strtab = 0, pltgot = 0, rel = 0, symtab = 0;
    ElfW(Word) soname_off = 0, relsz = 0;
    ElfW(Addr) base = info->dlpi_addr;
    #ifdef DEBUG
    printf("lib: %s\n", (*info).dlpi_name);
    #endif
    for(i = 0; i < info->dlpi_phnum;i++){
        if(info->dlpi_phdr[i].p_type == PT_DYNAMIC){
            int index = 0;
            dyn = (ElfW(Dyn)*)(info->dlpi_addr + info->dlpi_phdr[i].p_vaddr);
            while(dyn[index].d_tag != DT_NULL){
                switch(dyn[index].d_tag){
                case DT_SONAME:
                    soname_off = dyn[index].d_un.d_val; break;
                case DT_STRTAB:
                    strtab = dyn[index].d_un.d_ptr; break;
                case DT_PLTGOT:
                    pltgot = dyn[index].d_un.d_ptr; break;
                case DT_JMPREL:
                    rel = dyn[index].d_un.d_ptr; break;
                case DT_PLTRELSZ:
                    relsz = dyn[index].d_un.d_val; break;
                case DT_SYMTAB:
                    symtab = dyn[index].d_un.d_ptr; break;
                }
                index += 1;
            }
        break;
        }
    }
    if(pltgot)
        base = 0;
    if(strtab && soname_off){
        soname = (char*)(base + strtab + soname_off);
        #ifdef DEBUG
        printf("\tNAME: %s\n", soname);
        #endif
        if(!strcmp(soname, cb_data->lib_name)){
            cb_data->result = info;
            cb_data->size = size;
            cb_data->rel = (ElfW(Addr)*)rel;
            cb_data->relsz = relsz;
            cb_data->symtab = (ElfW(Sym)*)symtab;
            cb_data->base = info->dlpi_addr;
            return LIB_FOUND_CODE;
        }
    }
    return 0;
}

int intercept(struct call_cnt ** desc, char const * lib_name){
    int i;
    struct callback_data data;
    ElfW(Rel)* rel;
    data.lib_name = lib_name;
    data.result = NULL;
    data.size = 0;
    data.rel = NULL;
    if (dl_iterate_phdr(dl_phdr_callback, &data) != LIB_FOUND_CODE){
        #ifdef DEBUG
        perror("library not found");
        #endif
        return -1;
    }
    if (data.rel == NULL){
        #ifdef DEBUG
        perror("realocation table not found");
        #endif
        return -1;
    }
    rel = (ElfW(Rel)*)data.rel;
    *desc = mmap(0, sizeof(struct call_cnt),
        PROT_WRITE|PROT_READ, MAP_PRIVATE|MAP_ANONYMOUS, 0, 0);
    if (*desc == (void*)-1){
        #ifdef DEBUG
        perror("mmap call_cnt");
        #endif
        return -1;
    }
    (*desc)->len = data.relsz / sizeof(ElfW(Rel));
    (*desc)->entries = mmap(0, sizeof(struct call_cnt_entry) * (*desc)->len,
        PROT_WRITE|PROT_READ|PROT_EXEC, MAP_PRIVATE|MAP_ANONYMOUS, 0, 0);
    if ((*desc)->entries == (void*)-1){
        #ifdef DEBUG
        perror("mmap entries");
        #endif
        return -1;
    }
    for(i = 0; i * sizeof(ElfW(Rel)) < data.relsz; i++){
        if (ELF32_R_TYPE(rel[i].r_info) == R_386_JMP_SLOT){
            struct call_cnt_entry* cur = &((*desc)->entries[i]);
            ElfW(Addr)* gotentry = (ElfW(Addr)*) (data.base + rel[i].r_offset);
            cur->code.iinc[0] = '\xF0';
            cur->code.iinc[1] = '\xFF';
            cur->code.iinc[2] = '\x05';
            cur->code.cnt_addr = (ElfW(Addr))&cur->counter;
            cur->code.imov = '\xB8';
            cur->code.ijmp[0] = '\xFF';
            cur->code.ijmp[1] = '\xE0';
            cur->code.jmp_addr = *gotentry;
            *gotentry = (ElfW(Addr))&cur->code;
            cur->shndx = data.symtab[ELF32_R_SYM(rel[i].r_info)].st_shndx;
            cur->gotent = gotentry;
        }
    }
    return 0;
}

int release_stats(struct call_cnt * desc){
    if(munmap(desc->entries, sizeof(struct call_cnt_entry) * desc->len)){
        #ifdef DEBUG
        perror("munmap entries");
        #endif
        return -1;
    }
    if(munmap(desc, sizeof(struct call_cnt))){
        #ifdef DEBUG
        perror("munmap call_cnt");
        #endif
        return -1;
    }
    return 0;
}


int stop_intercepting(struct call_cnt * desc){
    size_t i;
    for(i = 0; i < desc->len; i++){
        *(desc->entries[i].gotent) = desc->entries[i].code.jmp_addr;
    }
    return 0;
}

ssize_t get_num_calls(struct call_cnt * desc, int intern){
    size_t i;
    ssize_t res = 0;
    for(i = 0; i < desc->len; i++){
        if(desc->entries[i].shndx == SHN_UNDEF){
            if(!intern)
                res += desc->entries[i].counter;
        } else {
            if(intern)
                res += desc->entries[i].counter;
        }
    }
    return res;
}

ssize_t get_num_intern_calls(struct call_cnt * desc){
    return get_num_calls(desc, 1);
}

ssize_t get_num_extern_calls(struct call_cnt * desc){
    return get_num_calls(desc, 0);
}

int print_stats_to_stream(FILE * stream, struct call_cnt * desc){
    size_t i;
    Dl_info info;
    for(i = 0; i < desc->len; i++){
        if(dladdr((void*)desc->entries[i].code.jmp_addr, &info) &&
                info.dli_sname){
            if(fprintf(stream, "%s: %d\n",
                info.dli_sname, desc->entries[i].counter) < 0)
                return -1;
        }else{
            if(fprintf(stream, "0x%x: %d\n", 
                desc->entries[i].code.jmp_addr, desc->entries[i].counter) < 0)
                return -1;
        }
    }
    return 0;
}
