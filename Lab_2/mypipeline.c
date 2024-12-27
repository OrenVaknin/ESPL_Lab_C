#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <linux/limits.h>
#include <errno.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>

int main (int argc, char** argv) {
    int pipefd[2];
    pid_t c1, c2;
    pipe(pipefd);
    
    fprintf(stderr, "parent_process>forking...\n");
    c1 = fork();
    
    if (c1 != 0) {
        fprintf(stderr, "parent_process>created process with id: %d\n", c1);
    }
    if (c1 == 0) { //child process
        fprintf(stderr, "child1>redirecting stdout to the write end of the pipe\n");
        close(STDOUT_FILENO);
        dup(pipefd[1]);
        close(pipefd[1]);
        fprintf(stderr, "child1>going to execute cmd: ls -l\n");
        if (execlp("ls", "ls", "-l", NULL) == -1) {
            fprintf(stderr, "ls execution has failed.\n");
            _exit(EXIT_FAILURE);
        }
    }
    else { //parent process
        fprintf(stderr, "parent_process>closing the write end of the pipe...\n");
        close(pipefd[1]);
        c2 = fork();
        if (c2 == 0) { //second child process
            fprintf(stderr, "child2>redirecting stdin to the read end of the pipe\n");
            close(STDIN_FILENO);
            dup(pipefd[0]);
            close(pipefd[0]);
            fprintf(stderr, "child2>going to execute command: tail -n 2\n");
            if (execlp("tail", "tail", "-n", "2", NULL) == -1) {
                fprintf(stderr, "tail execution has failed.\n");
                _exit(EXIT_FAILURE);
            }
        }
        else { //parent process
            fprintf(stderr, "parent_process>closing the read end of the pipe...\n");
            close(pipefd[0]);
            fprintf(stderr, "parent_process>waiting for child processes to terminate...\n");
            waitpid(c1, NULL, 0);
            waitpid(c2, NULL, 0);
            fprintf(stderr, "parent_process>exiting...\n");
            _exit(EXIT_SUCCESS);
        }

    }
}