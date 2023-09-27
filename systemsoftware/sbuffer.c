/**
 * \author Koen Eelen
 */

#include <stdlib.h>
#include <stdio.h>
#include "sbuffer.h"
#include <string.h>
#include <semaphore.h>
#include <pthread.h>
#include <inttypes.h>
#ifndef NUMBER_OF_CONSUMERS
#define NUMBER_OF_CONSUMERS 2
#endif

/**
 * basic node for the buffer, these nodes are linked together to create the buffer
 */
typedef struct sbuffer_node {
    struct sbuffer_node *next;  /**< a pointer to the next node*/
    sensor_data_t data;         /**< a structure containing the data */
    int consumed_by[NUMBER_OF_CONSUMERS];
} sbuffer_node_t;

/**
 * a structure to keep track of the buffer
 */
struct sbuffer {
    sbuffer_node_t *head;       /**< a pointer to the first node in the buffer */
    sbuffer_node_t *tail;       /**< a pointer to the last node in the buffer */
    bool terminate;
    pthread_rwlock_t * lock;
    int pfds[2];
    sem_t consumer_locks[NUMBER_OF_CONSUMERS];
    int consumer_ids[NUMBER_OF_CONSUMERS];
};

void sbuffer_insert_consumer_id(sbuffer_t *buffer, int consumer_id)
{
    for(int i = 0; i<sizeof(buffer->consumer_ids)/sizeof(int); i++)
    {
        if (buffer->consumer_ids[i] == 0)
        {
            buffer->consumer_ids[i] = consumer_id;
            return;
        }
    }
}

int sbuffer_init(sbuffer_t **buffer) {
    *buffer = malloc(sizeof(sbuffer_t));
    if (*buffer == NULL) return SBUFFER_FAILURE;
    (*buffer)->head = NULL;
    (*buffer)->tail = NULL;
    (*buffer)->terminate = false;
    (*buffer)->lock = malloc(sizeof(pthread_rwlock_t));
    pthread_rwlock_init((*buffer)->lock, NULL);
    for(int i = 0; i<sizeof((*buffer)->consumer_ids)/sizeof(int); i++)
    {
        (*buffer)->consumer_ids[i] = 0;
        sem_init(&((*buffer)->consumer_locks[i]),0,0);
    }
    return SBUFFER_SUCCESS;
}

int sbuffer_free(sbuffer_t **buffer) {
    sbuffer_node_t *dummy;
    if ((buffer == NULL) || (*buffer == NULL)) {
        return SBUFFER_FAILURE;
    }
    while ((*buffer)->head) {
        dummy = (*buffer)->head;
        (*buffer)->head = (*buffer)->head->next;
        free(dummy);
    }
    free((*buffer)->lock);
    free(*buffer);
    *buffer = NULL;
    return SBUFFER_SUCCESS;
}

int sbuffer_consume(sbuffer_t *buffer, sensor_data_t *data, int consumer_id)
{
    int index = -1;
    for(int i = 0; i<sizeof(buffer->consumer_ids)/sizeof(int); i++)
    {
        if (buffer->consumer_ids[i] == consumer_id)
        {
            index = i;
        }
    }
    if (buffer->head == NULL) 
    {
        if(!buffer->terminate) sem_wait(&(buffer->consumer_locks[index]));
        return SBUFFER_NO_DATA;
    }
    if(!buffer->terminate) sem_wait(&(buffer->consumer_locks[index]));

    if (buffer == NULL) return SBUFFER_FAILURE;
    pthread_rwlock_wrlock(buffer->lock);
    index = -1;
    for(int i = 0; i<sizeof(buffer->consumer_ids)/sizeof(int); i++)
    {
        if (buffer->consumer_ids[i] == consumer_id)
        {
            index = i;
        }
    }
    if(index == -1) 
    {
        pthread_rwlock_unlock(buffer->lock);
        return SBUFFER_FAILURE;
    }
    if (buffer->head == NULL) 
    {
        return SBUFFER_NO_DATA;
        pthread_rwlock_unlock(buffer->lock);
    }
    sbuffer_node_t * dummy = buffer->head;
    while(dummy->consumed_by[index] == consumer_id && dummy != buffer->tail)
    {
        dummy = dummy->next;
    } 

    dummy->consumed_by[index] = consumer_id;
    *data = dummy->data;
    for(int i = 0; i<sizeof(buffer->consumer_ids)/sizeof(int); i++)
    {
        if(dummy->consumed_by[i] != buffer->consumer_ids[i]) 
        {
            pthread_rwlock_unlock(buffer->lock);
            return SBUFFER_SUCCESS;
        }
        if (buffer->terminate && dummy == buffer->tail)
        {
            pthread_rwlock_unlock(buffer->lock);
            return SBUFFER_FAILURE;
        }
    }
    sbuffer_pop(buffer);
    //printf("-----------BUFFER AFTER POP");
    //_sbuffer_print_content(buffer);
    pthread_rwlock_unlock(buffer->lock);
    return SBUFFER_SUCCESS;
}

void sbuffer_pop(sbuffer_t *buffer) {
    sbuffer_node_t * dummy = buffer->head;
    if (buffer->head == buffer->tail) 
    {
        buffer->head = buffer->tail = NULL;
    } else 
    {
        buffer->head = buffer->head->next;
    }
    free(dummy);;
}

int sbuffer_insert(sbuffer_t *buffer, sensor_data_t *data) {
    if (buffer == NULL) return SBUFFER_FAILURE;
    sbuffer_node_t *dummy;
    dummy = malloc(sizeof(sbuffer_node_t));
    dummy->data = *data;
    dummy->next = NULL;
    for(int i = 0; i<sizeof(buffer->consumer_ids)/sizeof(int); i++)
    {
        dummy->consumed_by[i] = 0;
        sem_post(&(buffer->consumer_locks[i]));
    }
    pthread_rwlock_wrlock(buffer->lock);
    if (buffer->tail == NULL)
    {
        buffer->head = buffer->tail = dummy;
    } else
    {
        buffer->tail->next = dummy;
        buffer->tail = buffer->tail->next;
    }
    //printf("-----------BUFFER AFTER INSERT");
    //_sbuffer_print_content(buffer);
    pthread_rwlock_unlock(buffer->lock);
    return SBUFFER_SUCCESS;
}

void _sbuffer_print_content(sbuffer_t * buffer)
{
    printf("\n##### Printing SBUFFER Content Summary #####\n");
    sbuffer_node_t * dummy = buffer->head;
    for(int i = 0; dummy != NULL; dummy = dummy->next, i++)
    {
        printf("%d: %p | %p | %"PRIu16" - %g - %ld, [r1,r2]=[%d, %d]\n", i, dummy, dummy->next, dummy->data.id, dummy->data.value, dummy->data.ts, dummy->consumed_by[0], dummy->consumed_by[1]);
    }
    printf("\n");
    fflush(stdout);
}


void sbuffer_remove_locks(sbuffer_t * buffer)
{
    buffer->terminate = true;
    for(int i = 0; i<sizeof(buffer->consumer_ids)/sizeof(int); i++)
    {
        sem_post(&(buffer->consumer_locks[i]));
    }
}

void sbuffer_add_pfds(sbuffer_t * buffer, int pfds[])
{
    for(int i = 0; i<sizeof(buffer->consumer_ids)/sizeof(int); i++)
    {
        buffer->pfds[i] = pfds[i];
    }
}

int sbuffer_get_pfd(sbuffer_t * buffer)
{
    return buffer->pfds[1];
}
