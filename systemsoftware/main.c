#define _GNU_SOURCE  
#include <pthread.h>
#include "connmgr.h"
#include "sbuffer.h"
#include "datamgr.h"
#include "sensor_db.h"
#include <unistd.h>
#include <stdio.h>
#include <string.h>

#define MAX 100
#define STORAGEMGR_ID 11
#define DATAMGR_ID 22

int port;
sbuffer_t *sbuffer;
pthread_t connmgr_thread, datamgr_thread, storagemgr_thread;

void *start_connmgr(){
    connmgr_listen(port, sbuffer);
    connmgr_free();
    pthread_exit(0);
}

void *start_datamgr(){
    FILE *fp = fopen("room_sensor.map", "r");
    datamgr_parse_from_buffer(fp, sbuffer, DATAMGR_ID);
    datamgr_free();
    fclose(fp);
    pthread_exit(0);
}

void *start_storagemgr(){
    DBCONN *conn = init_connection(1, sbuffer);
    if(conn != NULL){
        insert_sensor_from_buffer(conn, sbuffer, STORAGEMGR_ID);
        disconnect(conn);
    }
    pthread_exit(0);
}

int main(int argc, char *argv[])
{
    if (argc != 2)
    {
        printf("Error: no port given.\n");
        exit(EXIT_SUCCESS);
    }

    port = atoi(argv[1]);
    int pfds[2];
    int result;
    char * write_buffer;
    result = pipe(pfds);

    pid_t child_pid;
    child_pid = fork();

    if(child_pid == 0)
    {
        //child code
        char recv_buf[MAX];
        close(pfds[1]);
        FILE * log = fopen("gateway.log", "w");
        int sequence = 0;
        do
        {
            result = read(pfds[0], recv_buf, MAX);
            asprintf(&write_buffer, "%d %ld %s\n", sequence, time(NULL), recv_buf);
            fwrite(write_buffer, strlen(write_buffer), 1, log);
            free(write_buffer);
            sequence++;
        } while (result > 0);
        close(pfds[0]);
        fclose(log);
    } else {
        sbuffer_init(&sbuffer);
        sbuffer_insert_consumer_id(sbuffer, DATAMGR_ID);
        sbuffer_insert_consumer_id(sbuffer, STORAGEMGR_ID);
        sbuffer_add_pfds(sbuffer, pfds);
        pthread_create(&connmgr_thread, NULL, start_connmgr, NULL);
        pthread_create(&datamgr_thread, NULL, start_datamgr, NULL);
        pthread_create(&storagemgr_thread, NULL, start_storagemgr, NULL);
        pthread_join(connmgr_thread, NULL);
        pthread_join(datamgr_thread, NULL);
        pthread_join(storagemgr_thread, NULL);
    }
    sbuffer_free(&sbuffer);
}
