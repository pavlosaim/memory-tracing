/* 	Miscelaneous functions for malloc tracking, physical address tracking etc.
        Malloc tracking functions.
 */
#include "gl_basics.h"
#include "gl_misc.h"
#include "gl_malloc_replacements.h"

#include "pub_tool_vki.h"
#include "pub_tool_libcbase.h"
#include "pub_tool_libcprint.h"
#include "pub_tool_libcfile.h"
#include "pub_tool_mallocfree.h"
#include "pub_tool_oset.h"


extern Int global_pid;
extern Bool clo_map_virt_to_phys;
extern Int fd_trace;
extern OSet* malloc_list;
extern void (*wrout)(Int, HChar *);

/* global data address compare function */
Word addrCmp_global(const void* key, const void* elem)
{
  if( (*(const Addr*)key) < ((const gblock_t*) elem)->addr )
    return -1;
  else if((*(const Addr*)key) >= ( ((const gblock_t*) elem)->addr + ((const gblock_t*) elem)->size ) )
    return 1;
  else
    return 0;
}

/* mmap data address compare function */
Word addrCmp_mmap(const void* key, const void* elem)
{
  if( (*(const Addr*)key) < ((const mblock_t*) elem)->addr )
    return -1;
  else if((*(const Addr*)key) >= ( ((const mblock_t*) elem)->addr + ((const mblock_t*) elem)->size ) )
    return 1;
  else
    return 0;
}

void counters_init(global_cnt_t *global_counter){
  global_counter->cnt        = 1;
  global_counter->instr      = 0;
  global_counter->tinstr   = 0;
  global_counter->load       = 0;
  global_counter->tload    = 0;
  global_counter->store      = 0;
  global_counter->tstore   = 0;
  global_counter->modify     = 0;
  global_counter->tmodify  = 0;

  global_counter->stack      = 0;
  global_counter->mmap       = 0;
  global_counter->heap       = 0;
  global_counter->global     = 0;
  global_counter->custom     = 0;

  global_counter->memprint   = 0;

  global_counter->malloc_calls  = 0;
  global_counter->calloc_calls  = 0;
  global_counter->realloc_calls = 0;
  global_counter->free_calls    = 0;
  global_counter->mmap_calls    = 0;
  global_counter->unmap_calls   = 0;

  return;
}

void _flush_obj(hblock_t* hc){
  /*flush_out_deleted_object*/
  HChar tmp[512];
  HChar tmp2[128];

  VG_(sprintf)(tmp2, "%s_%llu_X", hc->name, hc->instance);
  VG_(sprintf)(tmp,"%-27s %18llu\n", tmp2, hc->refs);

  //VG_(write)(fd_dynstats, tmp, VG_(strlen)(tmp));

  return;
}

void counters_flush(global_cnt_t *global_counter){
  //hblock_t* hc;
  HChar tmp[512];

  /* XXX 
  VG_(sprintf)(tmp, "%15llu %12llu %12llu %12llu %12llu %12lld\n",
      global_counter->tinstr,
      global_counter->stack,
      global_counter->heap,
      global_counter->global,
      global_counter->global + global_counter->heap + global_counter->stack,
      global_counter->memprint);
  VG_(write)(fd_flush, tmp, VG_(strlen)(tmp));
  global_counter->cnt++;
  */

  /*reset counters*/
  global_counter->instr    = 0;
  global_counter->load     = 0;
  global_counter->store    = 0;
  global_counter->modify   = 0;

  global_counter->stack    = 0;
  global_counter->heap     = 0;
  global_counter->global   = 0;

  /* Print X I XxX */
  VG_(sprintf)(tmp, "X INST %llu\n", global_counter->tinstr);
  wrout(fd_trace, tmp);
  //VG_(write)(fd_trace, tmp, VG_(strlen)(tmp));
  global_counter->total_lines++;

  /* Dynamic Structure Information*/

  /* reiterate malloc_list */
/* XXX
  VG_(OSetGen_ResetIter)(malloc_list);
  while((hc = VG_(OSetGen_Next)(malloc_list))){
    VG_(sprintf)(tmp,"%-21s_%-9llu  %15llu\n", hc->name, hc->instance, hc->refs);
    VG_(write)(fd_dynstats, tmp, VG_(strlen)(tmp));
    hc->refs=0;
  }
  VG_(OSetGen_ResetIter)(malloc_list);

  VG_(sprintf)(tmp, "%llu\n", global_counter->tinstr);
  VG_(write)(fd_dynstats, tmp, VG_(strlen)(tmp));
*/

  return;
}

void gl_trace_values(Addr addr, SizeT size, HChar* _values)
{
  switch((Int)size){
    case 8:
      VG_(sprintf)(_values, " [%#llx]", *(ULong*)addr);
      break;
    case 4:
      VG_(sprintf)(_values, " [%#x]", *(UInt*)addr);
      break;
    case 2:
      VG_(sprintf)(_values, " [%#x]", *(UChar*)addr);
      break;
    case 16:
      /* we don't support this case yet *(Double*)addr not sure if needed*/
      VG_(sprintf)(_values, " [%#x]", 0);
      break;
    case 1:
      VG_(sprintf)(_values, " [%#x]", *(UChar*)addr);
      break;
    default:
      VG_(sprintf)(_values, " [%#llx]", *(ULong*)addr);
  }
}

/*
 * Mapping virtual to physical addresses
 */

/* Software TLB, for phys. tracing*/
sTLBtype sTLB[sTLB_SIZE];

Addr gl_get_physaddr(Addr addr)
{
  Addr vpa, addr_offset;	/* virtual page address, and addr offset */
  Addr ppa, paddr; 				/* physical page address, and addr offset */

  Int index;
  UWord TAG;

  Int pagemap_fd;
  Int bytesread=0;
  HChar pagemap[128];
  ULong o_index;

  /* Determine virtual page address and offset */
  vpa = addr & ~(PAGE_SIZE-1);
  addr_offset = addr & (PAGE_SIZE-1);

  /* look up sTLB */
  index = ((addr >> 12) & (sTLB_SIZE-1));
  TAG = addr >> 23; /* (Log_2(PAGE_SIZE)=12 + Log_2(sTLB_SIZE)=11) */

  if(TAG == sTLB[index].TAG){
    ppa = sTLB[index].ppa;
    paddr = ppa + addr_offset;
    return paddr;
  }

  VG_(sprintf)(pagemap, "/proc/%d/pagemap", global_pid);
  pagemap_fd = VG_(fd_open)(pagemap, VKI_O_RDONLY, VKI_S_IRUSR|VKI_S_IWUSR);

  o_index = (vpa / PAGE_SIZE) * sizeof(ULong);
  /*Off64T o = */
  VG_(lseek)(pagemap_fd, o_index, VKI_SEEK_SET);
  /* We're not using it but if(o!=o_index) we can't seek to loc. */

  bytesread = VG_(read)(pagemap_fd, &ppa, sizeof(ULong));

  if(bytesread == 0){
    VG_(umsg)("Something is wrong with reading the physical address, disable virt_phys mapping...\n");
    VG_(umsg)("Global pid: %d\n", global_pid);
    clo_map_virt_to_phys=False;
    return 0;
  }

  /* Update sTLB */
  sTLB[index].TAG = TAG;
  sTLB[index].ppa = ppa;

  paddr = ppa + addr_offset;

  VG_(close)(pagemap_fd);
  return paddr;
}
