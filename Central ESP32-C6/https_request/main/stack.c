

#include "stack.h"


void stack_init(stack* s, int initial_capacity) {
    s->items = (sensor_data*)malloc(initial_capacity * sizeof(sensor_data));
    s->size = 0;
    s->capacity = initial_capacity;
}

void stack_push(stack* s, sensor_data data) {
    if (s->size == s->capacity) {
        s->capacity *= 2;
        s->items = (sensor_data*)realloc(s->items, s->capacity * sizeof(sensor_data));
    }
    s->items[s->size++] = data;
}

sensor_data stack_pop(stack* s) {
    if (s->size == 0) {
        fprintf(stderr, "Stack underflow\n");
        exit(EXIT_FAILURE);
    }
    return s->items[--s->size];
}

void stack_free(stack* s) {
    free(s->items);
    s->items = NULL;
    s->size = s->capacity = 0;
}

char *formatted_int(int num) {
    int whole_part = num / 100;
    int decimal_part = num % 100;
    char *formatted_num = (char *)calloc(14 , sizeof(char));
    sprintf(formatted_num, "%d.%02d", whole_part, decimal_part);  
    return formatted_num;
}


char *sensor_data_format(sensor_data item) {
    char *json_buffer = (char *)malloc(128);
    char *formatted_temp = formatted_int(item.temp);
    char *formatted_hum = formatted_int(item.hum);

    sprintf(json_buffer, "\"SENS\":\"%s\",\"CAMU\":\"%s\",\"TEMP\":%s,\"HUM\":%s,\"TIMESTAMP\":\"%s\"", item.sens, item.camu, formatted_temp, formatted_hum, item.timestamp);
    free(formatted_temp);
    free(formatted_hum);
    return json_buffer;
}
