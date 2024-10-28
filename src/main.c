#define _XOPEN_SOURCE 700

#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "libpq-fe.h"
#include "sdm_lib.h"

#define defered_return(val) \
  do { result = (val); goto defer; } while (0)

typedef struct {
  char id[32];
  char name[256];
  char table[64];
} ArchiverAttr;

typedef struct {
  double *data;
  size_t length;
  size_t capacity;
} DynDoubleArray;

typedef struct {
  struct tm time_struct;
  int micros;
} AccurateTime;

typedef struct {
  AccurateTime *data;
  size_t length;
  size_t capacity;
} DynTimeArray;

int get_ids_and_tables(PGconn *conn, char *search_string, ArchiverAttr **attrs) {
  (void) search_string;
  PGresult *res = PQexec(conn, 
               "SELECT att_conf_id, att_name, table_name "
               "FROM att_conf "
               "WHERE att_name ~ '.*r3.*dcct.*inst.*' "
               "ORDER BY att_conf_id;");
  if (PQresultStatus(res) != PGRES_TUPLES_OK) {
    fprintf(stderr, "%s", PQerrorMessage(conn));
    PQclear(res);
    return -1;
  }

  if (PQnfields(res) != 3) {
    fprintf(stderr, "The wrong number of fields came back from the DB");
    return -1;
  }

  int num_hits = PQntuples(res);

  *attrs = malloc(num_hits * sizeof(ArchiverAttr));
  memset(*attrs, 0, num_hits * sizeof(ArchiverAttr));

  for (int hit=0; hit<num_hits; hit++) {
    *attrs[hit] = (ArchiverAttr){0};
    strncpy(attrs[hit]->id,    PQgetvalue(res, hit, 0), sizeof(attrs[hit]->id)/sizeof(char));
    strncpy(attrs[hit]->name,  PQgetvalue(res, hit, 1), sizeof(attrs[hit]->name)/sizeof(char));
    strncpy(attrs[hit]->table, PQgetvalue(res, hit, 2), sizeof(attrs[hit]->table)/sizeof(char));
  }

  return num_hits;
}

int get_single_attr_data(PGconn *conn, DynDoubleArray *data_arr, DynTimeArray *time_arr) {
  char *query_str = 
    "SELECT * FROM att_scalar_devdouble "
    "WHERE att_conf_id = 15714 AND "
    "data_time BETWEEN '2024-10-27 12:00:00 +01:00' AND '2024-10-27 12:01:00 +01:00' "
    "ORDER BY data_time";
  PGresult *res = PQexec(conn, query_str);
  if (PQresultStatus(res) != PGRES_TUPLES_OK) {
    fprintf(stderr, "%s", PQerrorMessage(conn));
    PQclear(res);
    return -1;
  }

  int num_hits = PQntuples(res);
  printf("I got %d hits back\n", num_hits);
  int nFields = PQnfields(res);
  printf("Each hit has %d fields\n", nFields);

  size_t num_data_pts = (size_t)PQntuples(res);
  SDM_ENSURE_ARRAY_MIN_CAP(*data_arr, num_data_pts + 1);
  SDM_ENSURE_ARRAY_MIN_CAP(*time_arr, num_data_pts + 1);

  for (size_t i=0; i<num_data_pts; i++) {
    SDM_ARRAY_PUSH(*data_arr, atof(PQgetvalue(res, i, 2)));
    struct tm time_struct;
    char *time_str = PQgetvalue(res, i, 1);
    time_str = strptime(time_str, "%Y-%m-%d %H:%M:%S", &time_struct);
    int micros = 0;
    int factor = 100 * 1000;
    if (*time_str == '.') {
      time_str++;
      while (isdigit(*time_str)) {
        micros += (*time_str - '0') * factor;
        factor /= 10;
        time_str++;
      }
    }
    printf("");

    time_struct.tm_hour += 1;
    mktime(&time_struct);

    SDM_ARRAY_PUSH(*time_arr, ((AccurateTime){.time_struct=time_struct, .micros=micros}));
  }

  return num_hits;
}

int main(void) {
  int result = 0;
  char *conn_str = NULL;
  PGconn *conn = NULL;
  PGresult *res = NULL;
  ArchiverAttr *attrs = NULL;

  const char *pass_env_str = "ARCHIVER_PASS";

  const char *db_type = "postgresql://hdb_viewer";
  const char *db_pass = getenv(pass_env_str);
  const char *db_url  = "timescaledb.maxiv.lu.se";
  const char *db_port = "15432";
  const char *db_name = "hdb_machine";

  size_t conn_str_size = 
                  strlen(db_type) + 
                  strlen(db_pass) + 
                  strlen(db_url) + 
                  strlen(db_port) + 
                  strlen(db_name) + 
                  5;

  conn_str = malloc(conn_str_size * sizeof(char*));
  memset(conn_str, 0, conn_str_size);

  snprintf(conn_str, conn_str_size, "%s:%s@%s:%s/%s", 
           db_type, db_pass, db_url, db_port, db_name);
  printf("[INFO] Using conn_str = %s\n", conn_str);

  conn = PQconnectdb(conn_str);
  if (PQstatus(conn) != CONNECTION_OK) {
    fprintf(stderr, "%s", PQerrorMessage(conn));
    defered_return(1);
  }

  attrs = NULL;
  int num_hits = get_ids_and_tables(conn, ".*r3.*dcct.*inst.*", &attrs);

  for (int i = 0; i < num_hits; i++) {
    printf("| %s | %s | %s |", attrs[i].id, attrs[i].name, attrs[i].table);
    printf("\n");
  }

  PQclear(res);

  DynDoubleArray data_array = {0};
  DynTimeArray time_array = {0};
  get_single_attr_data(conn, &data_array, &time_array);

  assert(data_array.length == time_array.length && "Data and time vectors should be the same length");

  for (size_t i=0; i<data_array.length; i++) {
    char time_str[30];
    memset(time_str, 0, 30);
    strftime(time_str, 30, "%Y-%m-%d_%H:%M:%S", &time_array.data[i].time_struct);
    time_str[19] = '.';
    sprintf(&(time_str[20]), "%d", time_array.data[i].micros);
    printf("%s, %0.11f\n", time_str, data_array.data[i]);
  }

  SDM_ARRAY_FREE(data_array);
  SDM_ARRAY_FREE(time_array);

defer:
  if (conn_str) free(conn_str);
  if (conn)     PQfinish(conn);
  if (attrs)    free(attrs);
  return result;
}

