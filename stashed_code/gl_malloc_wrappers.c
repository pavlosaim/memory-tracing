/* comments */
/* We have malloc wrappers and malloc tracing functions here */

#include "gl_configure.h"

#include "gl_misc.h"
#include "gl_malloc_wrappers.h"
#include "gl_misc.h"

#include "pub_tool_vki.h"
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

#ifdef GL_MALLOC_WRAPPER
/* Local definitions/declarations */
OSet* malloc_list = NULL;				   // OrderedSet of malloc ranges

/* For tracing the callstack */
#define STRACEBUF 16
Addr ips[STRACEBUF];

extern Bool clo_enable_parsing;
extern Char clo_prog_lang;

/* For segment tracing */
extern Addr tStack;
extern Addr bHeap; 
extern Addr tHeap; 

/* application counters */
extern cntType mycounter;

/* user malloc interface variables */
HChar* user_snmarray[MAX_USER_ML_STRUCTS];
UInt   snmcnt = 0,
       snmitt = 0;

/* Log variables */
extern HChar line_buffer[FILE_LEN];

/*-----------------------------------------------*/
/*--- Malloc and friends, Parsing Functions 	---*/
/*-----------------------------------------------*/
Word addrCmp(const void* key, const void* elem)
{
	if( (*(Addr*)key) < ((HB_Chunk*) elem)->data)
		return -1;
	else if((*(Addr*)key) >= (((HB_Chunk*)elem)->data + ((HB_Chunk*)elem)->req_szB))
		return 1;
	else
		return 0;
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

	DO("malloc"														);
	DO("__builtin_new"										);
	DO("operator new(unsigned)"						);
	DO("operator new(unsigned long)"			);
	DO("__builtin_vec_new"								);
	DO("operator new[](unsigned)"					);
	DO("operator new[](unsigned long)"		);
	DO("calloc"												 		);
	DO("realloc"											 		);
	DO("memalign"											 		);
	DO("posix_memalign"										);
	DO("valloc"												 		);
	DO("operator new(unsigned, std::nothrow_t const&)"		 		);
	DO("operator new[](unsigned, std::nothrow_t const&)"	 		);
	DO("operator new(unsigned long, std::nothrow_t const&)"	 	);
	DO("operator new[](unsigned long, std::nothrow_t const&)"	);
#if defined(VGP_ppc32_aix5) || defined(VGP_ppc64_aix5)
	DO("malloc_common"										 );
	DO("calloc_common"										 );
	DO("realloc_common"										 );
	DO("memalign_common"									 );
#elif defined(VGO_darwin)
	DO("malloc_zone_malloc"									 );
	DO("malloc_zone_calloc"									 );
	DO("malloc_zone_realloc"								 );
	DO("malloc_zone_memalign"								 );
	DO("malloc_zone_valloc" 								 );
#endif
}

void init_ignore_fns(void)
{
	/* create the empty list */
	ignore_fns = VG_(newXA)(VG_(malloc), "gl.main.iif.1",VG_(free), sizeof(Char*));
}


/*-----------------------------------*/
/*--- Heap Management Functions 	---*/
/*-----------------------------------*/
static
HB_Chunk* _record_block(ThreadId tid, void* p, SizeT req_szB,
		               HChar* mstruct, HChar* dir, HChar* filename, UInt lineno)
{
	Int hb_enum = 0;
	
	HB_Chunk* hc = VG_(OSetGen_AllocNode)(malloc_list, sizeof(HB_Chunk));
	tl_assert(hc != NULL);
	
	if(gl_search_list)(/*ret_mal*/ NULL, mstruct, &hb_enum,
				         /*dir*/NULL, /*filename*/NULL, /*lineno*/ 0)){
		hc->data = (Addr)p;
		VG_(strcpy)(hc->struct_name, mstruct);
		hc->req_szB = req_szB;
		hc->enum_name = hb_enum;
    hc->refs=0;
		VG_(OSetGen_Insert)(malloc_list, hc);
	}
	else{
		gl_add_list(mstruct, dir, filename, lineno);
		VG_(strcpy)(hc->struct_name,mstruct);
		hc->data = (Addr)p;
		hc->req_szB = req_szB;
    hc->refs=0;
		hc->enum_name = 0;
		VG_(OSetGen_Insert)(malloc_list, hc);
	}	
	
	return hc;
}

static __inline__
void record_block(ThreadId tid, void* p, SizeT req_szB, HChar* _mname, Int alloctype)
{
	HChar filename[FILE_LEN], dir[FILE_LEN];
	mlist_type* ret_mal = NULL;
	
	HChar dfpath[256];
	HChar mstruct[128];
	UInt lineno = 0;
	Int malfd;
	Bool found_dirname;
	
  /* Get stack information, and search for the caller's ip.
	 * Try to get the linenum, dir, filename. */
	VG_(get_StackTrace)(tid, ips, STRACEBUF, NULL, NULL, 0);
  if(clo_prog_lang == 'C')	
    VG_(get_filename_linenum)(ips[1], filename, FILE_LEN, dir, FILE_LEN, &found_dirname, &lineno);
  else if(clo_prog_lang == 'F')
	  VG_(get_filename_linenum)(ips[2], filename, FILE_LEN, dir, FILE_LEN, &found_dirname, &lineno);
  else
    VG_(tool_panic)("Something is wrong, clo_prog_lang funky value!\n");

//  /* calling a redundant call or something */
//  if(VG_(strcmp)(filename, "vg_wrap_malloc.c") == 0){
//    return;
//  }

	if(snmcnt < 1){
    if(!found_dirname || clo_enable_parsing == False){
      VG_(strcpy)(mstruct, "_sysres_");  
    }
    else{
      if(filename != NULL){
        int i = 0;
        for(i=0; i<VG_(strlen)(filename); i++){
          if(filename[i] == '.'){
            filename[i] = '_';
          }
        }
        VG_(sprintf)(mstruct, "%s-%u", filename, lineno);
      }
      else{
        VG_(strcpy)(mstruct, "_sysres_");
      }
//      if(!gl_search_list(&ret_mal, /*mstruct*/NULL, /*hb_enum*/NULL, dir, filename, lineno)){
//        VG_(sprintf)(dfpath, "%s/%s", dir, filename);
//        malfd = VG_(fd_open)(dfpath, VKI_O_RDONLY, VKI_S_IRUSR|VKI_S_IWUSR);
//        
//        if(malfd != -1)
//          gl_parse_malloc_line(&malfd, mstruct, lineno, alloctype);
//        else
//          VG_(strcpy)(mstruct, "_sysres_");
//        
//        VG_(close)(malfd);
//      }
//      else{
//        VG_(strcpy)(mstruct, ret_mal->struct_name);
//      }
    }
	}
	else{
		VG_(strcpy)(mstruct, user_snmarray[snmitt]);
    VG_(free)(user_snmarray[snmitt]);
    user_snmarray[snmitt] = NULL;
    snmitt++;

    if(snmitt == snmcnt){
      snmcnt = 0;
      snmitt = 0;
    }
  }
  
  if(tHeap < ((Addr)p + (Addr)req_szB))
		tHeap = (Addr)p + (Addr)req_szB;
  if(bHeap > (Addr)p)
		bHeap = (Addr)p;
	
	HB_Chunk* hc = VG_(OSetGen_Lookup)(malloc_list, &p);
	if(hc == NULL){
		hc = _record_block(tid, p, req_szB, mstruct, dir, filename, lineno);
    mycounter.memprint += req_szB;
	}
	else{
		/* Update it, this may be the first instance
		 * This is some kind of bug, ignore it*/
		hc->data = (Addr)p;
		VG_(strcpy)(hc->struct_name, mstruct);
		hc->req_szB = req_szB;
	}
  
	VG_(sprintf)(line_buffer, "X %d %s %09lx %lu %s %d\n", tid, _mname, (HWord) p, req_szB, mstruct, hc->enum_name);
	VG_(write)(fd_trace, (void *)line_buffer, VG_(strlen)(line_buffer));
  mycounter.total_lines++;
  
	return;
}

static __inline__
void unrecord_block(ThreadId tid, void* p)
{
  
  /* remove HB_Chunk from malloc_list */
	HB_Chunk* hc = VG_(OSetGen_Lookup)(malloc_list, &p);
  //tl_assert(hc != NULL);

  if(hc == NULL)
    return;
  
  mycounter.memprint -= hc->req_szB;

  VG_(sprintf)(line_buffer, "X %d FREE %09lx %lu %s %u\n", tid, (HWord) p, hc->req_szB, hc->struct_name, hc->enum_name);
	VG_(write)(fd_trace, (void *)line_buffer, VG_(strlen)(line_buffer));
	 
  /*for dyn_stat printing*/ 
  _flush_obj(hc);

  HB_Chunk* tmp_hc = VG_(OSetGen_Remove)(malloc_list, hc);
  tl_assert(tmp_hc != NULL);
  /* actually free the chunk, and the heap block (if necessary) */
  VG_(OSetGen_FreeNode)(malloc_list, hc);
}

static __inline__
void realloc_block(ThreadId tid, void* p_new, void* p_old, SizeT new_req_szB)
{
	HChar filename[FILE_LEN], dir[FILE_LEN];
	mlist_type* ret_mal;

	HChar dfpath[256];
	HChar mstruct[128];
	UInt lineno = 0;
	Int malfd;
	Bool found_dirname;

  HB_Chunk* hc;

	/* Get stack information, and search caller's ip*/
	VG_(get_StackTrace)(tid, ips, STRACEBUF, NULL, NULL, 0);
  if(clo_prog_lang == 'C')	
    VG_(get_filename_linenum)(ips[1], filename, FILE_LEN, dir, FILE_LEN, &found_dirname, &lineno);
  else if(clo_prog_lang == 'F')
	  VG_(get_filename_linenum)(ips[2], filename, FILE_LEN, dir, FILE_LEN, &found_dirname, &lineno);
  else
    VG_(tool_panic)("Something is wrong, clo_prog_lang funky value!\n");

  if(VG_(strcmp)(filename, "vg_wrap_malloc.c") == 0){
    return;
  }

	if(snmcnt < 1){
		if(!found_dirname || clo_enable_parsing == False){
			VG_(strcpy)(mstruct, "_sysres_");
		}
		else{
			if(!gl_search_list(&ret_mal, /*mstruct*/ NULL, /*hb_enum*/NULL, dir, filename, lineno)){
				VG_(sprintf)(dfpath, "%s/%s", dir, filename);
				malfd = VG_(fd_open)(dfpath, VKI_O_RDONLY, VKI_S_IRUSR);
				gl_parse_malloc_line(&malfd, mstruct, lineno, 2); 
				VG_(close)(malfd);
			}
			else{
				VG_(strcpy)(mstruct, ret_mal->struct_name);
			}
		}
	}
	else{
		VG_(strcpy)(mstruct, user_snmarray[snmitt]);
    VG_(free)(user_snmarray[snmitt]);
    user_snmarray[snmitt] = NULL;
    snmitt++;

    if(snmitt == snmcnt){
      snmcnt = 0;
      snmitt = 0;
    }
    
    VG_(umsg)("mstruct: %s\n", mstruct);
  }
	
	/* deal with the block */
  hc = VG_(OSetGen_Lookup)(malloc_list, &p_old);
  //tl_assert(hc != NULL);
  if(hc != NULL){
    /* remove the old block */
    hc = VG_(OSetGen_Remove)(malloc_list, hc);
    tl_assert(hc != NULL);
   
    mycounter.memprint -= hc->req_szB;
  }

	hc = VG_(OSetGen_Lookup)(malloc_list, &p_new);
	if(hc == NULL){
    /* Redundant if p_new == p_old, oh well */
		hc = _record_block(tid, p_new, new_req_szB, mstruct, dir, filename, lineno);
    mycounter.memprint += new_req_szB;
	}
	else{
		/* Update it, this may be the first instance, due to call malloc_init().
		 * Not sure if bug or normal, ignore it */
		VG_(strcpy)(hc->struct_name,mstruct);
    hc->data = (Addr)p_new;
    hc->req_szB = new_req_szB;
    hc->enum_name = 0;
    hc->refs=0;
  }
  	
  /* Check heap segment size */
	if(p_new){
		if(tHeap < ((Addr)p_new + (Addr)new_req_szB))
			tHeap = (Addr)p_new + (Addr)new_req_szB;
    if(bHeap > (Addr)p_new)
			bHeap = (Addr)p_new;
	}
	
	VG_(sprintf)(line_buffer, "X %d REALLOC %09lx %lu %s %d\n", tid, (HWord) p_new, new_req_szB, mstruct, hc->enum_name);
	VG_(write)(fd_trace, (void *)line_buffer, VG_(strlen)(line_buffer));
  mycounter.total_lines++;

	return;
}

/*-------------------------*/
/*--- Malloc Wrappers   ---*/
/*-------------------------*/
static void gl_malloc (ThreadId tid, void* p, SizeT szB)
{
  mycounter.malloc++;
  
  HChar _mname[6] = "MALLOC";

  if(!p)
		return;
  
	record_block(tid, p, szB, _mname, 0);
}

static void gl_calloc (ThreadId tid, void* p, SizeT m, SizeT szB)
{
  mycounter.calloc++;
  
  HChar _mname[6] = "CALLOC";
  
  if(!p)
    return;

	record_block(tid, p, m*szB, _mname, 1);
}

static void gl_realloc (ThreadId tid, void* p_new, void* p_old, SizeT new_szB)
{
  mycounter.realloc++;
	
  if(!p_new)
		return;

	realloc_block(tid, p_new, p_old, new_szB);
}
static void gl_free (ThreadId tid __attribute__((unused)), void* p)
{

	if(!p)
		return;

  mycounter.free++;

  unrecord_block(tid, p);
}

void register_malloc_wrappers(void)
{
	VG_(needs_malloc_wrap) (
											gl_malloc,
											gl_calloc,
                      gl_free,
											gl_realloc);
}

#endif

/* ---- */
