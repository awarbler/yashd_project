// including standard libraries for various functionalitites for the client
#include <stdio.h> // standard input output library 
#include <stdlib.h> // standard library includes functions malloc() free() 
#include <unistd.h>         // posix api for close () 
#include <errno.h>          // error handling macros
#include <string.h> 
#include <sys/wait.h>


/**
 * @brief detects if a command has a pipe and splits it into two commands 

 * @return int returns 1 if a pip is found nd command is split otherwise 0
 */


int has_pipe(const char *command){
    return strchr(command, '|') != NULL; // return 1 if pipe i found else 0
}

void handle_pipe(const char *command) {
    int pipe_fd[2];
    pid_t pid1, pid2;
    char *cmd1, *cmd2;

    // split command by the pipe
    cmd1 = strtok(command, "|");
    cmd2 = strtok(NULL, "|");

    if (!cmd1 || !cmd2) {
        fprintf(stderr, "Invalid pipe command.\n");
        return -1;
    }

    // trim leading and trailing space for both commands
    cmd1 = strtok(cmd1, " \t\n");
    cmd2 = strtok(cmd2, " \t\n");

    // create a pipe 
    if (pipe(pipe_fd) == -1) {
        perror("Pipe failed");
        return -1;
    }
    // first child process to execute cmd1
    if (( pid1 == fork()) == 0) {
        close(pipe_fd[0]); // close read end 
        dup2(pipe_fd[1], STDOUT_FILENO); // redirect stdout to pipe write end
        close(pipe_fd[1]);

        // execute first command
        execlp(cmd1, cmd1, NULL);
        perror("Execution failed");
        exit(EXIT_FAILURE);
    }
    // second child process to execute cmd1
    if (( pid2 == fork()) == 0) {
        close(pipe_fd[1]); // close read end 
        dup2(pipe_fd[0], STDOUT_FILENO); // redirect stdout to pipe write end
        close(pipe_fd[0]);

        // execute first command
        execlp(cmd2, cmd2, NULL);
        perror("Execution failed");
        exit(EXIT_FAILURE);
    }
    // parent closes pipe and waits for both children to finish 
    close(pipe_fd[0]);
    close(pipe_fd[1]);

    waitpid(pid1, NULL, 0);
    waitpid(pid2, NULL, 0);

    return 0;

}

