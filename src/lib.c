#define _XOPEN_SOURCE 700
#include <assert.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "strptime.h"
#include "lib.h"
#include "sdm_lib.h"

#define MAX_QUERYSTR_LENGTH 256

int get_ids_and_tables(PGconn *conn, const char *search_string, ArchiverAttr **attrs) {
  char query_str[MAX_QUERYSTR_LENGTH];
  memset(query_str, 0, MAX_QUERYSTR_LENGTH);
  snprintf(query_str, MAX_QUERYSTR_LENGTH, 
           "SELECT att_conf_id, att_name, table_name FROM att_conf "
           "WHERE att_name ~ '%s' ORDER BY att_conf_id;", search_string);

  PGresult *res = PQexec(conn, query_str);
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
    strncpy((*attrs)[hit].id,    PQgetvalue(res, hit, 0), sizeof(attrs[hit]->id)/sizeof(char) - 1);
    strncpy((*attrs)[hit].name,  PQgetvalue(res, hit, 1), sizeof(attrs[hit]->name)/sizeof(char) - 1);
    strncpy((*attrs)[hit].table, PQgetvalue(res, hit, 2), sizeof(attrs[hit]->table)/sizeof(char) - 1);
  }

  PQclear(res);

  return num_hits;
}

static bool attr_is_scalar(ArchiverAttr attr) {
  char *check_str = "att_scalar";
  return strncmp(attr.table, check_str, strlen(check_str)) == 0;
}

int get_single_attr_data(
                  PGconn *conn, 
                  ArchiverAttr attr,
                  DataSet *dataset,
                  struct tm start, struct tm stop) {
  char query_str[512];
  char start_str[32];
  char stop_str[32];
  memset(query_str, 0, sizeof(query_str)/sizeof(char));
  memset(start_str, 0, sizeof(start_str)/sizeof(char));
  memset(stop_str, 0, sizeof(stop_str)/sizeof(char));

  if (start.tm_isdst == 1) {
    start.tm_hour -= 1;
    mktime(&start);
  }
  if (stop.tm_isdst == 1) {
    stop.tm_hour -= 1;
    mktime(&stop);
  }

  strftime(start_str, sizeof(start_str)/sizeof(char), "%Y-%m-%d %H:%M:%S %z", &start);
  strftime(stop_str, sizeof(stop_str)/sizeof(char), "%Y-%m-%d %H:%M:%S %z", &stop);

  snprintf(query_str, sizeof(query_str)/sizeof(char), 
          "SELECT * FROM %s WHERE att_conf_id = %s AND "
          "data_time BETWEEN '%s' AND '%s' " "ORDER BY data_time",
          attr.table, attr.id, start_str, stop_str);
  PGresult *res = PQexec(conn, query_str);
  if (PQresultStatus(res) != PGRES_TUPLES_OK) {
    fprintf(stderr, "%s", PQerrorMessage(conn));
    PQclear(res);
    return -1;
  }

  size_t num_data_pts = (size_t)PQntuples(res);
  if (num_data_pts > MAX_ARRAY_LENGTH) {
    fprintf(stderr, "DB returned %zu points, which exceeds the maximum of %d\n",
            num_data_pts, MAX_ARRAY_LENGTH);
    fprintf(stderr, "To change this limit, edit the MAX_ARRAY_LENGTH value in the ");
    fprintf(stderr, "source file and recompile.\n");
    exit(1);
  }

  if (attr_is_scalar(attr)) {
    dataset->type = DATATYPE_SCALAR;
    SDM_ENSURE_ARRAY_MIN_CAP((dataset->as.scalar_array), num_data_pts);
  } else {
    dataset->type = DATATYPE_VECTOR;
    SDM_ENSURE_ARRAY_MIN_CAP((dataset->as.vector_array), num_data_pts);
  }
  SDM_ENSURE_ARRAY_MIN_CAP((dataset->time_array), num_data_pts);

  for (size_t i=0; i<num_data_pts; i++) {
    struct tm time_struct = {0};
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

    // mktime(&time_struct);
    time_struct.tm_hour += 1;
    mktime(&time_struct);

    SDM_ARRAY_PUSH(dataset->time_array, ((AccurateTime){.time_struct=time_struct, .micros=micros}));

    char *db_val_str = PQgetvalue(res, i, 2);
    if (dataset->type == DATATYPE_SCALAR) {
      if (strcmp(attr.table, "att_scalar_devboolean")==0) {
        if (strcmp(db_val_str, "t")==0) {
          SDM_ARRAY_PUSH(dataset->as.scalar_array, 1.0);
        } else if (strcmp(db_val_str, "f")==0) {
          SDM_ARRAY_PUSH(dataset->as.scalar_array, 0.0);
        } else {
          assert(0 && "unreachable code was reached");
        }
      } else {
        SDM_ARRAY_PUSH(dataset->as.scalar_array, atof(db_val_str));
      }
    } else if (dataset->type == DATATYPE_VECTOR) {
      SDM_ARRAY_PUSH((dataset->as.vector_array), (DynScalarArray){0});
      size_t item_number = dataset->as.vector_array.length - 1;
      if (*db_val_str == '{') db_val_str++;
      while (*db_val_str != '}') {
        double val = strtod(db_val_str, &db_val_str);
        SDM_ARRAY_PUSH((dataset->as.vector_array.data[item_number]), val);
        while (*db_val_str == ',') {
          db_val_str++;
        }
      }
      fprintf(stderr, "%s\n", db_val_str);
      // fprintf(stderr, "\nWARNING: Vector acquisition is not implemented.\n\n");
      // break;
    }
  }

  PQclear(res);

  return num_data_pts;
}

