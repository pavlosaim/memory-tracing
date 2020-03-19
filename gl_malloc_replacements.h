/* Part of Gleipnir, see copyright files for copyright instructions */
/* Computer Systems Research Laboratory, University of North Texas */

#ifndef __MALLOC_WRAPPERS_H
#define __MALLOC_WRAPPERS_H

#include "gl_basics.h"

/* malloc_list node */
typedef struct _hblock {
	Addr addr;				/* start address */
	HChar name[128];
	SizeT req_szB;			/* size requested */
	SizeT slop_szB;			/* slop size */
  ULong trefs;
  ULong refs;
	ULong instance;
} hblock_t;

/* malloc_name_list node */
typedef struct _malloc_name_t {
  HChar name[128];
  ULong instance;
} malloc_name_t;

/* Unused, but may be required later */
void init_alloc_fns(void);
void init_ignore_fns(void);

Word addrCmp_heap(const void* key, const void* elem);
Word nameCmp(const void* key, const void* elem);

extern Int fd_trace;
extern HChar  line_buffer[FILE_LEN];
extern HChar *user_malloc_name_array[MAX_USER_NAMES];
extern UInt   user_malloc_name_cnt,
              user_malloc_name_itt;

void gl_register_malloc_replacements(void);

#endif
