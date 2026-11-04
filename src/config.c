#define _POSIX_C_SOURCE 200809L
#include "supervisor.h"
#include "logger.h"

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *trim(char *value)
{
    char *end;
    while (isspace((unsigned char)*value) != 0) { value++; }
    end = value + strlen(value);
    while ((end > value) && (isspace((unsigned char)end[-1]) != 0)) { end--; }
    *end = '\0';
    return value;
}

static int parse_unsigned(const char *text, unsigned int *value)
{
    char *end = NULL;
    unsigned long parsed;
    errno = 0;
    parsed = strtoul(text, &end, 10);
    if ((errno != 0) || (end == text) || (*trim(end) != '\0') ||
        (parsed > 100000UL)) { return -1; }
    *value = (unsigned int)parsed;
    return 0;
}

static void free_process(process_t *process)
{
    size_t i;
    if (process == NULL) { return; }
    for (i = 0U; i < process->argc; i++) { free(process->argv[i]); }
    for (i = 0U; i < process->dependency_count; i++) { free(process->dependencies[i]); }
    free(process->dependencies);
    free(process);
}

/* Supports single/double quoted tokens and backslash-escaped characters. */
static int split_command(process_t *process, const char *command)
{
    size_t pos = 0U;
    size_t length = strlen(command);
    while (pos < length) {
        char token[SUPERVISOR_COMMAND_MAX];
        size_t used = 0U;
        char quote = '\0';
        bool started = false;
        while ((pos < length) && (isspace((unsigned char)command[pos]) != 0)) { pos++; }
        while (pos < length) {
            char ch = command[pos];
            if ((quote == '\0') && (isspace((unsigned char)ch) != 0)) { break; }
            if ((ch == '\\') && (pos + 1U < length)) {
                pos++;
                ch = command[pos];
            } else if ((ch == '\'') || (ch == '"')) {
                if (quote == '\0') { quote = ch; started = true; pos++; continue; }
                if (quote == ch) { quote = '\0'; pos++; continue; }
            }
            if (used + 1U >= sizeof(token)) { return -1; }
            token[used++] = ch;
            started = true;
            pos++;
        }
        if (quote != '\0') { return -1; }
        if (started) {
            if (process->argc >= SUPERVISOR_MAX_ARGS) { return -1; }
            token[used] = '\0';
            process->argv[process->argc] = strdup(token);
            if (process->argv[process->argc] == NULL) { return -1; }
            process->argc++;
        }
    }
    process->argv[process->argc] = NULL;
    return (process->argc > 0U) ? 0 : -1;
}

static int parse_dependencies(process_t *process, char *field)
{
    char *save = NULL;
    char *token;
    field = trim(field);
    if ((*field == '\0') || (strcmp(field, "-") == 0)) { return 0; }
    token = strtok_r(field, ",", &save);
    while (token != NULL) {
        const char *name = trim(token);
        char **grown;
        if ((*name == '\0') || (strlen(name) >= SUPERVISOR_NAME_MAX) ||
            (strcmp(name, process->name) == 0)) { return -1; }
        grown = realloc(process->dependencies,
                        (process->dependency_count + 1U) * sizeof(*grown));
        if (grown == NULL) { return -1; }
        process->dependencies = grown;
        process->dependencies[process->dependency_count] = strdup(name);
        if (process->dependencies[process->dependency_count] == NULL) { return -1; }
        process->dependency_count++;
        token = strtok_r(NULL, ",", &save);
    }
    return 0;
}

static process_t *find_unlocked(supervisor_t *supervisor, const char *name)
{
    process_t *item = supervisor->head;
    while ((item != NULL) && (strcmp(item->name, name) != 0)) { item = item->next; }
    return item;
}

static int visit_dependency(supervisor_t *supervisor, process_t *item,
                            process_t **stack, size_t *depth, size_t total)
{
    size_t i;
    if (item->state == PROCESS_WAITING_DEPENDENCY) { return -1; }
    if (item->state == PROCESS_RUNNING) { return 0; }
    if (*depth >= total) { return -1; }
    item->state = PROCESS_WAITING_DEPENDENCY;
    stack[(*depth)++] = item;
    for (i = 0U; i < item->dependency_count; i++) {
        process_t *dep = find_unlocked(supervisor, item->dependencies[i]);
        if ((dep == NULL) || (visit_dependency(supervisor, dep, stack, depth, total) != 0)) {
            return -1;
        }
    }
    item->state = PROCESS_STOPPED;
    (*depth)--;
    return 0;
}

static int validate_dependencies(supervisor_t *supervisor)
{
    process_t *item;
    process_t **stack;
    size_t total = 0U;
    int result = 0;
    for (item = supervisor->head; item != NULL; item = item->next) { total++; item->state = PROCESS_STOPPED; }
    stack = calloc(total, sizeof(*stack));
    if ((stack == NULL) && (total > 0U)) { return -1; }
    for (item = supervisor->head; item != NULL; item = item->next) {
        size_t i;
        for (i = 0U; i < item->dependency_count; i++) {
            if (find_unlocked(supervisor, item->dependencies[i]) == NULL) {
                logger_write("ERROR", "Unknown dependency '%s' for '%s'",
                             item->dependencies[i], item->name);
                result = -1;
            }
        }
    }
    if (result == 0) {
        for (item = supervisor->head; item != NULL; item = item->next) {
            process_t *reset;
            for (reset = supervisor->head; reset != NULL; reset = reset->next) {
                reset->state = PROCESS_STOPPED;
            }
            size_t depth = 0U;
            if (visit_dependency(supervisor, item, stack, &depth, total) != 0) {
                logger_write("ERROR", "Dependency cycle detected at '%s'", item->name);
                result = -1;
                break;
            }
        }
    }
    for (item = supervisor->head; item != NULL; item = item->next) { item->state = PROCESS_STOPPED; }
    free(stack);
    return result;
}

int supervisor_load_config(supervisor_t *supervisor, const char *path)
{
    FILE *file;
    char *line = NULL;
    size_t capacity = 0U;
    ssize_t length;
    unsigned int line_number = 0U;
    int result = 0;
    if ((supervisor == NULL) || (path == NULL)) { return -1; }
    file = fopen(path, "r");
    if (file == NULL) { logger_write("ERROR", "Cannot open config: %s", path); return -1; }
    while ((length = getline(&line, &capacity, file)) >= 0) {
        char *fields[5];
        char *cursor;
        char *separator;
        unsigned int count = 1U;
        process_t *process;
        const process_t *existing;
        (void)length;
        line_number++;
        cursor = trim(line);
        if ((*cursor == '\0') || (*cursor == '#')) { continue; }
        fields[0] = cursor;
        while (((separator = strchr(cursor, '|')) != NULL) && (count < 5U)) {
            *separator = '\0';
            cursor = separator + 1;
            fields[count++] = cursor;
        }
        if ((count != 5U) || (strchr(fields[4], '|') != NULL) ||
            (strlen(trim(fields[0])) >= SUPERVISOR_NAME_MAX) ||
            (strlen(trim(fields[4])) >= SUPERVISOR_COMMAND_MAX) ||
            (*trim(fields[0]) == '\0')) {
            logger_write("ERROR", "Invalid config syntax at line %u", line_number);
            result = -1; break;
        }
        process = calloc(1U, sizeof(*process));
        if (process == NULL) { result = -1; break; }
        (void)snprintf(process->name, sizeof(process->name), "%s", trim(fields[0]));
        (void)snprintf(process->command, sizeof(process->command), "%s", trim(fields[4]));
        if ((parse_unsigned(trim(fields[1]), &process->max_restarts) != 0) ||
            (parse_unsigned(trim(fields[2]), &process->restart_delay) != 0) ||
            (parse_dependencies(process, fields[3]) != 0) ||
            (split_command(process, process->command) != 0)) {
            logger_write("ERROR", "Invalid process definition at line %u", line_number);
            free_process(process); result = -1; break;
        }
        process->pid = -1;
        process->state = PROCESS_STOPPED;
        (void)pthread_mutex_lock(&supervisor->lock);
        existing = find_unlocked(supervisor, process->name);
        if (existing != NULL) {
            (void)pthread_mutex_unlock(&supervisor->lock);
            logger_write("ERROR", "Duplicate process name '%s'", process->name);
            free_process(process); result = -1; break;
        }
        process->next = supervisor->head;
        supervisor->head = process;
        (void)pthread_mutex_unlock(&supervisor->lock);
    }
    free(line);
    if (ferror(file) != 0) { result = -1; }
    (void)fclose(file);
    if ((result == 0) && (validate_dependencies(supervisor) != 0)) { result = -1; }
    if (result == 0) { logger_write("INFO", "Configuration loaded successfully"); }
    return result;
}
