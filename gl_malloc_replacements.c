/* comments */
/* We have malloc wrappers and malloc tracing functions here */

#include "gl_configure.h"

#include "gl_misc.h"
#include "gl_malloc_replacements.h"
#include "gl_misc.h"

#include "pub_tool_vki.h"
#include "pub_tool_replacemalloc.h"
#include "pub_tool_oset.h"
#include "pub_tool_libcbase.h"
#include "pub_tool_libcprint.h"
#include "pub_tool_libcfile.h"
#include "pub_tool_libcassert.h"
#include "pub_tool_stacktrace.h"
#include "pub_tool_debuginfo.h"
#include "pub_tool_tooliface.h"
#include "pub_tool_xarray.h"
#include "pub_tool_mallocfree.h"

#ifdef GL_MALLOC_REPLACEMENT

OSet* malloc_list;				   // OrderedSet of malloc ranges
OSet* malloc_name_list;

/* For tracing the callstack */
#define STRACEBUF 16
extern Addr ips[STRACEBUF];

extern Char clo_prog_lang;
extern UInt clo_malloc_sdepth;
extern Bool clo_trace_malloc_calls;

/* For segment tracing */
extern Addr tStack;
extern Addr bHeap; 
extern Addr tHeap; 

/* application counters */
extern global_cnt_t global_counter;

/* user malloc interface variables */
HChar* user_malloc_name_array[MAX_USER_NAMES];
UInt   user_malloc_name_cnt = 0,
       user_malloc_name_itt = 0;

/* Log variables */
extern HChar line_buffer[FILE_LEN];
extern void (*wrout)(Int, HChar *);

#define FILENAME_BUFFER_SIZE 256
#define DIRNAME_BUFFER_SIZE 128
#define MSTRUCT_SIZE 128

/*-----------------------------------------------*/
/*--- Malloc and friends, Parsing Functions 	---*/
/*-----------------------------------------------*/

Word addrCmp_heap(const void* key, const void* elem)
{
  if( (*(const Addr*)key) < ((const hblock_t*) elem)->addr)
    return -1;
  else if((*(const Addr*)key) >= (((const hblock_t*)elem)->addr + ((const hblock_t*)elem)->req_szB))
    return 1;
  else
    return 0;
}

Word nameCmp(const void* key, const void* elem)
{
  return VG_(strcmp)((const HChar*)key, ((const malloc_name_t*) elem)->name); 
}

/*-----------------------------------------------------------*/
/*--- Malloc Init Functions                               ---*/
/*-----------------------------------------------------------*/
static XArray* alloc_fns;
static XArray* ignore_fns;

void init_alloc_fns(void)
{
  alloc_fns = VG_(newXA)(VG_(malloc), "gl.main.iaf.1",
      VG_(free), sizeof(Char*));
#define DO(x)  { const HChar* s = x; VG_(addToXA)(alloc_fns, &s); }

  DO("malloc"                                             );
  DO("__builtin_new"                                      );
  DO("operator new(unsigned)"                             );
  DO("operator new(unsigned long)"                        );
  DO("__builtin_vec_new"                                  );
  DO("operator new[](unsigned)"                           );
  DO("operator new[](unsigned long)"                      );
  DO("calloc"                                             );
  DO("realloc"                                            );
  DO("memalign"                                           );
  DO("posix_memalign"                                     );
  DO("valloc"                                             );
  DO("operator new(unsigned, std::nothrow_t const&)"      );
  DO("operator new[](unsigned, std::nothrow_t const&)"    );
  DO("operator new(unsigned long, std::nothrow_t const&)" );
  DO("operator new[](unsigned long, std::nothrow_t const&)" );
#if defined(VGP_ppc32_aix5) || defined(VGP_ppc64_aix5)
  DO("malloc_common"      );
  DO("calloc_common"      );
  DO("realloc_common"     );
  DO("memalign_common"    );
#elif defined(VGO_darwin)
  DO("malloc_zone_malloc"    );
  DO("malloc_zone_calloc"    );
  DO("malloc_zone_realloc"   );
  DO("malloc_zone_memalign"  );
  DO("malloc_zone_valloc"    );
#endif
}

void init_ignore_fns(void)
{
  /* create the empty list */
  ignore_fns = VG_(newXA)(VG_(malloc), "gl.main.iif.1",
      VG_(free), sizeof(Char*));
}


/* Heap management functions (replacements) */
  static
hblock_t* _record_block(ThreadId tid, void* p, SizeT req_szB, SizeT slop_szB,
    HChar* mstruct, UInt mn_instance)
{
  hblock_t* hc = NULL;

  hc = VG_(OSetGen_AllocNode)(malloc_list, sizeof(hblock_t));
  tl_assert(hc != NULL);

  hc->addr = (Addr)p;
  VG_(strcpy)(hc->name, mstruct);
  hc->req_szB = req_szB;
  hc->slop_szB = slop_szB;
  hc->trefs = 0;
  hc->refs = 0;
  hc->instance = mn_instance;

  VG_(OSetGen_Insert)(malloc_list, hc);

  return hc;
}

  static __inline__
void*  alloc_and_record_block(ThreadId tid, SizeT req_szB, SizeT req_alignB, Bool is_zeroed)
{
  HChar mstruct[MSTRUCT_SIZE];
  HChar str[128];
  const HChar* filename = NULL;
  const HChar* dirname = NULL;
  Bool found_dirname = False;
  UInt lineno=0;
  
  UInt i=0;
  SizeT actual_szB = 0,
        slop_szB = 0;

  void* p;
  hblock_t* hc;
  malloc_name_t* mn;

  if( (SizeT) req_szB < 0){
    return NULL;
  }

  /* Allocate block */
  p = VG_(cli_malloc)(req_alignB, req_szB);
  if(p == NULL)
    return NULL;

  actual_szB = VG_(cli_malloc_usable_size)(p);
  slop_szB = actual_szB - req_szB;
  tl_assert(actual_szB >= req_szB);

  /* zero block (calloc) */
  if(is_zeroed){
    VG_(memset)(p, 0, actual_szB);
  }

  /* update segment info */
  if(tHeap < ((Addr)p + (Addr)actual_szB))
    tHeap = (Addr)p + (Addr)actual_szB;
  if(bHeap > (Addr)p)
    bHeap = (Addr)p;

  /* get allocation info */
  VG_(get_StackTrace)(tid, ips, STRACEBUF, NULL, NULL, 0);
  if(clo_prog_lang == 'C')	
    found_dirname = VG_(get_filename_linenum)(ips[1], &filename, &dirname, &lineno);
  else if(clo_prog_lang == 'F')
    found_dirname = VG_(get_filename_linenum)(ips[2], &filename, &dirname, &lineno);
  else
    VG_(tool_panic)("Something is wrong, clo_prog_lang funky value!\n");
  
  /* use malloc_sdepth if set */
  if(clo_malloc_sdepth > 1){
    found_dirname = VG_(get_filename_linenum)(ips[clo_malloc_sdepth], &filename, &dirname, &lineno);
  }

  /* Parse malloc debug info or use user client calls (GL_SET_MALLOC_NAME)
   *
   * User supplied means that the user invoked client calls.
   * Every set malloc() name client call is buffered, (look at gl_main.c).
   * The array of buffered names is "produced", and consumed by subsequent malloc() calls (FIFO).
   * This helps when setting multiple client-calls. For instance building a list of malloc() calls. */
  if(user_malloc_name_cnt < 1){
    if(found_dirname == False){ 
      VG_(strcpy)(mstruct, "_sysres_");  
    }
    else{
      if(found_dirname){
        for(i=0; filename[i] != '\0'; i++){
          if(filename[i] == '.')
            str[i] = '_';
          else
            str[i] = filename[i];
        }
        str[i] = '\0';
        VG_(sprintf)(mstruct, "%s_%u", str, lineno);
      }
      else{
        VG_(strcpy)(mstruct, "_sysres_");
      }
    }
  }
  else{
    VG_(strcpy)(mstruct, user_malloc_name_array[user_malloc_name_itt]);
    VG_(free)(user_malloc_name_array[user_malloc_name_itt]);
    user_malloc_name_array[user_malloc_name_itt] = NULL;
    user_malloc_name_itt++;

    if(user_malloc_name_itt == user_malloc_name_cnt){
      user_malloc_name_cnt = 0;
      user_malloc_name_itt = 0;
    }
  }

  /* look up struct enum */
  mn = VG_(OSetGen_Lookup)(malloc_name_list, mstruct);
  if(mn == NULL){
    mn = VG_(OSetGen_AllocNode)(malloc_name_list, sizeof(malloc_name_t));
    VG_(strcpy)(mn->name, mstruct);
    mn->instance = 0;
    VG_(OSetGen_Insert)(malloc_name_list, mn);
  }
  else{
    mn->instance++;
  }

  /* print */
  if(is_zeroed && clo_trace_malloc_calls == True){
    VG_(sprintf)(line_buffer, "X %d CALLOC %09lx %lu %s %llu\n", tid, (HWord) p, req_szB, mstruct, mn->instance);
    wrout(fd_trace, line_buffer);
  }
  else if(clo_trace_malloc_calls == True){
    VG_(sprintf)(line_buffer, "X %d MALLOC %09lx %lu %s %llu\n", tid, (HWord) p, req_szB, mstruct, mn->instance);
    wrout(fd_trace, line_buffer);
  }

  /* record */
  hc = VG_(OSetGen_Lookup)(malloc_list, &p);
  if(hc == NULL){
    hc = _record_block(tid, p, req_szB, slop_szB, mstruct, mn->instance);
    global_counter.memprint += actual_szB;
  }

  return p;
}

  static __inline__
void* realloc_block(ThreadId tid, void* p_old, SizeT new_req_szB)
{
  HChar mstruct[MSTRUCT_SIZE];
  HChar str[128];
  const HChar* filename = NULL;
  const HChar* dirname = NULL;
  Bool found_dirname = False;
  UInt lineno=0;

  hblock_t* hc = NULL;
  malloc_name_t* mn = NULL;

  void* p_new = NULL;

  SizeT old_req_szB = 0, old_slop_szB = 0,
        new_slop_szB = 0, new_actual_szB = 0;

  UInt i=0;

  /* Look up previous malloc record */
  hc = VG_(OSetGen_Lookup)(malloc_list, &p_old);
  if(hc == NULL)
    return NULL; /* bogus realloc? */

  old_req_szB = hc->req_szB;
  old_slop_szB = hc->slop_szB;

  /* remove old block */
  hc = VG_(OSetGen_Remove)(malloc_list, &p_old);

  /* Do the allocation if needed */
  if(new_req_szB <= old_req_szB + old_slop_szB){
    p_new = p_old;
    new_slop_szB = old_slop_szB + (old_req_szB - new_req_szB);
  }
  else{
    /* new size is bigger, get new block, copy, and free old */
    p_new = VG_(cli_malloc)(VG_(clo_alignment), new_req_szB);
    if(p_new == NULL){
      return NULL;
    }

    VG_(memcpy)(p_new, p_old, old_req_szB + old_slop_szB);
    VG_(cli_free)(p_old);

    new_actual_szB = VG_(cli_malloc_usable_size)(p_new);
    tl_assert(new_actual_szB >= new_req_szB);
    new_slop_szB = new_actual_szB - new_req_szB;

    global_counter.memprint += (new_req_szB + new_slop_szB);
  }

  /* update segment info */
  if(tHeap < ((Addr)p_new + (Addr)new_actual_szB))
    tHeap = (Addr)p_new + (Addr)new_actual_szB;
  if(bHeap > (Addr)p_new)
    bHeap = (Addr)p_new;

  /* get allocation info */
  VG_(get_StackTrace)(tid, ips, STRACEBUF, NULL, NULL, 0);
  if(clo_prog_lang == 'C')	
    found_dirname = VG_(get_filename_linenum)(ips[1], &filename, &dirname, &lineno);
  else if(clo_prog_lang == 'F')
    found_dirname = VG_(get_filename_linenum)(ips[2], &filename, &dirname, &lineno);
  else
    VG_(tool_panic)("Something is wrong, clo_prog_lang funky value!\n");
  
  /* use malloc_sdepth if set */
  if(clo_malloc_sdepth > 1){
    found_dirname = VG_(get_filename_linenum)(ips[clo_malloc_sdepth], &filename, &dirname, &lineno);
  }

  /* malloc structure parsing or user supplied */
  if(user_malloc_name_cnt < 1){
    if(found_dirname == False){
      VG_(strcpy)(mstruct, "_sysres_");
    }
    else {
      if(filename != NULL){
        for(i=0; filename[i] != '\0'; i++){
          if(filename[i] == '.')
            str[i] = '_';
          else
            str[i] = filename[i];
        }
        str[i] = '\0';
        VG_(sprintf)(mstruct, "%s_%u", str, lineno);
      }
      else{
        VG_(strcpy)(mstruct, "_sysres_");
      }
    }
  }
  else{
    VG_(strcpy)(mstruct, user_malloc_name_array[user_malloc_name_itt]);
    VG_(free)(user_malloc_name_array[user_malloc_name_itt]);
    user_malloc_name_array[user_malloc_name_itt] = NULL;
    user_malloc_name_itt++;

    if(user_malloc_name_itt == user_malloc_name_cnt){
      user_malloc_name_cnt = 0;
      user_malloc_name_itt = 0;
    }
  }

  /* look up struct enum */
  mn = VG_(OSetGen_Lookup)(malloc_name_list, mstruct);
  if(mn == NULL){
    mn = VG_(OSetGen_AllocNode)(malloc_name_list, sizeof(malloc_name_t));
    VG_(strcpy)(mn->name, mstruct);
    mn->instance = 0;
    VG_(OSetGen_Insert)(malloc_name_list, mn);
  }
  else{
    mn->instance++;
  }

  /* print */
  if(clo_trace_malloc_calls == True){
    VG_(sprintf)(line_buffer, "X %d REALLOC %09lx %lu %s %llu\n", tid, (HWord) p_new, new_req_szB, mstruct, mn->instance);
    wrout(fd_trace, line_buffer);
  }

  /* update record */
  if(p_new){
    hc->addr = (Addr)p_new;
    VG_(strcpy)(hc->name, mstruct);
    hc->req_szB = new_req_szB;
    hc->slop_szB = new_slop_szB;
    hc->trefs = 0;
    hc->refs = 0;
    hc->instance = mn->instance;
  }

  VG_(OSetGen_Insert)(malloc_list, hc);

  global_counter.memprint += new_req_szB;

  return p_new;
}

  static __inline__
void unrecord_block(ThreadId tid, void* p)
{
  /* remove hblock_t from malloc_list */
  hblock_t* hc = VG_(OSetGen_Remove)(malloc_list, &p);
  tl_assert(hc != NULL);

  global_counter.memprint -= hc->req_szB;

  if(clo_trace_malloc_calls == True){
    VG_(sprintf)(line_buffer, "X %d FREE %09lx %s %llu\n", tid, (HWord) p, hc->name, hc->instance);
    wrout(fd_trace, line_buffer);
  }

  /*for dyn_stat printing*/
  _flush_obj(hc);

  /* actually free the chunk, and the heap block (if necessary) */
  VG_(OSetGen_FreeNode)(malloc_list, hc);
}


/*--------------------------*/
/*--- Malloc Replacement ---*/
/*--------------------------*/
static void* gl_malloc (ThreadId tid, SizeT szB)
{
  global_counter.malloc_calls++;

  return alloc_and_record_block(tid, szB, VG_(clo_alignment), False);
}

static void* gl_calloc (ThreadId tid, SizeT m, SizeT szB)
{
  global_counter.calloc_calls++;

  return alloc_and_record_block(tid, m*szB, VG_(clo_alignment), True);
}

static void* gl_realloc (ThreadId tid, void* p_old, SizeT new_szB)
{
  global_counter.realloc_calls++;

  return realloc_block(tid, p_old, new_szB);
}

static void gl_free (ThreadId tid __attribute__((unused)), void* p)
{
  if(!p)
    return;

  global_counter.free_calls++;

  unrecord_block(tid, p);
  VG_(cli_free)(p);
}

static void* gl__builtin_new (ThreadId tid, SizeT szB)
{
  global_counter.malloc_calls++;

  return alloc_and_record_block(tid, szB, VG_(clo_alignment), False);
}

static void* gl__builtin_vec_new (ThreadId tid, SizeT szB)
{
  global_counter.malloc_calls++;

  return alloc_and_record_block(tid, szB, VG_(clo_alignment), False);
}

static void* gl_memalign (ThreadId tid, SizeT alignB, SizeT szB)
{
  global_counter.malloc_calls++;

  return alloc_and_record_block(tid, szB, alignB, False);
}

static void gl__builtin_delete (ThreadId tid, void* p)
{

  if(!p)
    return;

  global_counter.free_calls++;

  unrecord_block(tid, p);
  VG_(cli_free)(p);
}

static void gl__builtin_vec_delete (ThreadId tid, void* p)
{
  if(!p)
    return;

  global_counter.free_calls++;

  unrecord_block(tid, p);
  VG_(cli_free)(p);
}

static SizeT gl_cli_malloc_usable_size (ThreadId tid, void* p)
{
  hblock_t* hc = VG_(OSetGen_Lookup)(malloc_list, &p);

  return (hc ? (hc->req_szB + hc->slop_szB) : 0);
}

void gl_register_malloc_replacements(void)
{
  VG_(needs_malloc_replacement) (
      gl_malloc,
      gl__builtin_new,
      gl__builtin_vec_new,
      gl_memalign,
      gl_calloc,
      gl_free,
      gl__builtin_delete,
      gl__builtin_vec_delete,
      gl_realloc,
      gl_cli_malloc_usable_size,
      0);
}

#endif
