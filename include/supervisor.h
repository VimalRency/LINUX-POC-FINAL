#ifndef SUPERVISOR_H
#define SUPERVISOR_H

#ifdef __cplusplus
extern "C" {
#endif

#include <pthread.h>
#include <signal.h>
#include <stdbool.h>
#include <stddef.h>
#include <sys/types.h>
#include <time.h>

/* Set only by the process signal handler; read by the supervisor main thread. */
extern volatile sig_atomic_t supervisor_signal_received;

/* Thread-safe stop request shared by worker threads. */
int supervisor_stop_requested_load(void);
void supervisor_stop_requested_store(int value);

#define SUPERVISOR_MAX_ARGS 32U
#define SUPERVISOR_NAME_MAX 64U
#define SUPERVISOR_COMMAND_MAX 512U

typedef enum {
    PROCESS_STOPPED = 0,
    PROCESS_RUNNING,
    PROCESS_WAITING_RESTART,
    PROCESS_WAITING_DEPENDENCY,
    PROCESS_PERMANENTLY_FAILED
} process_state_t;

typedef struct process {
    char name[SUPERVISOR_NAME_MAX];
    char command[SUPERVISOR_COMMAND_MAX];
    char *argv[SUPERVISOR_MAX_ARGS + 1U];
    size_t argc;
    unsigned int max_restarts;
    unsigned int restart_delay;
    char **dependencies;
    size_t dependency_count;
    unsigned int restart_count;
    pid_t pid;
    process_state_t state;
    time_t next_restart;
    int last_exit_status;
    struct process *next;
} process_t;

typedef struct {
    process_t *head;
    pthread_mutex_t lock;
    bool shutting_down;
    int heartbeat_fd;
    char log_path[256];
} supervisor_t;

int supervisor_init(supervisor_t *supervisor, const char *log_path);
void supervisor_destroy(supervisor_t *supervisor);
int supervisor_load_config(supervisor_t *supervisor, const char *path);
int supervisor_start_all(supervisor_t *supervisor);
int supervisor_dependencies_ready(const supervisor_t *supervisor, const process_t *process);
void supervisor_run(supervisor_t *supervisor);
void supervisor_stop_all(supervisor_t *supervisor);
void supervisor_print_status(supervisor_t *supervisor);
void supervisor_mark_shutdown(supervisor_t *supervisor);
process_t *supervisor_find_process(supervisor_t *supervisor, const char *name);

#ifdef __cplusplus
}
#endif

#endif
