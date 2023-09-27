/**
 * \author Koen Eelen
 */
#define _GNU_SOURCE  
#include <stdlib.h>
#include <stdio.h>
#include "config.h"
#include "lib/dplist.h"
#include "datamgr.h"
#include "sbuffer.h"
#include <assert.h>
#include <string.h>
#include <inttypes.h>
#include <unistd.h>


#define ERROR_HANDLER(condition, ...)    do {                       \
                      if (condition) {                              \
                        printf("\nError: in %s - function %s at line %d: %s\n", __FILE__, __func__, __LINE__, __VA_ARGS__); \
                        exit(EXIT_FAILURE);                         \
                      }                                             \
                    } while(0)


typedef struct {
    sensor_id_t sensor_id;
    room_id_t room_id; 
    sensor_value_t temperatures[RUN_AVG_LENGTH];
    sensor_ts_t last_modified;
} sensor_t;

dplist_t * sensor_list;
void *sensor_copy(void *sensor);
void sensor_free(void **sensor);
int sensor_compare(void *x, void *y);

void datamgr_parse_from_buffer(FILE *fp_sensor_map, sbuffer_t *sbuffer, int datamgr_id)
{
    sensor_list = dpl_create(sensor_copy,sensor_free,sensor_compare);
    room_id_t room_id;
    sensor_id_t sensor_id;

    //get list of sensors
    while (fscanf(fp_sensor_map, "%hd %hd", &room_id, &sensor_id) == 2)
    {
        sensor_t * sensor = malloc(sizeof(sensor_t));
        assert(sensor!=NULL);
        sensor->sensor_id = sensor_id;
        sensor->room_id = room_id;
        memset(sensor->temperatures, 0, RUN_AVG_LENGTH*sizeof(sensor_value_t));
        sensor_list = dpl_insert_at_index(sensor_list, sensor, 0, false);
    }

    time_t last_read = time(NULL);
    bool terminate = false;
    while(!terminate)
    {
        sensor_data_t data;
        int reading = sbuffer_consume(sbuffer, &data, datamgr_id);

        if(reading==SBUFFER_SUCCESS)
        {
            //printf("reading data: %"PRIu16" - %g - %ld\n", data.id, data.value, data.ts);
            last_read = time(NULL);
            char * msg;
            sensor_t sensor;
            sensor.sensor_id = data.id;
            sensor_t * dummy = dpl_get_element(sensor_list, &sensor);
            if (dummy == NULL) 
            {
                printf("Received sensor data with invalid sensor node ID:%"PRIu16"\n", data.id);
                asprintf(&msg, "Received sensor data with invalid sensor node ID:%"PRIu16, data.id);
                write(sbuffer_get_pfd(sbuffer), msg, strlen(msg)+1);
                free(msg);
            } else {
                //temperature
                double temp = 0;
                temp += data.value;
                for(int i=RUN_AVG_LENGTH-1 ;0<i ;i--)
                {
                    dummy->temperatures[i]=dummy->temperatures[i-1];
                    temp += dummy->temperatures[i];
                }
                double running_avg = temp/RUN_AVG_LENGTH;
                dummy->temperatures[0] = data.value;
                dummy->last_modified = data.ts;
                
                if (running_avg < SET_MIN_TEMP && !(dummy->temperatures[RUN_AVG_LENGTH-1]==0))
                {
                    printf("The sensor node with id:%"PRIu16" reports it’s too cold (running avg %f)\n", data.id, running_avg);
                    asprintf(&msg, "The sensor node with id:%"PRIu16" reports it’s too cold (running avg %f)", data.id, running_avg);
                    write(sbuffer_get_pfd(sbuffer), msg, strlen(msg)+1);
                    free(msg);
                } else if (running_avg > SET_MAX_TEMP && !(dummy->temperatures[RUN_AVG_LENGTH-1]==0))
                {
                    printf("The sensor node with id:%"PRIu16" reports it’s too hot (running avg %f)\n", data.id, running_avg);
                    asprintf(&msg, "The sensor node with id:%"PRIu16" reports it’s too hot (running avg %f)", data.id, running_avg);
                    write(sbuffer_get_pfd(sbuffer), msg, strlen(msg)+1);
                    free(msg);
                }
            }
        
        } else {
            //printf("datamgr is polling...\n");
        }

        if(last_read + TIMEOUT < time(NULL))
        {
            printf("DATAMGR TIMEOUT\n");
            terminate = true;
        }
    }
}

void datamgr_free()
{
    dpl_free(&sensor_list, true);
}

uint16_t datamgr_get_room_id(sensor_id_t sensor_id)
{
    for (int i = 0; i < dpl_size(sensor_list); i++)
    {
        sensor_t * sensor = (sensor_t *)(dpl_get_element_at_index(sensor_list,i));
        if (sensor->sensor_id == sensor_id)
        {
            return sensor->room_id;
        }
    }
    ERROR_HANDLER(true, "Wrong sensor data"); //if we get this far, the sensor has not been found
}

sensor_value_t datamgr_get_avg(sensor_id_t sensor_id)
{
    for (int i = 0; i < dpl_size(sensor_list); i++)
    {
        sensor_t * sensor = (sensor_t *)(dpl_get_element_at_index(sensor_list,i));
        if (sensor->sensor_id == sensor_id)
        {
            if (sensor->temperatures[RUN_AVG_LENGTH-1]==0) return 0;
            double temp = 0;
            for(int i=0; i < RUN_AVG_LENGTH; i++)
            {
                temp += sensor->temperatures[i];
            }
            double running_avg = temp/RUN_AVG_LENGTH;
            return running_avg;
        }
    }
    ERROR_HANDLER(true, "Wrong sensor data"); //if we get this far, the sensor has not been found
}

time_t datamgr_get_last_modified(sensor_id_t sensor_id)
{
    for (int i = 0; i < dpl_size(sensor_list); i++)
    {
        sensor_t * sensor = (sensor_t *)(dpl_get_element_at_index(sensor_list,i));
        if (sensor->sensor_id == sensor_id)
        {
            return sensor->last_modified;
        }
    }
    ERROR_HANDLER(true, "Wrong sensor data"); //if we get this far, the sensor has not been found
}

int datamgr_get_total_sensors()
{
    return dpl_size(sensor_list);
}

void * sensor_copy(void * sensor)
{
    sensor_t * dummy = malloc(sizeof(sensor_t));
    assert(dummy != NULL);
    dummy->sensor_id = ((sensor_t*)sensor)->sensor_id;
    dummy->room_id = ((sensor_t*)sensor)->room_id;
    *dummy->temperatures = *((sensor_t*)sensor)->temperatures;
    return dummy;
}

void sensor_free(void **sensor)
{
    free(*sensor);
    *sensor = NULL;
}

int sensor_compare(void *x, void *y)
{
    if(((sensor_t*)x)->sensor_id == ((sensor_t*)y)->sensor_id) return 0;
    else return 1;
}

