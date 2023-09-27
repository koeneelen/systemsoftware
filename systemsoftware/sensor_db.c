/**
 * \author Koen Eelen
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include "config.h"
#include "sensor_db.h"
#include <sqlite3.h>
#include <string.h>
#include <inttypes.h>
#include <unistd.h>


typedef int (*callback_t)(void *, int, char **, char **);
int execute_query(DBCONN *conn, char * query, callback_t f);

DBCONN *init_connection(char clear_up_flag, sbuffer_t * sbuffer)
{
    sqlite3 *db;
    char *err_msg = 0;
    char *sql;
    char *msg;
    
    int rc = sqlite3_open(TO_STRING(DB_NAME), &db);

    //check if connection succesful
    if (rc != SQLITE_OK)
    {
        printf("Unable to connect to SQL server.\n");
        asprintf(&msg, "Unable to connect to SQL server.");
        write(sbuffer_get_pfd(sbuffer), msg, strlen(msg)+1);
        free(msg);
        return NULL;
        int i = 0;
        while(rc != SQLITE_OK && i < 2)
        {
            sqlite3_free(err_msg);
            sqlite3_close(db);
            printf("Reconnecting to SQL server.\n");
            asprintf(&msg, "Reconnecting to SQL server.");
            write(sbuffer_get_pfd(sbuffer), msg, strlen(msg)+1);
            free(msg);
            rc = sqlite3_open(TO_STRING(DB_NAME), &db);
            i++;
            sleep(5);
        }
        if(i == 1 && rc != SQLITE_OK)
        {
            printf("Connection to SQL server could not be established.\n");
            asprintf(&msg, "Connection to SQL server could not be established.");
            write(sbuffer_get_pfd(sbuffer), msg, strlen(msg)+1);
            free(msg);
            sbuffer_remove_locks(sbuffer);
        }
    }

    //initiate sql statement in case 
    if(clear_up_flag)
    {
        sql = 
            "DROP TABLE IF EXISTS "TO_STRING(TABLE_NAME)";"
            "CREATE TABLE "TO_STRING(TABLE_NAME)"(id INTEGER PRIMARY KEY AUTOINCREMENT, sensor_id INTEGER, sensor_value DECIMAL(4,2), sensor_time TIMESTAMP, upload_time TIMESTAMP);";
    } else
    {
        sql = "CREATE TABLE IF NOT EXISTS "TO_STRING(TABLE_NAME)"(id INTEGER PRIMARY KEY AUTOINCREMENT, sensor_id INTEGER, sensor_value DECIMAL(4,2), timestamp TIMESTAMP, upload_time TIMESTAMP);";
    }
    //execute sql stuff and check for errors
    rc = sqlite3_exec(db, sql, 0, 0, &err_msg);
    if (rc != SQLITE_OK)
    {
        fprintf(stderr, "Cannot open database: %s\n", sqlite3_errmsg(db));
        sqlite3_free(err_msg);
        sqlite3_close(db);
        return NULL;
    }
    printf("New table "TO_STRING(TABLE_NAME) " created.\n");
    asprintf(&msg, "New table "TO_STRING(TABLE_NAME) " created.");
    write(sbuffer_get_pfd(sbuffer), msg, strlen(msg)+1);
    free(msg);

    printf("Connection to SQL server established.\n");
    asprintf(&msg, "Connection to SQL server established.");
    write(sbuffer_get_pfd(sbuffer), msg, strlen(msg)+1);
    free(msg);
    return db;
}

void disconnect(DBCONN *conn)
{
    sqlite3_close(conn);
}


int insert_sensor(DBCONN *conn, sensor_id_t id, sensor_value_t value, sensor_ts_t ts)
{
    char * sql = "INSERT INTO "TO_STRING(TABLE_NAME)"(sensor_id, sensor_value, sensor_time, upload_time) VALUES(@sensor_id, @sensor_value, @sensor_time, @upload_time);";
    sqlite3_stmt *pStmt;
    int rc = sqlite3_prepare_v2(conn, sql, -1, &pStmt, 0);
    
    if (rc != SQLITE_OK ) {
        fprintf(stderr, "Failed to prepare statement\n");
        fprintf(stderr, "Cannot open database: %s\n", sqlite3_errmsg(conn));
        sqlite3_close(conn);
        return 1;
    } 
    int sensor_id = sqlite3_bind_parameter_index(pStmt, "@sensor_id");
    int sensor_value = sqlite3_bind_parameter_index(pStmt, "@sensor_value");
    int sensor_time = sqlite3_bind_parameter_index(pStmt, "@sensor_time");
    int upload_time = sqlite3_bind_parameter_index(pStmt, "@upload_time");
    sqlite3_bind_int(pStmt, sensor_id, id);
    sqlite3_bind_double(pStmt, sensor_value, value);
    sqlite3_bind_int(pStmt, sensor_time, ts);
    sqlite3_bind_int(pStmt, upload_time, time(NULL));

    rc = sqlite3_step(pStmt);
    if (rc != SQLITE_DONE) {
        printf("execution failed: %s", sqlite3_errmsg(conn));
        sqlite3_close(conn);
        return 1;
    }
    sqlite3_finalize(pStmt); 
    return 0;
}


int insert_sensor_from_buffer(DBCONN *conn, sbuffer_t *sbuffer, int storagemgr_id)
{
    time_t last_upload = time(NULL);
    bool terminate = false;
    while(!terminate)
    {
        sensor_data_t data; 
        int reading = sbuffer_consume(sbuffer, &data, storagemgr_id);
        if(reading==SBUFFER_SUCCESS)
        {
            last_upload = time(NULL);
            int s = insert_sensor(conn, data.id, data.value, data.ts);
            if(s == 1)
            {
                char * msg;
                printf("Connection to SQL server lost.\n");
                asprintf(&msg, "Connection to SQL server lost.");
                write(sbuffer_get_pfd(sbuffer), msg, strlen(msg)+1);
                free(msg);
            }
        }

        if(last_upload + TIMEOUT < time(NULL))
        {
            printf("STORAGEMGR TIMEOUT\n");
            terminate = true;
        }
    }
    return 0;
}


int find_sensor_all(DBCONN *conn, callback_t f)
{
    char *err_msg = 0;
    char *sql = "SELECT * FROM "TO_STRING(TABLE_NAME)";";
        
    int rc = sqlite3_exec(conn, sql, f, 0, &err_msg);
    if (rc != SQLITE_OK ) {
        
        fprintf(stderr, "Failed to select data\n");
        fprintf(stderr, "SQL error: %s\n", err_msg);
        sqlite3_free(err_msg);
        sqlite3_close(conn);
        return 1;
    } 
    sqlite3_close(conn);
    return 0;
}


int find_sensor_by_value(DBCONN *conn, sensor_value_t value, callback_t f)
{
    char * sql = 0;
    asprintf(&sql, "SELECT * FROM %s WHERE sensor_value = %f;", TO_STRING(TABLE_NAME),value);
    return execute_query(conn, sql, f);
}


int find_sensor_exceed_value(DBCONN *conn, sensor_value_t value, callback_t f)
{
    char * sql = 0;
    asprintf(&sql, "SELECT * FROM %s WHERE sensor_value > %f;", TO_STRING(TABLE_NAME),value);
    return execute_query(conn, sql, f);
}


int find_sensor_by_timestamp(DBCONN *conn, sensor_ts_t ts, callback_t f)
{
    char * sql = 0;
    asprintf(&sql, "SELECT * FROM %s WHERE timestamp = %ld;", TO_STRING(TABLE_NAME),ts);
    return execute_query(conn, sql, f);
}


int find_sensor_after_timestamp(DBCONN *conn, sensor_ts_t ts, callback_t f)
{
    char * sql = 0;
    asprintf(&sql, "SELECT * FROM %s WHERE timestamp > %ld;", TO_STRING(TABLE_NAME),ts);
    return execute_query(conn, sql, f);
}


int execute_query(DBCONN *conn, char * query, callback_t f)
{
    char * err_msg = 0;
    int rc = sqlite3_exec(conn, query, f, 0, &err_msg);
    free(query);
    if (rc != SQLITE_OK ) {
        fprintf(stderr, "Failed to select data\n");
        fprintf(stderr, "SQL error: %s\n", err_msg);
        sqlite3_free(err_msg);
        sqlite3_close(conn);
        return 1;
    } 
    sqlite3_close(conn);
    return 0;
}
