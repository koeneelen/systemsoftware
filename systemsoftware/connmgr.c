#define _GNU_SOURCE  

#include "connmgr.h"
#include <sys/epoll.h>    
#include "sbuffer.h"
#include <string.h>
#include <unistd.h>


#define FILE_ERROR(fp, error_msg)    do {               \
                      if ((fp)==NULL) {                 \
                        printf("%s\n",(error_msg));     \
                        exit(EXIT_FAILURE);             \
                      }                                 \
                    } while(0)

dplist_t * connection_list;

void *connection_copy(void *sensor);
void connection_free(void **sensor);
int connection_compare(void *x, void *y);

void connmgr_listen(int port_number, sbuffer_t *sbuffer){
    /*---Define local variables & such---*/
    connection_list = dpl_create(connection_copy, connection_free, connection_compare);
    tcpsock_t *server;
    int fd;
    bool terminate = false;
    char * msg;

    /*---Start tcp connection---*/
    if(tcp_passive_open(&server, port_number) != TCP_NO_ERROR) {
        printf("server geraakt niet gemaakt\n");
    }
    if(tcp_get_sd(server,&fd) != TCP_NO_ERROR) {
        printf("socket not yet bound\n");
    }

    /*---Add socket to list---*/
    connection_t * server_connection = malloc(sizeof(connection_t));
    server_connection->socket = server;
    server_connection->last_record = time(NULL); 
    connection_list = dpl_insert_at_index(connection_list, server_connection, 0, false);

    /*---Add socket to epoll---*/
    int epfd = epoll_create(1);
    struct epoll_event event;
    event.events = EPOLLIN | EPOLLRDHUP;
    event.data.fd = fd;
    int s = epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &event);
    if (s == -1)
    {
        perror ("epoll_ctl");
        abort ();
    }

    struct epoll_event events[64];

    /*---Start loop---*/
    while(!terminate)
    {
        /*--- IF NO CONNECTIONS -> CHECK FOR TIMEOUT --- */
        if (dpl_size(connection_list) == 1 && server_connection->last_record + TIMEOUT - 0.0001 < time(NULL))
        {
            printf("CONNMGR TIMEOUT\n");
            tcp_close(&(server_connection->socket)); 
            connection_list = dpl_remove_at_index(connection_list,0,true);
            sbuffer_remove_locks(sbuffer);
            terminate = true;
            break;
        }

        /*--- CHECK FOR TIMEOUTS --- */
        for (int k = 1; k<dpl_size(connection_list); k++)
        {
            connection_t * dummy = dpl_get_element_at_index(connection_list,k);
            if(dummy->last_record + TIMEOUT < time(NULL))
            {
                printf("SENSOR TIMEOUT\n");
                tcp_close(&(dummy->socket)); 
                connection_list = dpl_remove_at_index(connection_list,k,true);
                server_connection->last_record = time(NULL);
                break;
            }
        }

        /*---lets start polling & checking---*/
        int num_ready = epoll_wait(epfd, events, 64, TIMEOUT*1000);
        for(int i = 0; i < num_ready; i++) 
        {
            // check for EPOLLRDHUP events
            if(events[i].events & EPOLLRDHUP)
            {
                for (int a = 1; a<dpl_size(connection_list); a++)
                {
                    connection_t * dummy = dpl_get_element_at_index(connection_list,a);
                    int dummy_sd = 0;
                    if(tcp_get_sd(dummy->socket,&dummy_sd) != TCP_NO_ERROR) {
                        printf("socket not yet bound\n");
                    }
                    if(dummy_sd == events[i].data.fd)
                    {
                        printf("A sensor node with id:%d has closed the connection.\n", dummy->sensor_id);
                        asprintf(&msg, "A sensor node with id:%d has closed the connection.", dummy->sensor_id);
                        write(sbuffer_get_pfd(sbuffer), msg, strlen(msg)+1);
                        free(msg);
                
                        tcp_close(&(dummy->socket)); 
                        connection_list = dpl_remove_at_index(connection_list,a,true);
                        server_connection->last_record = time(NULL);
                        break;
                    }
                }
            }

            // check for EPOLLIN events
            if(events[i].events & EPOLLIN)
            {
                if (events[i].data.fd == fd)
                {
                    tcpsock_t * sensor_socket;

                    if(tcp_wait_for_connection(server, &sensor_socket) != TCP_NO_ERROR) exit(EXIT_FAILURE);
                    int fd;
                    if(tcp_get_sd(sensor_socket,&fd) != TCP_NO_ERROR) { 
                        printf("socket not yet bound\n");
                    }
                    event.data.fd = fd;
                    int s = epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &event);
                    if (s == -1)
                    {
                        perror ("epoll_ctl");
                        abort ();
                    }

                    /*---Add new connection to list---*/
                    connection_t dummy;
                    dummy.socket = sensor_socket;
                    dummy.last_record = time(NULL); 
                    dummy.sensor_id = -1;
                    connection_list = dpl_insert_at_index(connection_list, &dummy, 10000, true);
                }   else
                {
                    for (int j = 1; j<dpl_size(connection_list); j++)
                    {
                        connection_t * dummy = dpl_get_element_at_index(connection_list,j);
                        int dummy_sd = 0;
                        int result = 0;
                        if(tcp_get_sd(dummy->socket,&dummy_sd) != TCP_NO_ERROR) {
                            printf("socket not yet bound\n");
                        }
                        if(dummy_sd == events[i].data.fd)
                        {  
                            int bytes;
                            sensor_data_t data;
                            bytes = sizeof(data.id);
                            result = tcp_receive(dummy->socket, (void *) &data.id, &bytes);
                            bytes = sizeof(data.value);
                            result = tcp_receive(dummy->socket, (void *) &data.value, &bytes);
                            bytes = sizeof(data.ts);
                            result = tcp_receive(dummy->socket, (void *) &data.ts, &bytes);
                            dummy->last_record = data.ts;
                            if ((result == TCP_NO_ERROR) && bytes) {
                                sbuffer_insert(sbuffer, &data);
                            }
                            if(dummy->sensor_id == -1)
                            {
                                printf("A sensor node with id:%d has opened a new connection.\n", data.id);
                                asprintf(&msg, "A sensor node with id:%d has opened a new connection.", data.id);
                                write(sbuffer_get_pfd(sbuffer), msg, strlen(msg)+1);
                                free(msg);
                                dummy->sensor_id = data.id;
                            }
                        }
                    }
                }
            }
        }
    }
}

void connmgr_free()
{
    dpl_free(&connection_list, true);
    connection_list = NULL;
}


void * connection_copy(void * connection)
{
    connection_t * dummy = malloc(sizeof(connection_t));
    assert(dummy != NULL);
    dummy->last_record = ((connection_t*)connection)->last_record;
    dummy->socket =((connection_t*)connection)->socket;
    dummy->sensor_id =((connection_t*)connection)->sensor_id;
    return dummy;
}

void connection_free(void **x)
{
    free(*x);
    *x = NULL;
}

int connection_compare(void *x, void *y)
{
    if(((connection_t*)x)->last_record == ((connection_t*)y)->last_record) return 0;
    else return 1;
}
