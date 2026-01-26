#include <math.h>
#include <stdio.h>
#define _XOPEN_SOURCE 700
#define _GNU_SOURCE
#include <assert.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "lib.h"
#include "sdm_lib.h"

#define MAX_QUERYSTR_LENGTH 256

int get_ids_and_tables(PGconn *conn, const char *search_string, ArchiverAttrs *attrs) {
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

  for (int hit=0; hit<num_hits; hit++) {
    ArchiverAttr attr = {0};
    strncpy(attr.id,    PQgetvalue(res, hit, 0), ATTR_ID_LENGTH - 1);
    strncpy(attr.name,  PQgetvalue(res, hit, 1), ATTR_NAME_LENGTH - 1);
    strncpy(attr.table, PQgetvalue(res, hit, 2), ATTR_TABLE_LENGTH - 1);
    SDM_ARRAY_PUSH((*attrs), attr);
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
  printf("get_single_attr_data called for %s\n", attr.name);
  char query_str[2048];
  char start_str[256];
  char stop_str[256];
  memset(query_str, 0, sizeof(query_str)/sizeof(char));
  memset(start_str, 0, sizeof(start_str)/sizeof(char));
  memset(stop_str, 0, sizeof(stop_str)/sizeof(char));

  // Use Central European Time for database query
  // mktime() already normalized the time structures with proper DST handling,
  // so we can use them directly with CET/CEST timezone
  
  const char* timezone_str = start.tm_isdst > 0 ? "CEST" : "CET";
  sprintf(start_str, "%02d-%02d-%02d %02d:%02d:%02d %s",
           start.tm_year+1900,
           start.tm_mon+1,
           start.tm_mday,
           start.tm_hour,
           start.tm_min,
           start.tm_sec,
           timezone_str);

  timezone_str = stop.tm_isdst > 0 ? "CEST" : "CET";
  sprintf(stop_str, "%02d-%02d-%02d %02d:%02d:%02d %s",
           stop.tm_year+1900,
           stop.tm_mon+1,
           stop.tm_mday,
           stop.tm_hour,
           stop.tm_min,
           stop.tm_sec,
           timezone_str);

  sprintf(query_str,
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

    // The following is used instead of strptime since that does not exist on Windows
    int year, month, day, hour, minute, second;
    sscanf(time_str, "%d-%d-%d %d:%d:%d",
           &year, &month, &day, &hour, &minute, &second);
    while (*time_str != '.') time_str++;
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
    // Database returns UTC time, parse it and convert to local time
    time_struct.tm_year = year - 1900;
    time_struct.tm_mon = month - 1;
    time_struct.tm_mday = day;
    time_struct.tm_hour = hour;
    time_struct.tm_min = minute;
    time_struct.tm_sec = second;
    time_struct.tm_isdst = 0; // UTC time has no DST
    
    // Determine if this date falls during CEST (last Sunday in March to last Sunday in October)
    // CEST runs from 02:00 UTC last Sunday in March to 02:00 UTC last Sunday in October
    bool is_summer_time = false;
    
    // Simple DST determination for Central Europe
    if (time_struct.tm_mon > 2 && time_struct.tm_mon < 9) { // April to September
        is_summer_time = true;
    } else if (time_struct.tm_mon == 2) { // March
        // Check if after last Sunday in March (after 01:00 UTC on last Sunday)
        int last_sunday = 31; // Start from 31st and work backwards
        while (last_sunday > 0) {
            struct tm check_tm = {0};
            check_tm.tm_year = time_struct.tm_year;
            check_tm.tm_mon = 2; // March
            check_tm.tm_mday = last_sunday;
            check_tm.tm_isdst = 0;
            mktime(&check_tm);
            if (check_tm.tm_wday == 0) { // Sunday
                if (time_struct.tm_mday > last_sunday || 
                    (time_struct.tm_mday == last_sunday && time_struct.tm_hour >= 1)) {
                    is_summer_time = true;
                }
                break;
            }
            last_sunday--;
        }
    } else if (time_struct.tm_mon == 9) { // October
        // Check if before last Sunday in October (before 01:00 UTC on last Sunday)
        int last_sunday = 31;
        while (last_sunday > 0) {
            struct tm check_tm = {0};
            check_tm.tm_year = time_struct.tm_year;
            check_tm.tm_mon = 9; // October
            check_tm.tm_mday = last_sunday;
            check_tm.tm_isdst = 0;
            mktime(&check_tm);
            if (check_tm.tm_wday == 0) { // Sunday
                if (time_struct.tm_mday < last_sunday || 
                    (time_struct.tm_mday == last_sunday && time_struct.tm_hour < 1)) {
                    is_summer_time = true;
                }
                break;
            }
            last_sunday--;
        }
    }
    
    // Apply UTC to local time conversion
    int hour_offset = is_summer_time ? 2 : 1; // CEST = UTC+2, CET = UTC+1
    time_struct.tm_hour += hour_offset;
    
    // Handle overflow
    while (time_struct.tm_hour >= 24) {
        time_struct.tm_hour -= 24;
        time_struct.tm_mday += 1;
        
        // Handle month/year overflow
        int days_in_month;
        if (time_struct.tm_mon == 1) { // February
            // Simple leap year check
            int year = time_struct.tm_year + 1900;
            bool is_leap = (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
            days_in_month = is_leap ? 29 : 28;
        } else if (time_struct.tm_mon == 3 || time_struct.tm_mon == 5 || time_struct.tm_mon == 8 || time_struct.tm_mon == 10) {
            days_in_month = 30;
        } else {
            days_in_month = 31;
        }
        
        if (time_struct.tm_mday > days_in_month) {
            time_struct.tm_mday = 1;
            time_struct.tm_mon += 1;
            if (time_struct.tm_mon > 11) {
                time_struct.tm_mon = 0;
                time_struct.tm_year += 1;
            }
        }
    }
    
    time_struct.tm_isdst = is_summer_time ? 1 : 0;
    mktime(&time_struct); // Normalize the structure

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
        char *end_ptr;
        double val = strtod(db_val_str, &end_ptr);
        if (db_val_str == end_ptr)
            val = NAN;
        SDM_ARRAY_PUSH(dataset->as.scalar_array, val);
      }
    } else if (dataset->type == DATATYPE_VECTOR) {
      SDM_ARRAY_PUSH((dataset->as.vector_array), (DynScalarArray){0});
      size_t item_number = dataset->as.vector_array.length - 1;
      if (*db_val_str == '{') db_val_str++;
      while (*db_val_str != '}') {
        char *end_ptr;
        double val = strtod(db_val_str, &end_ptr);
        if (db_val_str == end_ptr)
            val = NAN;
        db_val_str = end_ptr;
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

