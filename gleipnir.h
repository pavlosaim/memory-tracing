/*
	 Add license stuff here, this is open source, code copied/modified/added from
	 lackey and callgrind.

Authors: Tomislav Janjusic [tjanjusic@unt.edu]
Inst:	 Computer Systems Research Laboratory, University of North Texas
Denton, Texas
 */

#ifndef __GLEIPNIR_H
#define __GLEIPNIR_H

/* Set proper path */
#include "../include/valgrind.h"

/* stuff from callgrind, not sure, ordering matters here */

typedef
enum {
  VG_USERREQ__GSTART,
  VG_USERREQ__GSTOP,
  VG_USERREQ__START,
  VG_USERREQ__STOP,
  VG_USERREQ__MARK_A,
  VG_USERREQ__MARK_B,
  VG_USERREQ__MARK_C,
  VG_USERREQ__MARK_D,
  VG_USERREQ__MARK_E,
  VG_USERREQ__RESET_MARKS,
  VG_USERREQ__MARK_STR,
  VG_USERREQ__UMSG,

  VG_USERREQ__SET_MALLOC_NAME,
  VG_USERREQ__UPDATE_MALLOC,

  VG_USERREQ__SET_MMAP_NAME,
  VG_USERREQ__UPDATE_MMAP,

  VG_USERREQ__RECORD_GLOBAL,
  VG_USERREQ__UNRECORD_GLOBAL,

  VG_USERREQ__RECORD_CUSTOM_MEM,
  VG_USERREQ__UNRECORD_CUSTOM_MEM,
  
  VG_USERREQ__GROUP,

  VG_USERREQ__RENAME_TRACE
} Vg_GleipnirClientRequest;

/*start/stop instr macros */
#define GL_GSTART                                        \
	VALGRIND_DO_CLIENT_REQUEST_EXPR(0, VG_USERREQ__GSTART, \
			0, 0, 0, 0, 0)

#define GL_GSTOP                                         \
	VALGRIND_DO_CLIENT_REQUEST_EXPR(0, VG_USERREQ__GSTOP,  \
			0, 0, 0, 0, 0)

#define GL_START      																   \
	VALGRIND_DO_CLIENT_REQUEST_EXPR(0 , VG_USERREQ__START, \
			0, 0, 0, 0, 0)

#define GL_STOP      																	  \
	VALGRIND_DO_CLIENT_REQUEST_EXPR(0 , VG_USERREQ__STOP, \
			0, 0, 0, 0, 0)

/* code section markers */
#define GL_MARK_A                                        \
	VALGRIND_DO_CLIENT_REQUEST_EXPR(0, VG_USERREQ__MARK_A, \
			0, 0, 0, 0, 0)

#define GL_MARK_B                                        \
	VALGRIND_DO_CLIENT_REQUEST_EXPR(0, VG_USERREQ__MARK_B, \
			0, 0, 0, 0, 0)

#define GL_MARK_C                                        \
	VALGRIND_DO_CLIENT_REQUEST_EXPR(0, VG_USERREQ__MARK_C, \
			0, 0, 0, 0, 0)

#define GL_MARK_D                                        \
	VALGRIND_DO_CLIENT_REQUEST_EXPR(0, VG_USERREQ__MARK_D, \
			0, 0, 0, 0, 0)

#define GL_MARK_E                                        \
	VALGRIND_DO_CLIENT_REQUEST_EXPR(0, VG_USERREQ__MARK_E, \
			0, 0, 0, 0, 0)

#define GL_RESET_MARKS                                        \
  VALGRIND_DO_CLIENT_REQUEST_EXPR(0, VG_USERREQ__RESET_MARKS, \
      0, 0, 0, 0, 0)

#define GL_MARK_STR(_qzz_string)                           \
	VALGRIND_DO_CLIENT_REQUEST_EXPR(0, VG_USERREQ__MARK_STR, \
			_qzz_string, 0, 0, 0, 0)

/* same but will print PID as a user message from tool */
#define GL_UMSG(_qzz_string)                           \
	VALGRIND_DO_CLIENT_REQUEST_EXPR(0, VG_USERREQ__UMSG, \
			_qzz_string, 0, 0, 0, 0)

/* record malloced region */
#define GL_SET_MALLOC_NAME(_qzz_desc)                             \
	VALGRIND_DO_CLIENT_REQUEST_EXPR(0, VG_USERREQ__SET_MALLOC_NAME, \
			(_qzz_desc), 0, 0, 0, 0)

#define GL_UPDATE_MALLOC(_qzz_addr, _qzz_desc)                  \
	VALGRIND_DO_CLIENT_REQUEST_EXPR(0, VG_USERREQ__UPDATE_MALLOC, \
			(_qzz_addr), (_qzz_desc), 0, 0, 0)

/* record mmaped memory region */
#define GL_SET_MMAP_NAME(_qzz_desc)                             \
	VALGRIND_DO_CLIENT_REQUEST_EXPR(0, VG_USERREQ__SET_MMAP_NAME, \
			(_qzz_desc), 0, 0, 0, 0)

#define GL_UPDATE_MMAP(_qzz_addr, _qzz_desc)                  \
	VALGRIND_DO_CLIENT_REQUEST_EXPR(0, VG_USERREQ__UPDATE_MMAP, \
			(_qzz_addr), (_qzz_desc), 0, 0, 0)

/* record global regions */
#define GL_RECORD_GLOBAL(_qzz_addr, _qzz_size, _qzz_desc)       \
	VALGRIND_DO_CLIENT_REQUEST_EXPR(0, VG_USERREQ__RECORD_GLOBAL, \
			(_qzz_addr), (_qzz_size), (_qzz_desc), 0, 0)

#define GL_UNRECORD_GLOBAL(_qzz_addr)                             \
	VALGRIND_DO_CLIENT_REQUEST_EXPR(0, VG_USERREQ__UNRECORD_GLOBAL, \
			(_qzz_addr), 0, 0, 0, 0)

/* record custom memory region */
#define GL_RECORD_CUSTOM_MEM(_qzz_addr, _qzz_size, _qzz_desc)       \
	VALGRIND_DO_CLIENT_REQUEST_EXPR(0, VG_USERREQ__RECORD_CUSTOM_MEM, \
			(_qzz_addr), (_qzz_size), (_qzz_desc), 0, 0)

#define GL_UNRECORD_CUSTOM_MEM(_qzz_addr)                             \
	VALGRIND_DO_CLIENT_REQUEST_EXPR(0, VG_USERREQ__UNRECORD_CUSTOM_MEM, \
			(_qzz_addr), 0, 0, 0, 0)

/* set grouping */
#define GL_GROUP(_qzz_addr, _qzz_desc) \
        VALGRIND_DO_CLIENT_REQUEST_EXPR(0, VG_USERREQ__RECORD_CUSTOM_MEM, \
                        (_qzz_addr), (_qzz_desc), 0, 0, 0)

/* rename traces */
#define GL_RENAME_TRACE(_qzz_string)                           \
	VALGRIND_DO_CLIENT_REQUEST_EXPR(0, VG_USERREQ__RENAME_TRACE, \
			_qzz_string, 0, 0, 0, 0)

#endif /* __GLEIPNIR_H */
