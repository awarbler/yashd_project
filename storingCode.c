void add_job(pid_t pid, const char *command, int is_running, int is_background);
void update_job_markers(int current_job_index) ;
void print_jobs();
void fg_job(int job_id);
void bg_job(int job_id);


int connected_socket(int lsd) ;
int u__listening_socket(const char *path);

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




  