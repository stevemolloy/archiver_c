#define _XOPEN_SOURCE 700

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "libpq-fe.h"
#include "sdm_lib.h"
#include "lib.h"

void usage(char *program_name) {
  fprintf(stderr, "%s --start start_time --end end_time signal\n", program_name);
  return;
}

int main(int argc, char **argv) {
  char *program_name = SDM_shift_args(&argc, &argv);

  if (argc < 5) {
    fprintf(stderr, "[ERROR] Not enough arguments\n\n");
    fprintf(stderr, "Usage:\n");
    usage(program_name);
    fprintf(stderr, "\n");
    return 1;
  }

  const char *search_str, *start_str, *stop_str, *file_name;
  bool save_to_file = false;

  while (argc > 0) {
    char *arg_str = SDM_shift_args(&argc, &argv);

    if (strcmp(arg_str, "--start") == 0) {
      start_str = SDM_shift_args(&argc, &argv);
    } else if (strcmp(arg_str, "--end") == 0) {
      stop_str  = SDM_shift_args(&argc, &argv);
    } else if (strcmp(arg_str, "--file") == 0) {
      save_to_file = true;
      file_name = SDM_shift_args(&argc, &argv);
    } else {
      search_str = arg_str;
    }
  }

  struct tm start_tm = {0}, stop_tm = {0};
  strptime(start_str, "%Y-%m-%dT%H:%M:%S", &start_tm);
  mktime(&start_tm);
  strptime(stop_str, "%Y-%m-%dT%H:%M:%S", &stop_tm);
  mktime(&stop_tm);

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

  size_t conn_str_size = strlen(db_type) + strlen(db_pass) + strlen(db_url) + 
                         strlen(db_port) + strlen(db_name) + 5;

  conn_str = malloc(conn_str_size * sizeof(char*));
  memset(conn_str, 0, conn_str_size);

  snprintf(conn_str, conn_str_size, "%s:%s@%s:%s/%s", 
           db_type, db_pass, db_url, db_port, db_name);

  conn = PQconnectdb(conn_str);
  if (PQstatus(conn) != CONNECTION_OK) {
    fprintf(stderr, "%s", PQerrorMessage(conn));
    defered_return(1);
  }

  attrs = NULL;
  int num_matching_attrs = get_ids_and_tables(conn, search_str, &attrs);
  if (num_matching_attrs <= 0) {
    fprintf(stderr, "Search string not found in DB\n");
    defered_return(1);
  }

  PQclear(res);

  DynDataSetArray data_set_array = {0};
  SDM_ENSURE_ARRAY_MIN_CAP(data_set_array, (size_t)num_matching_attrs);
  for (size_t attr_num=0; attr_num<(size_t)num_matching_attrs; attr_num++) {
    get_single_attr_data(conn, attrs[attr_num], &data_set_array.data[attr_num], start_tm, stop_tm);
  }

  for (size_t attr_num=0; attr_num<(size_t)num_matching_attrs; attr_num++) {
    FILE *stream = NULL;

    if (save_to_file) {
      stream = stdout;
    } else {
      stream = stdout;
    }

    DataSet ds = data_set_array.data[attr_num];
    fprintf(stream, "\"# DATASET= %s\"\n", attrs[attr_num].name);
    fprintf(stream, "\"# SNAPSHOT_TIME= \"\n");
    for (size_t data_pt=0; data_pt < ds.data_array.length; data_pt++) {
      DynTimeArray t = ds.time_array;
      DynDoubleArray d = ds.data_array;
      char time_str[30];
      memset(time_str, 0, 30);
      strftime(time_str, 30, "%Y-%m-%d_%H:%M:%S.", &t.data[data_pt].time_struct);
      sprintf(&(time_str[20]), "%d", t.data[data_pt].micros);
      fprintf(stream, "%s, %0.11f\n", time_str, d.data[data_pt]);
    }
    fprintf(stream, "\n");
  }

  SDM_ARRAY_FREE(data_set_array.data[0].data_array);
  SDM_ARRAY_FREE(data_set_array.data[0].time_array);
  SDM_ARRAY_FREE(data_set_array);

defer:
  if (conn_str) FREE(conn_str);
  if (conn)     PQfinish(conn);
  if (attrs)    FREE(attrs);
  return result;
}

