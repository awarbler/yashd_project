// This is yashShell_helpers.c
// Troubleshooting the server
// 1. Error binding name to stream socket: Address already in use 
// Run: lsof -i :3820
// Run: kill -9 <PID>

// basic functionality for connecting to the server

#include "yashd.h"
#include "yashShell.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h> // need for waitpid
#include <fcntl.h> // for open() 
#include <termio.h>
#include <unistd.h>

// Max length for input command or Max number of jobs running at the same time: 20
# define MAX_INPUT 200
# define MAX_ARGS 100
# define MAX_JOBS 20 
#define BUFFER_SIZE 1024    // buffer size for communication

typedef struct job {
    pid_t pid;
    int job_id;
    char *command;
    int is_running;
    int is_stopped;
    int is_background;
    char job_marker; // + or - or " "
} job_t;


job_t jobs[MAX_JOBS];
int job_count = 0;
pid_t fg_pid = -1;

// function prototypes

// Shared function Prototypes // Job Control and Piping 
void execute_command(char **cmd_args, char *original_cmd);
void handle_redirection(char **cmd_args);
void handle_pipe(char **cmd_args_left, char **cmd_args_right);//function to handle piping commands
void apply_redirections(char **cmd_args);
void remove_elements(char **args, int start, int count);
void add_job(pid_t pid, const char *command, int is_running, int is_background);
void update_job_markers(int current_job_index) ;
void print_jobs();
void fg_job(int job_id);
void bg_job(int job_id);

void yash_loop();
void remove_newLine(char *line);
void run_in_background(char **cmd_args);
void setup_signal_handlers();
void handle_control(int psd, char control);
extern void handle_command(char *command, int psd);

// main shell loop that runs the shells
void yash_loop() {

    char line[MAX_INPUT]; // buffer for storing input commands

    //har *args[MAX_INPUT]; //array to store the command and arguments 
    while (1) {
        
        printf("# "); // display the shell prompt 

        // get user input
        if (fgets(line, sizeof(line), stdin) == NULL) {
            // if ctrl-d is pressed (eof), exit the shell
            if(feof(stdin)){
                printf("\nExiting shell ... \n");
                break;
            } else {
                perror("fgets error");
                continue;
            }
        }

        // Remove the newline character from the input line 
        line[strcspn(line, "\n")] = 0;

        // check for invalid combinations lif fg & and fg | echo
        if ((strstr(line, "fg &") !=NULL) || (strstr(line, "fg | ") != NULL )|| (strstr(line, "bg &") !=NULL) || (strstr(line, "bg | ") != NULL)){ //invalid combination, move to next line
            continue; // skp to the next iteration if no command was entered
        }


        // Check if the user input is empty by pressing enter without typing anything 
        if(strlen(line) == 0){
            continue; // skip to the next iteration if no command was entered
        }

        // handle jobs command
        if (strcmp(line, "jobs") == 0) {
            print_jobs();
            continue;
        }

        // handle fg command 
        if(strncmp(line, "fg", 2) == 0){
            int job_id = -1;
            if(sscanf(line, "fg %d", &job_id) != 1) {
                job_id = -1; // no job id provided
            }
            fg_job(job_id);
            continue;
        }

        // handle bg command 
        if(strncmp(line, "bg", 2) == 0){
            int job_id = -1;
            if(sscanf(line, "bg %d", &job_id) != 1) {
                job_id = -1; // no job id provided
            }
            bg_job(job_id);
            continue;
        }

        // check if input starts with a special character (<|> & and skip it if it does 
        if (line[0] == '<' || line[0] == '>' || line[0] == '|' || line[0] == '&'){
            printf("\n");
            continue;
        }
        // Make a copy of the command line 
        char *original_cmd = strdup(line);

        // check for a pipe and 
        char *pipe_position = strchr(line, '|');
        if(pipe_position){
            *pipe_position = '\0'; // split the input at |
            char *cmd_left = line;
            char *cmd_right = pipe_position + 1;

            // Parse both sides of the pipe into arguments 
            char *args_left[MAX_ARGS];
            char *args_right[MAX_ARGS];

            int i = 0;
            char *token = strtok(line, " ");
            while (token != NULL && i < MAX_ARGS - 1)
            {
                
                args_left[i++] = token;
                token = strtok(NULL, " ");
            }
            args_left[i] = NULL; 
            
            i = 0;
            token = strtok(cmd_right, " ");
            while (token != NULL)
            {
                args_right[i++] = token;
                token = strtok(NULL, " ");
            }
            args_right[i] = NULL;
            handle_pipe(args_left, args_right);   
        } else {
            //split the input line into command arguments
            char *args[MAX_INPUT];
            char *token = strtok(line, " ");
            int i = 0 ;
            while (token != NULL)
            {
                
                args[i++] = token;
                token = strtok(NULL, " ");
            }
            args[i] = NULL; // null terminate the array of arguments 
            //execute the command
            execute_command(args, original_cmd);
        }
        // After command finishes , display the prompt again 
        free(original_cmd);
        fflush(stdout);
        //printf("# "); 
    }
}

// function to execute the command by creating a child process using fork()
void execute_command(char **cmd_args, char *original_cmd) {

    pid_t pid;
    //pid_t pid = fork(); // create a new process 
    int status;
    int is_background = 0; 
    
    if (cmd_args[0]== NULL) {
        return;
    }
    // Check if the last argument is & indicating a background job
    for (int i = 0; cmd_args[i] != NULL; i++) {
        if (strcmp(cmd_args[i], "&") == 0) {
            is_background = 1;
            cmd_args[i] = NULL; // Remove & from the argument
            break;
        }
    }

    pid = fork();
    
    if (pid ==0) {
        // child process
        setpgid(0,0); // Set a new process group with the child pid

        //handle_redirection(cmd_args);
        apply_redirections(cmd_args);
        
        if (execvp(cmd_args[0], cmd_args) == -1){
            printf("\n# "); //  print a newline for invalide commands
            exit(EXIT_FAILURE);
        } 

    } else if (pid < 0) {
        // fork failed 
        return;
    } else {
        // parent process 
        //setpgid(pid, pid); // set child process group id to its own pid 

        if (is_background) {
            
            add_job(pid, original_cmd, 1, 1);
            printf("[%d] %d\n", jobs[job_count - 1].job_id, jobs[job_count - 1].pid);
        } else {
            fg_pid = pid;
            add_job(pid, original_cmd, 1, 0);

            // put the process group in the foreground and wait for it
            //tcsetpgrp(STDIN_FILENO, pid);
            waitpid(pid, &status, WUNTRACED);

            fg_pid = -1; // -1 Clears the foreground process 

            // Return control of the terminal 
            //tcsetpgrp(STDIN_FILENO, getpgrp());

            // handles the case where the job was stopped 
            if (WIFSTOPPED(status)) {
                // If the child wa stopped, mark it as stopped 
                // So we are placing a marker 
                jobs[job_count -1].is_running = 0;
                jobs[job_count -1].is_stopped = 1;
                printf("\n[%d]+  Stopped  %s\n", jobs[job_count - 1].job_id, jobs[job_count - 1].command);

                //printf("\n[%d]  - stopped %s\n", job_count , cmd_args[0]);

            } else { 
                //fg_pid = -1; 
                //job_count--;
                // The job is completed, now remove it from the job list
                free(jobs[job_count - 1].command);
                job_count--;
                update_job_markers(job_count-1);


                
            }
        }
    }
}

// function to handle file redirection 
void handle_redirection(char **cmd_args){
    apply_redirections(cmd_args);
}
// function to handle piping
void handle_pipe(char **cmd_args_left, char **cmd_args_right){
    int pipe_fd[2];
    pid_t pid1, pid2;
    
    if (pipe(pipe_fd) == -1){
        //perror("yash");
        return;
    }

    pid1 = fork(); // left child process
    if ((pid1 = fork() == 0)) {
        
        // first child left side command like ls
        dup2(pipe_fd[1], STDOUT_FILENO); // redirect stdout to the pipe
        close(pipe_fd[0]);
        close(pipe_fd[1]);

        handle_redirection(cmd_args_left);

        if (execvp(cmd_args_left[0], cmd_args_left) == -1) {
            perror("execvp failed for the left pipe"); // debugging
            printf("\n# "); //  print a newline for invalide commands
            exit(EXIT_FAILURE);
            //return;
        } 
    }

    pid2 = fork(); // right side of the pipe child process
    if ((pid2 = fork() == 0)) {
        
        dup2(pipe_fd[0], STDIN_FILENO); // redirect stdout to the pipe
        // Close both ends of the pipe in the child 
        close(pipe_fd[1]);
        close(pipe_fd[0]);

        handle_redirection(cmd_args_left);

        if (execvp(cmd_args_right[0], cmd_args_right) == -1) {
            perror("execvp failed for the left pipe"); // debugging
            printf("\n# "); //  print a newline for invalide commands
            exit(EXIT_FAILURE);
            //return;
        } 
    }

    // parent process closes pipe and waits for children to finish 
    close(pipe_fd[0]);
    close(pipe_fd[1]);

    waitpid(pid1,NULL,0);
    waitpid(pid2,NULL,0);
}

// signal handler for sigtstp ctrl z 
void sigint_handler(int sig){
    if (fg_pid > 0) {
        // If there is a foreground process, send a sigint to it 
        kill(fg_pid, SIGINT);

    } else {
        write(STDOUT_FILENO, "\n# ", 3);
        // tcflush(STDIN_FILENO, TCIFLUSH);
        fflush(stdout); // ensure the prompt is displayed immediately after the message
    }   
}

// signal handler for sigtstp ctrl z 
void sigtstp_handler(int sig){
    if (fg_pid > 0) {
        // if there is a foreground process , send sigint to it 
        kill(fg_pid, SIGTSTP);

        fg_pid = -1; // clear the foreground process 
    } else {
        write(STDOUT_FILENO, "\n# ", 3);
        fflush(stdout);
    }
        
}
// signal handler for sigtstp ctrl z 
void sigchld_handler(int sig){
    int status;
    pid_t pid;
    // Clean up zombie child processes non-blocking wait
    while ((pid = waitpid(-1, &status, WNOHANG)) > 0){ 
        for(int i =0; i < job_count; i++) {
            if (jobs[i].pid == pid) {
                if (WIFEXITED(status) || WIFSIGNALED(status)) {
                    // print done message if needed
                    printf("\n[%d]+ Done %s\n", jobs[i].job_id, jobs[i].command);
                    // Remove the job from the job list 
                    free(jobs[i]. command);
                    for (int j = i; j < job_count - 1; j++) {
                        jobs[j] = jobs[j + 1];
                    }
                    job_count--;
                    update_job_markers(job_count -1);
                }
                break;
            }
        }
    }
}

void add_job(pid_t pid, const char *command, int is_running, int is_background){

    if (job_count < MAX_JOBS){ 

        jobs[job_count].pid = pid;
        jobs[job_count].job_id = job_count + 1;
        jobs[job_count].command = strdup(command);
        jobs[job_count].is_running = is_running;
        jobs[job_count].is_stopped = 0;
        jobs[job_count].is_background = is_background;
        jobs[job_count].job_marker = ' ';
        job_count++;
        update_job_markers(job_count -1);
    } else {
        printf("Job list is full \n");
    }

}

void print_jobs() {
    for (int i = 0; i < job_count; i++) {
        printf("[%d]%c %s %s%s\n", jobs[i].job_id, jobs[i].job_marker, 
        jobs[i].is_running ? "Running" : "Stopped",  
        jobs[i].command, jobs[i].is_background ? " &" :"");
        //char job_marker = (i == job_count -1 ) ? '+' : '-';
        //if(jobs[i].is_running){    
    }
}

void fg_job(int job_id) {
    // Used from Dr.Y book Page-45-49 
    int status;
    int found = 0;

    // if no job id is provide bring the job with the + to foreground 
    if (job_id == -1) {
        for (int i = job_count -1; i >= 0; i--) {
            if (jobs[i].job_marker == '+') {
                job_id = jobs[i].job_id;
                break;
            }
        } 
    } 
    for (int i = 0; i < job_count; i++) {
        if (jobs[i].job_id == job_id) {
            found = 1;
            fg_pid = jobs[i].pid;
            printf("%s\n", jobs[i].command); // prints the command when bringing to the foreground
            fflush(stdout); //ensure the command is printed immediately 
            
            // brings the job to the foreground
            kill(-jobs[i].pid, SIGCONT);// wait for the process to cont. the stopped process
            
            waitpid(jobs[i].pid, &status, WUNTRACED); // wait for the process to finish or be stopped again 
            
            fg_pid = -1; // no longer a fg process 

            // Update job status if it was stopped again 
            if (WIFSTOPPED(status)){
                jobs[i].is_running = 0; // mark a stopped 
                jobs[i].is_stopped = 1;
                printf("\n[%d]+ Stopped %s\n", jobs[i].job_id, jobs[i].command);
            } else {
                
                // If the job is finished, remove it from the job list 
                free(jobs[i].command);
                for (int j = i; j < job_count - 1; j++) {
                    jobs[j] = jobs[j + 1 ];
                }
                job_count--;
                update_job_markers(job_count -1);
            }
            break;
        }
    } 
    if (!found) {
        printf("fg: job %d not found\n", job_id);
    }

}
void bg_job(int job_id) {
    int found = 0; 
    // If no job ID is provided bring the job with the + to the foreground 
    if (job_id == -1) {
        for (int i = job_count -1; i >= 0; i--) {
            if (jobs[i].job_marker == '+') {
                job_id = jobs[i].job_id;
                break;
            }
        } 
    } 
    for (int i = 0; i < job_count; i++) {
        if (jobs[i].job_id == job_id) {
            if (jobs[i].is_running) {
                printf("bg: job %d already running\n", job_id);
                return;
            }
            found = 1;
            jobs[i].is_running = 1;
            jobs[i].is_stopped = 0;
            jobs[i].is_background = 1;
            kill(-jobs[i].pid, SIGCONT);
            printf("[%d]%c %s    %s &\n", jobs[i].job_id,jobs[i].job_marker, "Running", jobs[i].command);
            update_job_markers(i);
            break;
        } 
    }
 
    if (!found) {
        printf("bg: job %d not found\n", job_id);
    }
}

void update_job_markers(int current_job_index) {
    for (int i = 0; i < job_count; i++) {
        jobs[i].job_marker = ' ';
    }
    if (job_count > 0) {
        jobs[job_count -1].job_marker = '+';

        if (job_count > 1) {
            jobs[job_count -2].job_marker = '-';
        }
    }
    
}

void remove_elements(char **args, int start, int count) {
    int i = start; 
    while (args[i + count] !=NULL) {
        args[i] = args[i + count];
        i++;
    }
    args[i] = NULL;
}

void setup_signal_handlers() {
    // Register the signal handler 
    signal(SIGINT, sigint_handler);
    signal(SIGTSTP, sigtstp_handler);
    signal(SIGCHLD, sigchld_handler);
}

void apply_redirections(char **cmd_args){
    int i = 0;
    int in_fd = -1, out_fd = -1, err_fd = -1;

    // loop through the command arguments to find redirection symbols 
    while (cmd_args[i] != NULL){
        // Input redirection 
        if (strcmp(cmd_args[i], "<") == 0) {
            if (cmd_args[i + 1] == NULL) {
                fprintf(stderr, " Error: expected filename after '<'\n");
                exit(EXIT_FAILURE);
            }

            in_fd = open(cmd_args[i + 1], O_RDONLY); // input redirection 

            if (in_fd < 0 ) {
                perror("Error opening file for input.");
                exit(EXIT_FAILURE); // exit child process on failure
            }
            // Redirect input from the file
            dup2(in_fd, STDIN_FILENO); // replace stdin with the file 
            close(in_fd);
            // Remove the redirection arguments from the command 
            remove_elements(cmd_args, i, 2);
            continue; // recheck current position 
            // Output redirection
        } else if (strcmp(cmd_args[i], ">") == 0) { 
            if (cmd_args[i + 1] == NULL) {
                fprintf(stderr, "Error: expected file name after '>\n");
                exit(EXIT_FAILURE);
            }
            // output direction 
            //TODO check it I need to delete if change below doesnt work out_fd = open(cmd_args[i + 1], O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH, 0644);
            out_fd = open(cmd_args[i + 1], O_WRONLY | O_CREAT | O_TRUNC, 0644);
            if (out_fd < 0) {
                perror("Error opening file for output");
                exit(EXIT_FAILURE);
            }
            // Redirect output to the file
            dup2(out_fd,STDOUT_FILENO);
            close(out_fd);

            remove_elements(cmd_args, i, 2);
            continue; // recheck current position 
        // Error Redirection   
        } else if (strcmp(cmd_args[i], "2>") == 0){
            // error redirection 
            //err_fd = open(cmd_args[i + 1],O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH );
            if (cmd_args[i + 1] == NULL) {
                //perror("yash expected filename after 2>\n");
                fprintf(stderr, "Error: expected filename after '2>'\n");
                exit (EXIT_FAILURE);
            }
            // Error redirection 
            err_fd = open(cmd_args[i + 1],O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH );
            if (err_fd < 0) {
                perror("Error opening file for error redirection");
                exit(EXIT_FAILURE);
            }
            // Redirect stderr to the file
            //printf("redirection output file : %s\n" , cmd_args[i+1]);
            dup2(err_fd,STDERR_FILENO);
            close(err_fd);

            remove_elements(cmd_args, i, 2);
            continue;
        }
        i++; // Move to the next argument if no redirection was found 
    }
}

void handle_control(int psd, char control) {
  // handle control signals 
  printf("Handling control signal: %c\n", control); // print the control signal for debugging 
  if (control == 'c') {
    // Send SIGINT
    if(fg_pid > 0) {   
      kill(fg_pid, SIGINT);
      printf("SIGINT sent to process Ctrl-C\n");

    } else {
      printf(" No foreground process to send sigint to .\n");
    }   
  } else if (control == 'z') {
      // Send SIGTSTP
      if (fg_pid > 0){
        kill(fg_pid, SIGTSTP);
        printf("SIGTSTP sent to process Ctrl-Z\n");

      } else {
        printf(" No foreground process to send SIGTSTP to .\n");
      }
  } else if (control == 'd') {
      // Send EOF signal
      printf("caught Ctrl-D\n");
      // Close the write end of the pipe to signal EOF to the child process
      //close(pipefd_stdin[1]);
    }
}

/*void handle_command(int psd, char *command) {
    int pipefd[2];
    pid_t pid;
    // Create a pipe to communicate between parent and child process 
    if(pipe(pipefd) == -1) {
        perror("Pipe failed");
        
        return; // exit function if pipe creation fails
    }

    // Fork a child process to execute the command
    if ((pid = fork()) == -1) {
        perror("fork");
        close(pipefd[0]);
        close(pipefd[1]);
        //exit(EXIT_FAILURE);
        return;
    }

    //  Child Process 
    if (pid == 0) {
        if (dup2(pipefd[1], STDOUT_FILENO) == -1) {
            perror("dup2 failed for stdout");
            exit(EXIT_FAILURE);
        }
        if (dup2(pipefd[1], STDERR_FILENO) == -1) {
            perror("dup2 failed for stdout");
            exit(EXIT_FAILURE);
        }

      close(pipefd[0]);
      close(pipefd[1]);

      char *args[] = {"/bin/sh", "-c", command, NULL};
      execvp(args[0], args); // execute the command
      perror("Execvp failed");
      exit(0);

    } else {
        // parent process 
        fg_pid = pid; // set the foreground pid
        // Close the right end of the pipe, and the child will write to it
        close(pipefd[1]); // 

        //Read the child's output from the pipe and send it to the client 
        char buffer [BUFFER_SIZE];
        ssize_t bytesRead; 
        while ((bytesRead = read(pipefd[0], buffer, sizeof(buffer))) > 0)
        {
            if (send(psd, buffer, bytesRead, 0) < 0) {
                perror("sending stream message");
                break;
            }
        }
        close(pipefd[0]); // close the read end of the pipe
        waitpid(pid, NULL, 0); 
        fg_pid = -1 ;
        // Send the initial prompt to the client
        if (send(psd, "\n# ", strlen("\n# "), 0) < 0) {
            perror("sending stream message");
        }
    }  
}*/
