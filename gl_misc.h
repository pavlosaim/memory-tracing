#ifndef __MISC_H
#define __MISC_H

#include "gl_basics.h"
#include "gl_malloc_replacements.h"

/*For physical page tracing*/
#define sTLB_SIZE 2048
#define PAGE_SIZE 4096

#define ROUNDDOWN_4K(addr) \
  ( (addr >> 12) << 12)

typedef struct _sTLB{
  UWord TAG;
  Addr ppa;
} sTLBtype;

typedef struct _vpagenode{
  struct _vpagenode* next;
  UWord addr;
  ULong refs;
} vpagenode;

/* global_block */
typedef struct _gblock{
  Addr addr;
  SizeT size;
  ULong trefs;
  ULong refs;
  ULong instance;
  HChar name[128];
} gblock_t;

/* mmap_name_list node */
typedef struct _mmap_name_t {
  HChar name[128];
  ULong instance;
} mmap_name_t;

/* mmap_list node */
typedef struct _mblock_t{
  Addr addr;
  SizeT size;
  ULong trefs;
  ULong refs;
  ULong instance;
  HChar name[128];
} mblock_t;

typedef struct _fnname_oset_t {
  Addr addr;
  SizeT size;
  HChar fnname[128];
  HChar soname[128];
} fn_name_t;

/* user mmap names */
HChar* user_mmap_name_array[MAX_USER_NAMES];

Addr gl_get_physaddr(Addr addr);

#define MAX_COUNTER 18446744073709551615ULL

typedef struct _counters{
  ULong cnt; /* control */

  ULong instr;
  ULong tinstr;
  ULong load;
  ULong tload;
  ULong store;
  ULong tstore;
  ULong modify;
  ULong tmodify;

  /* Stack, MMap, Heap, Global, Custom */
  ULong stack;   
  ULong mmap;   
  ULong heap;
  ULong global;
  ULong custom;

  ULong memprint;

  ULong malloc_calls;
  ULong calloc_calls;
  ULong realloc_calls;
  ULong free_calls;

  ULong mmap_calls;
  ULong unmap_calls;

  ULong total_lines;
} global_cnt_t;

void gl_trace_values(Addr addr, SizeT size, HChar* _values);
void counters_init(global_cnt_t*);
void counters_flush(global_cnt_t*);
void _flush_obj(hblock_t* hc);

Word addrCmp_global(const void* key, const void* elem);
Word addrCmp_mmap(const void* key, const void* elem);
Word addrCmp_fnname(const void* key, const void* elem);


#endif
