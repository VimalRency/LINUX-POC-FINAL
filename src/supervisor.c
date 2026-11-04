#define _POSIX_C_SOURCE 200809L
#include "supervisor.h"
#include "logger.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

static pthread_mutex_t stop_flag_mutex = PTHREAD_MUTEX_INITIALIZER;
static int supervisor_stop_requested = 0;
volatile sig_atomic_t supervisor_signal_received = 0;

int supervisor_stop_requested_load(void)
{
    int value = 0;
    (void)pthread_mutex_lock(&stop_flag_mutex);
    value = supervisor_stop_requested;
    (void)pthread_mutex_unlock(&stop_flag_mutex);
    return value;
}

void supervisor_stop_requested_store(int value)
{
    (void)pthread_mutex_lock(&stop_flag_mutex);
    supervisor_stop_requested = (value != 0) ? 1 : 0;
    (void)pthread_mutex_unlock(&stop_flag_mutex);
}

static const char *state_name(process_state_t state)
{
    switch (state) {
        case PROCESS_RUNNING: return "RUNNING";
        case PROCESS_WAITING_RESTART: return "WAITING_RESTART";
        case PROCESS_WAITING_DEPENDENCY: return "WAITING_DEPENDENCY";
        case PROCESS_PERMANENTLY_FAILED: return "FAILED";
        case PROCESS_STOPPED:
        default: return "STOPPED";
    }
}

static int start_process(process_t *process)
{
    pid_t child = fork();

    if (child < 0) {
        logger_write("ERROR", "fork failed for %s: %s",
                     process->name, strerror(errno));
        return -1;
    }

    if (child == 0) {
        execvp(process->argv[0], process->argv);
        (void)fprintf(stderr, "execvp failed for %s: %s\n",
                      process->name, strerror(errno));
        _exit(127);
    }

    process->pid = child;
    process->state = PROCESS_RUNNING;
    logger_write("INFO", "Started %s with PID %ld",
                 process->name, (long)child);
    return 0;
}

int supervisor_init(supervisor_t *supervisor, const char *log_path)
{
    if ((supervisor == NULL) || (log_path == NULL) ||
        (strlen(log_path) >= sizeof(supervisor->log_path))) {
        return -1;
    }

    (void)memset(supervisor, 0, sizeof(*supervisor));
    supervisor_stop_requested_store(0);
    supervisor_signal_received = 0;
    supervisor->heartbeat_fd = -1;
    if (pthread_mutex_init(&supervisor->lock, NULL) != 0) {
        return -1;
    }
    (void)snprintf(supervisor->log_path, sizeof(supervisor->log_path),
                   "%s", log_path);

    if (logger_init(log_path) != 0) {
        (void)pthread_mutex_destroy(&supervisor->lock);
        return -1;
    }
    return 0;
}

process_t *supervisor_find_process(supervisor_t *supervisor, const char *name)
{
    process_t *current;

    if ((supervisor == NULL) || (name == NULL)) {
        return NULL;
    }

    (void)pthread_mutex_lock(&supervisor->lock);
    current = supervisor->head;
    while ((current != NULL) && (strcmp(current->name, name) != 0)) {
        current = current->next;
    }
    (void)pthread_mutex_unlock(&supervisor->lock);
    return current;
}

int supervisor_dependencies_ready(const supervisor_t *supervisor, const process_t *process)
{
    size_t index;
    if ((supervisor == NULL) || (process == NULL)) { return 0; }
    for (index = 0U; index < process->dependency_count; index++) {
        const process_t *dependency = supervisor->head;
        while ((dependency != NULL) &&
               (strcmp(dependency->name, process->dependencies[index]) != 0)) {
            dependency = dependency->next;
        }
        if ((dependency == NULL) || (dependency->state != PROCESS_RUNNING) ||
            (dependency->pid <= 0)) { return 0; }
    }
    return 1;
}

int supervisor_start_all(supervisor_t *supervisor)
{
    size_t total = 0U;
    size_t started = 0U;
    size_t pass;
    process_t *current;
    int result = 0;

    if (supervisor == NULL) { return -1; }
    (void)pthread_mutex_lock(&supervisor->lock);
    for (current = supervisor->head; current != NULL; current = current->next) { total++; }
    for (pass = 0U; (pass < total) && (started < total); pass++) {
        bool progress = false;
        for (current = supervisor->head; current != NULL; current = current->next) {
            if ((current->state == PROCESS_STOPPED) ||
                (current->state == PROCESS_WAITING_DEPENDENCY)) {
                if (supervisor_dependencies_ready(supervisor, current) != 0) {
                    if (start_process(current) == 0) {
                        started++;
                        progress = true;
                    } else {
                        current->state = PROCESS_PERMANENTLY_FAILED;
                        result = -1;
                    }
                } else {
                    current->state = PROCESS_WAITING_DEPENDENCY;
                }
            }
        }
        if (!progress) { break; }
    }
    if (started < total) {
        for (current = supervisor->head; current != NULL; current = current->next) {
            if (current->state == PROCESS_WAITING_DEPENDENCY) {
                logger_write("INFO", "%s waiting for dependencies", current->name);
            }
        }
    }
    (void)pthread_mutex_unlock(&supervisor->lock);
    return result;
}

void supervisor_print_status(supervisor_t *supervisor)
{
    const process_t *current;

    if (supervisor == NULL) {
        return;
    }

    (void)pthread_mutex_lock(&supervisor->lock);
    (void)printf("\n%-20s %-10s %-10s %-12s\n",
                 "PROCESS", "PID", "STATE", "RESTARTS");
    (void)printf("------------------------------------------------------\n");
    current = supervisor->head;
    while (current != NULL) {
        (void)printf("%-20s %-10ld %-10s %u/%u\n",
                     current->name, (long)current->pid,
                     state_name(current->state), current->restart_count,
                     current->max_restarts);
        current = current->next;
    }
    (void)pthread_mutex_unlock(&supervisor->lock);
}

static void handle_child_exit(process_t *process, int status)
{
    bool failed = true;

    process->last_exit_status = status;
    process->pid = -1;

    if (WIFEXITED(status) && (WEXITSTATUS(status) == 0)) {
        failed = false;
        process->state = PROCESS_STOPPED;
        logger_write("INFO", "%s exited normally", process->name);
    } else {
        if (WIFEXITED(status)) {
            logger_write("WARN", "%s exited with code %d",
                         process->name, WEXITSTATUS(status));
        } else if (WIFSIGNALED(status)) {
            logger_write("WARN", "%s terminated by signal %d",
                         process->name, WTERMSIG(status));
        }
    }

    if (failed) {
        if (process->restart_count < process->max_restarts) {
            process->restart_count++;
            process->next_restart = time(NULL) +
                                    (time_t)process->restart_delay;
            process->state = PROCESS_WAITING_RESTART;
            logger_write("INFO", "Restart scheduled for %s (%u/%u)",
                         process->name, process->restart_count,
                         process->max_restarts);
        } else {
            process->state = PROCESS_PERMANENTLY_FAILED;
            logger_write("ERROR", "%s exhausted restart limit",
                         process->name);
        }
    }
}

void supervisor_run(supervisor_t *supervisor)
{
    time_t last_dashboard = 0;

    if (supervisor == NULL) {
        return;
    }

    while ((supervisor->shutting_down == false) &&
           (supervisor_signal_received == 0) &&
           (supervisor_stop_requested_load() == 0)) {
        process_t *current;
        time_t now = time(NULL);

        if (supervisor->heartbeat_fd >= 0) {
            const unsigned char heartbeat = 0xA5U;
            ssize_t bytes_written = write(supervisor->heartbeat_fd, &heartbeat,
                                          sizeof(heartbeat));
            if ((bytes_written < 0) && (errno != EINTR) &&
                (errno != EAGAIN) && (errno != EWOULDBLOCK)) {
                logger_write("ERROR", "Supervisor heartbeat write failed: %s",
                             strerror(errno));
                supervisor_stop_requested_store(1);
            }
        }

        (void)pthread_mutex_lock(&supervisor->lock);
        current = supervisor->head;
        while (current != NULL) {
            if (current->pid > 0) {
                int status = 0;
                pid_t result = waitpid(current->pid, &status, WNOHANG);
                if (result == current->pid) {
                    handle_child_exit(current, status);
                } else if ((result < 0) && (errno != EINTR)) {
                    logger_write("ERROR", "waitpid failed for %s: %s",
                                 current->name, strerror(errno));
                }
            }

            if ((current->state == PROCESS_WAITING_DEPENDENCY) &&
                (supervisor_dependencies_ready(supervisor, current) != 0) &&
                (supervisor->shutting_down == false)) {
                if (start_process(current) != 0) {
                    current->state = PROCESS_PERMANENTLY_FAILED;
                }
            }

            if ((current->state == PROCESS_WAITING_RESTART) &&
                (now >= current->next_restart) &&
                (supervisor_dependencies_ready(supervisor, current) != 0) &&
                (supervisor->shutting_down == false)) {
                if (start_process(current) != 0) {
                    if (current->restart_count >= current->max_restarts) {
                        current->state = PROCESS_PERMANENTLY_FAILED;
                    } else {
                        current->restart_count++;
                        current->next_restart = now +
                            (time_t)current->restart_delay;
                    }
                }
            }
            current = current->next;
        }
        (void)pthread_mutex_unlock(&supervisor->lock);

        if ((now - last_dashboard) >= 5) {
            supervisor_print_status(supervisor);
            last_dashboard = now;
        }
        (void)sleep(1U);
    }
    supervisor_stop_requested_store(1);
}

void supervisor_mark_shutdown(supervisor_t *supervisor)
{
    if (supervisor != NULL) {
        supervisor->shutting_down = true;
    }
}

void supervisor_stop_all(supervisor_t *supervisor)
{
    process_t *current;
    int attempt;

    if (supervisor == NULL) {
        return;
    }

    supervisor->shutting_down = true;
    (void)pthread_mutex_lock(&supervisor->lock);
    current = supervisor->head;
    while (current != NULL) {
        if (current->pid > 0) {
            (void)kill(current->pid, SIGTERM);
        }
        current = current->next;
    }
    (void)pthread_mutex_unlock(&supervisor->lock);

    for (attempt = 0; attempt < 5; attempt++) {
        bool all_stopped = true;
        (void)pthread_mutex_lock(&supervisor->lock);
        current = supervisor->head;
        while (current != NULL) {
            if (current->pid > 0) {
                int status = 0;
                pid_t result = waitpid(current->pid, &status, WNOHANG);
                if (result == current->pid) {
                    current->pid = -1;
                    current->state = PROCESS_STOPPED;
                } else {
                    all_stopped = false;
                }
            }
            current = current->next;
        }
        (void)pthread_mutex_unlock(&supervisor->lock);
        if (all_stopped) {
            break;
        }
        (void)sleep(1U);
    }

    (void)pthread_mutex_lock(&supervisor->lock);
    current = supervisor->head;
    while (current != NULL) {
        if (current->pid > 0) {
            (void)kill(current->pid, SIGKILL);
            (void)waitpid(current->pid, NULL, 0);
            current->pid = -1;
            current->state = PROCESS_STOPPED;
        }
        current = current->next;
    }
    (void)pthread_mutex_unlock(&supervisor->lock);
    logger_write("INFO", "Supervisor shutdown complete");
}

void supervisor_destroy(supervisor_t *supervisor)
{
    process_t *current;
    process_t *next;
    size_t index;

    if (supervisor == NULL) {
        return;
    }

    supervisor_stop_all(supervisor);
    current = supervisor->head;
    while (current != NULL) {
        next = current->next;
        for (index = 0U; index < current->argc; index++) {
            free(current->argv[index]);
        }
        for (index = 0U; index < current->dependency_count; index++) {
            free(current->dependencies[index]);
        }
        free(current->dependencies);
        free(current);
        current = next;
    }
    supervisor->head = NULL;
    logger_close();
    (void)pthread_mutex_destroy(&supervisor->lock);
}
