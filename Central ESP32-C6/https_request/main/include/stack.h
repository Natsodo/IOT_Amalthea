#include <stdio.h>
#include <stdint.h>
#include <string.h>

typedef struct sensor_data {
    char *sens;
    int temp; 
    int hum;
    char *camu;
    char *timestamp;
} sensor_data;

typedef struct stack {
    sensor_data* items;
    int size;
    int capacity;
} stack;


void stack_init(stack* s, int initial_capacity);
void stack_push(stack* s, sensor_data item);
sensor_data stack_pop(stack* s);
void stack_free(stack* s);
char *sensor_data_format(sensor_data item);