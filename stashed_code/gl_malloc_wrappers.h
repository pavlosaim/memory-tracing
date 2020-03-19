/* Part of Gleipnir, see copyright files for copyright instructions */
/* Computer Systems Research Laboratory, University of North Texas */

#ifndef __MALLOC_WRAPPERS_H
#define __MALLOC_WRAPPERS_H

#include "gl_basics.h"

/* malloc_list node */
typedef struct _HB_Chunk {
	Addr data;				/* start address */
	HChar struct_name[128];
	SizeT req_szB;			/* size requested */
	SizeT slop_szB;			/* slop size */
  ULong trefs;
  ULong refs;
	ULong enum_name;
} HB_Chunk;

/* mstruct_list node */
typedef struct _mstructNode {
  HChar struct_name[128];
  ULong instance;
} mstructNode;

/* Unused, but may be required later */
void init_alloc_fns(void);
void init_ignore_fns(void);

Word addrCmp(const void* key, const void* elem);
Word fnameCmp(const void* key, const void* elem);

extern Int fd_trace;
extern HChar line_buffer[FILE_LEN];
extern HChar* user_snmarray[MAX_USER_ML_STRUCTS];
extern UInt   snmitt,
              snmcnt;

void GL_(register_malloc_replacements)(void);

#endif
