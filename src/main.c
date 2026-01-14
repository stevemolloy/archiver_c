
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "sdm_lib.h"
#include "lib.h"

void usage(FILE *sink, char *program_name) {
  fprintf(sink, "%s --start/-s <start_time> --end/-e <end_time> [--file/-f <filename>] <attribute>\n", program_name);
  fprintf(sink, "\t<start_time> and <end_time> should be given in the format: %%Y-%%m-%%dT%%H:%%M:%%S\n");
  return;
}

typedef struct {
  char *start_str;
  char *stop_str;
  char *search_str;
  bool save_to_file;
  char *filename_arg;
  bool verbose;
} InputArgs;

void print_input_args(const InputArgs* args) {
    printf("Searching for attribute: \"%s\"\n", args->search_str);
    printf("Start date/time: \"%s\"\n", args->start_str);
    printf("Stop date/time: \"%s\"\n", args->stop_str);
    if (args->save_to_file) {
        printf("Saving to file: \"%s\"\n", args->filename_arg);
    } else {
        printf("Not saving to a file\n");
    }
    if (args->verbose) {
        printf("Verbose output was selected\n");
    }
}

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
      usage(stdout, program_name);
      defered_return(0);
    } else if ((strcmp(arg_str, "--start") == 0) || (strcmp(arg_str, "-s") == 0)) {
      input_args.start_str = SDM_shift_args(&argc, &argv);
    } else if ((strcmp(arg_str, "--end") == 0) || (strcmp(arg_str, "-e") == 0)) {
      input_args.stop_str  = SDM_shift_args(&argc, &argv);
    } else if ((strcmp(arg_str, "--file") == 0) || (strcmp(arg_str, "-f") == 0)) {
      input_args.save_to_file = true;
      input_args.filename_arg = SDM_shift_args(&argc, &argv);
    } else if ((strcmp(arg_str, "--verbose") == 0)) {
      input_args.verbose = true;
    } else {
      input_args.search_str = arg_str;
    }
  }

  if (input_args.verbose) {
      printf("------------------INFO----------------------\n");
      print_input_args(&input_args);
      printf("------------------INFO----------------------\n");
  }

  if (!check_input_args(input_args)) {
    fprintf(stderr, "ERROR: Incorrect input arguments\n");
    usage(stderr, program_name);
    defered_return(1);
  }

  struct tm start_tm = {0}, stop_tm = {0};
  // The following is used instead of strptime since that does not exist on Windows
  int year, month, day, hour, minute, second;
  sscanf(input_args.start_str, "%d-%d-%dT%d:%d:%d",
         &year, &month, &day, &hour, &minute, &second);
  start_tm.tm_year = year - 1900;
  start_tm.tm_mon = month - 1;
  start_tm.tm_mday = day;
  start_tm.tm_hour = hour;
  start_tm.tm_min = minute;
  start_tm.tm_sec = second;
  // strptime(input_args.start_str, "%Y-%m-%dT%H:%M:%S", &start_tm);
  // mktime(&start_tm);
  sscanf(input_args.stop_str, "%d-%d-%dT%d:%d:%d",
         &year, &month, &day, &hour, &minute, &second);
  stop_tm.tm_year = year - 1900;
  stop_tm.tm_mon = month - 1;
  stop_tm.tm_mday = day;
  stop_tm.tm_hour = hour;
  stop_tm.tm_min = minute;
  stop_tm.tm_sec = second;
  // strptime(input_args.stop_str, "%Y-%m-%dT%H:%M:%S", &stop_tm);
  // mktime(&stop_tm);

  const char *pass_env_str = "ARCHIVER_PASS";
  const char *db_type = "postgresql://hdb_viewer";
  const char *db_pass = getenv(pass_env_str);
  if (!db_pass || strlen(db_pass)==0) {
      printf("ERROR: No password found in the %s environment variable. Please set this variable.\n", pass_env_str);
      defered_return(1);
  }
  const char *db_url  = "timescaledb.maxiv.lu.se";
  const char *db_port = "15432";
  const char *db_name = "hdb_machine";

  size_t conn_str_size = 0;
  conn_str_size += strlen(db_type);
  conn_str_size += strlen(db_pass);
  conn_str_size += strlen(db_url);
  conn_str_size += strlen(db_port);
  conn_str_size += strlen(db_name);
  conn_str_size += 5;

  conn_str = malloc(conn_str_size * sizeof(char));
  memset(conn_str, 0, conn_str_size);

  snprintf(conn_str, conn_str_size, "%s:%s@%s:%s/%s", 
           db_type, db_pass, db_url, db_port, db_name);

  if (input_args.verbose) {
      printf("INFO: Connection string: %s\n", conn_str);
  }

  conn = PQconnectdb(conn_str);
  if (PQstatus(conn) != CONNECTION_OK) {
    fprintf(stderr, "%s", PQerrorMessage(conn));
    defered_return(1);
  }

  attrs = NULL;
  int num_matching_attrs = get_ids_and_tables(conn, input_args.search_str, &attrs);
  if (num_matching_attrs <= 0) {
    fprintf(stderr, "ERROR: Search string not found in DB\n");
    defered_return(1);
  }

  if (input_args.verbose) {
      printf("INFO: Found %d attribute(s) matching \"%s\"\n", num_matching_attrs, input_args.search_str);
  }

  PQclear(res);

  DynDataSetArray data_set_array = {0};
  SDM_ENSURE_ARRAY_MIN_CAP(data_set_array, (size_t)num_matching_attrs);
  for (size_t attr_num=0; attr_num<(size_t)num_matching_attrs; attr_num++) {
    if (input_args.verbose) {
        printf("INFO: Querying the database for %s\n", attrs[attr_num].name);
    }
    get_single_attr_data(conn, attrs[attr_num], &data_set_array.data[attr_num], start_tm, stop_tm);
  }

  for (size_t attr_num=0; attr_num<(size_t)num_matching_attrs; attr_num++) {
    if (input_args.save_to_file) {
      filename = malloc((strlen(input_args.filename_arg) + 32) * sizeof(char));
      if (filename == NULL) {
        fprintf(stderr, "ERROR: Could not allocate memory.\n");
        defered_return(1);
      }
      sprintf(filename, "%s-%04zu.csv", input_args.filename_arg, attr_num+1);
      stream = fopen(filename, "w");
      if (stream == NULL) {
        fprintf(stderr, "ERROR: Could not open %s: %s\n", filename, strerror(errno));
        defered_return(1);
      }
    } else {
      stream = stdout;
    }

    DataSet ds = data_set_array.data[attr_num];
    fprintf(stream, "\"# DATASET= %s\"\n", attrs[attr_num].name);
    fprintf(stream, "\"# SNAPSHOT_TIME= \"\n");

    size_t total_datapoints = ds.type==DATATYPE_SCALAR ? 
      ds.as.scalar_array.length : ds.as.vector_array.length;
    for (size_t data_pt=0; data_pt < total_datapoints; data_pt++) {
      DynTimeArray t = ds.time_array;
      char time_str[128];
      memset(time_str, 0, sizeof(time_str)/sizeof(char));
      sprintf(time_str, "%02d-%02d-%02d_%02d:%02d:%02d.%06d",
              t.data[data_pt].time_struct.tm_year+1900,
              t.data[data_pt].time_struct.tm_mon+1,
              t.data[data_pt].time_struct.tm_mday,
              t.data[data_pt].time_struct.tm_hour,
              t.data[data_pt].time_struct.tm_min,
              t.data[data_pt].time_struct.tm_sec,
              t.data[data_pt].micros);

      // sprintf(&(time_str[20]), "%d", t.data[data_pt].micros);

      fprintf(stream, "%s, ", time_str);
      switch (ds.type) {
        case DATATYPE_SCALAR: {
          DynScalarArray d = ds.as.scalar_array;
          fprintf(stream, "%0.11f\n", d.data[data_pt]);
        } break;
        case DATATYPE_VECTOR: {
          DynVectorArray d = ds.as.vector_array;
          fprintf(stream, "[");
          for (size_t subpt=0; subpt<d.data[data_pt].length; subpt++) {
            if (subpt==0) {
              fprintf(stream, "%.17g", d.data[data_pt].data[subpt]);
            } else {
              fprintf(stream, ", %.17g", d.data[data_pt].data[subpt]);
            }
          }
          fprintf(stream, "]\n");
        } break;
      }

    }
    fprintf(stream, "\n");

    if (input_args.save_to_file) {
      fclose(stream);
      FREE(filename);
    }
  }

  for (size_t i=0; i<data_set_array.length; i++) {
    if (data_set_array.data[i].type == DATATYPE_SCALAR) {
      SDM_ARRAY_FREE(data_set_array.data[i].as.scalar_array);
    } else if (data_set_array.data[i].type == DATATYPE_VECTOR) {
      SDM_ARRAY_FREE(data_set_array.data[i].as.vector_array);
    }
    SDM_ARRAY_FREE(data_set_array.data[i].time_array);
  }
  SDM_ARRAY_FREE(data_set_array);

defer:
  if (conn_str) FREE(conn_str);
  if (conn)     PQfinish(conn);
  if (attrs)    FREE(attrs);
  if (filename) FREE(filename);
  return result;
}

