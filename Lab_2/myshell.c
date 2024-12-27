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
#include <fcntl.h>  



typedef struct process {
    cmdLine* cmd;         
    pid_t pid;           
    int status;           
    struct process* next;
} process;

#define TERMINATED -1
#define RUNNING 1
#define SUSPENDED 0
#define HISTLEN 10  // Maximum number of history entries

typedef struct history_entry {
    char *command_line;                // Pointer to the command line string
    struct history_entry *next;        // Pointer to the next history entry
} history_entry;
history_entry *history_head = NULL;  // Pointer to the head (oldest entry)
history_entry *history_tail = NULL;  // Pointer to the tail (newest entry)
int history_size = 0;                // Current size of the history list

void add_history_entry(const char *command_line);
void print_history();
char* get_history_command(int index);
void clear_history();

//all the implementations
void clear_history() {
    history_entry *current = history_head;
    while (current != NULL) {
        history_entry *temp = current;
        current = current->next;
        free(temp->command_line);
        free(temp);
    }
    history_head = NULL;
    history_tail = NULL;
    history_size = 0;
}
char* get_history_command(int index) {
    if (index < 1 || index > history_size) {
        fprintf(stderr, "Error: Invalid history index.\n");
        return NULL;
    }
    history_entry *current = history_head;
    int current_index = 1;
    while (current != NULL && current_index < index) {
        current = current->next;
        current_index++;
    }
    if (current != NULL) {
        return current->command_line;
    } else {
        fprintf(stderr, "Error: Command not found in history.\n");
        return NULL;
    }
}
void print_history() {
    history_entry *current = history_head;
    int index = 1;
    while (current != NULL) {
        printf("%d %s\n", index, current->command_line);
        current = current->next;
        index++;
    }
}
void add_history_entry(const char *command_line) {
    // Allocate memory for the new history entry
    history_entry *new_entry = (history_entry*)malloc(sizeof(history_entry));
    if (new_entry == NULL) {
        perror("malloc");
        exit(1);
    }
    
    // Allocate memory for the command line and copy it
    new_entry->command_line = strdup(command_line);
    if (new_entry->command_line == NULL) {
        perror("strdup");
        free(new_entry);
        exit(1);
    }
    new_entry->next = NULL;

    // Add the new entry to the end of the list
    if (history_tail == NULL) {
        // History list is empty
        history_head = new_entry;
        history_tail = new_entry;
    } else {
        history_tail->next = new_entry;
        history_tail = new_entry;
    }
    
    // Increment the history size
    history_size++;

    // If the history size exceeds HISTLEN, remove the oldest entry
    if (history_size > HISTLEN) {
        history_entry *temp = history_head;
        history_head = history_head->next;
        free(temp->command_line);
        free(temp);
        history_size--;
    }
}

process* process_list = NULL;
void freeProcessList(process* process_list);




void addProcess(process** process_list, cmdLine* cmd, pid_t pid) {
    process* new_process = (process*)malloc(sizeof(process));
    if (new_process == NULL) {
        perror("malloc");
        exit(1);
    }
    new_process->cmd = cmd;  // Store the original cmdLine
    new_process->pid = pid;
    new_process->status = RUNNING;
    new_process->next = NULL;

    if (*process_list == NULL) {
        *process_list = new_process;
    } else {
        process* temp = *process_list;
        while (temp->next != NULL)
            temp = temp->next;
        temp->next = new_process;
    }
}
// void updateProcessList(process **process_list) {
//     process* temp = *process_list;
//     int status;
//     while (temp != NULL) {
//         pid_t result = waitpid(temp->pid, &status, WNOHANG | WUNTRACED | WCONTINUED);
//         if (result != 0) {
//             if (WIFEXITED(status) || WIFSIGNALED(status)) {
//                 temp->status = TERMINATED;
//             } else if (WIFSTOPPED(status)) {
//                 temp->status = SUSPENDED;
//             } else if (WIFCONTINUED(status)) {
//                 temp->status = RUNNING;
//             }
//         }
//         temp = temp->next;
//     }
// }
void updateProcessList(process **process_list) {
    process* temp = *process_list;
    int status;
    while (temp != NULL) {
        pid_t result = waitpid(temp->pid, &status, WNOHANG | WUNTRACED | WCONTINUED);
        if (result == -1) {
            // An error occurred with waitpid
            if (errno == ECHILD) {
                // The child process does not exist. Mark it as terminated.
                temp->status = TERMINATED;
            } else {
                perror("waitpid");
            }
        } else if (result > 0) {
            // The status of the child process has changed
            if (WIFEXITED(status) || WIFSIGNALED(status)) {
                temp->status = TERMINATED;
            } else if (WIFSTOPPED(status)) {
                temp->status = SUSPENDED;
            } else if (WIFCONTINUED(status)) {
                temp->status = RUNNING;
            } else if (WIFEXITED(status)) {
                temp->status = TERMINATED;
            }
        }
        // If result == 0, no status change has occurred; do nothing.
        temp = temp->next;
    }
}

void printProcessList(process** process_list) {
    updateProcessList(process_list);
    printf("Index\tPID\tStatus\t\tCommand\n");
    process* temp = *process_list;
    int index = 0;
    process* prev = NULL;

    while (temp != NULL) {
        char* status_str;
        switch (temp->status) {
            case RUNNING:
                status_str = "Running";
                break;
            case SUSPENDED:
                status_str = "Suspended";
                break;
            case TERMINATED:
                status_str = "Terminated";
                break;
            default:
                status_str = "Unknown";
        }

        printf("%d\t%d\t%s\t%s\n", index, temp->pid, status_str, temp->cmd->arguments[0]);
        
        if (temp->status == TERMINATED) {
            if (prev == NULL) {
                *process_list = temp->next;
            } else {
                prev->next = temp->next;
            }
            freeCmdLines(temp->cmd);
            free(temp);
            temp = (prev == NULL) ? *process_list : prev->next;
        } else {
            prev = temp;
            temp = temp->next;
            index++;
        }
    }
}

void updateProcessStatus(process* process_list, int pid, int status) {
    while (process_list != NULL) {
        if (process_list->pid == pid) {
            process_list->status = status;
            break;
        }
        process_list = process_list->next;
    }
}

int execute(cmdLine* com, int flag);

struct shellcmd {char* cmd;
                 int num;
                 };

struct shellcmd ops[] = {
    {"cd", 1},
    {"stop", 2},
    {"wake", 3},
    {"term", 4}};
void freeProcessList(process* process_list) {
    process* temp;
    while (process_list != NULL) {
        temp = process_list;
        process_list = process_list->next;
        freeCmdLines(temp->cmd); // Free the cmdLine structure
        free(temp);              // Free the process structure
    }
}

int main(int argc, char **argv)
{
    int debugFlag = 0;
    char path[PATH_MAX];
    cmdLine *command = NULL; 
    char userInput[2048];

    // Parse command-line arguments
    for (int i = 0; i < argc; i++) {
        if (strcmp(argv[i], "-d") == 0) {
            debugFlag = 1;
        }
    }

    // Main loop
    while (1)
    {
        // Print the prompt
        if (getcwd(path, sizeof(path)) != NULL) {
            printf("\n%s> ", path);
        } else {
            perror("getcwd");
            exit(1);
        }

        // Read user input
        if (fgets(userInput, sizeof(userInput), stdin) == NULL) {
            break; // EOF encountered
        }

        // Remove newline character from userInput
        userInput[strcspn(userInput, "\n")] = '\0';

        if (strcmp(userInput, "") == 0) {
            continue; // Empty input, prompt again
        }

        // Check for 'quit' command
        if (strcmp(userInput, "quit") == 0) {
            break;
        }
        // Handle 'history' command
        if (strcmp(userInput, "history") == 0) {
            print_history();
            continue;
        }
        // Handle '!!' command
        if (strcmp(userInput, "!!") == 0) {
            if (history_tail == NULL) {
                fprintf(stderr, "No commands in history.\n");
                continue;
            }
            // Get the last command from history
            char *last_command = history_tail->command_line;
            printf("%s\n", last_command);  // Echo the command
            // Add to history and execute
            add_history_entry(last_command);
            command = parseCmdLines(last_command);
            if (command == NULL) {
                continue; // Parsing failed, prompt again
            }
            // Proceed to execute the command
        } 
        // Handle '!n' command
        else if (userInput[0] == '!' && isdigit(userInput[1])) {
            int index = atoi(&userInput[1]);
            char *history_command = get_history_command(index);
            if (history_command == NULL) {
                // Error message already printed in get_history_command
                continue;
            }
            printf("%s\n", history_command);  // Echo the command
            // Add to history and execute
            add_history_entry(history_command);
            command = parseCmdLines(history_command);
            if (command == NULL) {
                continue; // Parsing failed, prompt again
            }
            // Proceed to execute the command
        } 
        // For all other commands
        else {
            // Add non-history commands to history
            add_history_entry(userInput);
            // Parse the command line
            command = parseCmdLines(userInput);
            if (command == NULL) {
                continue; // Parsing failed, prompt again
            }
        }
        // Parse the command line
        command = parseCmdLines(userInput);
        if (command == NULL) {
            continue; // Parsing failed, prompt again
        }

        // Handle built-in commands
        if (strcmp(command->arguments[0], "cd") == 0) {
            // 'cd' command
            if (command->argCount < 2) {
                fprintf(stderr, "cd: missing argument\n");
            } else if (chdir(command->arguments[1]) == -1) {
                perror("cd failed");
            }
        } else if (strcmp(command->arguments[0], "procs") == 0) {
            // 'procs' command
            printProcessList(&process_list);
        } else if (strcmp(command->arguments[0], "stop") == 0) {
            // 'stop' command
            if (command->argCount < 2) {
                fprintf(stderr, "stop: missing PID\n");
            } else {
                int pid = atoi(command->arguments[1]);
                if (kill(pid, SIGTSTP) == -1) {
                    perror("kill");
                } else {
                    updateProcessStatus(process_list, pid, SUSPENDED);
                }
            }
        } else if (strcmp(command->arguments[0], "wake") == 0) {
            // 'wake' command
            if (command->argCount < 2) {
                fprintf(stderr, "wake: missing PID\n");
            } else {
                int pid = atoi(command->arguments[1]);
                if (kill(pid, SIGCONT) == -1) {
                    perror("kill");
                } else {
                    updateProcessStatus(process_list, pid, RUNNING);
                }
            }
        } else if (strcmp(command->arguments[0], "term") == 0) {
            // 'term' command
            if (command->argCount < 2) {
                fprintf(stderr, "term: missing PID\n");
            } else {
                int pid = atoi(command->arguments[1]);
                if (kill(pid, SIGINT) == -1) {
                    perror("kill");
                } else {
                    updateProcessStatus(process_list, pid, TERMINATED);
                }
            }
        } else {
            // External command
            if (execute(command, debugFlag) != 0) {
                perror("Execution failed");
            }
        }

        // Free the parsed command line
        //freeCmdLines(command);
        //command = NULL;
    }

    // Clean up process list
    freeProcessList(process_list);
    clear_history();

    return 0;
}


int execute(cmdLine *com, int flag) {
    if (com == NULL) {
        return -1;
    }

    // Check if the command contains a pipe
    if (com->next != NULL) {
        // Handle pipeline command

        // Check for invalid I/O redirections
        if (com->outputRedirect != NULL) {
            fprintf(stderr, "Error: Output redirection on left-hand command is invalid with a pipeline.\n");
            return -1;
        }
        if (com->next->inputRedirect != NULL) {
            fprintf(stderr, "Error: Input redirection on right-hand command is invalid with a pipeline.\n");
            return -1;
        }

        int pipefd[2];
        if (pipe(pipefd) == -1) {
            perror("pipe");
            return -1;
        }

        pid_t pid1 = fork();
        if (pid1 == -1) {
            perror("fork");
            return -1;
        }

        if (pid1 == 0) {
            // Child process 1: Execute left-hand side command
            // Redirect STDOUT to pipe write end
            close(pipefd[0]); // Close unused read end

            if (dup2(pipefd[1], STDOUT_FILENO) == -1) {
                perror("dup2");
                _exit(1);
            }
            close(pipefd[1]); // Close duplicate fd

            // Handle input redirection if any
            if (com->inputRedirect != NULL) {
                int fd_in = open(com->inputRedirect, O_RDONLY);
                if (fd_in < 0) {
                    perror("open");
                    _exit(1);
                }
                if (dup2(fd_in, STDIN_FILENO) == -1) {
                    perror("dup2");
                    _exit(1);
                }
                close(fd_in);
            }

            // Execute the command
            if (execvp(com->arguments[0], com->arguments) == -1) {
                perror("execvp");
                _exit(1);
            }
        }

        // Parent process continues
        pid_t pid2 = fork();
        if (pid2 == -1) {
            perror("fork");
            return -1;
        }

        if (pid2 == 0) {
            // Child process 2: Execute right-hand side command
            // Redirect STDIN from pipe read end
            close(pipefd[1]); // Close unused write end

            if (dup2(pipefd[0], STDIN_FILENO) == -1) {
                perror("dup2");
                _exit(1);
            }
            close(pipefd[0]); // Close duplicate fd

            // Handle output redirection if any
            if (com->next->outputRedirect != NULL) {
                int fd_out = open(com->next->outputRedirect, O_WRONLY | O_CREAT | O_TRUNC, 0644);
                if (fd_out < 0) {
                    perror("open");
                    _exit(1);
                }
                if (dup2(fd_out, STDOUT_FILENO) == -1) {
                    perror("dup2");
                    _exit(1);
                }
                close(fd_out);
            }

            // Execute the command
            if (execvp(com->next->arguments[0], com->next->arguments) == -1) {
                perror("execvp");
                _exit(1);
            }
        }

        // Parent process continues
        // Close pipe ends
        close(pipefd[0]);
        close(pipefd[1]);

        if (flag) {
            fprintf(stderr, "PID 1: %d\n", pid1);
            fprintf(stderr, "Command 1: %s\n", com->arguments[0]);
            fprintf(stderr, "PID 2: %d\n", pid2);
            fprintf(stderr, "Command 2: %s\n", com->next->arguments[0]);
        }

        if (com->blocking == 1) {
            // Foreground execution: Wait for both child processes
            waitpid(pid1, NULL, 0);
            waitpid(pid2, NULL, 0);

            // Free the cmdLine structures since they are no longer needed
            freeCmdLines(com);
        } else {
            // Background execution: Do not wait, add processes to process list
            addProcess(&process_list, com, pid1);
            addProcess(&process_list, com->next, pid2);
            // Do not free 'com' here; it will be freed when the process is removed from the process list
        }

    } else {
        // Single command execution
        pid_t pid = fork();
        if (pid == -1) {
            perror("fork");
            return -1;
        }

        if (pid == 0) {
            // Child process
            // Handle input redirection if any
            if (com->inputRedirect != NULL) {
                int fd_in = open(com->inputRedirect, O_RDONLY);
                if (fd_in < 0) {
                    perror("open");
                    _exit(1);
                }
                if (dup2(fd_in, STDIN_FILENO) == -1) {
                    perror("dup2");
                    _exit(1);
                }
                close(fd_in);
            }

            // Handle output redirection if any
            if (com->outputRedirect != NULL) {
                int fd_out = open(com->outputRedirect, O_WRONLY | O_CREAT | O_TRUNC, 0644);
                if (fd_out < 0) {
                    perror("open");
                    _exit(1);
                }
                if (dup2(fd_out, STDOUT_FILENO) == -1) {
                    perror("dup2");
                    _exit(1);
                }
                close(fd_out);
            }

            // Execute the command
            if (execvp(com->arguments[0], com->arguments) == -1) {
                perror("execvp");
                _exit(1);
            }
        }

        // Parent process
        if (flag) {
            fprintf(stderr, "PID: %d\n", pid);
            fprintf(stderr, "Command: %s\n", com->arguments[0]);
        }

        if (com->blocking == 1) {
            // Foreground execution: Wait for the child process
            waitpid(pid, NULL, 0);

            // Free the cmdLine structure since it is no longer needed
            freeCmdLines(com);
        } else {
            // Background execution: Do not wait, add process to process list
            addProcess(&process_list, com, pid);
            // Do not free 'com' here; it will be freed when the process is removed from the process list
        }
    }

    return 0;
}
