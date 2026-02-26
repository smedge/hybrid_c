#ifndef DATA_LOGS_H
#define DATA_LOGS_H

#include "screen.h"
#include "input.h"
#include <stdbool.h>

void DataLogs_initialize(void);
void DataLogs_cleanup(void);
void DataLogs_toggle(void);
bool DataLogs_is_open(void);
void DataLogs_update(Input *input, unsigned int ticks);
void DataLogs_render(const Screen *screen);

#endif
