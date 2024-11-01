#ifndef _SDM_LIB_H
#define _SDM_LIB_H

#include <stdbool.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#define FREE(ptr) do { free(ptr); ptr = NULL; } while (0)

#define defered_return(val) \
  do { result = (val); goto defer; } while (0)

#define SDM_ENSURE_ARRAY_CAP(da, cap) do {                      \
    (da).capacity = cap;                                        \
    (da).data = realloc((da).data,                              \
        (da).capacity * sizeof((da).data[0]));                  \
    memset((da).data, 0, (da).capacity * sizeof((da).data[0])); \
    if ((da).data == NULL) {                                    \
      fprintf(stderr, "ERR: Couldn't alloc memory.\n");         \
      exit(1);                                                  \
    }                                                           \
  } while (0)

#define SDM_ENSURE_ARRAY_MIN_CAP(da, cap) do {                    \
    if ((da).capacity < cap) {                                    \
      (da).capacity = cap;                                        \
      (da).data = realloc((da).data,                              \
          (da).capacity * sizeof((da).data[0]));                  \
      memset((da).data, 0, (da).capacity * sizeof((da).data[0])); \
      if ((da).data == NULL) {                                    \
        fprintf(stderr, "ERR: Couldn't alloc memory. \n");        \
        exit(1);                                                  \
      }                                                           \
    }                                                             \
  } while (0)

#define DEFAULT_CAPACITY 128

#define SDM_ARRAY_PUSH(da, item) do {                             \
    if (((da).capacity == 0) || ((da).data == NULL)) {            \
      (da).capacity = DEFAULT_CAPACITY;                           \
      (da).data = malloc((da).capacity * sizeof((da).data[0]));   \
      memset((da).data, 0, (da).capacity * sizeof((da).data[0])); \
      if ((da).data == NULL) {                                    \
        fprintf(stderr, "ERR: Couldn't alloc memory.\n");         \
        exit(1);                                                  \
      }                                                           \
    }                                                             \
    while ((da).length >= (da).capacity) {                        \
      (da).capacity *= 2;                                         \
      (da).data = realloc((da).data,                              \
           (da).capacity * sizeof((da).data[0]));                 \
      memset((da).data + (da).length, 0, ((da).capacity - (da).length) * sizeof((da).data[0])); \
      if ((da).data == NULL) {                                    \
        fprintf(stderr, "ERR: Couldn't alloc memory.\n");         \
        exit(1);                                                  \
      }                                                           \
    }                                                             \
    (da).data[(da).length++] = item;                              \
  } while (0);

#define SDM_ARRAY_FREE(da) do {                                \
    FREE((da).data);                                           \
    (da).length = 0;                                           \
    (da).capacity = 0;                                         \
  } while (0);

#define SDM_ARRAY_RESET(da) do { (da).length = 0; } while (0)

typedef struct {
  size_t length;
  char *data;
} SDM_StringView;

#define SDM_SV_F "%.*s"
#define SDM_SV_Vals(S) (int)(S).length, (S).data

char *SDM_read_entire_file(const char *file_path);

SDM_StringView SDM_cstr_as_sv(char *cstr);
SDM_StringView SDM_sized_str_as_sv(char *cstr, size_t length);

SDM_StringView SDM_sv_pop_by_delim(SDM_StringView *SV, const char delim);
void SDM_sv_trim(SDM_StringView *SV);

char *SDM_shift_args(int *argc, char ***argv);

#endif /* ifndef _SDM_LIB_H */

