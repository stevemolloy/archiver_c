#ifndef _LIB_H
#define _LIB_H

#define _XOPEN_SOURCE 700

#include<time.h>

#include "libpq-fe.h"

#define MAX_ARRAY_LENGTH (1024*1024*1024 / 8)

typedef struct {
  char id[32];
  char name[256];
  char table[64];
} ArchiverAttr;

typedef struct {
  struct tm time_struct;
  int micros;
} AccurateTime;

typedef struct {
  double *data;
  size_t length;
  size_t capacity;
} DynScalarArray;

typedef struct {
  DynScalarArray *data;
  size_t length;
  size_t capacity;
} DynVectorArray;

typedef struct {
  AccurateTime *data;
  size_t length;
  size_t capacity;
} DynTimeArray;

typedef enum {
  DATATYPE_SCALAR,
  DATATYPE_VECTOR,
} DataType;

typedef struct {
  DataType type;
  DynTimeArray time_array;
  union {
    DynScalarArray scalar_array;
    DynVectorArray vector_array;
  } as;
} DataSet;

typedef struct {
  DataSet *data;
  size_t length;
  size_t capacity;
} DynDataSetArray;

int get_ids_and_tables(PGconn *conn, const char *search_string, ArchiverAttr **attrs);
int get_single_attr_data(PGconn *conn, ArchiverAttr attr, DataSet *dataset, struct tm start, struct tm stop);

#endif // !_LIB_H

