/*
	 This file is part of Gleipnir, a memory tracing tool.

	 Copyright (C) ?-2013 Computer Systems Research Laboratory, 
	 University of North Texas, Denton TX
   contact: tjanjusic@unt.edu

 */


/* Basic definitions needed by (almost) all Gleipnir source files. */


#ifndef __GL_BASICS_H
#define __GL_BASICS_H

#include "pub_tool_basics.h"      // Addr

#define FILE_LEN 1024
#define MAX_USER_NAMES 50

#define GL_LIKELY(x) \
        __builtin_expect(x, 1)

#define GL_UNLIKELY(x) \
        __builtin_expect(x, 0)

#define PRINT(str) \
        VG_(printf)(str)

#define PRINT2(str, arg) \
        VG_(printf)(str, arg)

#define UMSG(str) \
        VG_(umsg)(str)

#define UMSG2(str, arg) \
        VG_(umsg)(str, arg);

#endif /* __GL_BASICS_H */
