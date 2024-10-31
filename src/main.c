#define _XOPEN_SOURCE 700

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "libpq-fe.h"
#include "sdm_lib.h"
#include "lib.h"

void usage(char *program_name) {
  fprintf(stderr, "%s --start/-s <start_time> --end/-e <end_time> [--file/-f <filename>] <signal>\n", program_name);
  return;
}

typedef struct {
  char *start_str;
  char *stop_str;
  char *search_str;
  bool save_to_file;
  char *filename_arg;
} InputArgs;

bool check_input_args(InputArgs inargs) {
  if (inargs.start_str==NULL) return false;
  if (inargs.stop_str==NULL) return false;
  if (inargs.search_str==NULL) return false;
  if (inargs.save_to_file && (inargs.filename_arg==NULL)) return false;
  return true;
}

int main(int argc, char **argv) {
  int result = 0;
  ArchiverAttr *attrs = NULL;
  PGconn *conn = NULL;
  char *conn_str = NULL;
  PGresult *res = NULL;

  char *program_name = SDM_shift_args(&argc, &argv);
  FILE *stream = NULL;
  char *filename = NULL;

  InputArgs input_args = {0};

  while (argc > 0) {
    char *arg_str = SDM_shift_args(&argc, &argv);

    if (strcmp(arg_str, "--help") == 0) {
      usage(program_name);
      defered_return(0);
    } else if ((strcmp(arg_str, "--start") == 0) || (strcmp(arg_str, "-s") == 0)) {
      input_args.start_str = SDM_shift_args(&argc, &argv);
    } else if ((strcmp(arg_str, "--end") == 0) || (strcmp(arg_str, "-e") == 0)) {
      input_args.stop_str  = SDM_shift_args(&argc, &argv);
    } else if ((strcmp(arg_str, "--file") == 0) || (strcmp(arg_str, "-f") == 0)) {
      input_args.save_to_file = true;
      input_args.filename_arg = SDM_shift_args(&argc, &argv);
    } else {
      input_args.search_str = arg_str;
    }
  }

  if (!check_input_args(input_args)) {
    fprintf(stderr, "Incorrect input arguments\n");
    usage(program_name);
    defered_return(1);
  }

  struct tm start_tm = {0}, stop_tm = {0};
  strptime(input_args.start_str, "%Y-%m-%dT%H:%M:%S", &start_tm);
  mktime(&start_tm);
  strptime(input_args.stop_str, "%Y-%m-%dT%H:%M:%S", &stop_tm);
  mktime(&stop_tm);

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
  int num_matching_attrs = get_ids_and_tables(conn, input_args.search_str, &attrs);
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
    if (input_args.save_to_file) {
      filename = malloc((strlen(input_args.filename_arg) + 32) * sizeof(char));
      if (filename == NULL) {
        fprintf(stderr, "Could not allocate memory.\n");
        defered_return(1);
      }
      sprintf(filename, "%s%04zu.csv", input_args.filename_arg, attr_num+1);
      stream = fopen(filename, "w");
      if (stream == NULL) {
        fprintf(stderr, "Could not open %s: %s\n", filename, strerror(errno));
        defered_return(1);
      }
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

    if (input_args.save_to_file) {
      fclose(stream);
      FREE(filename);
    }
  }

  SDM_ARRAY_FREE(data_set_array.data[0].data_array);
  SDM_ARRAY_FREE(data_set_array.data[0].time_array);
  SDM_ARRAY_FREE(data_set_array);

defer:
  if (conn_str) FREE(conn_str);
  if (conn)     PQfinish(conn);
  if (attrs)    FREE(attrs);
  if (filename) FREE(filename);
  return result;
}

