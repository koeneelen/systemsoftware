/**
 * \author Koen Eelen
 */

#ifndef _SBUFFER_H_
#define _SBUFFER_H_

#include "config.h"
#include "lib/dplist.h"

#define SBUFFER_FAILURE -1
#define SBUFFER_SUCCESS 0
#define SBUFFER_NO_DATA 1
#define SBUFFER_ALL_DATA_READ 2

typedef struct sbuffer sbuffer_t;

/**
 * Allocates and initializes a new shared buffer
 * \param buffer a double pointer to the buffer that needs to be initialized
 * \return SBUFFER_SUCCESS on success and SBUFFER_FAILURE if an error occurred
 */
int sbuffer_init(sbuffer_t **buffer);

/**
 * All allocated resources are freed and cleaned up
 * \param buffer a double pointer to the buffer that needs to be freed
 * \return SBUFFER_SUCCESS on success and SBUFFER_FAILURE if an error occurred
 */
int sbuffer_free(sbuffer_t **buffer);

/**
 * Removes the first sensor data in 'buffer' (at the 'head') and returns this sensor data as '*data'
 * If 'buffer' is empty, the function doesn't block until new sensor data becomes available but returns SBUFFER_NO_DATA
 * \param buffer a pointer to the buffer that is used
 * \param data a pointer to pre-allocated sensor_data_t space, the data will be copied into this structure. No new memory is allocated for 'data' in this function.
 * \return SBUFFER_SUCCESS on success and SBUFFER_FAILURE if an error occurred
 */
int sbuffer_remove(sbuffer_t *buffer, sensor_data_t *data);

int sbuffer_consume(sbuffer_t *buffer, sensor_data_t *data, int consumer_id);

void sbuffer_pop(sbuffer_t *buffer);
/**
 * Inserts the sensor data in 'data' at the end of 'buffer' (at the 'tail')
 * \param buffer a pointer to the buffer that is used
 * \param data a pointer to sensor_data_t data, that will be copied into the buffer
 * \return SBUFFER_SUCCESS on success and SBUFFER_FAILURE if an error occured
*/
int sbuffer_insert(sbuffer_t *buffer, sensor_data_t *data);

void * reader_copy(void * sensor);

void reader_free(void **sensor);

int reader_compare(void *x, void *y);

void _sbuffer_print_content(sbuffer_t * buffer);

void sbuffer_insert_consumer_id(sbuffer_t *buffer, int consumer_id);

void sbuffer_remove_locks(sbuffer_t * buffer);

void sbuffer_add_pfds(sbuffer_t * buffer, int pfds[]);

int sbuffer_get_pfd(sbuffer_t * buffer);

#endif  //_SBUFFER_H_
