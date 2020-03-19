#include "gleipnir.h"

void gl_global_start_(void);
void gl_global_stop_(void);
void gl_start_(void);
void gl_stop_(void);

void gl_mark_a_(void);
void gl_mark_b_(void);
void gl_mark_c_(void);
void gl_mark_d_(void);
void gl_mark_e_(void);
void gl_reset_marks_(void);
void gl_mark_str_(char*, int*);
void gl_umsg_(char *, int *);

void gl_set_malloc_name_(char *, int *);
void gl_update_malloc_(int *, char *, int *);

void gl_set_mmap_name_(char *, int *);
void gl_update_mmap_(int* , char *, int *);

void gl_record_global_(int* , int *, char *, int *);
void gl_unrecord_global_(int *);

void gl_record_custom_mem_(int *, int *, char *, int *);
void gl_unrecord_custom_mem_(int *);

void gl_rename_trace_(char*, int *);

extern void gl_global_start_(void)
{
  GL_GSTART;
}

extern void gl_global_stop_(void)
{
  GL_GSTOP;
}

extern void gl_start_(void)
{
  GL_START;
}

extern void gl_stop_(void)
{
  GL_STOP;
}

extern void gl_mark_a_(void)
{
  GL_STOP;
  GL_MARK_A;
  GL_START;
}

extern void gl_mark_b_(void)
{
  GL_STOP;
  GL_MARK_B;
  GL_START;
}

extern void gl_mark_c_(void)
{
  GL_STOP;
  GL_MARK_C;
  GL_START;
}

extern void gl_mark_d_(void)
{
  GL_STOP;
  GL_MARK_D;
  GL_START;
}

extern void gl_mark_e_(void)
{
  GL_STOP;
  GL_MARK_E;
  GL_START;
}

extern void gl_reset_marks_(void)
{
  GL_STOP;
  GL_RESET_MARKS;
  GL_START;
}

extern void gl_mark_str_(char* string, int* len)
{
  GL_STOP;
  char buffer[128];
  int i = 0, size = 0;

  buffer[i] = '\0';
  size = *len;
  
  for(i=0; i<size; i++){
    buffer[i] = string[i];
  }
  buffer[i] = '\0';
  
  GL_MARK_STR(buffer);

  GL_START;
}

extern void gl_umsg_(char* string, int* len)
{
  GL_STOP;
  char buffer[128];
  int i = 0, size = 0;

  buffer[i] = '\0';
  size = *len;
  
  for(i=0; i<size; i++){
    buffer[i] = string[i];
  }
  buffer[i] = '\0';
  
  GL_UMSG(buffer);

  GL_START;
}

extern void gl_set_malloc_name_(char* description, int* namelen)
{
  GL_STOP;
  
  char buffer[128];
  int i = 0, size = 0;

  size = *namelen;
  
  for(i=0; i<size; i++){
    buffer[i] = description[i];
  }
  buffer[i] = '\0';
  
  GL_SET_MALLOC_NAME(buffer);

  GL_START;
}

extern void gl_update_malloc_(int* addr, char* description, int* namelen)
{
  GL_STOP;

  char buffer[128];
  int i = 0, size = 0;

  size = *namelen;

  for(i=0; i<size; i++){
    buffer[i] = description[i];
  }
  buffer[i] = '\0';

  GL_UPDATE_MALLOC(*addr, buffer);

  GL_START;
}


extern void gl_set_mmap_name_(char* description, int* namelen)
{
  GL_STOP;
  
  char buffer[128];
  int i = 0, size = 0;

  size = *namelen;
  
  for(i=0; i<size; i++){
    buffer[i] = description[i];
  }
  buffer[i] = '\0';
  
  GL_SET_MMAP_NAME(buffer);

  GL_START;
}

extern void gl_update_mmap_(int* addr, char* description, int* namelen)
{
  GL_STOP;

  char buffer[128];
  int i = 0, size = 0;

  size = *namelen;

  for(i=0; i<size; i++){
    buffer[i] = description[i];
  }
  buffer[i] = '\0';

  GL_UPDATE_MMAP(*addr, buffer);

  GL_START;
}

extern void gl_record_global_(int* addr, int* size, char* description, int* namelen)
{
  GL_STOP;

  char buffer[128];
  int i = 0;

  for(i=0; i<*namelen; i++){
    buffer[i] = description[i];
  }
  buffer[i] = '\0';

  GL_RECORD_GLOBAL(*addr, *size, buffer);

  GL_START;
}

extern void gl_unrecord_global_(int* addr)
{
  GL_STOP;

  GL_UNRECORD_GLOBAL(*addr);

  GL_START;
}

extern void gl_record_custom_mem_(int* addr, int* size, char* description, int* namelen)
{
  GL_STOP;

  char buffer[128];
  int i = 0;

  for(i=0; i<*namelen; i++){
    buffer[i] = description[i];
  }
  buffer[i] = '\0';

  GL_RECORD_GLOBAL(*addr, *size, buffer);

  GL_START;
}

extern void gl_unrecord_custom_mem_(int* addr)
{
  GL_STOP;

  GL_UNRECORD_GLOBAL(*addr);

  GL_START;
}


extern void gl_rename_trace_(char* string, int* len)
{
  GL_STOP;
  char buffer[128];
  int i = 0, size = 0;

  buffer[i] = '\0';
  size = *len;
  
  for(i=0; i<size; i++){
    buffer[i] = string[i];
  }
  buffer[i] = '\0';
  
  GL_RENAME_TRACE(buffer);

  GL_START;
}


