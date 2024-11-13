void add_job(pid_t pid, const char *command, int is_running, int is_background);
void update_job_markers(int current_job_index) ;
void print_jobs();
void fg_job(int job_id);
void bg_job(int job_id);
void add_job(pid_t pid, const char *command, int is_running, int is_background);
void update_job_markers(int current_job_index) ;
void fg_job(int job_id);
void bg_job(int job_id);
void bg_command(char **cmd_args);

int connected_socket(int lsd) ;
int u__listening_socket(const char *path);

void add_job(pid_t pid, const char *command, int is_running, int is_background) {
    if (job_count < MAX_JOBS) {
        // Set the job marker for the new job as '+' and others as '-'
        for (int i = 0; i < job_count; i++) {
            jobs[i].job_marker = '-';
        }

        jobs[job_count].pid = pid;
        jobs[job_count].job_id = (job_count > 0) ? jobs[job_count - 1].job_id + 1 : 1;
        jobs[job_count].command = strdup(command);
        jobs[job_count].is_running = is_running;
        jobs[job_count].is_background = is_background;
        jobs[job_count].job_marker = '+';  // Most recent job gets '+'
        job_count++;
    } else {
        printf("Job list is full\n");
    }
}
void print_jobs() {

    char buffer[BUFFER_SIZE];
    for (int i = 0; i < job_count; i++) {
        snprintf(buffer, BUFFER_SIZE, "[%d]%c %s %s%s\n" ,
        jobs[i].job_id,
        jobs[i].job_marker,
        jobs[i].is_running ? "Running" : "Stopped",
        jobs[i].command,
        jobs[i].is_background ? " &" : "");
    //for (int i = 0; i < job_count; i++) {
    //    printf(psd,"[%d]%c %s %s%s\n", jobs[i].job_id, jobs[i].job_marker, 
    //    jobs[i].is_running ? "Running" : "Stopped",  
    //    jobs[i].command, jobs[i].is_background ? " &" :"");
    //    //char job_marker = (i == job_count -1 ) ? '+' : '-';
    //    //if(jobs[i].is_running){    
//
    //    // Testing purposes to see in client 
        //jobs[i].is_running ? "Running" : "Stopped",  
        //jobs[i].command, jobs[i].is_background ? " &" :"");
        //printf("[%d]%c %s %s%s\n", jobs[i].job_id, jobs[i].job_marker, 
        // Send the job status to the client 
        send(psd, buffer, strlen(buffer), 0);
    }
}
void fg_job(int job_id) {
    // Used from from Dr.Y book Page-45-49 
    int status;

    for (int i = 0; i < job_count; i++) {
        if (jobs[i].job_id == job_id) {
            jobs[i].is_running = 1;
            fg_pid = jobs[i].pid;// Setting a foreground process
            
            //jobs[i].is_running = 1; // mark as running  
            // print to both client and server 
            dprintf(psd, "%s\n", jobs[i].command); // prints teh command when bringing to the foreground
            printf("%s\n", jobs[i].command); // 

            fflush(stdout); //ensure teh command is printed immediately 
            // brings the job to the foreground
            kill(-jobs[i].pid, SIGCONT);// wait for the process to cont. the stopped proces
            waitpid(jobs[i].pid, &status, WUNTRACED); // wait for the process to finish or be stopped again should it be zero or wuntrance

            fg_pid = -1; // Clear foreground process , no longer a fg process 

            // update job status if it was stopped again 
            if (WIFSTOPPED(status)) {
                jobs[i].is_running = 0; // mark a stopped 
            } else {
                // if the job finished remove it from the job list
                dprintf(psd, "[%d] Done %s\n", jobs[i].job_id, jobs[i].command);  
                printf("[%d] Done %s\n", jobs[i].job_id, jobs[i].command); 

                for (int j = i; j < job_count - 1; j++) {
                    jobs[j] = jobs[j + 1 ];
                }
                job_count--;
            }

            // Once completed mark the job as done or remove it 
            //printf("[%d] Done %s\n", jobs[i].job_id, jobs[i].command);
            //break;

            return;
        }
    }  
    dprintf(psd, "job not found\n");
    printf("Job not found\n");
}
void bg_job(int job_id) {
    for (int i = 0; i < job_count; i++) {
        if (jobs[i].job_id == job_id && jobs[i].is_stopped) {
            jobs[i].is_running = 1;
            jobs[i].is_background = 1;
            jobs[i].is_stopped = 0;
            
            kill(-jobs[i].pid, SIGCONT);

            char buffer[BUFFER_SIZE];
            snprintf(buffer, BUFFER_SIZE, "[%d]%c %s &\n", jobs[i].job_id, jobs[i].job_marker, jobs[i].command);

            dprintf(psd, "%s# ", buffer); // prints teh command when bringing to the foreground
            //printf("[%d] + %s &\n", jobs[i].job_id, jobs[i].command);
            return;
            //break;
        }
    }
    dprintf(psd, "Job not found\n");
    //printf("Job not found or already running\n");
}
void bg_command(char **cmd_args) {
    pid_t pid = fork();
    if (pid == 0) {
        // Child process
        execvp(cmd_args[0], cmd_args);
        perror("execvp failed");  // If execvp fails
        exit(EXIT_FAILURE);
    } else if (pid > 0) {
        // Parent process
        add_job(pid, cmd_args[0], 1, 1);  // Mark as running and in background
        update_job_markers(job_count - 1);
    }
}
void update_job_markers(int current_job_index) {
    for (int i = 0; i < job_count; i++) {
        //jobs[i].job_marker = ' ';
        if (i == current_job_index) {
            jobs[i].job_marker = '+';
        } else {
            jobs[i].job_marker = '-';
        }
    }
  
}
void print_jobs() {
    for (int i = 0; i < job_count; i++) {
        char job_marker = (i == job_count -1 ) ? '+' : '-';
        //if(jobs[i].is_running){
        printf("[%d]%c %s %s\n", jobs[i].job_id, job_marker, jobs[i].is_running ? "Running" : "Stopped",  jobs[i].command);
        //printf("[%d]%c %s %s\n", jobs[i].job_id, (i == job_count - 1) ? '+' : '-', jobs[i].is_running ? "Running" : "Stopped",  jobs[i].command);
        //} else {
        //printf("[%d] %c - stopped %s\n", jobs[i].job_id,job_marker, jobs[i].command);
    }
}
void fg_job(int job_id) {
    // Used from from Dr.Y book Page-45-49 
    int status;

    for (int i = 0; i < job_count; i++) {
        if (jobs[i].job_id == job_id) {
            fg_pid = jobs[i].pid;
            
            //jobs[i].is_running = 1; // mark as running 

            printf("%s\n", jobs[i].command); // prints teh command when bringing to the foreground

            fflush(stdout); //ensure teh command is printed immediately 
            // brings the job to the foreground
            kill(-jobs[i].pid, SIGCONT);// wait for the process to cont. the stopped proces
    
            waitpid(jobs[i].pid, &status, WUNTRACED); // wait for the process to finish or be stopped again 

            fg_pid = -1; // no longer a fg process 

            // update job status if it was stopped again 
            if (WIFSTOPPED(status)){
                jobs[i].is_running = 0; // mark a stopped 

            } else {
                // if the job finished remove it from the job list 
                for (int j = i; j < job_count - 1; j++) {
                    jobs[j] = jobs[j + 1 ];
                }
                job_count--;
            }
            
             return;
        }
    }  
    printf("Job not found\n");
            //tcsetpgrp(STDIN_FILENO, getpgrp());
            /*if(WIFSTOPPED(status)){
                jobs[i].is_running = 0; // mark as stopped
            } else {
                // the child has terminated normally 
                fg_pid = -1; // no longer a fg process  
                // remove job from the list 
                for (int j = i; j < job_count - 1; j++){
                    jobs[j] = jobs[j + 1];
                }*/
                
            
            /*do
            {
                
                waitpid(jobs[i].pgid, &status, WUNTRACED | WCONTINUED);
                if(WIFSTOPPED(status)){
                    // the child has terminated normally 
                    fg_pid = -1; // no longer a fg process  
                    // remove job from the list 
                    for (int j = i; j < job_count - 1; j++){
                        jobs[j] = jobs[j + 1];
                    }
                    job_count--;
                    
                } else if (WIFSIGNALED(status)) {
                    // the child was killed by a signal
                    printf(" Job [%d] was killed by a signal %d\n", jobs[i].job_id, WTERMSIG(status));
                    fg_pid = -1; // no longer a fg process 
                    // remove job from the list 
                    for (int j = i; j < job_count - 1; j++){
                        jobs[j] = jobs[j + 1];
                    }
                    job_count--;
                } else if (WIFSTOPPED(status))
                {
                    // the chiled was stopped  
                    jobs[i].is_running = 0; // mark as stopped if stopped agains
                    printf(" Job [%d] was stopped by a signal %d\n", jobs[i].job_id, WSTOPSIG(status));
                } else if (WIFCONTINUED(status))
                {
                    // the child continued  
                    printf("jobs [%d] continued\n", jobs[i].job_id);
                }   
            } while (!WIFEXITED(status)&& !WIFSIGNALED(status));*/

}
void bg_job(int job_id) {
    for (int i = 0; i < job_count; i++) {
        if (jobs[i].job_id == job_id && !jobs[i].is_running) {
            kill(-jobs[i].pid, SIGCONT);
            jobs[i].is_running = 1;
            printf("[%d] + %s &\n", jobs[i].job_id, jobs[i].command); // prints teh command when bringing to the foreground
            return;
        }
    }
    printf("Job not found or already running\n");
}
//void add_job(pid_t pid, char *command, int is_running, int is_background)
void add_job(pid_t pid, const char *command, int is_running, int is_background) {

    if (job_count < MAX_JOBS){ 
        for (int i = 0; i < job_count; i++) {
            // set all existing jobs to hava " -" marker
            jobs[i].is_running = 0;
        }

        jobs[job_count].pid = pid;
        jobs[job_count].job_id = job_count + 1;
        jobs[job_count].command = strdup(command);
        jobs[job_count].is_running = is_running;
        job_count++;
    } else {
        printf("Job list is full \n");
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
void print_jobs() {
    for (int i = 0; i < job_count; i++) {
        char job_marker = jobs[i].job_marker;  // Use the stored marker for each job
        printf("[%d]%c %s %s\n", jobs[i].job_id, job_marker, jobs[i].is_running ? "Running" : "Stopped", jobs[i].command);
    }
}
void add_job(pid_t pid, const char *command, int is_running, int is_background) {
    if (job_count < MAX_JOBS) {
        // Set the job marker for the new job as '+' and others as '-'
        for (int i = 0; i < job_count; i++) {
            jobs[i].job_marker = '-';
        }

        jobs[job_count].pid = pid;
        jobs[job_count].job_id = (job_count > 0) ? jobs[job_count - 1].job_id + 1 : 1;
        jobs[job_count].command = strdup(command);
        jobs[job_count].is_running = is_running;
        jobs[job_count].is_background = is_background;
        jobs[job_count].job_marker = '+';  // Most recent job gets '+'
        job_count++;
    } else {
        printf("Job list is full\n");
    }
}
void fg_job(int job_id) {
    int status;
    for (int i = 0; i < job_count; i++) {
        if (jobs[i].job_id == job_id) {
            fg_pid = jobs[i].pid;
            printf("%s\n", jobs[i].command); // Print command like Bash does
            fflush(stdout);
            kill(jobs[i].pid, SIGCONT);
            waitpid(jobs[i].pid, &status, WUNTRACED);
            fg_pid = -1;

            if (WIFSTOPPED(status)) {
                jobs[i].is_running = 0;
            } else {
                // Remove completed job from the list
                for (int j = i; j < job_count - 1; j++) {
                    jobs[j] = jobs[j + 1];
                }
                job_count--;
            }
            update_job_markers(i);
            return;
        }
    }
    printf("Job not found\n");
}
void bg_job(int job_id) {
    for (int i = 0; i < job_count; i++) {
        if (jobs[i].job_id == job_id && !jobs[i].is_running) {
            kill(jobs[i].pid, SIGCONT);
            jobs[i].is_running = 1;
            printf("[%d]%c %s &\n", jobs[i].job_id, jobs[i].job_marker, jobs[i].command);
            update_job_markers(i);
            return;
        }
    }
    printf("Job not found or already running\n");
}
void bg_command(char **cmd_args) {
    pid_t pid = fork();
    if (pid == 0) {
        // Child process
        execvp(cmd_args[0], cmd_args);
        perror("execvp failed");  // If execvp fails
        exit(EXIT_FAILURE);
    } else if (pid > 0) {
        // Parent process
        add_job(pid, cmd_args[0], 1, 1);  // Mark as running and in background
        update_job_markers(job_count - 1);
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
  /* from the teachers code 
  // Redirecting stderr to u_log_path 
  log = fopen(u_log_path, "aw"); // attach stderr to u_log_path 
  fd = fileno(log);  /* obtain file descriptor of the log 
  dup2(fd, STDERR_FILENO);
  close (fd);
  / /From this point on printing to stderr will go to /tmp/u-echod.log */
  //From this point on printf and scanf have no effect */
// Create pipes for communication between parent and child
          if (pipe(pipefd_stdout) == -1 || pipe(pipefd_stdin) == -1) {
            perror("pipe");
            exit(EXIT_FAILURE);
          }
          // Fork a child process to execute the command
          if ((pid = fork()) == -1) {
              perror("fork");
              exit(EXIT_FAILURE);
          }
          if (pid == 0) {
            // Child process
          
            close(pipefd_stdout[0]); // Close the read end of the stdout pipe
            close(pipefd_stdin[1]);  // Close the write end of the stdin pipe
            
            dup2(pipefd_stdout[1], STDOUT_FILENO); // Redirect stdout to the write end of the stdout pipe
            dup2(pipefd_stdout[1], STDERR_FILENO); // Redirect stderr to the write end of the stdout pipe
            dup2(pipefd_stdin[0], STDIN_FILENO);   // Redirect stdin to the read end of the stdin pipe
            
            close(pipefd_stdout[1]);
            close(pipefd_stdin[0]);

            /*// tokenize the command string into arguments
            char *args[10];
            int i = 0;
            args[i] = strtok(command, " ");
            while (args[i] != NULL && i < 9) { 
              i++;
              args[i] = strtok(NULL, " ");
            }*/

            // execute the command 
            //if (execvp(args[0], args) == -1) {
            if (execvp(cmd_args[0], cmd_args)) {
              perror("execvp failed");
              exit(EXIT_FAILURE);
            } //exit(EXIT_SUCCESS);
          } else {
            // Parent process
            close(pipefd_stdout[1]); // Close the write end of the stdout pipe
            close(pipefd_stdin[0]);  // Close the read end of the stdin pipe
            //commandRunning = 1;

            // Read the child's output from the pipe and send it to the client socket
            while ((bytesRead = read(pipefd_stdout[0], buffer, sizeof(buffer) - 1)) > 0) {
              buffer[bytesRead] = '\0';
              send(psd, buffer, bytesRead, 0);
            }

            close(pipefd_stdout[0]);

            // Wait for the child process to finish
            waitpid(pid, NULL, 0);
            //commandRunning = 0;

            pid = -1; 

            if (send(psd, "\n# ", sizeof("\n# "), 0) <0 ){
            perror("sending stream message");
            } else {
              printf("sent # to clients s4"); //TODO: DELETE AFTER TESTING 

            }
            //close(pipefd_stdout[0]);
          }
          // // Send # to indicate the end of the command output
          if (send(psd, "\n# ", sizeof("\n# "), 0) <0 ){
             perror("sending stream message");
          } else{
            printf("sent over to client  #3 ");
          }
        } 
        else if (strncmp(buffer, "CTL ", 4) == 0) { 
          // Handle: CTL<blank><char[c|z|d]>\n
          char controlChar = buffer[4];
          if (controlChar == 'c') { 
            if (pid > 0) {
              kill(pid, SIGINT);
              printf("Sent SIGINT to child process %d\n", pid);
            } else {
              printf("No process to send SIGINT\n");
            }
          } else if (controlChar == 'z') {
            if (pid > 0) {
              kill(pid, SIGINT);
              printf("Sent SIGTSTP to child process %d\n", pid);
            } else {
              printf("No process to send SIGINT\n");
            }

          } else if (controlChar == 'd') {
            //if (pipefd_stdin[1] != -1) {
              close(pipefd_stdin[1]);
              //pipefd_stdin[1] = -1;
              printf("Closed write end of stdin pipe\n");
              pthread_exit(NULL);
            //}
            //commandRunning = 0;
          }


/* my code befor change
// check for a pipe and
    char *pipe_position = strchr(original_cmd, '|');
    if(pipe_position){
        *pipe_position = '\0'; // split the input at |
        char *cmd_left = strtok(original_cmd, '|');
        char *cmd_right = strtok(NULL, '|');
        // Parse both sides of the pipe into arguments
        char *args_left[MAX_ARGS];
        char *args_right[MAX_ARGS];
        int i = 0; // parse both commands
        args_left[i] = strtok(cmd_left, " ");

        while (args_left[i] != NULL && i < MAX_ARGS - 1)
        {
            i++;
            args_left[i] = strtok(NULL, " ");
        }
        args_left[i] = NULL;

        i = 0;
        args_right[i] = strtok(cmd_right, " ");
        while (args_right[i] != NULL && i < MAX_ARGS - 1)
        {
            i++;
            args_right[i] = strtok(NULL, " ");
        }

        handle_pipe(args_left, args_right, psd);
*/
/*char *pipe_position = strchr(command, '|'); // Check if command has a pipe |
            if (pipe_position != NULL) {
                *pipe_position = '\0'; // split the input at |

                char *cmd_left = command; // left part before the |
                char *cmd_right = pipe_position + 1;

                // Parse both sides of the pipe into arguments
                char *cmd_args_left[MAX_ARGS];
                char *cmd_args_right[MAX_ARGS];
                int i = 0;
                cmd_args_left[i] = strtok(cmd_left, " ");
                while (cmd_args_left[i] != NULL && i < MAX_ARGS - 1) {
                    i++;
                    cmd_args_left[i] = strtok(NULL, " ");
                }
                cmd_args_left[i] = NULL;

                i = 0;
                cmd_args_right[i] = strtok(cmd_right, " ");
                while (cmd_args_right[i] != NULL && i < MAX_ARGS - 1)
                {
                    i++;
                    cmd_args_right[i] = strtok(NULL, " ");
                }
                handle_pipe(cmd_args_left, cmd_args_right, psd);
            }*/

//
//void *serveClient(void *args) {
//    ClientArgs *clientArgs = (ClientArgs *)args;
//    int psd = clientArgs->psd;
//    struct sockaddr_in from = clientArgs->from;
//    char buffer[BUFFER_SIZE]; // Maybe 205 as the command can only have 200 characters
//    int bytesRead;
//    int pipefd_stdout[2] = {-1, -1};
//    int pipefd_stdin[2] = {-1, -1};
//    pthread_t p;
//    pid_t pid = -1;
//    // int commandRunning = 0;
//
//
//    // Send initial prompt to the client
//    if (send(psd, PROMPT, sizeof(PROMPT), 0) < 0){
//        perror("Sending Prompt # 1");
//        close(psd);
//        free(clientArgs);
//        pthread_exit(NULL);
//    } else {
//        printf("sent over to client prompt from s1");
//    }
//
//    /**  get data from  client and send it back */
//    for (;;)
//    { // infinite loop to keep the server running
//        printf("\n...server is waiting...\n ");
//        cleanup(buffer); // clear the buffer
//
//        bytesRead = recv(psd, buffer, sizeof(buffer), 0);
//
//        // Receive command from client
//        if (bytesRead <= 0) { // read data from the client
//            if (bytesRead == 0) {
//                // connection closed by client
//                printf("Connection closed by client\n");
//            } else {
//                perror("Error receiving stream message\n");
//            }
//            close(psd);
//            free(clientArgs);
//            pthread_exit(NULL);
//        }
//
//        buffer[bytesRead] = '\0'; // NULL terminate the received data
//
//        printf("Received Buffer: %s", buffer);
//
//        // Handle: CMD<blank><Command_String>\n
//        if (strncmp(buffer, "CMD ", 4) == 0) {
//            // Extract the command string
//            char *command = buffer + 4;
//            command[strcspn(command, "\n")] = '\0'; // Remove the newline character
//            printf("Command Received: %s\n", command);
//           //check for pipes 
//            if (strchr(command, '|') !=NULL) {
                // validatePipes(command, psd);
                // continue;
              //}
//            if (!validateCommand(command, psd)){
//              continue;// skip invalid commands

//             }
//            // validateCommand
//            validateCommand(command, psd); // validation for debugging
//
//            // Tokenize the command into arguements
//            //char *cmd_args[10];
//            //int i = 0;
////
//            //cmd_args[i] = strtok(command, " ");
//            //while (cmd_args[i] != NULL && i < 9)
//            //{
//            //    i++;
//            //    cmd_args[i] = strtok(NULL, " ");
//            //}
//            //cmd_args[i] = NULL;
//            // Allocate memory for LogRequestArgs
//            LogRequestArgs *args = (LogRequestArgs *)malloc(sizeof(LogRequestArgs));
//            if (args == NULL) {
//                perror("Error allocating memory for log request");
//                continue;
//            }
//            // Copy the request and client information to the struct
//            strncpy(args->request, command, sizeof(args->request) - 1);
//            args->from = from;
//
//            // Create a new thread for logging
//            if (pthread_create(&p, NULL, logRequest, (void *)args) != 0) {
//                perror("Error creating log request thread");
//                free(args); // Free the allocated memory if thread creation fails
//            } else {
//                pthread_detach(p); // Detach the thread to allow it to run independently
//            }
//
//            // Create pipes for communication between parent and child
//            if (pipe(pipefd_stdout) == -1 || pipe(pipefd_stdin) == -1) {
//                perror("pipe");
//                exit(EXIT_FAILURE);
//            }
//
//            // Fork a child process to execute the command
//            if ((pid = fork()) == -1)
//            {
//                perror("fork");
//                exit(EXIT_FAILURE);
//            }
//
//            if (pid == 0) {
//                // Child process
//                close(pipefd_stdout[0]); // Close the read end of the stdout pipe
//                close(pipefd_stdin[1]);  // Close the write end of the stdin pipe
//                dup2(pipefd_stdout[1], STDOUT_FILENO); // Redirect stdout to the write end of the stdout pipe
//                dup2(pipefd_stdout[1], STDERR_FILENO); // Redirect stderr to the write end of the stdout pipe
//                dup2(pipefd_stdin[0], STDIN_FILENO);   // Redirect stdin to the read end of the stdin pipe
//                close(pipefd_stdout[1]);
//                close(pipefd_stdin[0]);
//
//                    /// tokenize the command string into arguments
//                    // char *args[10];
//                    // int i = 0;
//                    // args[i] = strtok(command, " ");
//                    // while (args[i] != NULL && i < 9) {
//                    //   i++;
//                    //   args[i] = strtok(NULL, " ");
//                    // }
//
//                // execute the command
//                // if (execvp(args[0], args) == -1) {
//                if (execvp(cmd_args[0], cmd_args))
//                {
//                    perror("execvp failed");
//                    exit(EXIT_FAILURE);
//                } // exit(EXIT_SUCCESS);
//            } else  {
//                // Parent process
//                close(pipefd_stdout[1]); // Close the write end of the stdout pipe
//                close(pipefd_stdin[0]);  // Close the read end of the stdin pipe
//                // commandRunning = 1;
//
//                // Read the child's output from the pipe and send it to the client socket
//                while ((bytesRead = read(pipefd_stdout[0], buffer, sizeof(buffer) - 1)) > 0) {
//                    buffer[bytesRead] = '\0';
//                    send(psd, buffer, bytesRead, 0);
//                }
//
//                close(pipefd_stdout[0]);
//
//                // Wait for the child process to finish
//                waitpid(pid, NULL, 0);
//                // commandRunning = 0;
//
//                pid = -1;
//
//                if (send(psd, "\n# ", sizeof("\n# "), 0) < 0) {
//                    perror("sending stream message");
//                } else {
//                    printf("sent # to clients s4"); // TODO: DELETE AFTER TESTING
//                }
//                // close(pipefd_stdout[0]);
//            }
//            // // Send # to indicate the end of the command output
//
//            if (send(psd, "\n# ", sizeof("\n# "), 0) < 0) {
//                perror("sending stream message");
//            } else {
//                printf("sent over to client  #3 ");
//            }
//        } 
//        else if (strncmp(buffer, "CTL ", 4) == 0) {
//            // Handle: CTL<blank><char[c|z|d]>\n
//            char controlChar = buffer[4];
//            if (controlChar == 'c') {
//                if (pid > 0) {
//                    kill(pid, SIGINT);
//                    printf("Sent SIGINT to child process %d\n", pid);
//                } else {
//                    printf("No process to send SIGINT\n");
//                }
//            } else if (controlChar == 'z') {
//                if (pid > 0) {
//                    kill(pid, SIGINT);
//                    printf("Sent SIGTSTP to child process %d\n", pid);
//                } else {
//                    printf("No process to send SIGINT\n");
//                }
//            } else if (controlChar == 'd') {
//                // if (pipefd_stdin[1] != -1) {
//                close(pipefd_stdin[1]);
//                // pipefd_stdin[1] = -1;
//                printf("Closed write end of stdin pipe\n");
//                pthread_exit(NULL);
//                //}
//                // commandRunning = 0;
//            }
//            } else {
//                // Handle plain text input
//                // if (commandRunning) {
//                // Write the plain text to the stdin of the running command
//                // write(pipefd_stdin[1], buffer, bytesRead);
//                //}
//                // else {
//                // If no command is running, send an error message
//            const char *errorMsg = "Error: No command is currently running.\n#";
//            send(psd, errorMsg, strlen(errorMsg), 0);
//        }
//    }
//    pthread_exit(NULL);
//}
//
            //}
            //else if (strchr(command, '&') != NULL) { //  add to the bg with &
//                command[strlen(command) - 1] = '\0';  // Remove '&'
//                char *cmd_args[MAX_ARGS];
//                int i =0;
//                cmd_args[i] = strtok(command, " ");
//                while (cmd_args[i] != NULL && i < MAX_ARGS -1)
//                {
//                    i++;
//                    cmd_args[i] = strtok(NULL, " ");
//                }
//                bg_command(cmd_args);
//                send(psd, PROMPT, strlen(PROMPT), 0); // Send prompt back to the client 
//                continue;
//            }
            // Check for background command (ends with '&')
            //if (strchr(command, '&') != NULL) {
            //    command[strlen(command) - 1] = '\0';  // Remove '&'
                //char *cmd_args[MAX_ARGS];
                //int i = 0;
                //  add job function starts here
            //    add_job(pid, command, 1, 1); // add job to jobs array
                //cmd_args[i] = strtok(command, " ");
                //while (cmd_args[i] != NULL && i < MAX_ARGS - 1) {
                //    i++;
                //    cmd_args[i] = strtok(NULL, " ");
                //}
                //cmd_args[i] = NULL;
                //bg_command(cmd_args);  // Start background job
                //send(psd, PROMPT, strlen(PROMPT), 0);
                //continue;
            //} //  end background command 
//void validatePipes(const char *command, int psd) {// Check for pipes in the command
//    char *pipe_position = strchr(command, '|');
//    if (pipe_position) {
//        // Split the command into left and right commands
//        *pipe_position = '\0';
//        char *cmd_left = (char *)command; // Left part before the pipe
//        char *cmd_right = (char *)pipe_position + 1; // Right part after the pipe
//
//        // Tokenize both left and right commands
//        char *cmd_args_left[MAX_ARGS];
//        char *cmd_args_right[MAX_ARGS];
//        int i = 0;
//
//        // Tokenize left part
//        cmd_args_left[i] = strtok(cmd_left, " ");
//        while (cmd_args_left[i] != NULL && i < MAX_ARGS - 1) {
//            i++;
//            cmd_args_left[i] = strtok(NULL, " ");
//        }
//        cmd_args_left[i] = NULL;
//
//        // Tokenize right part
//        i = 0;
//        cmd_args_right[i] = strtok(cmd_right, " ");
//        while (cmd_args_right[i] != NULL && i < MAX_ARGS - 1) {
//            i++;
//            cmd_args_right[i] = strtok(NULL, " ");
//        }
//        cmd_args_right[i] = NULL;
//
//        // Handle piping by calling handle_pipe()
//        handle_pipe(cmd_args_left, cmd_args_right, psd);
//
//        // Send prompt after piping execution
//        send(psd, "\n# ", strlen("\n# "), 0);
//        return;
//    }
//
//    // No pipes detected, continue execution or validation
//    const char *errorMsg = "No pipes found in the command.\n#";
//    send(psd, errorMsg, strlen(errorMsg), 0);
//}
    // No pipes detected, continue execution or validation
    //const char *errorMsg = "No pipes found in the command.\n#";
    //send(psd, errorMsg, strlen(errorMsg), 0);
//}
/*void validatePipes(const char *command, int psd) {
    char *command_copy = strdup(command);
    if (!command_copy) {
        send(psd, "Memory allocation error\n# ", 25, 0);
        return;
    }
    // Find the pipe character
    char *pipe_pos = strchr(command_copy, '|');
    if (!pipe_pos) {
        free(command_copy);
        return;
    }
    // Split the command into left and right parts
    *pipe_pos = '\0';
    char *left_cmd = command_copy;
    char *right_cmd = pipe_pos + 1;
    // Remove leading/trailing spaces
    while (*left_cmd == ' ') left_cmd++;
    while (*right_cmd == ' ') right_cmd++;
    // Create argument arrays
    char *left_args[MAX_ARGS];
    char *right_args[MAX_ARGS];
    // Parse left command
    int i = 0;
    left_args[i] = strtok(left_cmd, " \t");
    while (left_args[i] != NULL && i < MAX_ARGS - 1) {
        i++;
        left_args[i] = strtok(NULL, " \t");
    }
    left_args[i] = NULL;
    // Parse right command
    i = 0;
    right_args[i] = strtok(right_cmd, " \t");
    while (right_args[i] != NULL && i < MAX_ARGS - 1) {
        i++;
        right_args[i] = strtok(NULL, " \t");
    }
    right_args[i] = NULL;
    // Execute the pipe
    handle_pipe(left_args, right_args, psd);

    free(command_copy);
}*/
//void validateCommand(const char *command, int psd)
//{
//    // check if the user input is empty pressing enter without types anything
//    if (strlen(command) == 0)
//    {
//        const char *errorMsg = "No Command \n#";
//        send(psd, errorMsg, strlen(errorMsg), 0);
//        return; // skp to the next iteration if no command was entered
//    }
//
//    // check for invalid combinations lif fg & and fg | echo
//    //if ((strstr(command, "fg &") != NULL) || (strstr(command, "fg | ") != NULL) || (strstr(command, "bg &") != NULL) || (strstr(command, "bg | ") != NULL))
//    //{
//    //    // invalid combination, move to next line
//    //    const char *errorMsg = "Invalid command combination fg &, fg |, bg &, bg |\n# ";
//    //    send(psd, errorMsg, strlen(errorMsg), 0);
//    //    return; // skp to the next iteration if no command was entered
//    //}
//
//
//    // check if input starts with a special character and skip 
//    if (command[0]== '<' ||command[0]== '>' || command[0]== '|' || command[0]== '&') {
//        const char *errorMsg = " Invalid command: commands cannot start with <>|& \n#";
//        send(psd, errorMsg, strlen(errorMsg), 0);
//    }
//    if (strcmp(command, "jobs") == 0)
//    // allow job command to list running or stopped jobs 
//    {
//        // print_jobs();
//        send(psd, "\n# ", strlen("\n# "), 0);
//        return;
//    }
//    // Check for redirection or pipes
//    if (strstr(command, ">") || strstr(command, "<") || strstr(command, "2<")) {
//        //  apply redirection 
//        char *cmd_args[MAX_ARGS];
//        int i = 0;
//
//        cmd_args[i] = strtok((char *)command, " ");
//        while (cmd_args[i] !=NULL && i < MAX_ARGS -1) {
//            i++;
//            cmd_args[i] = strtok(NULL, " ");
//        }
//        cmd_args[i] = NULL;
//
//        apply_redirections(cmd_args, psd);
//        return; 
//    }
//
//    // if command contains a pipe, vaildate pipes
//    if (strstr(command, "|")) {
//        validatePipes(command, psd);
//        return;
//    }
//
//    // execute command ifno special symbols are found 
//    char *cmd_args[MAX_ARGS];
//    int i = 0;
//    cmd_args[i] =  strtok((char *)command, " ");
//    while (cmd_args[i] != NULL && i < MAX_ARGS -1) {
//        i++;
//        cmd_args[i] = strtok(NULL, " ");
//    }
//    cmd_args[i] = NULL;
//
//    //execute_command(cmd_args, (char *)command, psd);
//    send(psd, "\n# ", strlen("\n# "), 0); // 
//
//}
// This is where the server will be set up yashd.c
// Troubleshooting the server
// 1. Error binding name to stream socket: Address already in use 
// Run: lsof -i :3820
// Run: kill -9 <PID>

// basic functionality for connecting to the server

#include "yashd.h"

#define PROMPT "\n# "
#define MAX_INPUT 200
#define MAX_ARGS 100
#define MAX_JOBS 20 
#define BUFFER_SIZE 1024  
#define PATHMAX 255
int psd; 
/* Initialize path variables */

static char* server_path = "./tmp/yashd";
char* log_path = "./tmp/yashd.log";
static char* pid_path = "./tmp/yashd.pid";
//static char socket_path[PATHMAX + 1];


/* Initialize mutex lock */
pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;


void reusePort(int sock);
void *serveClient(void *args);
void *logRequest(void *args);

//void execute_command(char **cmd_args, char *original_cmd, int psd);
void apply_redirections(char **cmd_args, int psd);
void handle_pipe(char **commands, int psd);
void validatePipes(const char *command, int psd);
int validateCommand(const char *command, int psd);

void send_psd(int fd, const char *buf, size_t len, int flags);
char **tokenize_command(char *command);
int find_pipe_index(char **args);
char **split_string(char *str);
void receive_output_from_server(int sockfd);

void add_job(pid_t pid, const char *command, int is_running, int is_background);
void update_job_markers(int current_job_index) ;
void print_jobs();
void fg_job(int job_id);
void bg_job(int job_id);


void remove_newLine(char *line);
void run_in_background(char **cmd_args);
void setup_signal_handlers();
void handle_control(int psd, char control);



/* create thread argument struct for logRequest() thread */
typedef struct LogRequestArgs {
  char request[200];
  struct sockaddr_in from;
} LogRequestArgs;

/* create thread argument struct for client thread */
typedef struct {
    int psd;
    struct sockaddr_in from;
} ClientArgs;

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


/**
 * @brief  If we are waiting reading from a pipe and
 *  the interlocutor dies abruptly (say because
 *  of ^C or kill -9), then we receive a SIGPIPE
 *  signal. Here we handle that.
 */
void sig_pipe(int n) 
{
   perror("Broken pipe signal");
   exit(1);
}

/**
 * @brief Handler for SIGCHLD signal 
 */
void sig_chld(int n)
{
    int status;
    fprintf(stderr, "Child terminated\n");
    //while (waitpid(-1, NULL, WNOHANG) > 0);
    wait(&status); /* So no zombies */
}

void daemon_init(const char * const path, uint mask)
{
  pid_t pid;
  char buff[256];
  int fd;
  int k;

  /* put server in background (with init as parent) */
  if ((pid = fork() ) < 0 ) {
    perror("daemon_init: cannot fork");
    exit(0);
  } else if (pid > 0) /* The parent */
    exit(0);

  /* the child */

  /* Close all file descriptors that are open */
  for (k = getdtablesize()-1; k>0; k--)
      close(k);

  /* Redirecting stdin, stdout, and stdout to /dev/null */
  if ((fd = open("/dev/null", O_RDWR)) < 0) {
    perror("Open");
    exit(0);
  }
  dup2(fd, STDIN_FILENO);      /* detach stdin */
  dup2(fd, STDOUT_FILENO);     /* detach stdout */
  dup2(fd, STDERR_FILENO);     /* detach stderr */
  close (fd);


  /* Establish handlers for signals */
  if ( signal(SIGCHLD, sig_chld) < 0 ) {
    perror("Signal SIGCHLD");
    exit(1);
  }
  if ( signal(SIGPIPE, sig_pipe) < 0 ) {
    perror("Signal SIGPIPE");
    exit(1);
  }

  /* Change directory to specified directory */
  chdir(path); 

  /* Set umask to mask (usually 0) */
  umask(mask); 
  
  /* Detach controlling terminal by becoming sesion leader */
  setsid();

  /* Put self in a new process group */
  pid = getpid();
  setpgrp(); /* GPI: modified for linux */

  /* Make sure only one server is running */
  if ((k = open(pid_path, O_RDWR | O_CREAT, 0666) ) < 0 )
    exit(1);
  if ( lockf(k, F_TLOCK, 0) != 0)
    exit(0);

  /* Save server's pid without closing file (so lock remains)*/
  sprintf(buff, "%6d", pid);
  write(k, buff, strlen(buff));

  return;
}


int main(int argc, char **argv ) {
    int   sd, psd; 
    struct   sockaddr_in server; 
    struct  hostent *hp;
    struct sockaddr_in from;
    socklen_t  fromlen;
    socklen_t length;
    //int listenfd;

    // TO-DO: Initialize the daemon
    // daemon_init(server_path, 0);
    // printf("Daemon initialzied.\n")
    
    hp = gethostbyname("localhost");
    if (hp == NULL) {
        perror("Error getting hostname");
        exit(-1);
    }
    bcopy(hp->h_addr, &(server.sin_addr), hp->h_length);
    printf("(TCP/Server INET ADDRESS is: %s )\n", inet_ntoa(server.sin_addr));
    
    /** Construct name of socket */
    server.sin_family = AF_INET;    
    server.sin_addr.s_addr = htonl(INADDR_ANY);
    server.sin_port = htons(3820);  
    
    // Create the socket to send and redeived
    // this is the u__listening_socket (const char *path)
    sd = socket (AF_INET, SOCK_STREAM, IPPROTO_TCP); // returns a socket descrp. -1 error errno
    if (sd < 0) {
	    perror("opening stream socket");
	    exit(-1);
    }
    
    reusePort(sd); // add this to u__listening_socket 
    //  this is also in u__lostening socket 
    if (bind(sd, (struct sockaddr *) &server, sizeof(server) ) < 0 ) {
      close(sd);
      perror("Error binding name to stream socket");
      exit(-1);
    }
    
    // Print out the server port
    length = sizeof(server);
    if (getsockname (sd, (struct sockaddr *)&server,&length) ) {
      perror("Error getting socket name");
      exit(0);
    }
    printf("Server Port is: %d\n", ntohs(server.sin_port));
    
    // listen for incoming connection
    listen(sd,4);

    fromlen = sizeof(from);
    printf("Server is ready to receive...........\n");

    for(;;){
      printf("Server is waiting.......\n");

      psd  = accept(sd, (struct sockaddr *)&from, &fromlen);

      if (psd < 0) {
        perror("Accepting connection");
        continue;
      }

      // Allocate memory for ClientArgs
      ClientArgs *clientArgs = malloc(sizeof(ClientArgs));
      if (clientArgs == NULL) {
          perror("Error allocating memory for client arguments");
          close(psd);
          continue;
      }

      printf("Memory allocated for client arguments\n");

      // Set the client arguments
      clientArgs->psd = psd;
      clientArgs->from = from;

      // Create a new thread to serve the client
      pthread_t thread;
      if (pthread_create(&thread, NULL, serveClient, (void *)clientArgs) != 0) {
        perror("Error creating client thread");
        free(clientArgs);
        close(psd);
        continue;
      }

      printf("Thread was created to serve a client\n");

      pthread_detach(thread); // Detach the thread to unblock and to allow it to run independently.

    }
    // Destroy the mutex
    pthread_mutex_destroy(&lock);// Destroy the mutex at the end
    return 0;
}



void *logRequest(void *args) {
    LogRequestArgs *logArgs = (LogRequestArgs *)args;
    char *request = logArgs->request;
    struct sockaddr_in from = logArgs->from;

    time_t rawtime;
    struct tm *timeinfo;
    char timeString[80];
    FILE *logFile;

    time(&rawtime);
    timeinfo = localtime(&rawtime);
    strftime(timeString, 80, "%b %d %H:%M:%S", timeinfo);

    pthread_mutex_lock(&lock); // wrap CS in lock ...
    
    printf("Opening log file at path: %s\n", log_path);
    
    logFile = fopen(log_path, "a");
    if (logFile == NULL) {
        perror("Error opening log file");
        pthread_mutex_unlock(&lock); // Unlock the mutex before returning
        return NULL;
    }

    // Debugging statement to check if writing to the file
    printf("Writing to log file: %s\n", request);


    fprintf(logFile,"%s yashd[%s:%d]: %s\n", timeString, inet_ntoa(from.sin_addr), ntohs(from.sin_port), request);
    fclose(logFile);
    pthread_mutex_unlock(&lock); // ... unlock
    free(args);
    
    pthread_exit(NULL);
 }


void cleanup(char *buf)
{
    int i;
    for(i=0; i<BUFFER_SIZE; i++) buf[i]='\0';
}
//
//void *serveClient(void *args) {
//    ClientArgs *clientArgs = (ClientArgs *)args;
//    int psd = clientArgs->psd;
//    struct sockaddr_in from = clientArgs->from;
//    char buffer[BUFFER_SIZE]; // Maybe 205 as the command can only have 200 characters
//    int bytesRead;
//    int pipefd_stdout[2] = {-1, -1};
//    int pipefd_stdin[2] = {-1, -1};
//    pthread_t p;
//    pid_t pid = -1;
//    // int commandRunning = 0;
//
//
//    // Send initial prompt to the client
//    if (send(psd, PROMPT, sizeof(PROMPT), 0) < 0){
//        perror("Sending Prompt # 1");
//        close(psd);
//        free(clientArgs);
//        pthread_exit(NULL);
//    } else {
//        printf("sent over to client prompt from s1");
//    }
//
//    /**  get data from  client and send it back */
//    for (;;)
//    { // infinite loop to keep the server running
//        printf("\n...server is waiting...\n ");
//        cleanup(buffer); // clear the buffer
//
//        bytesRead = recv(psd, buffer, sizeof(buffer), 0);
//
//        // Receive command from client
//        if (bytesRead <= 0) { // read data from the client
//            if (bytesRead == 0) {
//                // connection closed by client
//                printf("Connection closed by client\n");
//            } else {
//                perror("Error receiving stream message\n");
//            }
//            close(psd);
//            free(clientArgs);
//            pthread_exit(NULL);
//        }
//
//        buffer[bytesRead] = '\0'; // NULL terminate the received data
//
//        printf("Received Buffer: %s", buffer);
//
//        // Handle: CMD<blank><Command_String>\n
//        if (strncmp(buffer, "CMD ", 4) == 0) {
//            // Extract the command string
//            char *command = buffer + 4;
//            command[strcspn(command, "\n")] = '\0'; // Remove the newline character
//            printf("Command Received: %s\n", command);
//           //check for pipes 
//            if (strchr(command, '|') !=NULL) {
                // validatePipes(command, psd);
                // continue;
              //}
//            if (!validateCommand(command, psd)){
//              continue;// skip invalid commands

//             }
//            // validateCommand
//            validateCommand(command, psd); // validation for debugging
//
//            // Tokenize the command into arguements
//            //char *cmd_args[10];
//            //int i = 0;
////
//            //cmd_args[i] = strtok(command, " ");
//            //while (cmd_args[i] != NULL && i < 9)
//            //{
//            //    i++;
//            //    cmd_args[i] = strtok(NULL, " ");
//            //}
//            //cmd_args[i] = NULL;
//            // Allocate memory for LogRequestArgs
//            LogRequestArgs *args = (LogRequestArgs *)malloc(sizeof(LogRequestArgs));
//            if (args == NULL) {
//                perror("Error allocating memory for log request");
//                continue;
//            }
//            // Copy the request and client information to the struct
//            strncpy(args->request, command, sizeof(args->request) - 1);
//            args->from = from;
//
//            // Create a new thread for logging
//            if (pthread_create(&p, NULL, logRequest, (void *)args) != 0) {
//                perror("Error creating log request thread");
//                free(args); // Free the allocated memory if thread creation fails
//            } else {
//                pthread_detach(p); // Detach the thread to allow it to run independently
//            }
//
//            // Create pipes for communication between parent and child
//            if (pipe(pipefd_stdout) == -1 || pipe(pipefd_stdin) == -1) {
//                perror("pipe");
//                exit(EXIT_FAILURE);
//            }
//
//            // Fork a child process to execute the command
//            if ((pid = fork()) == -1)
//            {
//                perror("fork");
//                exit(EXIT_FAILURE);
//            }
//
//            if (pid == 0) {
//                // Child process
//                close(pipefd_stdout[0]); // Close the read end of the stdout pipe
//                close(pipefd_stdin[1]);  // Close the write end of the stdin pipe
//                dup2(pipefd_stdout[1], STDOUT_FILENO); // Redirect stdout to the write end of the stdout pipe
//                dup2(pipefd_stdout[1], STDERR_FILENO); // Redirect stderr to the write end of the stdout pipe
//                dup2(pipefd_stdin[0], STDIN_FILENO);   // Redirect stdin to the read end of the stdin pipe
//                close(pipefd_stdout[1]);
//                close(pipefd_stdin[0]);
//
//                    /// tokenize the command string into arguments
//                    // char *args[10];
//                    // int i = 0;
//                    // args[i] = strtok(command, " ");
//                    // while (args[i] != NULL && i < 9) {
//                    //   i++;
//                    //   args[i] = strtok(NULL, " ");
//                    // }
//
//                // execute the command
//                // if (execvp(args[0], args) == -1) {
//                if (execvp(cmd_args[0], cmd_args))
//                {
//                    perror("execvp failed");
//                    exit(EXIT_FAILURE);
//                } // exit(EXIT_SUCCESS);
//            } else  {
//                // Parent process
//                close(pipefd_stdout[1]); // Close the write end of the stdout pipe
//                close(pipefd_stdin[0]);  // Close the read end of the stdin pipe
//                // commandRunning = 1;
//
//                // Read the child's output from the pipe and send it to the client socket
//                while ((bytesRead = read(pipefd_stdout[0], buffer, sizeof(buffer) - 1)) > 0) {
//                    buffer[bytesRead] = '\0';
//                    send(psd, buffer, bytesRead, 0);
//                }
//
//                close(pipefd_stdout[0]);
//
//                // Wait for the child process to finish
//                waitpid(pid, NULL, 0);
//                // commandRunning = 0;
//
//                pid = -1;
//
//                if (send(psd, "\n# ", sizeof("\n# "), 0) < 0) {
//                    perror("sending stream message");
//                } else {
//                    printf("sent # to clients s4"); // TODO: DELETE AFTER TESTING
//                }
//                // close(pipefd_stdout[0]);
//            }
//            // // Send # to indicate the end of the command output
//
//            if (send(psd, "\n# ", sizeof("\n# "), 0) < 0) {
//                perror("sending stream message");
//            } else {
//                printf("sent over to client  #3 ");
//            }
//        } 
//        else if (strncmp(buffer, "CTL ", 4) == 0) {
//            // Handle: CTL<blank><char[c|z|d]>\n
            char controlChar = buffer[4];
            if (controlChar == 'c') {
                if (pid > 0) {
                    kill(pid, SIGINT);
                    printf("Sent SIGINT to child process %d\n", pid);
                } else {
                    printf("No process to send SIGINT\n");
                }
            } else if (controlChar == 'z') {
                if (pid > 0) {
                    kill(pid, SIGINT);
                    printf("Sent SIGTSTP to child process %d\n", pid);
                } else {
                    printf("No process to send SIGINT\n");
                }
            } else if (controlChar == 'd') {
                // if (pipefd_stdin[1] != -1) {
                close(pipefd_stdin[1]);
                // pipefd_stdin[1] = -1;
                printf("Closed write end of stdin pipe\n");
                pthread_exit(NULL);
                //}
                // commandRunning = 0;
            }
            } else {
//                // Handle plain text input
//                // if (commandRunning) {
//                // Write the plain text to the stdin of the running command
//                // write(pipefd_stdin[1], buffer, bytesRead);
//                //}
//                // else {
//                // If no command is running, send an error message
//            const char *errorMsg = "Error: No command is currently running.\n#";
//            send(psd, errorMsg, strlen(errorMsg), 0);
//        }
//    }
//    pthread_exit(NULL);
//}
//
void *serveClient(void *args) {
    ClientArgs *clientArgs = (ClientArgs *)args;
    int psd = clientArgs->psd;
    struct sockaddr_in from = clientArgs->from;
    char buffer[BUFFER_SIZE];
    int bytesRead;
    pthread_t p;
    pid_t pid = -1;

    // Send initial prompt
    if (send(psd, PROMPT, sizeof(PROMPT), 0) < 0) {
        perror("Sending Prompt");
        close(psd);
        free(clientArgs);
        pthread_exit(NULL);
    }

    for (;;) {
        printf("\n...server is waiting...\n");
        cleanup(buffer);

        bytesRead = recv(psd, buffer, sizeof(buffer), 0);
        if (bytesRead <= 0) {
            if (bytesRead == 0) {
                printf("Connection closed by client\n");
            } else {
                perror("Error receiving stream message\n");
            }
            close(psd);
            free(clientArgs);
            pthread_exit(NULL);
        }

        buffer[bytesRead] = '\0';
        printf("Received Buffer: %s", buffer);

        if (strncmp(buffer, "CMD ", 4) == 0) {
            char *command = buffer + 4;
            command[strcspn(command, "\n")] = '\0';
            printf("Command Received: %s\n", command);

            if (strlen(command) == 0) {
                send(psd, "\n# ", 3, 0);
                continue;
            }

            // Check for pipe
            if (strchr(command, '|') != NULL) {
                validatePipes(command, psd);
                send(psd, "\n# ", 3, 0);
                continue;
            }
            
            // First validate the command
            if (!validateCommand(command, psd)) {
                printf("Invalid command: %s\n", command);
                send(psd, "\n# ", 3, 0);
                continue;  // Skip invalid commands
            }

            // Log the command
            LogRequestArgs *args = (LogRequestArgs *)malloc(sizeof(LogRequestArgs));
            if (args != NULL) {
                strncpy(args->request, command, sizeof(args->request) - 1);
                args->from = from;
                if (pthread_create(&p, NULL, logRequest, (void *)args) == 0) {
                    pthread_detach(p);
                } else {
                    free(args);
                }
            }

            // Parse command and check for redirections
            char *cmd_args[MAX_ARGS];
            int i = 0;
            char *input_file = NULL;
            char *output_file = NULL;
            
            // Tokenize the command
            cmd_args[i] = strtok(command, " ");
            while (cmd_args[i] != NULL && i < MAX_ARGS - 1) {
                if (strcmp(cmd_args[i], ">") == 0) {
                    cmd_args[i] = NULL;
                    output_file = strtok(NULL, " ");
                    break;
                } else if (strcmp(cmd_args[i], "<") == 0) {
                    cmd_args[i] = NULL;
                    input_file = strtok(NULL, " ");
                    break;
                }
                i++;
                cmd_args[i] = strtok(NULL, " ");
            }
            cmd_args[i] = NULL;

            // Create pipe for output
            int pipe_fd[2];
            if (pipe(pipe_fd) == -1) {
                perror("pipe");
                continue;
            }

            pid = fork();
            if (pid == -1) {
                perror("fork");
                close(pipe_fd[0]);
                close(pipe_fd[1]);
                continue;
            }

            if (pid == 0) { // Child process
                close(pipe_fd[0]);

                // Handle input redirection
                if (input_file != NULL) {
                    int fd = open(input_file, O_RDONLY);
                    if (fd == -1) {
                        perror("open input file");
                        exit(EXIT_FAILURE);
                    }
                    dup2(fd, STDIN_FILENO);
                    close(fd);
                }

                // Handle output redirection
                if (output_file != NULL) {
                    int fd = open(output_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
                    if (fd == -1) {
                        perror("open output file");
                        exit(EXIT_FAILURE);
                    }
                    dup2(fd, STDOUT_FILENO);
                    close(fd);
                } else {
                    dup2(pipe_fd[1], STDOUT_FILENO);
                }
                
                dup2(pipe_fd[1], STDERR_FILENO);
                close(pipe_fd[1]);

                execvp(cmd_args[0], cmd_args);
                perror("execvp failed");
                exit(EXIT_FAILURE);
            }

            // Parent process
            close(pipe_fd[1]);

            // Read and send output
            char read_buffer[BUFFER_SIZE];
            ssize_t bytes_read;
            while ((bytes_read = read(pipe_fd[0], read_buffer, sizeof(read_buffer) - 1)) > 0) {
                read_buffer[bytes_read] = '\0';
                send(psd, read_buffer, bytes_read, 0);
            }

            close(pipe_fd[0]);
            waitpid(pid, NULL, 0);
            pid = -1;

            // Send prompt
            send(psd, "\n# ", 3, 0);
        }
        // Handle control commands
        else if (strncmp(buffer, "CTL ", 4) == 0) {
            // ... (rest of control handling remains the same)
        }
    }
    return NULL;
}
void reusePort(int s)
{
    int one = 1;

    if (setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (char *)&one, sizeof(one)) == -1)
    {
        printf("error in setsockopt,SO_REUSEPORT \n");
        exit(-1);
    }
}
/*void handle_pipe(char **cmd_args_left, char **cmd_args_right, int psd){
    int pipe_fd[2];
    pid_t pid1, pid2;
    char buffer[BUFFER_SIZE];
    ssize_t bytesRead;

    if (pipe(pipe_fd) == -1){
        perror("pipe");
        return;
    }

    pid1 = fork(); // Left command
    if (pid1 == 0) {
        // First child (left side of the pipe)
        dup2(pipe_fd[1], STDOUT_FILENO); // Redirect stdout to the pipe
        close(pipe_fd[0]);
        close(pipe_fd[1]);

        // Apply redirection and execute left command
        apply_redirections(cmd_args_left, psd);
        if (execvp(cmd_args_left[0], cmd_args_left) == -1) {
            perror("execvp failed for the left side of the pipe");
            exit(EXIT_FAILURE);
        }
    }

    pid2 = fork(); // Right command
    if (pid2 == 0) {
        // Second child (right side of the pipe)
        dup2(pipe_fd[0], STDIN_FILENO); // Redirect stdin to the pipe
        close(pipe_fd[1]);
        close(pipe_fd[0]);

        // Apply redirection and execute right command
        apply_redirections(cmd_args_right, psd);
        if (execvp(cmd_args_right[0], cmd_args_right) == -1) {
            perror("execvp failed for the right side of the pipe");
            exit(EXIT_FAILURE);
        }
    }

    // Parent process closes pipe and waits for children to finish
    close(pipe_fd[1]); // Close the write end of the pipe

    // Read from the pipe and send output to the client socket
    while ((bytesRead = read(pipe_fd[0], buffer, sizeof(buffer))) > 0) {
        if (send(psd, buffer, bytesRead, 0) < 0) {
            perror("Error sending output to client");
        }
    }

    close(pipe_fd[0]); // Close the read end of the pipe
    waitpid(pid1, NULL, 0);
    waitpid(pid2, NULL, 0);
}*/
void handle_pipe(char **commands, int psd) {
    int pipe_fd[2];
    pid_t pid1, pid2;

    // Debugging: Print the commands being processed
    printf("Debug: commands[0] = %s\n", commands[0]);
    printf("Debug: commands[1] = %s\n", commands[1]);

    if (pipe(pipe_fd) == -1) {
        perror("pipe");
        send_psd(psd, "Error creating pipe\n# ", 21, 0);
        return;
    }

    // Fork the first child to run the left command
    if ((pid1 = fork()) == -1) {
        perror("fork");
        return;
    }
    if (pid1 == 0) {
        dup2(pipe_fd[1], STDOUT_FILENO);
        close(pipe_fd[0]);
        close(pipe_fd[1]);

        // Tokenize and execute the left command
        char **left_cmd = split_string(commands[0]);
        printf("Debug: Executing left command: %s\n", left_cmd[0]);

        if (execvp(left_cmd[0], left_cmd) == -1) {
            perror("execvp failed for left command");  // Error message if execvp fails
            exit(EXIT_FAILURE);
        }
    }

    // Fork the second child to run the right command
    if ((pid2 = fork()) == -1) {
        perror("fork");
        return;
    }
    if (pid2 == 0) {
        dup2(pipe_fd[0], STDIN_FILENO);
        close(pipe_fd[1]);
        close(pipe_fd[0]);

        // Tokenize and execute the right command
        char **right_cmd = split_string(commands[1]);
        printf("Debug: Executing right command: %s\n", right_cmd[0]);

        if (execvp(right_cmd[0], right_cmd) == -1) {
            perror("execvp failed for right command");  // Error message if execvp fails
            exit(EXIT_FAILURE);
        }
    }

    close(pipe_fd[0]);
    close(pipe_fd[1]);

    waitpid(pid1, NULL, 0);
    waitpid(pid2, NULL, 0);

    send_psd(psd, "\n# ", 3, 0);
}
// signal handler for sigtstp ctrl z
void sigint_handler(int sig)
{
    if (fg_pid > 0)
    {
        // if there is a foreground process , send sigint to it
        kill(fg_pid, SIGINT);
    }

    // write(STDOUT_FILENO, "\n", 1);

    // write(STDOUT_FILENO, "# ", 2);
    //  handle ctrz z prevent stopping the shell
    // printf("\nCaught SIGINT(Ctrl-C), but the shell is not stopped.\n#" );
    write(STDOUT_FILENO, "\n# ", 3);
    // tcflush(STDIN_FILENO, TCIFLUSH);
    fflush(stdout); // ensure the prompt is displayed immediately after the message
}
// signal handler for sigtstp ctrl z
void sigtstp_handler(int sig)
{
    if (fg_pid > 0)
    {
        // if there is a foreground process , send sigint to it
        kill(fg_pid, SIGTSTP);

        // find the command associat with the foreground process and add it
        for (int i = 0; i < job_count; i++)
        {
            if (jobs[i].pid == fg_pid)
            {
                jobs[i].is_running = 0;
                // printf("\n[%d]      Stopped %s\n" ,jobs[i].job_id, jobs[i].command);
                break;
            }
        }

        fg_pid = -1; // clear the foreground process

        // reprint the print
        // took these out becaue I didnt need a two new prompt lines
        // write(STDOUT_FILENO, "# ", 2);
        // tcflush(STDIN_FILENO, TCIFLUSH);
        // printf("# ");
        fflush(stdout); // ensure the prompt is displayed immediately after the message
    }
}
// signal handler for sigtstp ctrl z
void sigchld_handler(int sig)
{
    int status;
    pid_t pid;
    // Clean up zombie child processes non blocking wait
    while ((pid = waitpid(-1, &status, WNOHANG)) > 0)
    {
        for (int i = 0; i < job_count; i++)
        {
            if (jobs[i].pid == pid)
            {
                if (WIFEXITED(status) || WIFSIGNALED(status))
                {
                    // print done messageif needed
                    printf("\n[%d] Done %s\n", jobs[i].job_id, jobs[i].command);
                    // remove the job from the job list
                    for (int j = i; j < job_count - 1; j++)
                    {
                        jobs[j] = jobs[j + 1];
                    }
                    job_count--;
                }
                break;
            }
        }
    }
}
void apply_redirections(char **cmd_args, int psd) {
    int i = 0;
    int in_fd = -1, out_fd = -1, err_fd = -1;
    char *input_file = NULL;
    char *output_file = NULL;
    char *error_file = NULL;

    while (cmd_args[i] != NULL) {
        if (strcmp(cmd_args[i], "<") == 0 && cmd_args[i + 1] != NULL) {
            input_file = strdup(cmd_args[i +1]);
            cmd_args[i] = NULL;
            i++;
        }
        else if (strcmp(cmd_args[i], ">") == 0 && cmd_args[i + 1] != NULL) {
            output_file = strdup(cmd_args[i +1]);
            cmd_args[i] = NULL;
            i++;
        } else if (strcmp(cmd_args[i], "2>") == 0 && cmd_args[i +1] != NULL) {
            error_file = strdup(cmd_args[i +1]);
            cmd_args[i] = NULL;
            i++;
        }
    }
    if (input_file != NULL) {
        in_fd = open(input_file, O_RDONLY); // input redirection
        if (in_fd < 0)
        {
            cmd_args[i] = NULL;
            perror("Error opening input file Redirection");
            send(psd, "Error opening input file \n#", 30, 0);
            exit(EXIT_FAILURE); // exit child process on failure
        }
        dup2(in_fd, STDIN_FILENO); // replace stdin with the file // socket file descriptor
        close(in_fd);
    }

    if (output_file != NULL) {
        out_fd = open(output_file, O_WRONLY | O_CREAT | O_TRUNC, 0644); // added 0666
        if (out_fd < 0)
        {
            perror("Error redirection > opening output file");
            send(psd, "Error opening output file \n#", 29, 0);
            exit(EXIT_FAILURE);
            // return;
        }
        // debugging
        printf("redirection output file : %s\n", cmd_args[i + 1]);
        dup2(out_fd, STDOUT_FILENO);
        close(out_fd);

    }

    if (error_file != NULL) {
        err_fd = open(error_file, O_WRONLY | O_CREAT | O_TRUNC,0666);
        if (err_fd < 0)
        {
            perror("redirection 2> error opening error file");
            send(psd, "Error opening error file \n#", 25, 0);
            exit(EXIT_FAILURE);
            // return;
        }
        // printf("redirection output file : %s\n" , cmd_args[i+1]);
        dup2(err_fd, STDERR_FILENO);
        close(err_fd);

    }

    execvp(cmd_args[0], cmd_args);

    // remove null entries from cmd_args
    //int write_index = 0;
    //for (int read_index = 0; cmd_args[read_index] != NULL; read_index++) {
    //    if (cmd_args[read_index] != NULL) 
    //    {
    //        cmd_args[write_index] = cmd_args[read_index];
    //        write_index;
    //    }
    //}
    //cmd_args[write_index] = NULL;

    /*if (strcmp(cmd_args[i], "<") == 0)
    {
        in_fd = open(cmd_args[i + 1], O_RDONLY); // input redirection
        if (in_fd < 0)
        {
            cmd_args[i] = NULL;
            // cmd_args[i - 1] = NULL;
            perror("Error opening input file for Redirection < failed yashd");
            send(psd, "Error opening input file \n#", 25, 0);
            exit(EXIT_FAILURE); // exit child process on failure
            // return;
        }
        dup2(in_fd, STDIN_FILENO); // replace stdin with the file // socket file descriptor
        close(in_fd);
        // shift arguements left to remove redirecton operator and file name
        // doing this because err1.txt and err2.txt are not getting created for redirection
        //for (int j = i; cmd_args[j + 2] != NULL; j++)
        //{
        //    cmd_args[j] = cmd_args[j + 2];
        //}
        // shift remaing arguments
        cmd_args[i] = NULL; // remove <
        // cmd_args[i + 1] = NULL; // remove the file name
        i--; // adjust index to recheck this position
    }
    else if (strcmp(cmd_args[i], ">") == 0)
    {
        // output direction
        //out_fd = open(cmd_args[i + 1], O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH, 0666);
        out_fd = open(cmd_args[i + 1], O_WRONLY | O_CREAT | O_TRUNC, 0666); // added 0666
        if (out_fd < 0)
        {
            perror("Error redirection > opening output file");
            send(psd, "Error opening output file \n#", 26, 0);
            exit(EXIT_FAILURE);
            // return;
        }
        // debugging
        printf("redirection output file : %s\n", cmd_args[i + 1]);
        dup2(out_fd, STDOUT_FILENO);
        close(out_fd);
        // dup2(psd, STDOUT_FILENO); // also direct to client socket 
        // shift arguements left to remove redirecton operator and file name
        // doing this because err1.txt and err2.txt are not getting created for redirection
        //for (int j = i; cmd_args[j + 2] != NULL; j++)
        //{
        //    cmd_args[j] = cmd_args[j + 2];
        //}
        cmd_args[i] = NULL;
        // cmd_args[i + 1] = NULL;
        i--;
    }
    else if (strcmp(cmd_args[i], "2>") == 0)
    {
        // error redirection
        //err_fd = open(cmd_args[i + 1], O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH);
        err_fd = open(cmd_args[i + 1], O_WRONLY | O_CREAT | O_TRUNC,0666);
        if (err_fd < 0)
        {
            perror("redirection 2> error opening error file");
            send(psd, "Error opening error file \n#", 25, 0);
            exit(EXIT_FAILURE);
            // return;
        }
        // printf("redirection output file : %s\n" , cmd_args[i+1]);
        dup2(err_fd, STDERR_FILENO);
        close(err_fd);
        //dup2(psd, STDERR_FILENO); // Direct to client socket 
        // shift arguements left to remove redirecton operator and file name
        // doing this because err1.txt and err2.txt are not getting created for redirection
        //for (int j = i; cmd_args[j + 2] != NULL; j++)
        //{
        //    cmd_args[j] = cmd_args[j + 2];
        //}
        cmd_args[i] = NULL;
        // cmd_args[i + 1] = NULL;
        i--;
    }
    i++;*/
}
int validateCommand(const char *command, int psd)
{
    // check if input starts with a special character and skip 
    if (command[0]== '<' ||command[0]== '>' || command[0]== '|' || command[0]== '&') {
        const char *errorMsg = " Invalid command: commands cannot start with <>|& \n#";
        send(psd, errorMsg, strlen(errorMsg), 0);
        return 0;
    }

    // check for invalid combinations lif fg & and fg | echo
    if ((strstr(command, "fg &") != NULL) || (strstr(command, "fg | ") != NULL) || (strstr(command, "bg &") != NULL) || (strstr(command, "bg | ") != NULL))
    {
        // invalid combination, move to next line
        const char *errorMsg = "Invalid command combination fg &, fg |, bg &, bg |\n# ";
        send(psd, errorMsg, strlen(errorMsg), 0);
        return 0; // skp to the next iteration if no command was entered
    }

    // Parse command into arguements // execute command ifno special symbols are found 
    char *cmd_args[MAX_ARGS];
    int i = 0;
    char *command_copy = strdup(command);

    cmd_args[i] =  strtok(command_copy, " ");
    while (cmd_args[i] != NULL && i < MAX_ARGS -1) {
        i++;
        cmd_args[i] = strtok(NULL, " ");
    }
    cmd_args[i] = NULL;
    
    // create pipes for captureing error output
    int error_pipe[2];
    if (pipe(error_pipe) == -1) {
        perror("Pipe error");
        free(command_copy);
        return 0; // changed form return to return 0
    }
    
    //if (strcmp(command, "jobs") == 0)
    //// allow job command to list running or stopped jobs 
    //{
    //    print_jobs();
    //    send(psd, "\n# ", strlen("\n# "), 0);
    //    return;
    //}

    pid_t pid = fork();

    if (pid ==0) {
        // child process 
        close(error_pipe[0]);
        dup2(error_pipe[1], STDERR_FILENO); // redirect stderr to pipe
        close(error_pipe[1]);

        execvp(cmd_args[0], cmd_args);
        // if execvp returns there was an error 
        char error_msg[256];
        snprintf(error_msg, sizeof(error_msg), "Command '%s' not found \n", cmd_args[0]);
        write(STDERR_FILENO, error_msg, strlen(error_msg));
        exit(EXIT_FAILURE);
    } else if(pid > 0) {
        // Parent process
        close(error_pipe[1]);

        // read any error output
        char error_buffer[1024] = {0};
        ssize_t bytes_read = read(error_pipe[0],error_buffer, sizeof(error_buffer) -1);
        close(error_pipe[0]);

        int status;
        waitpid(pid, &status, 0);

        if (WIFEXITED(status) && WEXITSTATUS(status) != 0) {
            // command was invalid or failed 
            send(psd, error_buffer, strlen(error_buffer), 0); // 
            send(psd, "\n# ", 3 , 0); // 
            free(command_copy);
            return 0;
        }
    }
    free(command_copy);
    return -1; // added return 1 for success

}
/*int validateCommand(const char *command, int psd) {
    // Check if command starts with special characters
    if (command[0] == '<' || command[0] == '>' || command[0] == '|' || command[0] == '&') {
        const char *errorMsg = "Invalid command: commands cannot start with <>|&\n# ";
        send(psd, errorMsg, strlen(errorMsg), 0);
        return 0;  // Changed from return; to return 0
    }

    // Check for invalid combinations
    if (strstr(command, "fg &") != NULL || strstr(command, "fg |") != NULL) {
        const char *errorMsg = "Invalid command: fg cannot be used with & or |\n# ";
        send(psd, errorMsg, strlen(errorMsg), 0);
        return 0;  // Changed from return; to return 0
    }

    // Parse command into arguments
    char *cmd_args[MAX_ARGS];
    int i = 0;
    char *cmd_copy = strdup(command);  // Make a copy as strtok modifies the string
    
    cmd_args[i] = strtok(cmd_copy, " ");
    while (cmd_args[i] != NULL && i < MAX_ARGS - 1) {
        i++;
        cmd_args[i] = strtok(NULL, " ");
    }
    cmd_args[i] = NULL;

    // Create pipes for capturing error output
    int error_pipe[2];
    if (pipe(error_pipe) == -1) {
        perror("pipe");
        free(cmd_copy);
        return 0;  // Changed from return; to return 0
    }

    pid_t pid = fork();
    if (pid == 0) {
        // Child process
        close(error_pipe[0]);
        dup2(error_pipe[1], STDERR_FILENO);  // Redirect stderr to pipe
        close(error_pipe[1]);

        execvp(cmd_args[0], cmd_args);
        // If execvp returns, there was an error
        char error_msg[256];
        snprintf(error_msg, sizeof(error_msg), "Command '%s' not found\n", cmd_args[0]);
        write(STDERR_FILENO, error_msg, strlen(error_msg));
        exit(EXIT_FAILURE);
    } else if (pid > 0) {
        // Parent process
        close(error_pipe[1]);
        
        // Read any error output
        char error_buffer[1024] = {0};
        ssize_t bytes_read = read(error_pipe[0], error_buffer, sizeof(error_buffer) - 1);
        close(error_pipe[0]);

        int status;
        waitpid(pid, &status, 0);

        if (WIFEXITED(status) && WEXITSTATUS(status) != 0) {
            // Command was invalid or failed
            send(psd, error_buffer, strlen(error_buffer), 0);
            send(psd, "\n# ", 3, 0);
            free(cmd_copy);
            return 0;
        }
    }

    free(cmd_copy);
    return 1;  // Added return 1 for success
}
*/
int recData(int psd, char *buffer)
{
    int bytesRead = recv(psd, buffer, BUFFER_SIZE, 0);
    if (bytesRead <= 0)
    {
        if (bytesRead == 0)
        {
            // connection closed by the client
            printf("Connection closed by client");
        }
        else
        {
            perror("Error receiving stream message\n");
        }
        close(psd);
        return -1;
    }
    buffer[bytesRead] = '\0';
    printf("Received buffer: %s", buffer);
    return bytesRead;
}
void validatePipes(const char *command, int psd) {
    char *command_copy = strdup(command);
    if (!command_copy) {
        send(psd, "Memory allocation error\n# ", 25, 0);
        return;
    }

    // Locate the pipe position and split the command
    char *pipe_position = strchr(command_copy, '|');
    if (!pipe_position) {
        printf("Debug: No pipe found in command.\n");
        free(command_copy);
        return;
    }

    *pipe_position = '\0';
    char *left_cmd = command_copy;
    char *right_cmd = pipe_position + 1;

    // Trim leading whitespace
    while (*left_cmd == ' ') left_cmd++;
    while (*right_cmd == ' ') right_cmd++;

    char **left_args = split_string(left_cmd);
    char **right_args = split_string(right_cmd);

    // Debugging output for tokenized commands
    printf("Debug: Left command: %s\n", left_args[0]);
    for (int i = 0; left_args[i] != NULL; i++) {
        printf("left_args[%d]: %s\n", i, left_args[i]);
    }

    printf("Debug: Right command: %s\n", right_args[0]);
    for (int i = 0; right_args[i] != NULL; i++) {
        printf("right_args[%d]: %s\n", i, right_args[i]);
    }

    handle_pipe((char *[]){left_cmd, right_cmd, NULL}, psd);
    free(command_copy);
}
void send_psd(int fd, const char *buf, size_t len, int flags) {
    send(fd, buf, len, flags);
}
char **tokenize_command(char *command) {
    char **tokens = malloc(MAX_ARGS * sizeof(char *));
    if (tokens == NULL) {
        perror("Memory allocation error");
        exit(EXIT_FAILURE);
    }

    int i = 0;
    char *token = strtok(command, " \t");
    while (token != NULL && i < MAX_ARGS - 1) {
        tokens[i] = token;
        i++;
        token = strtok(NULL, " \t");
    }
    tokens[i] = NULL;

    return tokens;
}
int find_pipe_index(char **args) {
    int i;
    for (i = 0; args[i] != NULL; i++) {
        if (strcmp(args[i], "|") == 0) return i;
    }
    return -1;
}

char **split_string(char *str) {
    char **tokens = malloc(MAX_ARGS * sizeof(char *));
    if (tokens == NULL) {
        perror("Memory allocation error");
        exit(EXIT_FAILURE);
    }

    int i = 0;
    char *token = strtok(str, " \t");
    while (token != NULL && i < MAX_ARGS - 1) {
        tokens[i] = token;
        i++;
        token = strtok(NULL, " \t");
    }
    tokens[i] = NULL;

    return tokens;
}
void receive_output_from_server(int sockfd) {
    char buffer[BUFFER_SIZE];
    int bytes_received;
    
    printf("Debug: Receiving output from server...\n");
    while ((bytes_received = recv(sockfd, buffer, sizeof(buffer) - 1, 0)) > 0) {
        buffer[bytes_received] = '\0';
        printf("%s", buffer);
        fflush(stdout);
    }

    if (bytes_received == -1) {
        perror("Error receiving output from server");
    }
}
/*void handle_pipe(char **cmd_args_left, char **cmd_args_right, int psd){
    int pipe_fd[2];
    pid_t pid1, pid2;
    char buffer[BUFFER_SIZE];
    ssize_t bytesRead;

    if (pipe(pipe_fd) == -1){
        perror("pipe");
        return;
    }

    pid1 = fork(); // Left command
    if (pid1 == 0) {
        // First child (left side of the pipe)
        dup2(pipe_fd[1], STDOUT_FILENO); // Redirect stdout to the pipe
        close(pipe_fd[0]);
        close(pipe_fd[1]);

        // Apply redirection and execute left command
        apply_redirections(cmd_args_left, psd);
        if (execvp(cmd_args_left[0], cmd_args_left) == -1) {
            perror("execvp failed for the left side of the pipe");
            exit(EXIT_FAILURE);
        }
    }

    pid2 = fork(); // Right command
    if (pid2 == 0) {
        // Second child (right side of the pipe)
        dup2(pipe_fd[0], STDIN_FILENO); // Redirect stdin to the pipe
        close(pipe_fd[1]);
        close(pipe_fd[0]);

        // Apply redirection and execute right command
        apply_redirections(cmd_args_right, psd);
        if (execvp(cmd_args_right[0], cmd_args_right) == -1) {
            perror("execvp failed for the right side of the pipe");
            exit(EXIT_FAILURE);
        }
    }

    // Parent process closes pipe and waits for children to finish
    close(pipe_fd[1]); // Close the write end of the pipe

    // Read from the pipe and send output to the client socket
    while ((bytesRead = read(pipe_fd[0], buffer, sizeof(buffer))) > 0) {
        if (send(psd, buffer, bytesRead, 0) < 0) {
            perror("Error sending output to client");
        }
    }

    close(pipe_fd[0]); // Close the read end of the pipe
    waitpid(pid1, NULL, 0);
    waitpid(pid2, NULL, 0);
}*/


  if (fg_pid > 0) { 
        for (int i = 0; i < job_count; i++) {
            if (jobs[i].pid == fg_pid) {
                jobs[i].is_running = 0;
                jobs[i].is_stopped = 1;
                break;
            }
        }
        // clear the foreground process
        fg_pid = -1;

        // send a prompt to the cleint to indicate readiness for new input 
        if(send(psd, PROMPT, strlen(PROMPT), 0) < 0) {
            perror("Error sending prompt after ctrl z");
        }
        // Store the suspended process ID
        sus_pid = fg_pid;

        // Send a msg to the client indication process has been suspended
        const char *msg = "Process suspended type fg to resume \n#  ";
        send(psd, msg, strlen(msg), 0);
    }

    (strchr(command, '&') != NULL) { // check for background command ends with &
                // Parse and manage background commands ending with &
                command[strlen(command) - 1] = '\0';  // Remove '&'
                char *cmd_args[MAX_ARGS];
                int i =0;
                cmd_args[i] = strtok(command, " ");
                while (cmd_args[i] != NULL && i < MAX_ARGS -1)
                    {
                        i++;
                        cmd_args[i] = strtok(NULL, " ");
                    }
                // Check child pid 
                pid_t pid = fork();
                if (pid == 0) {
                    execvp(cmd_args[0], cmd_args);// execut the command in the background 
                    perror("execvp failed");
                    exit(EXIT_FAILURE);
                } else {
                    // Parent process 
                    add_job(pid, command, 1, 1); // Add job as background 
                    send(psd, PROMPT, strlen(PROMPT), 0); // Send prompt after command
                }
                continue;




// Moving code into functions 

// This is where the server will be set up yashd.c
// Troubleshooting the server
// 1. Error binding name to stream socket: Address already in use 
// Run: lsof -i :3820
// Run: kill -9 <PID>
void handle_control(int psd, char control, pid_t pid);
void handle_command(char *command, int psd, struct sockaddr_in from);
void handle_background_command(char *command, int psd);
void handle_pipe_command(char *command, int psd);
void handle_job_command(const char *command, int psd);
void handle_redirection(char **cmd_args, int psd, char *input_file, char *output_file);
// Function prototypes
void cleanup(char *buf);
void log_command(const char *command, struct sockaddr_in from);
void parse_command(char *command, char **cmd_args);
void *serveClient(void *args) {
    ClientArgs *clientArgs = (ClientArgs *)args;
    int psd = clientArgs->psd;
    struct sockaddr_in from = clientArgs->from;
    char buffer[BUFFER_SIZE];
    int bytesRead;

    // Send initial prompt
    send(psd, PROMPT, strlen(PROMPT), 0);

    for (;;) {
        cleanup(buffer);
        bytesRead = recv(psd, buffer, sizeof(buffer), 0);
        if (bytesRead <= 0) {
            close(psd);
            free(clientArgs);
            pthread_exit(NULL);
        }

        buffer[bytesRead] = '\0';
        printf("Received Buffer: %s", buffer);

        // Handle control commands (e.g., CTL c, CTL z)
        if (strncmp(buffer, "CTL ", 4) == 0) {
            handle_control(psd, buffer[4]);
            continue;
        }

        // Handle command input (starting with "CMD ")
        if (strncmp(buffer, "CMD ", 4) == 0) {
            char *command = buffer + 4;
            command[strcspn(command, "\n")] = '\0';
            handle_command(command, psd, from);
        }
    }
    return NULL;
}
void handle_command(char *command, int psd, struct sockaddr_in from) {
    // Validate the command
    if (!validateCommand(command, psd)) {
        send(psd, PROMPT, strlen(PROMPT), 0);
        return;
    }

    // Log the command
    log_command(command, from);

    // Check for job control commands
    if (strcmp(command, "jobs") == 0 || strncmp(command, "fg", 2) == 0 || strncmp(command, "bg", 2) == 0) {
        handle_job_command(command, psd);
        send(psd, PROMPT, strlen(PROMPT), 0);
        return;
    }

    // Check for background command
    if (strchr(command, '&') != NULL) {
        handle_background_command(command, psd);
        return;
    }

    // Check for pipe command
    if (strchr(command, '|') != NULL) {
        handle_pipe_command(command, psd);
        return;
    }

    // Parse and execute command (with redirection if necessary)
    char *cmd_args[MAX_ARGS];
    parse_command(command, cmd_args);
    handle_redirection(cmd_args, psd, NULL, NULL);
}
void handle_background_command(char *command, int psd) {
    command[strlen(command) - 1] = '\0'; // Remove the '&' character

    char *cmd_args[MAX_ARGS];
    parse_command(command, cmd_args);

    pid_t pid = fork();
    if (pid == 0) {
        // Child process
        setsid(); // Detach from terminal
        execvp(cmd_args[0], cmd_args);
        perror("execvp failed");
        exit(EXIT_FAILURE);
    } else if (pid > 0) {
        // Parent process
        add_job(pid, command, 1, 1);
        char msg[BUFFER_SIZE];
        snprintf(msg, BUFFER_SIZE, "[%d] %d\n", job_count, pid);
        send(psd, msg, strlen(msg), 0);
        send(psd, PROMPT, strlen(PROMPT), 0);
    } else {
        perror("fork failed");
    }
}
void handle_pipe_command(char *command, int psd) {
    char *left_cmd = strtok(command, "|");
    char *right_cmd = strtok(NULL, "|");

    if (!left_cmd || !right_cmd) {
        send(psd, "Error: Invalid pipe command\n# ", 29, 0);
        return;
    }

    char *cmd_args_left[MAX_ARGS];
    char *cmd_args_right[MAX_ARGS];
    parse_command(left_cmd, cmd_args_left);
    parse_command(right_cmd, cmd_args_right);

    handle_pipe(cmd_args_left, cmd_args_right, psd);
}
void handle_job_command(const char *command, int psd) {
    if (strcmp(command, "jobs") == 0) {
        print_jobs(psd);
    } else if (strncmp(command, "fg", 2) == 0) {
        int job_id = atoi(command + 3);
        fg_job(job_id, psd);
    } else if (strncmp(command, "bg", 2) == 0) {
        int job_id = atoi(command + 3);
        bg_job(job_id, psd);
    } else {
        send(psd, "Invalid job command\n# ", 22, 0);
    }
}


void parse_command(char *command, char **cmd_args) {
    int i = 0;
    cmd_args[i] = strtok(command, " ");
    while (cmd_args[i] != NULL && i < MAX_ARGS - 1) {
        i++;
        cmd_args[i] = strtok(NULL, " ");
    }
    cmd_args[i] = NULL;
}

void handle_pipe(char **cmd_args_left, char **cmd_args_right, int psd) {
    int pipe_fd[2];
    pid_t pid1, pid2;

    if (pipe(pipe_fd) == -1) {
        perror("pipe");
        send(psd, "Error creating pipe\n# ", 21, 0);
        return;
    }

    // First child for left command
    if ((pid1 = fork()) == -1) {
        perror("fork");
        close(pipe_fd[0]);
        close(pipe_fd[1]);
        return;
    }

    if (pid1 == 0) { // Child process
        close(pipe_fd[0]);  // Close read end of pipe

        // Check and apply redirections for left command
        int out_fd = -1, in_fd = -1;
        for (int i = 0; cmd_args_left[i] != NULL; i++) {
            if (strcmp(cmd_args_left[i], ">") == 0) {
                out_fd = open(cmd_args_left[i + 1], O_WRONLY | O_CREAT | O_TRUNC, 0644);
                if (out_fd == -1) {
                    perror("open output file");
                    exit(EXIT_FAILURE);
                }
                dup2(out_fd, STDOUT_FILENO);
                cmd_args_left[i] = NULL; // Remove redirection operator
            } else if (strcmp(cmd_args_left[i], "<") == 0) {
                in_fd = open(cmd_args_left[i + 1], O_RDONLY);
                if (in_fd == -1) {
                    perror("open input file");
                    exit(EXIT_FAILURE);
                }
                dup2(in_fd, STDIN_FILENO);
                cmd_args_left[i] = NULL; // Remove redirection operator
            }
        }

        // Redirect stdout to pipe if no file redirection
        if (out_fd == -1) dup2(pipe_fd[1], STDOUT_FILENO);
        
        close(pipe_fd[1]);

        execvp(cmd_args_left[0], cmd_args_left);
        perror("execvp failed for left command");
        exit(EXIT_FAILURE);
    }

    // Second child for right command
    if ((pid2 = fork()) == -1) {
        perror("fork");
        close(pipe_fd[0]);
        close(pipe_fd[1]);
        kill(pid1, SIGTERM);
        return;
    }

    if (pid2 == 0) { // Child process
        close(pipe_fd[1]);  // Close write end of pipe

        // Check and apply redirections for right command
        int in_fd = -1;
        for (int i = 0; cmd_args_right[i] != NULL; i++) {
            if (strcmp(cmd_args_right[i], "<") == 0) {
                in_fd = open(cmd_args_right[i + 1], O_RDONLY);
                if (in_fd == -1) {
                    perror("open input file");
                    exit(EXIT_FAILURE);
                }
                dup2(in_fd, STDIN_FILENO);
                cmd_args_right[i] = NULL; // Remove redirection operator
            }
        }

        // Set up output to go back to socket
        dup2(psd, STDOUT_FILENO);
        dup2(psd, STDERR_FILENO);

        // Redirect stdin to pipe if no file redirection
        if (in_fd == -1) dup2(pipe_fd[0], STDIN_FILENO);
        
        close(pipe_fd[0]);

        execvp(cmd_args_right[0], cmd_args_right);
        perror("execvp failed for right command");
        exit(EXIT_FAILURE);
    }

    // Parent process closes both ends of the pipe and waits for children
    close(pipe_fd[0]);
    close(pipe_fd[1]);
    waitpid(pid1, NULL, 0);
    waitpid(pid2, NULL, 0);

    send(psd, "\n# ", 3, 0); // Send prompt to client
}
void apply_redirections(char **cmd_args, int psd) {
    int i = 0;
    int in_fd = -1, out_fd = -1, err_fd = -1;
    char *input_file = NULL;
    char *output_file = NULL;
    char *error_file = NULL;

    while (cmd_args[i] != NULL) {
        if (strcmp(cmd_args[i], "<") == 0 && cmd_args[i + 1] != NULL) {
            input_file = strdup(cmd_args[i +1]);
            cmd_args[i] = NULL;
            i++;
        }
        else if (strcmp(cmd_args[i], ">") == 0 && cmd_args[i + 1] != NULL) {
            output_file = strdup(cmd_args[i +1]);
            cmd_args[i] = NULL;
            i++;
        } else if (strcmp(cmd_args[i], "2>") == 0 && cmd_args[i +1] != NULL) {
            error_file = strdup(cmd_args[i +1]);
            cmd_args[i] = NULL;
            i++;
        }
    }
    if (input_file != NULL) {
        in_fd = open(input_file, O_RDONLY); // input redirection
        if (in_fd < 0)
        {
            cmd_args[i] = NULL;
            perror("Error opening input file Redirection");
            send(psd, "Error opening input file \n#", 30, 0);
            exit(EXIT_FAILURE); // exit child process on failure
        }
        dup2(in_fd, STDIN_FILENO); // replace stdin with the file // socket file descriptor
        close(in_fd);
    }

    if (output_file != NULL) {
        out_fd = open(output_file, O_WRONLY | O_CREAT | O_TRUNC, 0644); // added 0666
        if (out_fd < 0)
        {
            perror("Error redirection > opening output file");
            send(psd, "Error opening output file \n#", 29, 0);
            exit(EXIT_FAILURE);
            // return;
        }
        // debugging
        printf("redirection output file : %s\n", cmd_args[i + 1]);
        dup2(out_fd, STDOUT_FILENO);
        close(out_fd);

    }

    if (error_file != NULL) {
        err_fd = open(error_file, O_WRONLY | O_CREAT | O_TRUNC,0666);
        if (err_fd < 0)
        {
            perror("redirection 2> error opening error file");
            send(psd, "Error opening error file \n#", 25, 0);
            exit(EXIT_FAILURE);
            // return;
        }
        // printf("redirection output file : %s\n" , cmd_args[i+1]);
        dup2(err_fd, STDERR_FILENO);
        close(err_fd);

    }

    execvp(cmd_args[0], cmd_args);

    // remove null entries from cmd_args
    int write_index = 0;
    for (int read_index = 0; cmd_args[read_index] != NULL; read_index++) {
        if (cmd_args[read_index] != NULL) 
        {
            cmd_args[write_index] = cmd_args[read_index];
            write_index;
        }
    }
    cmd_args[write_index] = NULL;
}
int validateCommand(const char *command, int psd) {
    
    if (strcmp(command, "jobs") == 0 
        || strncmp(command, "fg",2) == 0
        || strncmp(command, "bg",2) == 0) {
        return 1;
    } 

    // Check for invalid combinations
    if (strstr(command, "fg &") != NULL || strstr(command, "fg |") != NULL) {
        const char *errorMsg = "Invalid command: fg cannot be used with & or |\n# ";
        send(psd, errorMsg, strlen(errorMsg), 0);
        return 0;  // Changed from return; to return 0
    }
    // Check if command starts with special characters
    if (command[0] == '<' || command[0] == '>' || command[0] == '|' || command[0] == '&') {
        const char *errorMsg = "Invalid command: commands cannot start with <>|&\n# ";
        send(psd, errorMsg, strlen(errorMsg), 0);
        return 0;  // Changed from return; to return 0
    }


    // Parse command into arguments
    char *cmd_args[MAX_ARGS];
    int i = 0;
    char *cmd_copy = strdup(command);  // Make a copy as strtok modifies the string
    
    cmd_args[i] = strtok(cmd_copy, " ");
    while (cmd_args[i] != NULL && i < MAX_ARGS - 1) {
        i++;
        cmd_args[i] = strtok(NULL, " ");
    }
    cmd_args[i] = NULL;

    // Create pipes for capturing error output
    int error_pipe[2];
    if (pipe(error_pipe) == -1) {
        perror("pipe");
        free(cmd_copy);
        return 0;  // Changed from return; to return 0
    }

    pid_t pid = fork();
    if (pid == 0) {
        // Child process
        close(error_pipe[0]);
        dup2(error_pipe[1], STDERR_FILENO);  // Redirect stderr to pipe
        close(error_pipe[1]);

        execvp(cmd_args[0], cmd_args);
        // If execvp returns, there was an error
        char error_msg[256];
        snprintf(error_msg, sizeof(error_msg), "Command '%s' not found\n", cmd_args[0]);
        write(STDERR_FILENO, error_msg, strlen(error_msg));
        exit(EXIT_FAILURE);
    } else if (pid > 0) {
        // Parent process
        close(error_pipe[1]);
        
        // Read any error output
        char error_buffer[1024] = {0};
        ssize_t bytes_read = read(error_pipe[0], error_buffer, sizeof(error_buffer) - 1);
        close(error_pipe[0]);

        int status;
        waitpid(pid, &status, 0);

        if (WIFEXITED(status) && WEXITSTATUS(status) != 0) {
            // Command was invalid or failed
            send(psd, error_buffer, strlen(error_buffer), 0);
            send(psd, "\n# ", 3, 0);
            free(cmd_copy);
            return 0;
        }
    }

    free(cmd_copy);
    return 1;  // Added return 1 for success
}
void validatePipes(const char *command, int psd) {
    char *command_copy = strdup(command);
    if (!command_copy) {
        send(psd, "Memory allocation error\n# ", 25, 0);
        return;
    }

    // Find the pipe character
    char *pipe_pos = strchr(command_copy, '|');
    if (!pipe_pos) {
        free(command_copy);
        return;
    }

    // Split the command into left and right parts
    *pipe_pos = '\0';
    char *left_cmd = command_copy;
    char *right_cmd = pipe_pos + 1;

    // Remove leading/trailing spaces
    while (*left_cmd == ' ') left_cmd++;
    while (*right_cmd == ' ') right_cmd++;

    // Create argument arrays
    char *left_args[MAX_ARGS];
    char *right_args[MAX_ARGS];
    
    // Parse left command
    int i = 0;
    left_args[i] = strtok(left_cmd, " ");
    while (left_args[i] != NULL && i < MAX_ARGS - 1) {
        i++;
        left_args[i] = strtok(NULL, " \t");
    }
    left_args[i] = NULL;

    // Parse right command
    i = 0;
    right_args[i] = strtok(right_cmd, " ");
    while (right_args[i] != NULL && i < MAX_ARGS - 1) {
        i++;
        right_args[i] = strtok(NULL, " ");
    }
    right_args[i] = NULL;

    // Execute the pipe
    handle_pipe(left_args, right_args, psd);
    
    free(command_copy);
}
void print_jobs() {
    char buffer[BUFFER_SIZE];
    for (int i = 0; i < job_count; i++) {
        snprintf(buffer, BUFFER_SIZE, "[%d]%c %s %s%s\n" ,
        jobs[i].job_id,
        jobs[i].job_marker,
        jobs[i].is_running ? "Running" : "Stopped",
        jobs[i].command,
        jobs[i].is_background ? " &" : "");
    //for (int i = 0; i < job_count; i++) {
    //    printf(psd,"[%d]%c %s %s%s\n", jobs[i].job_id, jobs[i].job_marker, 
    //    jobs[i].is_running ? "Running" : "Stopped",  
    //    jobs[i].command, jobs[i].is_background ? " &" :"");
    //    //char job_marker = (i == job_count -1 ) ? '+' : '-';
    //    //if(jobs[i].is_running){    

        // Testing purposes to see in client 
        //jobs[i].is_running ? "Running" : "Stopped",  
        //jobs[i].command, jobs[i].is_background ? " &" :"");
        //printf("[%d]%c %s %s%s\n", jobs[i].job_id, jobs[i].job_marker, 
        // Send the job status to the client 
        send(psd, buffer, strlen(buffer), 0);
    }
}
void add_job(pid_t pid, const char *command, int is_running, int is_background) {
    if (job_count >= MAX_JOBS) {
        printf("Job list is full\n");
        return;
    }
    // Set the job marker for the new job as '+' and others as '-'
    for (int i = 0; i < job_count; i++) {
        jobs[i].job_marker = '-';
    }
    jobs[job_count].pid = pid;
    jobs[job_count].job_id = (job_count > 0) ? jobs[job_count - 1].job_id + 1 : 1;
    jobs[job_count].command = strdup(command);
    jobs[job_count].is_running = is_running;
    jobs[job_count].is_stopped = !is_running;
    jobs[job_count].is_background = is_background;
    jobs[job_count].job_marker = '+';  // Most recent job gets '+'
    job_count++;

    printf("Added job: [%d] %c %s %s\n",
           jobs[job_count - 1].job_id,
           jobs[job_count - 1].job_marker,
           is_running ? "Running" : "Stopped",
           command);

    
}
void bg_job(int job_id, int psd) {
    for (int i = 0; i < job_count; i++) {
        if (jobs[i].job_id == job_id && jobs[i].is_stopped) {
            jobs[i].is_running = 1;
            jobs[i].is_background = 1;
            jobs[i].is_stopped = 0;
            
            kill(-jobs[i].pid, SIGCONT);

            char buffer[BUFFER_SIZE];
            snprintf(buffer, BUFFER_SIZE, "[%d]%c %s &\n", jobs[i].job_id, jobs[i].job_marker, jobs[i].command);
            dprintf(psd, "%s# ", buffer); // prints teh command when bringing to the foreground
            
            return;
            //break;
        }
    }
    dprintf(psd, "Job not found\n");
}
void fg_job(int job_id, int psd) {
    // Used from from Dr.Y book Page-45-49 
    int status;

    for (int i = 0; i < job_count; i++) {
        if (jobs[i].job_id == job_id) {
            jobs[i].is_running = 1;
            fg_pid = jobs[i].pid;// Setting a foreground process
            
            //jobs[i].is_running = 1; // mark as running  
            // print to both client and server 
            dprintf(psd, "%s\n", jobs[i].command); // prints teh command when bringing to the foreground
            printf("%s\n", jobs[i].command); // 

            fflush(stdout); //ensure teh command is printed immediately 
            // brings the job to the foreground
            kill(-jobs[i].pid, SIGCONT);// wait for the process to cont. the stopped proces
            waitpid(jobs[i].pid, &status, WUNTRACED); // wait for the process to finish or be stopped again should it be zero or wuntrance

            fg_pid = -1; // Clear foreground process , no longer a fg process 

            // update job status if it was stopped again 
            if (WIFSTOPPED(status)) {
                jobs[i].is_running = 0; // mark a stopped 
            } else {
                // if the job finished remove it from the job list
                dprintf(psd, "[%d] Done %s\n", jobs[i].job_id, jobs[i].command);  
                printf("[%d] Done %s\n", jobs[i].job_id, jobs[i].command); 

                for (int j = i; j < job_count - 1; j++) {
                    jobs[j] = jobs[j + 1 ];
                }
                job_count--;
            }

            // Once completed mark the job as done or remove it 
            //printf("[%d] Done %s\n", jobs[i].job_id, jobs[i].command);
            //break;

            return;
        }
    }  
    dprintf(psd, "job not found\n");
    printf("Job not found\n");
}
void fg_job(int job_id, int psd) { // Used from from Dr.Y book Page-45-49 
    int status;

    for (int i = 0; i < job_count; i++) {
        if (jobs[i].job_id == job_id) {
            fg_pid = jobs[i].pid;// Setting a foreground process
            jobs[i].is_running = 1;
            //fg_pid = jobs[i].pid;// Setting a foreground process
            //jobs[i].is_running = 1; // mark as running  
            // print to both client and server 
            dprintf(psd, "%s\n", jobs[i].command); // prints teh command when bringing to the foreground
            printf("%s\n", jobs[i].command); // 

            fflush(stdout); //ensure teh command is printed immediately 
            // brings the job to the foreground
            kill(-jobs[i].pid, SIGCONT);// wait for the process to cont. the stopped proces
            waitpid(jobs[i].pid, &status, WUNTRACED); // wait for the process to finish or be stopped again should it be zero or wuntrance

            fg_pid = -1; // Clear foreground process , no longer a fg process 

            // update job status if it was stopped again 
            if (WIFSTOPPED(status)) {
                jobs[i].is_running = 0; // mark a stopped 
            } else {
                // if the job finished remove it from the job list
                dprintf(psd, "[%d] Done %s\n", jobs[i].job_id, jobs[i].command);  
                printf("[%d] Done %s\n", jobs[i].job_id, jobs[i].command); 

                for (int j = i; j < job_count - 1; j++) {
                    jobs[j] = jobs[j + 1 ];
                }
                job_count--;
            }

            // Once completed mark the job as done or remove it 
            //printf("[%d] Done %s\n", jobs[i].job_id, jobs[i].command);
            //break;

            return;
        }
    }  
    dprintf(psd, "job not found\n");
    printf("Job not found\n");
}
void bg_command(char **cmd_args) {
    pid_t pid = fork();
    if (pid == 0) {
        // Child process
        execvp(cmd_args[0], cmd_args);
        perror("execvp failed");  // If execvp fails
        exit(EXIT_FAILURE);
    } else if (pid > 0) {
        // Parent process
        add_job(pid, cmd_args[0], 1, 1);  // Mark as running and in background
        update_job_markers(job_count - 1);
    }
}
//void bg_command(char **cmd_args) {
//    pid_t pid = fork();
//    if (pid == 0) {
//        // Child process
//        execvp(cmd_args[0], cmd_args);
//        perror("execvp failed");  // If execvp fails
//        exit(EXIT_FAILURE);
//    } else if (pid > 0) {
//        // Parent process
//        add_job(pid, cmd_args[0], 1, 1);  // Mark as running and in background
//        update_job_markers(job_count - 1);
//    }
//}
void update_job_markers(int current_job_index) {
    for (int i = 0; i < job_count; i++) {
        //jobs[i].job_marker = ' ';
        if (i == current_job_index) {
            jobs[i].job_marker = '+';
        } else {
            jobs[i].job_marker = '-';
        }
    }
  
}
void setup_signal_handlers() {
    signal(SIGINT, sigint_handler);
    printf("Signal int handlers initialized.\n");

    signal(SIGTSTP, sigtstp_handler);
    printf("Signal tsp handlers initialized.\n");

    signal(SIGCHLD, sigchld_handler);
    printf("Signal child handlers initialized.\n");

    signal(SIGPIPE, sig_pipe);
    printf("Signal pipe handlers initialized.\n");
}
/**
 * @brief  If we are waiting reading from a pipe and
 *  the interlocutor dies abruptly (say because
 *  of ^C or kill -9), then we receive a SIGPIPE
 *  signal. Here we handle that.
 */
void sig_pipe(int n) 
{
   perror("Broken pipe signal");
   exit(1);
}
/**
 * @brief Handler for SIGCHLD signal 
// */
//void sig_chld(int n)
//{
//    int status;
//    fprintf(stderr, "Child terminated\n");
//    while (waitpid(-1, NULL, WNOHANG) > 0);
//    //wait(&status); /* So no zombies */
//}
// signal handler for sigtstp ctrl z
void sigint_handler(int sig)
{
    printf("SIGINT (Ctrl+C) handler called in daemon process\n");
    fflush(stdout);
    printf("sigint handler called\n");
    if (fg_pid > 0)
    {
        // if there is a foreground process , send sigint to it
        kill(fg_pid, SIGINT);
    }

    write(STDOUT_FILENO, "\n# ", 3);
    fflush(stdout); // ensure the prompt is displayed immediately after the message
}
// signal handler for sigtstp ctrl z
void sigtstp_handler(int sig)
{
    printf("SIGTSTP (Ctrl+Z) handler called in daemon process\n");
    fflush(stdout);
    printf("sigtstp handler called\n");
    if (fg_pid > 0)
    {
        // if there is a foreground process , send sigint to it
        kill(fg_pid, SIGTSTP);

        // find the command associat with the foreground process and add it
        for (int i = 0; i < job_count; i++)
        {
            if (jobs[i].pid == fg_pid)
            {
                jobs[i].is_running = 0;
                jobs[i].is_stopped = 1;
                jobs[i].job_marker = '+'; // set marker for the most recent job
                // printf("\n[%d]      Stopped %s\n" ,jobs[i].job_id, jobs[i].command);
                break;
            }
        }

        fg_pid = -1; // clear the foreground process
        dprintf(psd, "dprint\n# ");

        // Send a prompt to the client to indicate readiness for new input
        if (send(psd,  "\n# ", 3, 0) < 0) {
            perror("Error sending prompt after Ctrl Z");
        }

        fflush(stdout); // ensure the prompt is displayed immediately after the message
        //const char *msg = "Type fg to resume.\n";
        //send(psd, msg, strlen(msg), 0);
    }
}
// signal handler for sigtstp ctrl z
void sigchld_handler(int sig)
{
    printf("SIGCHLD handler called in daemon process\n");
    fflush(stdout);
    printf("SIGCHLD handler called\n");
    int status;
    pid_t pid;
    // Clean up zombie child processes non blocking wait
    while ((pid = waitpid(-1, &status, WNOHANG)) > 0)
    {
        for (int i = 0; i < job_count; i++)
        {
            if (jobs[i].pid == pid)
            {
                if (WIFEXITED(status) || WIFSIGNALED(status))
                {
                    // print done messageif needed
                    printf("\n[%d] Done %s\n", jobs[i].job_id, jobs[i].command);
                    // remove the job from the job list
                    for (int j = i; j < job_count - 1; j++)
                    {
                        jobs[j] = jobs[j + 1];
                    }
                    job_count--;
                }
                break;
            }
        }
    }
}

if (controlChar == 'c') {
                if (pid > 0) {
                    kill(pid, SIGINT);
                    const char *msg = "Sent sigint to foreground process.\n# ";
                    printf("Sent SIGINT to child process %d\n# ", pid);
                    send(psd, msg, strlen(msg), 0);
                } else {
                    const char *msg = "No foreground process to send sigint.\n";
                    send(psd, msg, strlen(msg), 0);
                    printf("No process to send SIGINT\n");
                }
            } else if (controlChar == 'z') {
                if (pid > 0) {
                    kill(pid, SIGTSTP);
                    const char *msg = "Sent SIGTSTP to foreground process.\n# ";
                    send(psd, msg, strlen(msg), 0);
                    printf("Sent SIGTSTP to child process %d\n", fg_pid);
                } else {
                    const char *msg = "No foreground process to send SIGTSTP.\n# ";
                    send(psd, msg, strlen(msg), 0);
                    printf("No process to send SIGINT\n");
                }
            } else if (controlChar == 'd') {
                // if (pipefd_stdin[1] != -1) {
                close(pipefd_stdin[1]);
                // pipefd_stdin[1] = -1;
                const char *msg = "Closed write end of the stdin pipe exiting thread.\n# ";
                send(psd, msg, strlen(msg), 0);
                printf("Closed write end of stdin pipe\n");
                pthread_exit(NULL);
                //}
                // commandRunning = 0;
            } else {
                const char *errorMsg = "Error: No command is currently running.\n#";
                send(psd, errorMsg, strlen(errorMsg), 0);
            }
        }