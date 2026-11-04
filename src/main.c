#define _DEFAULT_SOURCE
#define _POSIX_C_SOURCE 200809L
#include "supervisor.h"
#include "procmon.h"
#include "logger.h"

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <sys/stat.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <unistd.h>

static void handle_signal(int signal_number)
{
    (void)signal_number;
    supervisor_signal_received = 1;
}

typedef struct {
    int read_fd;
    unsigned int timeout_ms;
} watchdog_context_t;

static void *watchdog_thread(void *argument)
{
    watchdog_context_t *context = argument;
    unsigned char buffer[64];

    if (context == NULL) {
        return NULL;
    }

    while (supervisor_stop_requested_load() == 0) {
        struct pollfd descriptor;
        int poll_result;
        descriptor.fd = context->read_fd;
        descriptor.events = POLLIN;
        descriptor.revents = 0;
        poll_result = poll(&descriptor, 1U, (int)context->timeout_ms);
        if (poll_result > 0) {
            if ((descriptor.revents & POLLIN) != 0) {
                ssize_t bytes_read = read(context->read_fd, buffer, sizeof(buffer));
                if (bytes_read < 0) {
                    if ((errno != EINTR) && (errno != EAGAIN) &&
                        (errno != EWOULDBLOCK)) {
                        logger_write("ERROR", "Watchdog pipe read failed: %s", strerror(errno));
                        supervisor_stop_requested_store(1);
                        break;
                    }
                }
            }
        } else if (poll_result == 0) {
            if (supervisor_stop_requested_load() == 0) {
                logger_write("ERROR", "Supervisor heartbeat timeout; watchdog requested shutdown");
                supervisor_stop_requested_store(1);
            }
            break;
        } else if (errno != EINTR) {
            logger_write("ERROR", "Watchdog poll failed");
            supervisor_stop_requested_store(1);
            break;
        }
    }
    return NULL;
}

static int install_signal_handlers(void)
{
    struct sigaction action;

    (void)memset(&action, 0, sizeof(action));
    action.sa_handler = handle_signal;
    if (sigemptyset(&action.sa_mask) != 0) {
        return -1;
    }
    if ((sigaction(SIGINT, &action, NULL) != 0) ||
        (sigaction(SIGTERM, &action, NULL) != 0) ||
        (sigaction(SIGHUP, &action, NULL) != 0)) {
        return -1;
    }
    return 0;
}
/* cppcheck-suppress constParameter */
int main(int argc, char *argv[])
{
    supervisor_t supervisor;
    procmon_context_t monitor_context;
    pthread_t monitor_thread;
    pthread_t watchdog;
    bool monitor_started = false;
    bool watchdog_started = false;
    int heartbeat_pipe[2] = {-1, -1};
    watchdog_context_t watchdog_context;
    const char *config_path = "config/services.conf";
    const char *log_path = "logs/supervisor.log";

    if ((argc > 3) || ((argc == 3) && (strcmp(argv[1], "--daemon") != 0))) {
        (void)fprintf(stderr, "Usage: %s [--daemon] [configuration-file]\n", argv[0]);
        return EXIT_FAILURE;
    }
    if ((argc == 2) && (strcmp(argv[1], "--daemon") == 0)) {
        if (daemon(1, 0) != 0) { perror("daemon"); return EXIT_FAILURE; }
    } else if (argc == 2) {
        config_path = argv[1];
    } else if (argc == 3) {
        config_path = argv[2];
        if (daemon(1, 0) != 0) { perror("daemon"); return EXIT_FAILURE; }
    }

    if (install_signal_handlers() != 0) {
        (void)fprintf(stderr, "Failed to install signal handlers\n");
        return EXIT_FAILURE;
    }
 
    if ((mkdir("logs", 0755) != 0) && (errno != EEXIST)) {
        perror("Failed to create logs directory");
        return EXIT_FAILURE;
    }

    if (supervisor_init(&supervisor, log_path) != 0) {
        (void)fprintf(stderr, "Failed to initialize supervisor/log file\n");
        return EXIT_FAILURE;
    }

    if (supervisor_load_config(&supervisor, config_path) != 0) {
        (void)fprintf(stderr, "Failed to load configuration: %s\n",
                      config_path);
        supervisor_destroy(&supervisor);
        return EXIT_FAILURE;
    }

    (void)printf("Starting ADAS Process Supervisor...\n");
    if (supervisor_start_all(&supervisor) != 0) {
        logger_write("WARN", "One or more services could not be started initially");
    }

    if (pipe(heartbeat_pipe) == 0) {
        int flags = fcntl(heartbeat_pipe[1], F_GETFL, 0);
        int read_flags = fcntl(heartbeat_pipe[0], F_GETFL, 0);
        if (flags >= 0) { (void)fcntl(heartbeat_pipe[1], F_SETFL, flags | O_NONBLOCK); }
        if (read_flags >= 0) { (void)fcntl(heartbeat_pipe[0], F_SETFL, read_flags | O_NONBLOCK); }
        supervisor.heartbeat_fd = heartbeat_pipe[1];
        watchdog_context.read_fd = heartbeat_pipe[0];
        watchdog_context.timeout_ms = 7000U;
        if (pthread_create(&watchdog, NULL, watchdog_thread, &watchdog_context) == 0) {
            watchdog_started = true;
        } else {
            logger_write("WARN", "Watchdog thread could not be created");
        }
    } else {
        logger_write("WARN", "Heartbeat pipe could not be created; watchdog disabled");
    }

    monitor_context.supervisor = &supervisor;
    monitor_context.interval_seconds = 3U;
    if (pthread_create(&monitor_thread, NULL, procmon_thread,
                       &monitor_context) == 0) {
        monitor_started = true;
    } else {
        logger_write("WARN", "Resource monitor thread could not be created");
    }

    supervisor_run(&supervisor);

    supervisor_mark_shutdown(&supervisor);
    if (heartbeat_pipe[1] >= 0) {
        const unsigned char shutdown_notice = 0x5AU;
        ssize_t bytes_written = write(heartbeat_pipe[1], &shutdown_notice,
                                      sizeof(shutdown_notice));
        if ((bytes_written < 0) && (errno != EINTR) &&
            (errno != EAGAIN) && (errno != EWOULDBLOCK)) {
            logger_write("WARN", "Unable to send watchdog shutdown notice: %s",
                         strerror(errno));
        }
    }
    if (monitor_started) {
        (void)pthread_join(monitor_thread, NULL);
    }
    if (watchdog_started) {
        (void)pthread_join(watchdog, NULL);
    }
    if (heartbeat_pipe[0] >= 0) { (void)close(heartbeat_pipe[0]); }
    if (heartbeat_pipe[1] >= 0) { (void)close(heartbeat_pipe[1]); }
    supervisor.heartbeat_fd = -1;
    supervisor_destroy(&supervisor);
    (void)printf("Supervisor stopped.\n");
    return EXIT_SUCCESS;
}
