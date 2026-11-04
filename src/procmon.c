#define _POSIX_C_SOURCE 200809L
#include "procmon.h"
#include "logger.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

typedef struct {
    pid_t pid;
    char name[SUPERVISOR_NAME_MAX];
} process_snapshot_t;

static long read_rss_kb(pid_t pid)
{
    char path[64];
    char line[256];
    FILE *file;
    long value = -1L;

    (void)snprintf(path, sizeof(path), "/proc/%ld/status", (long)pid);
    file = fopen(path, "r");
    if (file == NULL) {
        return -1L;
    }

    while (fgets(line, sizeof(line), file) != NULL) {
        if (sscanf(line, "VmRSS: %ld kB", &value) == 1) {
            break;
        }
    }
    (void)fclose(file);
    return value;
}

void *procmon_thread(void *argument)
{
    procmon_context_t *context = argument;

    if ((context == NULL) || (context->supervisor == NULL)) {
        return NULL;
    }

    while (supervisor_stop_requested_load() == 0) {
        const process_t *current;
        process_snapshot_t *snapshot = NULL;
        size_t count = 0U;
        size_t index = 0U;

        (void)pthread_mutex_lock(&context->supervisor->lock);
        for (current = context->supervisor->head; current != NULL;
             current = current->next) {
            if (current->pid > 0) {
                count++;
            }
        }
        if (count > 0U) {
            snapshot = calloc(count, sizeof(*snapshot));
        }
        if ((snapshot != NULL) || (count == 0U)) {
            for (current = context->supervisor->head;
                 (current != NULL) && (index < count); current = current->next) {
                if (current->pid > 0) {
                    snapshot[index].pid = current->pid;
                    (void)snprintf(snapshot[index].name,
                                   sizeof(snapshot[index].name), "%s", current->name);
                    index++;
                }
            }
        }
        (void)pthread_mutex_unlock(&context->supervisor->lock);

        if (snapshot != NULL) {
            for (index = 0U; index < count; index++) {
                long rss = read_rss_kb(snapshot[index].pid);
                if (rss >= 0L) {
                    logger_write("RESOURCE", "%s pid=%ld VmRSS=%ld kB",
                                 snapshot[index].name, (long)snapshot[index].pid, rss);
                }
            }
        }
        free(snapshot);
        (void)sleep(context->interval_seconds);
    }
    return NULL;
}
