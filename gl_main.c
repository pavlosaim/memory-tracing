/*--------------------------------------------------------------------*/
/*--- Gleipnir: A tracing and profiling Valgrind tool.   gl_main.c ---*/
/*--------------------------------------------------------------------*/

/*
   This file is part of Gleipnir, a memory tracing and profiling Valgrind
   tool. The tool does advanced memory tracing. A trace is annotated
   with with debug information for detailed cache analysis.

   Copyright (C) 2010-2012
   Computer Systems Research Laboratory
   University of North Texas, Denton TX
   tjanjusic@unt.edu

   Other former and current developers:
   Eric Schlueter
   Brandon Potter

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
   02111-1307, USA.

   The GNU General Public License is contained in the file COPYING.
 */

#include "gl_configure.h"

#include "config.h"
#include "gleipnir.h"
#include "gl_misc.h"
#include "gl_malloc_replacements.h"

#include "pub_tool_basics.h"
#include "pub_tool_hashtable.h"
#include "pub_tool_vki.h"
#include "pub_tool_tooliface.h"
#include "pub_tool_oset.h"		 	    /* heap block Oset */

#include "pub_tool_libcassert.h"
#include "pub_tool_libcbase.h"
#include "pub_tool_libcprint.h"
#include "pub_tool_libcfile.h"
#include "pub_tool_libcproc.h"

#include "pub_tool_threadstate.h"
#include "pub_tool_stacktrace.h"
#include "pub_tool_debuginfo.h"
#include "pub_tool_options.h"
#include "pub_tool_machine.h"       /* VG_(fnptr_to_fnentry) */
#include "pub_tool_xarray.h"
#include "pub_tool_mallocfree.h"
#include "pub_tool_seqmatch.h"

/* XXX temporary */
#define INSTRUMENT         \
    (clo_instrumentation_state &&     \
     clo_instrument_sonames_state &&  \
     clo_instrument_fn_state)

/* Tool global options */
static Bool global_instrumentation_state = True; /* overrides GL_START/STOP_INSTR;
                                                    default set to True */
/* Command Line Options */
Bool clo_instrumentation_state = False;
Bool clo_map_virt_to_phys = False; /*needed in gl_misc.c*/
Bool clo_trace_malloc_calls = True;/*needed in gl_malloc_replacements.c*/ 
Char clo_prog_lang = 'C';          /* Naive hack for some Fortran
                                      codes that do additional wrapping*/
UInt clo_malloc_sdepth = 1;

static Bool clo_track_pages = False;
static Bool clo_read_debug = False;
static Bool clo_multi_process = False;
static Bool clo_trace_instr = False;
static Bool clo_trace_values = False;
static ULong clo_flush_counter_at = MAX_COUNTER;
const  HChar* clo_gl_out = "gleipnir.%p";
static ULong clo_abort_at = -1; /* max value */
static HChar clo_abort_at_fn[128] = "0";
static HChar* clo_ignore_below_fns[64];
static Bool   clo_instrument_fn_state = True;
static Addr   clo_instrument_fn_state_sps = 0;
static HChar* clo_ignore_sonames[64];
static Bool   clo_instrument_sonames_state = True;
static HChar  clo_ignored_soname[64] = "";

static ULong gl_mark_cnt_A = 0;
static ULong gl_mark_cnt_B = 0;
static ULong gl_mark_cnt_C = 0;
static ULong gl_mark_cnt_D = 0;
static ULong gl_mark_cnt_E = 0;

#define DEBUG 0
#define MAX_DSIZE 512
#define FNNAME_BUFSIZE 256

const HChar* fnname;
HChar fnname_null[1] = "\0";
HChar current_soname[128];

Bool found_fn = False;

HChar *gl_file_name = NULL;
HChar old_gl_file_name[256];

Int global_pid = 0;
static Int local_pid = 0;

/* malloc records */
extern OSet* malloc_list;
extern OSet* malloc_name_list;

/* mmap records */
static OSet* mmap_list;
static OSet* mmap_name_list;

UInt user_mmap_name_cnt = 0,
     user_mmap_name_itt = 0;

/* global records */
static OSet* global_list = NULL;

/* function and soname records */
static OSet* fnname_list = NULL;

Word addrCmp_fnname(const void* key, const void* elem)
{
  if( (*(const Addr*)key) < ((const mblock_t*) elem)->addr )
    return -1;
  else if((*(const Addr*)key) >= ( ((const mblock_t*) elem)->addr + ((const mblock_t*) elem)->size ) )
    return 1;
  else
    return 0;
}

extern sTLBtype sTLB[sTLB_SIZE];

/* Page utilization */
static VgHashTable* virtual_page_list = NULL;   // Page_Chunks

/*-------------------------*/
/*--- Event Definitions	---*/
/*-------------------------*/
typedef IRExpr IRAtom;

typedef enum { Event_Ir, Event_Dr, Event_Dw, Event_Dm } EventKind;

typedef
struct {
  EventKind  ekind;
  IRAtom*    addr;
  Int        size;
} Event;

/* up to this many unnotified events are allowed.  Must be at least two,
   so that reads and writes to the same address can be merged into a modify.
   Beyond that, larger numbers just potentially induce more spilling due to
   extending live ranges of address temporaries. */

#define N_EVENTS 16 /* default */

static Event events[N_EVENTS];
static Int events_used = 0;

ThreadId _tid = 1; 

/* Segment Tracing */
/* This still needs to be improved, this shouldn't be manually set */
#define STRACEBUF 16
UInt nSps = 0;
Addr ips[STRACEBUF];
Addr sps[STRACEBUF];
Addr fps[STRACEBUF];

Addr tStack = (Addr) 0x1fffffffffffffffULL; /*set to highest?*/

Addr bHeap =  (Addr) 0x1fffffffffffffffULL;
Addr tHeap =  (Addr) 0x0;                   /* set to lowest */

Addr bMMap = (Addr) 0x1fffffffffffffffULL;
Addr tMMap = (Addr) 0x0;

/* global counters */
global_cnt_t global_counter;

/* active file descriptors */
Int fd_trace;

HChar line_buffer[FILE_LEN];
void (*wrout)(Int, HChar *) = NULL;

static void write_to_stdout(Int fd, HChar *buf)
{
  VG_(write)(1, buf, VG_(strlen)(buf));
  global_counter.total_lines++;
}

static void write_to_file(Int fd, HChar *buf)
{
  VG_(write)(fd, buf, VG_(strlen)(buf));
  global_counter.total_lines++;
}

static void init_clos(void)
{
  int i = 0;
  
  /* sonames && fnnames */
  for(i=0; i<64; i++){
    clo_ignore_sonames[i] = NULL;
    clo_ignore_below_fns[i] = NULL;
  }

  return;
}

static void gl_fini(Int exitcode);

static Bool gl_process_clo(const HChar* args)
{
  Int pagemap_fd;	
  HChar pagemap[128];
  SysRes sres;
  const HChar* clo_str;
  
  HChar* clo_str_buffer = clo_str_buffer = VG_(malloc)("clo_str_buffer", 128);

  if VG_BOOL_CLO(args, "--trace-from-start", clo_instrumentation_state) {}
  else if VG_BOOL_CLO(args, "--map-phys", clo_map_virt_to_phys) {
    VG_(sprintf)(pagemap, "/proc/%d/pagemap", VG_(getpid)());
    pagemap_fd = VG_(fd_open)(pagemap, VKI_O_RDONLY, VKI_S_IRUSR);

    /* Maybe we need to move this outside and check at the beginning?*/
    if(pagemap_fd < 0){
      VG_(umsg)("NOTICE!\nCan't find <%s>, virtual to physical address mapping disabled\n\n",pagemap);
      clo_map_virt_to_phys=False;
    }
    VG_(close)(pagemap_fd);
  }
  else if VG_BOOL_CLO(args, "--track-pages", clo_track_pages) {}
  else if VG_BOOL_CLO(args, "--trace-instructions", clo_trace_instr) {}
  else if VG_BOOL_CLO(args, "--trace-malloc-calls", clo_trace_malloc_calls) {}
  else if VG_BOOL_CLO(args, "--read-debug", clo_read_debug) {}
  else if VG_STR_CLO(args,  "--prog-lang", clo_str){
    if(clo_str[0] != 'F' && clo_str[0] != 'C'){
      VG_(tool_panic)("--prog-lang unknown input\n");
    }
    clo_prog_lang = clo_str[0];
  }
  else if VG_INT_CLO(args, "--malloc-sdepth", clo_malloc_sdepth) {}
  else if VG_BOOL_CLO(args, "--multi-process", clo_multi_process) {}
  else if VG_BOOL_CLO(args, "--trace-values", clo_trace_values) {}
  else if VG_INT_CLO(args, "--flush-at", clo_flush_counter_at){}
  else if VG_STR_CLO(args, "--out", clo_gl_out){
    Int a = VG_(strcmp)(clo_gl_out, "stdout");
    switch (a){
      case 0:
        wrout = &write_to_stdout;
        break;
      default:
        wrout = &write_to_file;

        VG_(sprintf)(line_buffer, "%s.%%p", clo_gl_out);
        gl_file_name = VG_(expand_file_name)("--out", line_buffer);
        
        /* why do I need the old_gl_file_name? XXX */ 
        VG_(strcpy)(old_gl_file_name, gl_file_name);

        /* get pid info */
        global_pid = VG_(getpid)();
        local_pid = global_pid;

        /* open trace file */
        sres = VG_(open)(gl_file_name, VKI_O_CREAT|VKI_O_TRUNC|VKI_O_WRONLY,
            VKI_S_IRUSR|VKI_S_IWUSR);
        if(sr_isError(sres)){
          VG_(umsg)("error: can't open file for writing\n");
          return False;
        }
        else{
          fd_trace = sr_Res(sres);
        }
        break;
    }
  }
  else if VG_STR_CLO(args, "--abort-at", clo_str) {
    UInt i;
    clo_abort_at = 0;

    /* check for validity must be <integer><k|m|g>*/
    for(i=0; i<VG_(strlen)(clo_str); i++){
      clo_abort_at *= 10;
      if(VG_(isdigit)(clo_str[i])){
        clo_abort_at += (UInt) clo_str[i] - 48; 
      }
      else{
        clo_abort_at /= 10;

        switch(VG_(tolower)(clo_str[i])){
          case 'k':
            clo_abort_at *= 1E3;
            VG_(umsg)("clo_abort_at: %llu\n", clo_abort_at);
            break;
          case 'm':
            clo_abort_at *= 1E6;
            VG_(umsg)("clo_abort_at: %llu\n", clo_abort_at);
            break;
          case 'g':
            clo_abort_at *= 1E9;
            VG_(umsg)("clo_abort_at: %llu\n", clo_abort_at);
            break;
          default:
            return False;
        }
      }
    }
    
    return True;
  }
  else if VG_STR_CLO(args, "--abort-at-fn", clo_str){
    VG_(strcpy)(clo_abort_at_fn, clo_str); 
  }
  else if VG_STR_CLO(args, "--ignore-below-fn", clo_str){
    UInt i = 0;
    HChar* token = NULL;
    VG_(strcpy)(clo_str_buffer, clo_str);

    token = VG_(strtok)(clo_str_buffer, ",");

    while(token != NULL){
      clo_ignore_below_fns[i] = VG_(malloc)("gl-fnname-malloc", sizeof( VG_(strlen)(token) ) + 1 );
      VG_(strcpy)(clo_ignore_below_fns[i], token);

      token = VG_(strtok)(NULL, ",");
      i++;
    }
  }
  else if VG_STR_CLO(args, "--ignore-sonames", clo_str){
    
    UInt i = 0;
    HChar* token = NULL;
    VG_(strcpy)(clo_str_buffer, clo_str);
    
    token = VG_(strtok)((HChar *) clo_str_buffer, ",");
    
    while(token != NULL){
        
      clo_ignore_sonames[i] = VG_(malloc)("gl-soname-malloc", sizeof( VG_(strlen)(token) ) + 1 );
      VG_(strcpy)(clo_ignore_sonames[i], token);
      
      token = VG_(strtok)(NULL, ",");
      i++;
    }
  }
  else {
    VG_(free)(clo_str_buffer);
    return False;
  }

  VG_(free)(clo_str_buffer);

  return True;
}

static void gl_print_usage(void)
{
  VG_(printf)(
      "    --trace-from-start=no|yes          Trace from the beginning of application [no].\n"
      "    --read-debug=no|yes                Read stack/global debug information [no].\n"
      "                                         (Yes, must enable --read-var-info=yes).\n"
      "    --malloc-sdepth=(int)              Set malloc stack-trace depth, (1) = call above vg_replace [default | C]\n"
      "                                                                     (2) = 1st function call wrapper [Fortan]\n"
      "                                                                     (3) = 2nd function call wrapper, etc.\n"
      "    --prog-lang='C'|'F'                Set the programming language C or Fortran, [C]\n"
      "    --multi-process=no|yes             Enable if application is multi-process [no].\n"
      "    --map-phys=no|yes                  Map virtual to physical addresses [no].\n"
      "    --track-pages=no|yes               Track the number of used virtual pages [no].\n"
      "    --trace-instructions=no|yes        Trace instructions [no].\n"
      "    --trace-malloc-calls=no|yes        Trace malloc calls [yes].\n"
      "    --trace-values=no|yes              Trace values [no].\n"
      "    --flush-at=(int)                   Flush at (int) executed instructions. This\n"
      "                                         will reset all counters [1<<63].\n"
      "    --out=(stdout | filename)          Redirect Gleipnir output to stdout or file [gleipnir.PID].\n"
      "    --abort-at=<integer><K|M|G>        Abort at <int> number of instructions.\n"
      "                                         Accepts <int><int[K|k]> <int[M|m]> <int[G|g]>.\n"
      "    --abort-at-fn=<string>             Abort at <string> function entry.\n"
      "    --ignore-below-fn<s1, s2, etc.>    Stop tracing below functions: (s1, s2, etc.) The trace resumes\n"
      "                                         when the function returns.\n" 
      "    --ignore-sonames=<s1, s2, etc.>    Stop tracing into libraries: (s1, s2, etc.)\n"
      "                                         Eg. <libc> will ignore anything from libc.so.X\n"
      );
}

static void gl_print_debug_usage(void)
{
  VG_(printf)(
      "			(none)\n"
      );
}

static void finish_trace_line(Addr addr, SizeT size, HChar* _debug_info)
{
  HChar _values[128];
  Int offset = 0;

  /* stack_global_debug */
  if( GL_UNLIKELY(clo_read_debug == True) ){
    XArray* debug_entry1 = VG_(newXA)(VG_(malloc), "", VG_(free), sizeof(HChar));
    XArray* debug_entry2 = VG_(newXA)(VG_(malloc), "", VG_(free), sizeof(HChar));

    /* arbitrary size? not really sure how large to make this */
    HChar debugString1[128] = "\0";
    HChar debugString2[128] = "\0";

    if(VG_(get_data_description_gleipnir)(debug_entry1, debug_entry2, addr) == True){
      VG_(strcpy)(debugString1, (HChar*)VG_(indexXA)(debug_entry1, 0));
      VG_(strcpy)(debugString2, (HChar*)VG_(indexXA)(debug_entry2, 0));

      VG_(sprintf)(_debug_info, "%s", debugString1);

      VG_(deleteXA)(debug_entry1);
      VG_(deleteXA)(debug_entry2);

      if(clo_trace_values == True){
        gl_trace_values(addr, size, _values);
        VG_(strcat)(_debug_info, _values); 
      }
      return;
    } 
  }

  /* ignore stack references */
  if( addr >= tStack ){
    _debug_info[0] = '\0';
    return;
  }

  /* search recorded malloc data */
  hblock_t *hc = VG_(OSetGen_Lookup)(malloc_list, &addr);
  if( hc != NULL ){
    offset = addr - hc->addr;

    VG_(sprintf)(_debug_info, "H-%llu %s.%d", hc->instance, hc->name, offset);
    hc->trefs++;
    hc->refs++;

    if(clo_trace_values == True){
      gl_trace_values(addr, size, _values);
      VG_(strcat)(_debug_info, _values); 
    }

    return;
  }

  /* search recorded mmap data */ 
  mblock_t *mc = VG_(OSetGen_Lookup)(mmap_list, &addr);
  if( mc != NULL ){
    offset = addr - mc->addr;

    VG_(sprintf)(_debug_info, "M-%llu %s.%d", mc->instance, mc->name, offset);
    mc->trefs++;
    mc->refs++;

    if(clo_trace_values == True){
      gl_trace_values(addr, size, _values);
      VG_(strcat)(_debug_info, _values);
    }

    return;
  }

  /* search recorded global data */
  gblock_t *gc = VG_(OSetGen_Lookup)(global_list, &addr);
  if(gc != NULL){
    offset = addr - gc->addr;

    VG_(sprintf)(_debug_info, "GS %s.%d", gc->name, offset);

    if(clo_trace_values == True){
      gl_trace_values(addr, size, _values);
      VG_(strcat)(_debug_info, _values);
    }
    return;
  }

  _debug_info[0] = '\0';
}

static VG_REGPARM(2) void trace_instr(Addr addr, SizeT size)
{
  global_counter.tinstr++;

  if( GL_UNLIKELY(global_counter.tinstr > clo_abort_at)){
    VG_(sprintf)(line_buffer, "X ABORT_AT %u at %llu\n", global_pid, global_counter.tinstr);
    wrout(fd_trace, line_buffer);
    
    gl_fini(0);
    VG_(exit)(0); 
  }
  

  if(INSTRUMENT == False)
    return;

  if(global_counter.instr == clo_flush_counter_at){
    counters_flush(&global_counter);
  }

  global_counter.instr++;

  Addr paddr;
  
  if(INSTRUMENT == True && VG_(get_fnname)(addr, &fnname) == False)
    fnname = fnname_null;
 
  if( GL_UNLIKELY(VG_(strcmp)(clo_abort_at_fn, fnname) == 0) ){
    VG_(sprintf)(line_buffer, "X ABORT_AT_FN %s %u at %llu\n", fnname, global_pid, global_counter.tinstr);
    wrout(fd_trace, line_buffer);

    gl_fini(0);
    VG_(exit)(0);
  }

  if( GL_UNLIKELY(clo_trace_instr == True) ){
    if(INSTRUMENT == True){
      if(clo_map_virt_to_phys == True){
        paddr = gl_get_physaddr(addr);
        VG_(sprintf)(line_buffer, "I %09lx %016lx %d %d %s::%s\n", addr, paddr, (Int)size, _tid, current_soname, fnname);
        wrout(fd_trace, line_buffer);
      }
      else{
        VG_(sprintf)(line_buffer, "I %09lx %d %d %s::%s\n", addr, (Int)size, _tid, current_soname, fnname);
        wrout(fd_trace, line_buffer);
      }
    }
  } 
}

static VG_REGPARM(2) void trace_load(Addr addr, SizeT size)
{
	int dontcare = 0;
  global_counter.tload++;

  if(INSTRUMENT == False)
    return;

  Addr paddr;
  Addr virt_page_addr;

  /*for prints*/
  HChar _address[64];
  _address[0] = '\0';
  HChar _sz_tid[32];
  _sz_tid[0] = '\0';
  HChar _debug_info[512];
  _debug_info[0] = '\0';

  global_counter.load++;

  if( GL_UNLIKELY(clo_map_virt_to_phys == True) ){
    paddr = gl_get_physaddr(addr);
    VG_(sprintf)(_address, "L %09lx %016lx", addr, paddr);
  }
  else{
    VG_(sprintf)(_address, "L %09lx", addr);
  }

  if( GL_UNLIKELY(clo_track_pages == True) ){
    virt_page_addr = ROUNDDOWN_4K(addr);
    vpagenode* vpn = VG_(HT_lookup)(virtual_page_list, (UWord) virt_page_addr);
    if(vpn == NULL){
      vpn = VG_(malloc)("virtualpagelist", sizeof(vpagenode));
      vpn->addr = virt_page_addr;
      vpn->refs = 1;
      VG_(HT_add_node)(virtual_page_list, vpn);
      vpn = VG_(HT_lookup)(virtual_page_list, (UWord) virt_page_addr);
    }
    else{
      vpn->refs++;				
    }
  }

  /*stack, global, or heap?*/
  if(addr >= tStack){
    VG_(sprintf)(_sz_tid, "%lu %d S", size, _tid);
    global_counter.stack++;
  }
  else if(addr >= bHeap && addr <=tHeap){	
    VG_(sprintf)(_sz_tid, "%lu %d H", size, _tid);
    global_counter.heap++;
  }
  else if(addr >= bMMap && addr <= tMMap){
    //VG_(sprintf)(_sz_tid, "%lu %d M", size, _tid);
    global_counter.global++;
  }
  else {
    //VG_(sprintf)(_sz_tid, "%lu %d G", size, _tid);
    global_counter.global++;
  }

  if( GL_UNLIKELY(fnname != NULL) ){
    finish_trace_line(addr, size, _debug_info);
    //VG_(sprintf)(line_buffer,"%s %s %s::%s %s\n",_address, _sz_tid, current_soname, fnname, _debug_info);
		// Unused current_soname adn fnname for mapvis
    VG_(sprintf)(line_buffer,"%s %s %s \n",_address, _sz_tid, _debug_info);
  }
  else if(dontcare == 0){
    //VG_(sprintf)(line_buffer,"%s %s %s\n",_address, _sz_tid, current_soname);
		// Unused current_soname adn fnname for mapvis
    VG_(sprintf)(line_buffer,"%s %s %s \n",_address, _sz_tid, fnname);
  }

  
  wrout(fd_trace, line_buffer);
}

static VG_REGPARM(2) void trace_store(Addr addr, SizeT size)
{
  global_counter.tstore++;

  if(INSTRUMENT == False)
    return;

  Addr paddr;
  Addr virt_page_addr;

  /*for prints*/
  HChar _address[64];
  _address[0] = '\0';
  HChar _sz_tid[32];
  _sz_tid[0] = '\0';
  HChar _debug_info[512];
  _debug_info[0] = '\0';

  global_counter.store++;


  if(clo_map_virt_to_phys == True){
    paddr = gl_get_physaddr(addr);
    VG_(sprintf)(_address, "S %09lx %016lx", addr, paddr);
  }
  else{
    VG_(sprintf)(_address, "S %09lx", addr);
  }

  if(clo_track_pages == True){
    virt_page_addr = ROUNDDOWN_4K(addr);
    vpagenode* vpn = VG_(HT_lookup)(virtual_page_list, (UWord) virt_page_addr);
    if(vpn == NULL){
      vpn = VG_(malloc)("virtualpagelist", sizeof(vpagenode));
      vpn->addr = virt_page_addr;
      vpn->refs = 1;
      VG_(HT_add_node)(virtual_page_list, vpn);
    }
    else{
      vpn->refs++;				
    }
  }

  /*stack, global, or heap?*/
  if(addr >= tStack){
    VG_(sprintf)(_sz_tid, "%lu %d S", size, _tid);
    global_counter.stack++;
  }
  else if(addr >= bHeap && addr <=tHeap){	
    VG_(sprintf)(_sz_tid, "%lu %d H", size, _tid);
    global_counter.heap++;
  }
  else{
    //VG_(sprintf)(_sz_tid, "%lu %d G", size, _tid);
    global_counter.global++;
  }

  if(fnname != NULL){
    finish_trace_line(addr, size, _debug_info);
    //VG_(sprintf)(line_buffer,"%s %s %s::%s %s\n",_address, _sz_tid, current_soname, fnname, _debug_info);
		// Unused current_soname adn fnname for mapvis
    VG_(sprintf)(line_buffer,"%s %s %s \n",_address, _sz_tid, _debug_info);
  }
  else{
    //VG_(sprintf)(line_buffer,"%s %s %s\n",_address, _sz_tid, current_soname);
		// Unused current_soname adn fnname for mapvis
    VG_(sprintf)(line_buffer,"%s %s %s \n",_address, _sz_tid, fnname);
  }
  wrout(fd_trace, line_buffer);
}

static VG_REGPARM(2) void trace_modify(Addr addr, SizeT size)
{
  global_counter.tmodify++;

  if(INSTRUMENT == False)
    return;

  Addr paddr;
  Addr virt_page_addr;

  /*for prints*/
  HChar _address[64];
  _address[0] = '\0';
  HChar _sz_tid[32];
  _sz_tid[0] = '\0';
  HChar _debug_info[512];
  _debug_info[0] = '\0';

  global_counter.modify++;

  if(clo_map_virt_to_phys == True){
    paddr = gl_get_physaddr(addr);
    VG_(sprintf)(_address, "M %09lx %016lx", addr, paddr);
  }
  else{
    VG_(sprintf)(_address, "M %09lx", addr);
  }

  if(clo_track_pages == True){
    virt_page_addr = ROUNDDOWN_4K(addr);
    vpagenode* vpn = VG_(HT_lookup)(virtual_page_list, (UWord) virt_page_addr);
    if(vpn == NULL){
      vpn = VG_(malloc)("virtualpagelist", sizeof(vpagenode));
      vpn->addr = virt_page_addr;
      vpn->refs = 1;
      VG_(HT_add_node)(virtual_page_list, vpn);
    }
    else{
      vpn->refs++;				
    }
  }

  /*stack, global, or heap?*/
  if(addr >= tStack){
    VG_(sprintf)(_sz_tid, "%lu %d S", size, _tid);
    global_counter.stack++;
  }
  else if(addr >= bHeap && addr <=tHeap){	
    VG_(sprintf)(_sz_tid, "%lu %d H", size, _tid);
    global_counter.heap++;
  }
  else{
    //VG_(sprintf)(_sz_tid, "%lu %d G", size, _tid);
    global_counter.global++;
  }

  if(fnname != NULL){
    finish_trace_line(addr, size, _debug_info);
    //VG_(sprintf)(line_buffer,"%s %s %s::%s %s\n", _address, _sz_tid, current_soname, fnname, _debug_info);
		// Unused current_soname adn fnname for mapvis
    VG_(sprintf)(line_buffer,"%s %s %s \n",_address, _sz_tid, _debug_info);
  }
  else{
    //VG_(sprintf)(line_buffer,"%s %s %s\n",_address, _sz_tid, current_soname);
		// Unused current_soname adn fnname for mapvis
    VG_(sprintf)(line_buffer,"%s %s %s \n",_address, _sz_tid, fnname);
  }
  wrout(fd_trace, line_buffer);
}

static void flushEvents(IRSB* sb)
{
  Int        i;
  const HChar*      helperName;
  void*      helperAddr;
  IRExpr**   argv;
  IRDirty*   di;
  Event*     ev;

  for (i = 0; i < events_used; i++) {

    ev = &events[i];

    /* decide on helper fn to call and args to pass it. */
    switch (ev->ekind) {
      case Event_Ir: helperName = "trace_instr";
                     helperAddr =  trace_instr;  break;

      case Event_Dr: helperName = "trace_load";
                     helperAddr =  trace_load;   break;

      case Event_Dw: helperName = "trace_store";
                     helperAddr =  trace_store;  break;

      case Event_Dm: helperName = "trace_modify";
                     helperAddr =  trace_modify; break;
      default:
                     tl_assert(0);
    }

    /* add the helper */
    argv = mkIRExprVec_2( ev->addr, mkIRExpr_HWord( ev->size ) );
    di   = unsafeIRDirty_0_N( /* regparms */ 2, 
        helperName, VG_(fnptr_to_fnentry)( helperAddr ),
        argv );
    addStmtToIRSB( sb, IRStmt_Dirty(di) );
  }
  events_used = 0;
}

/* WARNING:  If you aren't interested in instruction reads, you can omit the
   code that adds calls to trace_instr() in flushEvents().  However, you
   must still call this function, addEvent_Ir() -- it is necessary to add
   the Ir events to the events list so that merging of paired load/store
   events into modify events works correctly. */
static void addEvent_Ir ( IRSB* sb, IRAtom* iaddr, UInt isize )
{
  Event* evt;
  tl_assert( (VG_MIN_INSTR_SZB <= isize && isize <= VG_MAX_INSTR_SZB)
      || VG_CLREQ_SZB == isize );
  if (events_used == N_EVENTS)
    flushEvents(sb);
  tl_assert(events_used >= 0 && events_used < N_EVENTS);
  evt = &events[events_used];
  evt->ekind = Event_Ir;
  evt->addr  = iaddr;
  evt->size  = isize;
  events_used++;
}

  static
void addEvent_Dr ( IRSB* sb, IRAtom* daddr, Int dsize )
{
  Event* evt;
  tl_assert(isIRAtom(daddr));
  tl_assert(dsize >= 1 && dsize <= MAX_DSIZE);
  if (events_used == N_EVENTS)
    flushEvents(sb);
  tl_assert(events_used >= 0 && events_used < N_EVENTS);
  evt = &events[events_used];
  evt->ekind = Event_Dr;
  evt->addr  = daddr;
  evt->size  = dsize;
  events_used++;
}

  static
void addEvent_Dw ( IRSB* sb, IRAtom* daddr, Int dsize )
{
  Event* lastEvt;
  Event* evt;
  tl_assert(isIRAtom(daddr));
  tl_assert(dsize >= 1 && dsize <= MAX_DSIZE);

  /* is it possible to merge this write with the preceding read? */
  lastEvt = &events[events_used-1];
  if (events_used > 0
      && lastEvt->ekind == Event_Dr
      && lastEvt->size  == dsize
      && eqIRAtom(lastEvt->addr, daddr))
  {
    lastEvt->ekind = Event_Dm;
    return;
  }

  /* No.  Add as normal. */
  if (events_used == N_EVENTS)
    flushEvents(sb);
  tl_assert(events_used >= 0 && events_used < N_EVENTS);
  evt = &events[events_used];
  evt->ekind = Event_Dw;
  evt->size  = dsize;
  evt->addr  = daddr;
  events_used++;
}

static void pre_sb(Addr addr)
{ 

  /* update thread id*/
  _tid = VG_(get_running_tid)();
 
  /* if ignore_sonames[] is not set */
//  if( clo_ignore_sonames[0] == NULL ||
//      clo_instrumentation_state == False)
//    return;

  int i = 0;
  DebugInfo* dinfo = VG_(find_DebugInfo) ( addr );
  if( GL_UNLIKELY(dinfo == NULL) ) {
    VG_(strcpy)(current_soname, "NULL"); 
    return;
  }

  HChar soname[128];
  VG_(strcpy)(soname, VG_(DebugInfo_get_soname)(dinfo));

  /* parse the name */
  for(i=0; soname[i] != '.' && soname[i] != '\0'; i++){}
  soname[i] = '\0';
 
  if( clo_instrument_sonames_state == True){
    for(i=0; clo_ignore_sonames[i] != NULL && i<64; i++){
      if( VG_(strcmp)(clo_ignore_sonames[i], soname) == 0 ){
        clo_instrument_sonames_state = False;
        VG_(strcpy)(clo_ignored_soname, soname);
        soname[0] = '\0';
        break;
      }
    }
  }
  else{
    /* check if we jumped out of the library */
    if(VG_(strcmp)(clo_ignored_soname, soname) != 0){
      for(i=0; clo_ignore_sonames[i] != NULL && i<64; i++){
        if( VG_(strcmp)(clo_ignore_sonames[i], soname) == 0){
					clo_instrument_sonames_state = False;
					VG_(strcpy)(clo_ignored_soname, soname); //we jumped out but found new ignored lib
					soname[0] = '\0';
          return;
        }
      }
      clo_instrument_sonames_state = True;
      clo_ignored_soname[0] = '\0';
      soname[0] = '\0';
    }
  }
  
  VG_(strcpy)(current_soname, soname); 
  return;
}

static void mark_fnentry(void)
{
  Int i=0;
  
  nSps = VG_(get_StackTrace)(_tid, ips, STRACEBUF, sps, fps, 0);
  
  if(VG_(get_fnname)(ips[0], &fnname) == True){
    
    /* look up instr addr, do we have the fnname, lib info? */
    fn_name_t* fn = VG_(OSetGen_Lookup)(fnname_list, &ips[0]);

    if(fn == NULL) {
      fn = VG_(OSetGen_AllocNode)(fnname_list, sizeof(fn_name_t));
      fn->addr = ips[0];
      fn->size = 4;
      VG_(strcpy)(fn->fnname, fnname);
      VG_(strcpy)(fn->soname, current_soname);
      
      VG_(OSetGen_Insert)(fnname_list, fn);  
    }
    else{
      /* we already have it */
    }
  
    if(INSTRUMENT == True){
      VG_(sprintf)(line_buffer, "X FN_ENTRY %09lx %s::%s\n", ips[0], current_soname, fnname);
      wrout(fd_trace, line_buffer);
    }
  }

  /* ignore below this function? */
  if( clo_instrument_fn_state == True ){
    for(i=0; clo_ignore_below_fns[i] != NULL && i<64; i++){
      if(VG_(strcmp)(clo_ignore_below_fns[i], fnname) == 0){
        clo_instrument_fn_state = False;
        clo_instrument_fn_state_sps = fps[0];
        
        return;
      }
    }
  }

}

/* ------- */

/* track mmaps */
static void gl_new_mmap(Addr addr, SizeT size, Bool rr, Bool ww, Bool xx, ULong di)
{
  mblock_t *mc = NULL;

  mc = VG_(OSetGen_Lookup)(mmap_list, &addr);
  if(mc != NULL){
    return;
  }

  mc = VG_(OSetGen_AllocNode)(mmap_list, sizeof(mblock_t));
  tl_assert(mc != NULL);

  mc->addr = addr;
  mc->size = size;
  mc->instance = 0;
  mc->trefs = 0;
  mc->refs = 0;
  VG_(strcpy)(mc->name, "mmap_block");

  VG_(OSetGen_Insert)(mmap_list, mc);

  if(addr < bMMap)
    bMMap = addr;
  else if (addr + size > tMMap)
    tMMap = addr + size;

  VG_(sprintf)(line_buffer, "X MMAP %09lx %lu\n", addr, size);
  wrout(fd_trace, line_buffer);
}

static void gl_die_mmap(Addr addr, SizeT size)
{
  mblock_t* mc = VG_(OSetGen_Remove)(mmap_list, &addr);
  tl_assert(mc != NULL);

  VG_(OSetGen_FreeNode)(mmap_list, mc);
}

/*
 * Handle system calls, fork, clone, etc.
 */
static void gl_pre_sys_call(ThreadId tid, UInt syscallno, UWord* args, UInt nArgs)
{
}

static void gl_post_sys_call(ThreadId tid, UInt syscallno, UWord* args, UInt nArgs, SysRes res)
{
  if(syscallno == 56 || syscallno == 57 || syscallno == 58) {
    if(sr_Res(res) > 0) {
      VG_(sprintf)(line_buffer, "X FORK %d at %llu\n", (Int)sr_Res(res), global_counter.tinstr);
      wrout(fd_trace, line_buffer);
    }
  }
}

static void gl_pre_thread_create(const ThreadId ptid, const ThreadId ctid)
{
  VG_(sprintf)(line_buffer, "X THREAD_CREATE %d:%d\n", (Int)ptid, (Int)ctid);
  wrout(fd_trace, line_buffer);
}

/* Support start instrumentation client requests */
static Bool gl_handle_client_request(ThreadId tid, UWord* args, UWord *ret)
{
  gblock_t *gc;
  hblock_t *hc;
  mblock_t *mc;
  malloc_name_t *malloc_name;
  mmap_name_t   *mmap_name;

  Addr addr;
  SizeT size;
  HChar buffer[128];
  HChar buffer2[128];
  int i = 0;

  switch(args[0]) {
    case VG_USERREQ__MARK_A:
      VG_(sprintf)(line_buffer, "X MARK_A %llu at %llu\n", gl_mark_cnt_A, global_counter.tinstr);
      wrout(fd_trace, line_buffer);
      //VG_(write)(fd_trace, (void*)line_buffer, VG_(strlen)(line_buffer));

      gl_mark_cnt_A++;
      *ret = 0;
      break;

    case VG_USERREQ__MARK_B:
      VG_(sprintf)(line_buffer, "X MARK_B %llu at %llu\n", gl_mark_cnt_B, global_counter.tinstr);
      wrout(fd_trace, line_buffer);
      //VG_(write)(fd_trace, (void*)line_buffer, VG_(strlen)(line_buffer));

      gl_mark_cnt_B++;
      *ret = 0;
      break;

    case VG_USERREQ__MARK_C:
      VG_(sprintf)(line_buffer, "X MARK_C %llu at %llu\n", gl_mark_cnt_C, global_counter.tinstr);
      wrout(fd_trace, line_buffer);
      //VG_(write)(fd_trace, (void*)line_buffer, VG_(strlen)(line_buffer));

      gl_mark_cnt_C++;
      *ret = 0;
      break;

    case VG_USERREQ__MARK_D:
      VG_(sprintf)(line_buffer, "X MARK_D %llu at %llu\n", gl_mark_cnt_D, global_counter.tinstr);
      wrout(fd_trace, line_buffer);
      //VG_(write)(fd_trace, (void*)line_buffer, VG_(strlen)(line_buffer));

      gl_mark_cnt_D++;
      *ret = 0;
      break;

    case VG_USERREQ__MARK_E:
      VG_(sprintf)(line_buffer, "X MARK_E %llu at %llu\n", gl_mark_cnt_E, global_counter.tinstr);
      wrout(fd_trace, line_buffer);
      //VG_(write)(fd_trace, (void*)line_buffer, VG_(strlen)(line_buffer));

      gl_mark_cnt_E++;
      *ret = 0;
      break;

    case VG_USERREQ__RESET_MARKS:
      gl_mark_cnt_A = 0;
      gl_mark_cnt_B = 0;
      gl_mark_cnt_C = 0;
      gl_mark_cnt_D = 0;
      gl_mark_cnt_E = 0;

      *ret = 0;
      break;

    case VG_USERREQ__MARK_STR:
      VG_(sprintf)(line_buffer, "X MARK %s at %llu\n", (HChar*) args[1], global_counter.tinstr);
      wrout(fd_trace, line_buffer);
      //VG_(write)(fd_trace, (void*)line_buffer, VG_(strlen)(line_buffer));

      *ret = 0;
      break;

    case VG_USERREQ__GSTART:
      global_instrumentation_state = True;
      clo_instrumentation_state = True;

      *ret = 0;
      break;

    case VG_USERREQ__GSTOP:
      global_instrumentation_state = False;
      clo_instrumentation_state = False;
      fnname = fnname_null;

      *ret = 0;
      break;

    case VG_USERREQ__START:
      if(global_instrumentation_state == True){
        clo_instrumentation_state = True;
      }

      *ret = 0; 
      break;

    case VG_USERREQ__STOP:
      if(global_instrumentation_state == True){
        clo_instrumentation_state = False;
        fnname = fnname_null;
      }

      *ret = 0; 
      break;

      /* User record malloc regions */
    case VG_USERREQ__SET_MALLOC_NAME:
      user_malloc_name_array[user_malloc_name_cnt] = VG_(malloc)("structname", FILE_LEN);
      VG_(strcpy)(user_malloc_name_array[user_malloc_name_cnt], (HChar*) args[1]);
      user_malloc_name_cnt++;

      if(user_malloc_name_cnt > (MAX_USER_NAMES-1)){
        VG_(umsg)("gl_basics.h : #define MAX_USER_NAMES %d", MAX_USER_NAMES);
        VG_(tool_panic)("Number of user defined malloc structures exceeded preset amount.");
      }

      *ret = 0; 
      break;

    case VG_USERREQ__UPDATE_MALLOC:

      VG_(strcpy)(buffer, (HChar*) args[1]);
      addr = (UWord) args[2];

      /* Look up malloc_name */
      malloc_name = VG_(OSetGen_Lookup)(malloc_name_list, buffer);
      if(malloc_name == NULL){
        malloc_name = VG_(OSetGen_AllocNode)(malloc_name_list, sizeof(malloc_name_t));

        VG_(strcpy)(malloc_name->name, buffer);
        malloc_name->instance = 0;
        VG_(OSetGen_Insert)(malloc_name_list, malloc_name);
      }
      else{
        malloc_name->instance++;
      }

      /* Look up block */
      hc = VG_(OSetGen_Lookup)(malloc_list, &addr);
      if(hc != NULL) {
        VG_(strcpy)(hc->name, buffer);
        hc->refs = 0;
        hc->trefs = 0;
        hc->instance = malloc_name->instance;

        VG_(sprintf)(buffer, "X UPDATE_MALLOC %s-%llu %llu at %09lx\n",
            (HChar*) hc->name, hc->instance, (ULong) hc->req_szB, addr);
        wrout(fd_trace, line_buffer);
        //VG_(write)(fd_trace, (void*)buffer, VG_(strlen)(buffer));
      }
      else{
        VG_(sprintf)(buffer, "X UPDATE_MALLOC_failed %s-%llu at %09lx failed\n",
            (HChar*) malloc_name->name, malloc_name->instance, addr);
        wrout(fd_trace, line_buffer);
        //VG_(write)(fd_trace, (void*)buffer, VG_(strlen)(buffer));
      }

      *ret = 0;
      break;

      /* User record mmap regions */ 
    case VG_USERREQ__SET_MMAP_NAME:
      user_mmap_name_array[user_mmap_name_cnt] = VG_(malloc)("mmapname", FILE_LEN);
      VG_(strcpy)(user_mmap_name_array[user_mmap_name_cnt], (HChar*) args[1]);
      user_mmap_name_cnt++;

      if(user_mmap_name_cnt > (MAX_USER_NAMES-1)){
        VG_(umsg)("gl_misc.h from gl_main.c: #define MAX_USER_MMAP_NAMES %d", MAX_USER_NAMES);
        VG_(tool_panic)("Number of user defined mmap names exceeded preset amount.");
      }

      *ret = 0;
      break;

    case VG_USERREQ__UPDATE_MMAP:
      addr = (UWord) args[1];
      VG_(strcpy)(buffer, (HChar*) args[2]);

      VG_(umsg)("ADDRES %09lx \n", addr);

      /* Look up mmap name */
      mmap_name = VG_(OSetGen_Lookup)(mmap_name_list, buffer);
      if(mmap_name == NULL){
        mmap_name = VG_(OSetGen_AllocNode)(mmap_name_list, sizeof(mmap_name_t));

        VG_(strcpy)(mmap_name->name, buffer);
        mmap_name->instance = 0;
        VG_(OSetGen_Insert)(mmap_name_list, mmap_name);
      }
      else{
        mmap_name->instance++;
      }

      /* Look up block */
      mc = VG_(OSetGen_Lookup)(mmap_list, &addr);
      if(mc != NULL){
        VG_(strcpy)(mc->name, buffer);
        mc->refs = 0;
        mc->trefs = 0;
        mc->instance = mmap_name->instance;

        VG_(sprintf)(buffer, "X UPDATE_MMAP %s-%llu %llu at %09lx\n",
            (HChar*) mc->name, mc->instance, (ULong) mc->size, addr);
        wrout(fd_trace, line_buffer);
      }
      else{
        VG_(sprintf)(buffer, "X UPDATE_MMAP_failed %s-%llu at %09lx failed\n",
            (HChar*) mmap_name->name, mmap_name->instance, addr);
        wrout(fd_trace, line_buffer);
      }

      *ret = 0;
      break;

      /* User record global regions */
    case VG_USERREQ__RECORD_GLOBAL:

      addr = (UWord) args[1];
      size = (UInt) args[2];
      VG_(strcpy)(buffer, (HChar*) args[3]);

      gc = VG_(OSetGen_Lookup)(global_list, buffer);

      if(gc == NULL){
        gc = VG_(OSetGen_AllocNode)(global_list, sizeof(gblock_t));
        tl_assert(gc != NULL);

        VG_(strcpy)(gc->name, (HChar*) args[3]);
        gc->addr = addr;
        gc->size = size;

        VG_(OSetGen_Insert)(global_list, gc);
      }
      else{
        VG_(strcpy)(gc->name, (HChar*) args[3]);
        gc->addr = addr;
        gc->size = size;
      }

      break;

    case VG_USERREQ__UNRECORD_GLOBAL:
      addr = (UWord) args[1];
      gc = VG_(OSetGen_Remove)(global_list, &addr);
      VG_(OSetGen_FreeNode)(global_list, gc);

      break;

    case VG_USERREQ__UMSG:
      VG_(umsg)("X GL_UMSG %s at %llu\n", (HChar*) args[1], global_counter.tinstr);

      *ret = 0;

      break;

    case VG_USERREQ__RENAME_TRACE:
      VG_(strcpy)(buffer, (HChar*) args[1]);

      /* clean any whitespace if exists */
      for(i=0; i<VG_(strlen)( buffer ); i++){
        if(buffer[i] == ' ' || buffer[i] == '\t')
          buffer[i] = '_';
      }

      VG_(sprintf)(buffer, "%s.%%p", buffer);
      gl_file_name = VG_(expand_file_name)("", buffer);

      /* rename trace, dynstats, stats */ 
      VG_(rename)(old_gl_file_name, gl_file_name);

      VG_(sprintf)(buffer, "%s.stats", gl_file_name);
      VG_(sprintf)(buffer2, "%s.stats", old_gl_file_name);
      VG_(rename)(buffer2, buffer);

      VG_(sprintf)(buffer, "%s.dynstats", gl_file_name);
      VG_(sprintf)(buffer2, "%s.dynstats", old_gl_file_name);
      VG_(rename)(buffer2, buffer);

      *ret = 0;

      break;

    default:
      return False;
  }

  return True;
}

/*------------------------------------------------------------*/
/*--- Basic tool functions                                 ---*/
/*------------------------------------------------------------*/

/*
 * Gleipnir post command line option inits */
static void gl_post_clo_init(void)
{
  int i;
  SysRes sres;

  global_pid = VG_(getpid)();
  local_pid = global_pid;

  if(VG_(strcmp)(clo_gl_out, "stdout") != 0){

    if(gl_file_name == NULL) {
      gl_file_name = VG_(expand_file_name)("gl-out=false", "gleipnir.%p");
      VG_(strcpy)(old_gl_file_name, gl_file_name);
      
      wrout = &write_to_file;
    }

    /* open/create trace file */
    sres = VG_(open)(gl_file_name, VKI_O_CREAT|VKI_O_TRUNC|VKI_O_WRONLY, VKI_S_IRUSR|VKI_S_IWUSR);
    if(sr_isError(sres)){
      VG_(umsg)("error: can't open file for writing\n");
      return;
    }
    else{
      fd_trace = sr_Res(sres);
    }
  }

  /* initialize counter */
  counters_init(&global_counter);
  

  /* Init trace file */
  VG_(sprintf)(line_buffer, "X START 0:%d at %llu\n", local_pid, global_counter.tinstr);
  if(wrout == NULL){
    VG_(umsg)("null\n");
  }
    
  wrout(fd_trace, line_buffer);

  /* open and create a file for .stats*/
/* XXX no stats
  VG_(sprintf)(tmp, "%s.stats", gl_file_name);
  sres = VG_(open)(tmp, VKI_O_CREAT|VKI_O_TRUNC|VKI_O_WRONLY,
      VKI_S_IRUSR|VKI_S_IWUSR);
  if(sr_isError(sres)){
    VG_(umsg)("error: can't open file for writing (.stats)\n");
    return;
  }
  else{
    fd_flush = sr_Res(sres);
  }
*/

  /* open and create a file for .dyn_stats*/
/* XXX no stats
  VG_(sprintf)(tmp, "%s.dynstats", gl_file_name);
  sres = VG_(open)(tmp, VKI_O_CREAT|VKI_O_TRUNC|VKI_O_WRONLY,
      VKI_S_IRUSR|VKI_S_IWUSR);
  if(sr_isError(sres)){
    VG_(umsg)("error: can't open file for writing (.dynstats)\n");
    return;
  }
  else{
    fd_dynstats = sr_Res(sres);
  }
*/

  /* initialize sTLB */
  for(i=0; i<sTLB_SIZE; i++){
    sTLB[i].TAG = 0;
    sTLB[i].ppa = 0;
  }

  /* initialize current_soname */
  current_soname[0] = '\0';

//  /* initialize counters*/
//  VG_(sprintf)(tmp,"%-15s %-12s %-12s %-12s %-12s %-12s\n", 
//      "Instructions", "Stack", "Heap", "Global", 
//      "Total", "Memory Usg");
//  VG_(write)(fd_flush, tmp, VG_(strlen)(tmp));

  /* create memory region lists the key offset is important! */ 
  malloc_list = VG_(OSetGen_Create)(/* keyOffset */ 0, addrCmp_heap, VG_(malloc), "gl.main.malloc_list", VG_(free));
  mmap_list   = VG_(OSetGen_Create)(/* keyOffset */ 0, addrCmp_mmap, VG_(malloc), "gl.main.mmap_list", VG_(free));
  global_list = VG_(OSetGen_Create)(/* keyOffset */ 0, addrCmp_global, VG_(malloc), "gl.main.global_list", VG_(free));
  //XXX custom_mem_list = VG_(OSetGen_Create)(/* keyOffset */ 0, addrCmp, VG_(malloc), "gl.main.global_list", VG_(free));

  /* create mem_name_list */
  malloc_name_list = VG_(OSetGen_Create)(/* keyOffset */ 0, nameCmp, VG_(malloc), "gl.main.mstructList", VG_(free));
  mmap_name_list = VG_(OSetGen_Create)(/* keyOffset */ 0, nameCmp, VG_(malloc), "gl.main.mstructList", VG_(free));
  //XXX custom_mem_name_list = VG_(OSetGen_Create)(/* keyOffset */ 0, nameCmp, VG_(malloc), "gl.main.mstructList", VG_(free));

  /* create fnname and soname list */
  fnname_list = VG_(OSetGen_Create)(/* keyOffset */ 0, addrCmp_fnname, VG_(malloc), "gl.main.fnnameList", VG_(free));

  /* allocate vpage list */ 
  virtual_page_list = VG_(HT_construct)("gl.main.pagelist");

  /* clear user_malloc_name_array */
  for(i=0; i<MAX_USER_NAMES; i++){
    user_malloc_name_array[i] = NULL;
  }
}

/*
 * SB-instrument
 */
  static IRSB* gl_instrument( VgCallbackClosure* closure,
    IRSB* sbIn, 
    const VexGuestLayout* layout, 
    const VexGuestExtents* vge,
    const VexArchInfo* archinfo_host,
    IRType gWordTy, IRType hWordTy )
{

  IRExpr* data;
  IRDirty* di;
  Int        i;
  IRSB*      sbOut;
  IRTypeEnv* tyenv = sbIn->tyenv;

  if (gWordTy != hWordTy) {
    /* we don't currently support this case. */
    VG_(tool_panic)("host/guest word size mismatch");
  }

  /* For applications that fork/clone etc.
   * Check for a global vs local pid change,
   * create a new file and record change if it occured
   * Note that forked application MUST write into a file, otherwise
   * their output becomes entangled */
  if(clo_multi_process == True){
    local_pid = VG_(getpid)();
    if(local_pid != global_pid) { 
      SysRes sres;

      /* set a new file name*/
      gl_file_name = VG_(expand_file_name)("--gl-out", "gleipnir.%p");	

      /* close fd and open a new file for writing */
      VG_(close)(fd_trace);	
      sres = VG_(open)(gl_file_name, VKI_O_CREAT|VKI_O_TRUNC|VKI_O_WRONLY,
          VKI_S_IRUSR|VKI_S_IWUSR);
      if(sr_isError(sres)){
        VG_(umsg)("error: can't open file for writing\n");
        return NULL;
      }
      else{
        fd_trace = sr_Res(sres);
      }
      /* write the PID on top */
      VG_(sprintf)(line_buffer, "X START %d:%d at %llu\n", global_pid, local_pid, global_counter.tinstr);
      wrout(fd_trace, line_buffer); 

      /* reset the global pid , up to this point the child knows the parents PID*/
      global_pid = local_pid;
    }
  }
  
  if(!INSTRUMENT){ return sbIn; }

  /* Instrument */
  /* set up SB */
  sbOut = deepCopyIRSBExceptStmts(sbIn);

  /* copy verbatim any IR preamble preceding the first IMark */
  i = 0;
  while (i < sbIn->stmts_used && sbIn->stmts[i]->tag != Ist_IMark) {
    addStmtToIRSB( sbOut, sbIn->stmts[i] );
    i++;
  }

  /* update debug info - the current [i] statement is an IMark
   * because the preamble stopped at IMark */
  di = unsafeIRDirty_0_N(1, "pre_sb",
                         VG_(fnptr_to_fnentry)( &pre_sb ),
                         mkIRExprVec_1( mkIRExpr_HWord((HWord)sbIn->stmts[i]->Ist.IMark.addr) ));
  addStmtToIRSB( sbOut, IRStmt_Dirty(di) );

  events_used = 0;

  for (/* use current i */; i < sbIn->stmts_used; i++) {
    IRStmt* st = sbIn->stmts[i];
    if (!st || st->tag == Ist_NoOp) continue;


    switch (st->tag) {
      case Ist_NoOp:
      case Ist_AbiHint:
      case Ist_Put:
      case Ist_PutI:
      case Ist_MBE:
        addStmtToIRSB( sbOut, st );
        break;


      case Ist_IMark:

        /* mark fnentry */
        if (VG_(get_fnname_if_entry)(st->Ist.IMark.addr, &fnname)) {
          di = unsafeIRDirty_0_N(0, "mark_fnentry",
                  VG_(fnptr_to_fnentry)( &mark_fnentry ),
                  mkIRExprVec_0());
          addStmtToIRSB( sbOut, IRStmt_Dirty(di) );
        }

        /* WARNING: do not remove this function call, even if you
           aren't interested in instruction reads.  See the comment
           above the function itself for more detail. */
        addEvent_Ir( sbOut, mkIRExpr_HWord( (HWord)st->Ist.IMark.addr ),
            st->Ist.IMark.len );
        addStmtToIRSB( sbOut, st );
        break;

      case Ist_WrTmp:
        data = st->Ist.WrTmp.data;
        if (data->tag == Iex_Load) {
          addEvent_Dr( sbOut, data->Iex.Load.addr,
              sizeofIRType(data->Iex.Load.ty) );
        }
        addStmtToIRSB( sbOut, st );
        break;

      case Ist_Store:
        data  = st->Ist.Store.data;
        addEvent_Dw( sbOut, st->Ist.Store.addr,
            sizeofIRType(typeOfIRExpr(tyenv, data)) );
        addStmtToIRSB( sbOut, st );
        break;

      case Ist_Dirty: {
                        Int      dsize;
                        IRDirty* d = st->Ist.Dirty.details;
                        if (d->mFx != Ifx_None) {
                          /* This dirty helper accesses memory.  Collect the details. */
                          tl_assert(d->mAddr != NULL);
                          tl_assert(d->mSize != 0);
                          dsize = d->mSize;
                          if (d->mFx == Ifx_Read || d->mFx == Ifx_Modify)
                            addEvent_Dr( sbOut, d->mAddr, dsize );
                          if (d->mFx == Ifx_Write || d->mFx == Ifx_Modify)
                            addEvent_Dw( sbOut, d->mAddr, dsize );
                        } else {
                          tl_assert(d->mAddr == NULL);
                          tl_assert(d->mSize == 0);
                        }
                        addStmtToIRSB( sbOut, st );
                        break;
                      }

      case Ist_CAS: {
                      /* We treat it as a read and a write of the location.  I
                         think that is the same behaviour as it was before IRCAS
                         was introduced, since prior to that point, the Vex
                         front ends would translate a lock-prefixed instruction
                         into a (normal) read followed by a (normal) write. */
                      Int    dataSize;
                      IRType dataTy;
                      IRCAS* cas = st->Ist.CAS.details;
                      tl_assert(cas->addr != NULL);
                      tl_assert(cas->dataLo != NULL);
                      dataTy   = typeOfIRExpr(tyenv, cas->dataLo);
                      dataSize = sizeofIRType(dataTy);
                      if (cas->dataHi != NULL)
                        dataSize *= 2; /* since it's a doubleword-CAS */
                      addEvent_Dr( sbOut, cas->addr, dataSize );
                      addEvent_Dw( sbOut, cas->addr, dataSize );
                      addStmtToIRSB( sbOut, st );
                      break;
                    }

      case Ist_LLSC: {
                       IRType dataTy;
                       if (st->Ist.LLSC.storedata == NULL) {
                         /* LL */
                         dataTy = typeOfIRTemp(tyenv, st->Ist.LLSC.result);
                         addEvent_Dr( sbOut, st->Ist.LLSC.addr, sizeofIRType(dataTy) );
                       } else {
                         /* SC */
                         dataTy = typeOfIRExpr(tyenv, st->Ist.LLSC.storedata);
                         addEvent_Dw( sbOut, st->Ist.LLSC.addr, sizeofIRType(dataTy) );
                       }
                       addStmtToIRSB( sbOut, st );
                       break;
                     }

      case Ist_Exit: {
                       flushEvents(sbOut);
                       addStmtToIRSB( sbOut, st );   /* original statement */
                       break;
                     }

      default:
                     tl_assert(0);
    }
  }
  flushEvents(sbOut);

  return sbOut;
}

/* Gleipnir, finish instrumentation */
static void gl_fini(Int exitcode)
{

  /* finish trace */ 
  VG_(sprintf)(line_buffer, "X END %u at %llu\n", global_pid, global_counter.tinstr);
  wrout(fd_trace, line_buffer);

//  counters_flush(&global_counter); 
//  VG_(close)(fd_flush); 

  VG_(sprintf)(line_buffer, "X STATS\n");
  wrout(fd_trace, line_buffer);
  VG_(sprintf)(line_buffer, "  total_lines: %15llu\n", global_counter.total_lines);
  wrout(fd_trace, line_buffer);
  VG_(sprintf)(line_buffer, "     flush_at: %15llu\n", clo_flush_counter_at);
  wrout(fd_trace, line_buffer);
  VG_(sprintf)(line_buffer, "total_flushes: %15llu\n\n", global_counter.cnt);
  wrout(fd_trace, line_buffer);

  VG_(sprintf)(line_buffer, " malloc calls: %15llu\n", global_counter.malloc_calls);
  wrout(fd_trace, line_buffer);
  VG_(sprintf)(line_buffer, " calloc calls: %15llu\n", global_counter.calloc_calls);
  wrout(fd_trace, line_buffer);
  VG_(sprintf)(line_buffer, "realloc calls: %15llu\n", global_counter.realloc_calls);
  wrout(fd_trace, line_buffer);
  VG_(sprintf)(line_buffer, "   free calls: %15llu\n\n", global_counter.free_calls);
  wrout(fd_trace, line_buffer);

  VG_(sprintf)(line_buffer, " Instructions: %15llu\n", global_counter.tinstr);
  wrout(fd_trace, line_buffer);
  VG_(sprintf)(line_buffer, "        Loads: %15llu\n", global_counter.tload);
  wrout(fd_trace, line_buffer);
  VG_(sprintf)(line_buffer, "       Stores: %15llu\n", global_counter.tstore);
  wrout(fd_trace, line_buffer);
  VG_(sprintf)(line_buffer, "     Modifies: %15llu\n", global_counter.tmodify);
  wrout(fd_trace, line_buffer);

  if(clo_track_pages == True){	
    VG_(sprintf)(line_buffer, "\nUSED PAGES: %d\n", VG_(HT_count_nodes)(virtual_page_list));
    wrout(fd_trace, line_buffer);
  }

  VG_(sprintf)(line_buffer, "--\ninstruction soname:function\n");
  wrout(fd_trace, line_buffer);

  fn_name_t* fn;

  VG_(OSetGen_ResetIter)(fnname_list);

  while(( fn = VG_(OSetGen_Next)(fnname_list) ) != NULL){
    VG_(sprintf)(line_buffer, "%09lx %s:%s\n", fn->addr, fn->soname, fn->fnname);
    wrout(fd_trace, line_buffer);
  }
  
  VG_(close)(fd_trace);
}

// *** Stash code until I figure out how to use it properly.
//static void gl_new_stack_signal (Addr addr, SizeT len, ThreadId tid)
//{
//  HChar tmp[128];
//
//  if(clo_instrumentation_state == True){
//    VG_(sprintf)(tmp, "X NEW_STACK_SIGNAL %lx %lu\n", addr, len);
//    VG_(write)(fd_trace, tmp, VG_(strlen)(tmp) );
//  }
//}
//
//static void gl_die_stack_signal (Addr addr, SizeT len)
//{
//  HChar tmp[128];
//
//  if(clo_instrumentation_state == True){
//    VG_(sprintf)(tmp, "X DIE_STACK_SIGNAL %lx %lu\n", addr, len);
//    wrout(fd_trace, tmp);
//  }
//}

static void gl_new_stack(Addr addr, SizeT len)
{
  /* This is an arbitrary size, I'm not really sure
   * what the extent of this functionality is.
   * Given a local array, the addr returned by this function
   * does not extend to the top-most stack address (i.e. bottom-most) address.*/
  tStack = addr - (1<<20);
}

// *** stack code until I figure out what it does
static void gl_die_stack(Addr addr, SizeT len)
{
  if(clo_instrument_fn_state_sps > 0)
    if(addr >= clo_instrument_fn_state_sps){
      clo_instrument_fn_state = True;
      clo_instrument_fn_state_sps = 0;
      
      VG_(sprintf)(line_buffer, "X INST %llu\n", global_counter.tinstr);
      wrout(fd_trace, line_buffer);
    }
}

/*
 * Pre command line initis */
static void gl_pre_clo_init(void)
{
  VG_(details_name)            ("Gleipnir");
  VG_(details_version)         (NULL);
  VG_(details_description)     ("a Valgrind tool");
  VG_(details_copyright_author)(
      "Copyright (C) 2011-2012, and GNU GPL'd, by Computer Systems Research Laboratory\nUniversity of North Texas, Denton Texas.");
  VG_(details_bug_reports_to)  (VG_BUGS_TO);
  VG_(details_avg_translation_sizeB) ( 200 );

  VG_(basic_tool_funcs)        (gl_post_clo_init,
                                gl_instrument,
                                gl_fini);

  VG_(needs_command_line_options)(gl_process_clo,
      gl_print_usage,
      gl_print_debug_usage);

  VG_(track_new_mem_stack)             (gl_new_stack);
  VG_(track_die_mem_stack)             (gl_die_stack);
  
  /* experimental stack tracking */
  //  VG_(track_new_mem_stack_signal)      (gl_new_stack_signal);
  //  VG_(track_die_mem_stack_signal)      (gl_die_stack_signal);

  VG_(track_new_mem_mmap)   (gl_new_mmap);
  VG_(track_die_mem_munmap) (gl_die_mmap);

  VG_(needs_syscall_wrapper)( gl_pre_sys_call,
      gl_post_sys_call);

  VG_(track_pre_thread_ll_create) (gl_pre_thread_create);

  VG_(needs_client_requests) (gl_handle_client_request);

  gl_register_malloc_replacements();

  init_alloc_fns();
  init_ignore_fns();
  init_clos();
}

VG_DETERMINE_INTERFACE_VERSION(gl_pre_clo_init)

  /*--------------------------------------------------------------------*/
  /*--- end                                                gl_main.c ---*/
  /*--------------------------------------------------------------------*/
