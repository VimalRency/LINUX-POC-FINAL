#ifndef PROCMON_H
#define PROCMON_H

#include "supervisor.h"

typedef struct {
    supervisor_t *supervisor;
    unsigned int interval_seconds;
} procmon_context_t;

void *procmon_thread(void *argument);

#endif
