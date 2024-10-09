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


int handle_pipe(char *cmd_left[], char *cmd_right[]) {
    int pipe_fd[2];
    pid_t pid1, pid2;

    // create a pipe 
    if (pipe(pipe_fd) == -1) {
        perror("Pipe failed");
        return -1;
    }
    // first child process to execute cmd1
    if (( pid1 == fork()) == 0) {
        
        dup2(pipe_fd[1], STDOUT_FILENO); // redirect stdout to pipe write end
        close(pipe_fd[0]); // close read end 
        close(pipe_fd[1]);
        execvp(cmd_left[0], cmd_left); // close read end 
        perror("Execution failed for the left command");
        exit(EXIT_FAILURE);
    }
    // second child process to execute cmd1
    if (( pid2 == fork()) == 0) {
        dup2(pipe_fd[0], STDIN_FILENO); // redirect stdout to pipe write end
        close(pipe_fd[1]); // close read end 
        close(pipe_fd[0]);
        execvp(cmd_right[0], cmd_right); // close read end 
        perror("Execution failed for the right command");
        exit(EXIT_FAILURE);
    }
    // parent closes pipe and waits for both children to finish 
    close(pipe_fd[0]);
    close(pipe_fd[1]);

    waitpid(pid1, NULL, 0);
    waitpid(pid2, NULL, 0);

    return 0;

}

