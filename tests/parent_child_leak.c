#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

static void create_leak(size_t bytes)
{
    char *memory = malloc(bytes);

    if (memory == NULL) {
        perror("malloc");
        exit(EXIT_FAILURE);
    }

    memory[0] = 'A';

    /* Intentionally do not free memory */
}

int main(void)
{
    pid_t pid = fork();

    if (pid < 0) {
        perror("fork");
        return EXIT_FAILURE;
    }

    if (pid == 0) {
        printf("CHILD PID = %ld\n", (long)getpid());
        fflush(stdout);

        create_leak(200);

        return EXIT_SUCCESS;
    }

    printf("PARENT PID = %ld\n", (long)getpid());
    fflush(stdout);

    create_leak(100);

    (void)waitpid(pid, NULL, 0);

    return EXIT_SUCCESS;
}