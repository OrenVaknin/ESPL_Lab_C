#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <linux/limits.h>
#include <errno.h>
#include <unistd.h>
#include "LineParser.h" 
#include <signal.h>
#include <sys/wait.h>

int main (int argc, char** argv) {
    int pipefd[2];
    pid_t pid;

    if (pipe(pipefd) == -1) {
        perror("pipe operation failed");
        _exit(1);
    }
    pid = fork();
    if (pid == 0) { //child process 
        write(pipefd[1], "hello", 5);
        close(pipefd[1]);
        close(pipefd[0]); // child process doesn't read any input.
        _exit(EXIT_SUCCESS);
    }
    else {
        char message[5];
        read(pipefd[0], message, 5);
        close(pipefd[0]);
        close(pipefd[1]);
        printf("\nMessage received from child is - %s\n", message);
        _exit(EXIT_SUCCESS);
    }
    return 0;
}