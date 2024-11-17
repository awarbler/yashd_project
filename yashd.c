// This is where the server will be set up yashd.c
// Troubleshooting the server
// 1. Error binding name to stream socket: Address already in use 
// Run: lsof -i :3820 sudo lsof -i :3820
// Run: kill -9 <PID>

#include "yashd.h"
#include <bits/waitflags.h>

/* Initialize path variables */
static char* server_path = "./tmp/yashd";
char* log_path = "./tmp/yashd.log";
static char* pid_path = "./tmp/yashd.pid";
static char socket_path[PATHMAX + 1];

/* Initialize mutex lock */
pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

// Functions
void reusePort(int sock);
void *serveClient(void *args);
void *logRequest(void *args);
void log_command(const char *command, struct sockaddr_in from);

void apply_redirections(char *input_file, char *output_file, int psd);
void handle_pipe(char **cmd_args_left, char **cmd_args_right, int psd);
void validatePipes(const char *command, int psd);
int validateCommand(const char *command, int psd);
//void execute_command(char **cmd_args, char *input_file, char *output_file, int psd);

// jobs 
void bg_command(int job_id, int psd);
void fg_command(int job_id, int psd);
void print_jobs(int psd);
void update_job_status(pid_t pid, int is_running, int is_stopped);
void remove_job(pid_t pid);
void add_job(pid_t pid, const char *command, int is_background);

// Signals 
void sig_pipe(int n); 
void sig_chld(int n);
void handle_control(int psd, char control, pid_t pid);
void handle_cat_command(const char *command, int psd);
void cleanup(char *buf);

/* Struct for logRequest() thread */
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
    pid_t pid;          // Process id
    int job_id;         // Job Id
    char *command;      // Command string
    int is_running;     // 1 if running, 0 if stopped
    int is_stopped;     // 1 if stopped , 0 otherwise
    int is_background;  // 1 if background 0 if foreground
    char job_marker; // + or - or " "
} job_t;

job_t jobs[MAX_JOBS];
int job_count = 0;      // Number of active jobs
pid_t ch_pid = -1;      // child pid 
int next_job_id = 1;
int is_background = 0; 

void daemon_init(const char * const path, uint mask)
{
  pid_t pid;
  char buff[256];
  int fd;
  int k;

  /* put server in background (with init as parent) */
  if ((pid = fork() ) < 0 ) {
    perror("Error: daemon_init: cannot fork");
    exit(0);
  } else if (pid > 0) /* The parent */
    exit(0);

  /* the child */

  /* Close all file descriptors that are open */
  for (k = getdtablesize()-1; k>0; k--)
      close(k);

  /* Redirecting stdin, stdout, and stdout to /dev/null */
  if ((fd = open("/dev/null", O_RDWR)) < 0) {
    perror("Error:daemon_init: Open");
    exit(0);
  }
  dup2(fd, STDIN_FILENO);      /* detach stdin */
  dup2(fd, STDOUT_FILENO);     /* detach stdout */
  dup2(fd, STDERR_FILENO);     /* detach stderr */
  close (fd);

  /* Establish handlers for signals */
  if ( signal(SIGCHLD, sig_chld) < 0 ) {
    perror("Error:daemon_init: Signal SIGCHLD");
    exit(1);
  }
  if ( signal(SIGPIPE, sig_pipe) < 0 ) {
    perror("Error:daemon_init: Signal SIGPIPE");
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
    struct  hostent *hp, *gethostbyname();
    //struct  hostent *hp;
    struct sockaddr_in from;
    socklen_t  fromlen;
    socklen_t length;
    char ThisHost[80];

    /* get TCPServer1 Host information, NAME and INET ADDRESS */
    gethostname(ThisHost, MAXHOSTNAME);
    printf("----TCP/Server running at host NAME: %s\n", ThisHost);

    // TO-DO: Initialize the daemon
    // daemon_init(server_path, 0);
    // printf("Daemon initialzied.\n")
    

    if ((hp = gethostbyname(ThisHost)) == NULL ) {
      fprintf(stderr, "Error int main Can't find host %s\n", argv[1]);
      exit(-1);
    }

    hp = gethostbyname("localhost");
    if (hp == NULL) {
        perror("Error main getting hostname");
        exit(-1);
    }
    bcopy(hp->h_addr, &(server.sin_addr), hp->h_length);
    printf("(TCP/Server INET ADDRESS is: %s )\n", inet_ntoa(server.sin_addr));
    
    /** Construct name of socket */
    server.sin_family = AF_INET; 
    server.sin_port = htons(3820);    
    server.sin_addr.s_addr = htonl(INADDR_ANY);
     
    // Create the socket to send and redeived
    // this is the u__listening_socket (const char *path)
    sd = socket (AF_INET, SOCK_STREAM, IPPROTO_TCP); 
    if (sd < 0) {
	    perror("Error main opening stream socket");
	    exit(-1);
    }
    
    reusePort(sd); // add this to u__listening_socket 
    //  this is also in u__lostening socket 
    if (bind(sd,(struct sockaddr *) &server, sizeof(server) ) < 0 ) {
      close(sd);
      perror("Error main:  binding name to stream socket");
      exit(-1);
    }
    
    // Print out the server port
    length = sizeof(server);
    if (getsockname (sd, (struct sockaddr *)&server,&length) ) {
      perror("Error main: getting socket name");
      exit(0);
    }
    printf("Server Port is: %d\n", ntohs(server.sin_port));
    
    // listen for incoming connection
    listen(sd,4);
    fromlen = sizeof(from);
    printf("Server is ready to receive...........\n");

    for(;;){
        printf("Server is waiting.......\n");
        // Accept the new client connection 
        psd  = accept(sd,(struct sockaddr *)&from, &fromlen);

        if (psd < 0) {
            perror("Error Main Client Accepting connection");
            continue; // Retry the error 
        }

        // Allocate memory for ClientArgs
        ClientArgs *clientArgs = malloc(sizeof(ClientArgs));
        if (clientArgs == NULL) {
          perror("Error main allocating memory for client arguments");
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
        perror("Error main creating client thread");
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
void *serveClient(void *args) {
    ClientArgs *clientArgs = (ClientArgs *)args;
    int psd = clientArgs->psd;
    struct sockaddr_in from = clientArgs->from;
    char buffer[BUFFER_SIZE];
    int bytesRead;
    pid_t pid;

    // Send initial prompt
    if (send(psd, PROMPT, sizeof(PROMPT), 0) < 0) {
        //perror("Error serveClient: Sending Prompt");
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
        
        // Handle control commands directly
        if (strncmp(buffer, "CTL ", 4) == 0) {
            char controlChar = buffer[4];
            char msg[BUFFER_SIZE];
    
            if (controlChar == 'c') {
                // Handle Ctrl+C (SIGINT)
                if (ch_pid > 0) {
                    printf("Debug: Attempting to send SIGINT to PID: %d, PGID: %d\n", ch_pid, getpgid(ch_pid));
                   
                    if (kill(-ch_pid, SIGINT) == -1) {
                        perror("Error: kill failed for SIGINT");
                    } else {
                        printf("Debug: Sent SIGINT to fg process group [%d]\n", ch_pid);
                    }
                    snprintf(msg, sizeof(msg), "Sent SIGINT (Ctrl+C) to fg process [%d].\n# ", ch_pid);
                    send(psd, msg, strlen(msg), 0);
    
                    // Wait for the process to handle SIGINT
                    int status;
                    pid_t result = waitpid(ch_pid, &status, WUNTRACED );
                    if (result > 0 && WIFSIGNALED(status)) {
                        printf("Foreground process [%d] was terminated by signal %d.\n", ch_pid, WTERMSIG(status));
                        remove_job(ch_pid);
                        ch_pid = -1;
                    }
                } 
                else {
                    snprintf(msg, sizeof(msg), "No foreground process to send SIGINT (Ctrl+C).\n# ");
                    send(psd, msg, strlen(msg), 0);
                    printf("Debug: No process to send SIGINT.\n");
                }
            } else if (controlChar == 'z') {
                // Handle Ctrl+Z (SIGTSTP)
                if (ch_pid > 0) {
                    if (kill(-ch_pid, SIGTSTP) == -1) {
                        perror("Error: kill failed for SIGTSTP");
                    } else {
                        printf("Debug: Sent SIGTSTP to fg process group [%d]\n", ch_pid);
                        update_job_status(ch_pid, 0, 1); // Mark as stopped
                        }

                    int status;
                    pid_t result = waitpid(ch_pid, &status, WUNTRACED);

                    if (result > 0 && WIFSTOPPED(status)) {
                        printf("Foreground process [%d] was stopped (Ctrl+Z).\n", ch_pid);
                        update_job_status(ch_pid, 0, 1);
                        ch_pid = -1;
                    } else {
                        snprintf(msg, sizeof(msg), "No foreground process to send SIGTSTP (Ctrl+Z).\n# ");
                        send(psd, msg, strlen(msg), 0);
                        printf("Debug: No process to send SIGTSTP.\n");
                    }
                }
            } else if (controlChar == 'd') {
                // Handle Ctrl+D (EOF)
                snprintf(msg, sizeof(msg), "Received Ctrl+D. Closing connection.\n# ");
                printf("Debug: Received Ctrl+D from client. Closing connection.\n");
                send(psd, msg, strlen(msg), 0);
                close(psd);
                pthread_exit(NULL);
            } else {
                // Invalid control command
                snprintf(msg, sizeof(msg), "Error: Invalid control command received.\n# ");
                send(psd, msg, strlen(msg), 0);
                printf("Debug: Invalid control command received.\n");
            }
            continue; // Skip to the next iteration after handling control command
        }

        // Handle CMD
        if (strncmp(buffer, "CMD ", 4) == 0) {
            char *command = buffer + 4;
            command[strcspn(command, "\n")] = '\0';
            printf("Command Received: %s\n", command);

            // Check if command is cat > filename
            if (strncmp(command, "cat > ", 5) == 0) {
                // call the handle cat function 
                handle_cat_command(command, psd);
                continue; // Skip and wait for next command
            }
            // Check for pipe
            if (strchr(command, '|') != NULL) {
                // If command starts with a pipe, return an error message
                if (command[0] == '|') {
                    const char *errorMsg = "Invalid command: commands cannot start with a pipe (|)\n# ";
                    send(psd, errorMsg, strlen(errorMsg), 0);
                    continue;
                }
                // Log the command with pipe
                log_command(command, from);

                // Validate the command
                if (!validateCommand(command, psd)) {
                    continue; // Skip invalid commands
                }
                // Process the pipe command
                validatePipes(command, psd);
                send(psd, PROMPT, strlen(PROMPT), 0); // Send prompt back to the client
                continue;
            }
            // Jobs commands
            if (strcmp(command, "jobs") == 0) {
                print_jobs(psd);
                //send(psd, PROMPT, strlen(PROMPT), 0);
                continue;
            } else if (strncmp(command, "fg ", 3) == 0) {
                //int job_id = atoi(command + 3);
                fg_command(-1, psd);
                send(psd, PROMPT, strlen(PROMPT), 0);
                continue;
            } else if (strncmp(command, "bg ", 3) == 0) {
                //int job_id = atoi(command + 3);
                bg_command(-1, psd);
                send(psd, PROMPT, strlen(PROMPT), 0);
                continue;
            }
        
            // First validate the command
            if (!validateCommand(command, psd)) {
                send(psd, PROMPT, strlen(PROMPT), 0);
                continue;  // Skip invalid commands
            }
            // Log the command
            log_command(command, from);

            // Background & 
            // Check if the command should run in the background
            char *ampersand = strrchr(command, '&');
            if (ampersand && ampersand == command + strlen(command) - 1) {
                is_background = 1;
                *ampersand = '\0'; // Remove '&' character from the command
            } else {
                is_background = 0;
            }
 

            // Parse command and check / handle redirection
            char *cmd_args[MAX_ARGS];
            int i = 0;
            char *input_file = NULL;
            char *output_file = NULL;

            // Tokenize the command string
            char *token = strtok(command, " ");
            while (token != NULL && i < MAX_ARGS - 1) {
                if (strcmp(token, "<") == 0) {
                    // Input redirection
                    token = strtok(NULL, " ");
                    if (token != NULL) {
                        input_file = strdup(token);
                    }
                } else if (strcmp(token, ">") == 0) {
                    // Output redirection
                    token = strtok(NULL, " ");
                    if (token != NULL) {
                        output_file = strdup(token);
                    }
                } else {
                    // Regular command argument
                    cmd_args[i++] = token;
                }
                token = strtok(NULL, " ");
            }
            cmd_args[i] = NULL; // Null-terminate the argument list

            // Validate command arguments
            if (cmd_args[0] == NULL) {
                const char *errorMsg = "Error: Validate command Invalid command or empty input\n";
                send(psd, errorMsg, strlen(errorMsg), 0);
                return;
            }

            // Debugging: Print parsed arguments
            printf("Executing command: %s\n", cmd_args[0]);
            for (int j = 0; cmd_args[j] != NULL; j++) {
                printf("Argument %d: %s\n", j, cmd_args[j]);
            }
            //send(psd, PROMPT, strlen(PROMPT), 0); 

            // Fork the process and execute the command

            pid = fork(); // Create a new fork 
            
            if (pid == -1) {
                //perror("Error serveClient: Fork failed");
                return;
            }
            
            if (pid == 0) { // Child process
                // Set up signal handlers for the child process
                signal(SIGINT, SIG_DFL);
                signal(SIGTSTP, SIG_DFL);

                /// Redirect `stdout` and `stderr` to the client socket (`psd`)
                dup2(psd, STDOUT_FILENO); // Redirect stdout to socket
                dup2(psd, STDERR_FILENO); // Redirect stderr to socket

                
                // Set child in its own process group
                if (setpgid(0, 0) == -1) {
                    perror("Error: child process setpgid failed");
                    exit(EXIT_FAILURE);
                }
                printf("Child process: PID = %d, PGID = %d\n", getpid(), getpgid(0));
                // Apply input/output redirection if needed
                apply_redirections(input_file, output_file, psd);
                // Execute the command
                execvp(cmd_args[0], cmd_args);
                //perror("Error ServeClient: execvp failed");
                exit(EXIT_FAILURE);
            } 
            //if (pid > 0) { // Parent process
            ch_pid = pid;
            // Only transfer terminal control if child is in a different process group
            if (getpgid(0) != getpgid(pid)) {
                printf("Transferring terminal control to child process: PID = %d\n", pid);
                tcsetpgrp(STDIN_FILENO, ch_pid);
            }
            if (is_background) {
                // Rebuild the full command string from `cmd_args`
                char full_command[BUFFER_SIZE] = "";
                for (int j = 0; cmd_args[j] != NULL; j++) {
                    strcat(full_command, cmd_args[j]);
                    if (cmd_args[j + 1] != NULL) {
                        strcat(full_command, " ");
                    }
                }

                add_job(pid, full_command, 1);
                snprintf(buffer, sizeof(buffer), "Background job [%d] started: %s\n# ", pid, command);
                send(psd, buffer, strlen(buffer), 0);
            } else {
                ch_pid = pid; // Set foreground process
                int status;
                pid_t result = waitpid(pid, &status, WUNTRACED | WCONTINUED);
                tcsetpgrp(STDIN_FILENO, getpid()); // Restore terminal control to the server
                if (result == -1) {
                    //perror("Error ServeClient: waitpid failed");
                } else if (WIFSTOPPED(status)) {
                    update_job_status(ch_pid, 0, 1);
                } else {
                    remove_job(ch_pid);
                }
                ch_pid = -1;
            }

            //}
            // Send prompt
            send(psd, PROMPT, strlen(PROMPT), 0); 
        }
        // Handle control commands
        //else if (strncmp(buffer, "CTL ", 4) == 0) {
        //    char controlChar = buffer[4];
        //    // Handle Control +D
        //    if (controlChar == 'd') {
        //        printf("Recevied Ctrl+D from client. Closing connection.\n");
        //        // Send Message to the client
        //        const char *exitMsg = "Server rec. Ctrl+D. Clossing Connection.\n";
        //        send(psd, exitMsg, strlen(exitMsg), 0);
//
        //        // Close the client socket and exit the thread 
        //        close(psd);
        //        free(clientArgs);
        //        pthread_exit(NULL);
        //    }
        //    handle_control(psd, controlChar, ch_pid);
        //}    
        
    }
    return NULL;
}

void handle_pipe(char **cmd_args_left, char **cmd_args_right, int psd) {
    int pipe_fd[2];
    pid_t pid1, pid2;

    if (pipe(pipe_fd) == -1) {
        perror("Error handle_pipe: pipe creation failed.");
        send(psd, "Error handle_pipe: creating pipe\n# ", 21, 0);
        return;
    }

    // Fork the First child for left command
    if ((pid1 = fork()) == -1) { // fork pid 1 
        perror("Error handle_pipe: Fork failed left command");
        close(pipe_fd[0]);
        close(pipe_fd[1]);
        return;
    }

    if (pid1 == 0) { // Left child process

         // Check and apply redirections for left command
        int out_fd = -1, in_fd = -1;
        for (int i = 0; cmd_args_left[i] != NULL; i++) {
            if (strcmp(cmd_args_left[i], ">") == 0) {
                out_fd = open(cmd_args_left[i + 1], O_WRONLY | O_CREAT | O_TRUNC, 0644);
                if (out_fd == -1) {
                    perror("Error handle_pipe open output file");
                    exit(EXIT_FAILURE);
                }
                dup2(out_fd, STDOUT_FILENO);
                close(out_fd);
                
                cmd_args_left[i] = NULL; // Remove redirection operator
            } else if (strcmp(cmd_args_left[i], "<") == 0) {
                in_fd = open(cmd_args_left[i + 1], O_RDONLY);
                if (in_fd == -1) {
                    perror("Error handle_pipe open input file");
                    exit(EXIT_FAILURE);
                }
                dup2(in_fd, STDIN_FILENO);
                close(in_fd);
                cmd_args_left[i] = NULL; // Remove redirection operator
            }
        }

        // Redirect stdout to pipe if no file redirection
        if (out_fd == -1) dup2(pipe_fd[1], STDOUT_FILENO);
        // Redirect stdout to the write end of the pipe 
        //dup2(pipe_fd[1], STDOUT_FILENO); // Redirect stdout to pipe
        close(pipe_fd[0]);  // Close read end of pipe
        close(pipe_fd[1]);
        // Execute the left command
        execvp(cmd_args_left[0], cmd_args_left);
        //execute_command(cmd_args_left, NULL, NULL, psd);
        perror("Error: handle pipe- redirect cmd args left execvp failed ");
        exit(EXIT_FAILURE);
    }

    // Fork the Second child for right command
    if ((pid2 = fork()) == -1) {
        perror("Error handle_pipe:Failed fork right command");
        close(pipe_fd[0]);
        close(pipe_fd[1]);
        //kill(pid2, SIGTERM);
        return;
    }

    if (pid2 == 0) { // Right Child process
         // Check and apply redirections for right command
        int in_fd = -1, out_fd = -1;
        for (int i = 0; cmd_args_right[i] != NULL; i++) {
            if (strcmp(cmd_args_right[i], ">") == 0) {
                out_fd = open(cmd_args_right[i + 1], O_WRONLY | O_CREAT | O_TRUNC, 0644);
                if (out_fd == -1) {
                    perror("Error handle_pipe: open output file");
                    exit(EXIT_FAILURE);
                }
                dup2(out_fd, STDOUT_FILENO);
                close(out_fd);
                cmd_args_right[i] = NULL; // Remove redirection operator
            } else if (strcmp(cmd_args_right[i], "<") == 0) {
                in_fd = open(cmd_args_right[i + 1], O_RDONLY);
                if (in_fd == -1) {
                    perror("Error handle_pipe: open input file");
                    exit(EXIT_FAILURE);
                }
                dup2(in_fd, STDIN_FILENO);
                close(in_fd);
                cmd_args_right[i] = NULL; // Remove redirection operator
            }
        }

        // Redirect stdin to pipe if no input redirection
        if (in_fd == -1) dup2(pipe_fd[0], STDIN_FILENO);
        
        
        //dup2(pipe_fd[0], STDIN_FILENO); // Redirect stdin to pipe
        dup2(psd, STDOUT_FILENO); // Redirect stdin to pipe

        close(pipe_fd[0]);  // Close write end of pipe
        close(pipe_fd[1]);  // Close write end of pipe

        execvp(cmd_args_right[0], cmd_args_right);
        //execute_command(cmd_args_right, NULL, NULL, psd);
        perror("Error Hanlde pipe: execvp failed for right command");
        exit(EXIT_FAILURE);
    }

    // Parent process closes both ends of the pipe and waits for children
    close(pipe_fd[0]);
    close(pipe_fd[1]);
    // Wait for both the child processes to complete
    waitpid(pid1, NULL, 0);
    waitpid(pid2, NULL, 0);

    send(psd, PROMPT, 3, 0); // Send prompt to client
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

    // Execute the pipe command 
    handle_pipe(left_args, right_args, psd);

    free(command_copy);
}

void apply_redirections(char *input_file, char *output_file, int psd) {
    if (input_file != NULL) {
        int in_fd = open(input_file, O_RDONLY);
        if (in_fd < 0) {
            perror("Error opening input file");
            send(psd, "Error opening input file\n", 25, 0);
            exit(EXIT_FAILURE);
        }
        dup2(in_fd, STDIN_FILENO);
        close(in_fd);
    }

    if (output_file != NULL) {
        int out_fd = open(output_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (out_fd < 0) {
            perror("Error opening output file");
            send(psd, "Error opening output file\n", 25, 0);
            exit(EXIT_FAILURE);
        }
        dup2(out_fd, STDOUT_FILENO);
        close(out_fd);
    }
}

int validateCommand(const char *command, int psd) {
       // Reject commands that start with disallowed characters
    if (command[0] == '<' || command[0] == '>' || command[0] == '|' || command[0] == '&') {
        const char *errorMsg = "Invalid command: commands cannot start with <>|&\n";
        send(psd, errorMsg, strlen(errorMsg), 0);
        return 0;
    }

    // Check for mutually exclusive usage of | and &
    if (strchr(command, '|') && strchr(command, '&')) {
        const char *errorMsg = "Invalid usage: commands cannot use both | and & simultaneously\n";
        send(psd, errorMsg, strlen(errorMsg), 0);
        return 0;
    }

    // Detect if it's a job control command
    if (strncmp(command, "fg", 2) == 0 || strncmp(command, "bg", 2) == 0 || strcmp(command, "jobs") == 0) {
        char *cmd_copy = strdup(command);
        if (!cmd_copy) {
            perror("Memory allocation error");
            return 0;
        }

        char *token = strtok(cmd_copy, " ");
        token = strtok(NULL, " "); // Get the next token (e.g., job id or arguments)

        // Check for invalid usage of & or |
        //if (token && (strcmp(token, "&") == 0 || strcmp(token, "|") == 0)) {
        //    const char *errorMsg = "Invalid usage: job control commands cannot be used with & or |\n";
        //    send(psd, errorMsg, strlen(errorMsg), 0);
        //    free(cmd_copy);
        //    return 0;
        //}
        // Check for invalid usage with &, |, <, or >
        if (strchr(cmd_copy, '&') || strchr(cmd_copy, '|') || strchr(cmd_copy, '<') || strchr(cmd_copy, '>')) {
            const char *errorMsg = "Invalid usage: job control commands cannot be combined with &, |, <, or >\n";
            send(psd, errorMsg, strlen(errorMsg), 0);
            free(cmd_copy);
            return 0;
        }

        free(cmd_copy);
        return 1;
    }

    return 1; // Command is valid
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
void *logRequest(void *args) {
    LogRequestArgs *logArgs = (LogRequestArgs *)args; // g
    char *request = logArgs->request; // Get the request string 
    struct sockaddr_in from = logArgs->from; // Get the client address information 

    time_t rawtime; // Set current time variable
    struct tm *timeinfo;  //  struct for local time info
    char timeString[80]; // Buffer to store formatted time string 
    FILE *logFile; // File pointer for the log file 
    // Get the current time
    time(&rawtime);
    timeinfo = localtime(&rawtime);// Convert the time to local time 
    strftime(timeString, 80, "%b %d %H:%M:%S", timeinfo); // Format the time string as monthd day hour minute second 
    // Lock the mutext to ensure thread safe access to the log file
    pthread_mutex_lock(&lock); // wrap CS in lock ...
    // Print a debugging statement for opening the log file 
    printf("Opening log file at path: %s\n", log_path);
    // Open the log file in append mode
    logFile = fopen(log_path, "a");
    if (logFile == NULL) {
        // If the log file cannot open print an error statement
        perror("Error opening log file");
        pthread_mutex_unlock(&lock); // Unlock the mutex before returning
        return NULL;
    }

    // Debugging statement to check if writing to the file
    printf("Writing to log file: %s\n", request);

    // Write the log entry in the format 
    fprintf(logFile,"%s yashd[%s:%d]: %s\n", 
        timeString, 
        inet_ntoa(from.sin_addr), 
        ntohs(from.sin_port), request);
    fclose(logFile); // Close the log file 
    pthread_mutex_unlock(&lock); // ... unlock
    free(args); // Free the memory allocated for the log argument 
    
    pthread_exit(NULL);// Exit the thread 
 }
void log_command(const char *command, struct sockaddr_in from) {
    pthread_t log_thread;
    // IF log ares is Null
    LogRequestArgs *log_args = (LogRequestArgs *)malloc(sizeof(LogRequestArgs));
    if (log_args == NULL) {
        perror("Failed to allocate memory for log arguments");
        return;
    }
    // Prepare the command string for loggin 
    char log_command_str[BUFFER_SIZE];

    // Populate log arguments
    strncpy(log_args->request, command, sizeof(log_args->request) - 1);
    //strncpy(log_args->request, log_command_str, sizeof(log_args->request) - 1);
    log_args ->request[sizeof(log_args->request) - 1] = '\0'; // Ensures null termination 
    log_args->from = from;
    // Create a detached thrad for logging the command
    if (pthread_create(&log_thread, NULL, logRequest, (void *)log_args) != 0) {
        perror("Failed to create a logging thread");
        free(log_args);
        return;
    } 
    pthread_detach(log_thread);
}
void cleanup(char *buf)
{
    int i;
    for(i=0; i<BUFFER_SIZE; i++) buf[i]='\0';
}

void sig_pipe(int n) 
{
   perror("Broken pipe signal");
   exit(1);
}

void sig_chld(int n) {
    int status;
    pid_t pid;

    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        if (WIFEXITED(status)) {
            printf("Daemon: Child process [%d] exited with status %d.\n", pid, WEXITSTATUS(status));
            update_job_status(pid, 0, 0); // Mark the job as "Done"
        } else if (WIFSIGNALED(status)) {
            printf("Daemon: Child process [%d] was terminated by signal %d.\n", pid, WTERMSIG(status));
            update_job_status(pid, 0, 0); // Mark the job as "Done"
        }
    }
}

void handle_cat_command(const char *command, int psd) {
    // Extract file name after cat >
    char *filename = strtok((char *)(command + 5), " ");
    
    if (filename == NULL) {
        const char *errorMsg = " Error: Handle_cat_command no filename provided for redirection\n# ";// debugging No filename provide
        send(psd, errorMsg, strlen(errorMsg), 0);
        return;
    }
    // Open the specified file in write mode or create one
    int fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd == -1) {
        perror("open output file");
        exit(EXIT_FAILURE);
    }

    // Notify the client to start typing input and end with control d 
    const char *inputMsg = "Start typing input and end with Ctrl+D:\n";
    send(psd, inputMsg, strlen(inputMsg), 0);

    // Buffer to hold input from the client
    char buffer[BUFFER_SIZE];
    ssize_t bytesRead;

    // Loop to continuoulsy read input from the client 
    while ((bytesRead = recv(psd, buffer, sizeof(buffer) - 1, 0)) > 0) {
        buffer[bytesRead] = '\0'; // Null terminat the received data

        // Check for EOF control d ascii code 4
        if ( strchr(buffer, 4) != NULL) {
            break; // Exit the loop if control d is detected
        }
        // Write the received input to the specified file 
        if (write(fd, buffer, bytesRead) == -1) {
            // If write to file fails send an error message to client 
            perror("Error: writing to file");
            const char *errorMsg = "Error writing to file.\n#";
            send(psd, errorMsg, strlen(errorMsg), 0);
            close(fd); // close the file descriptor
            return;
        }
    }
    // Close the file after writing is complete 
    close(fd);
    // Notify the client the file write is complete
    const char *doneMsg = "File write is complete.\n# ";
    send(psd, doneMsg, strlen(doneMsg), 0);

}

//JOBS 
void add_job(pid_t pid, const char *command, int is_background) {
    if (job_count < MAX_JOBS) {
        char *job_command;

        // Check if the command already ends with `&`
        size_t command_len = strlen(command);
        if (is_background && command[command_len - 1] != '&') {
            // Append `&` to the command if it's a background job and doesn't already end with it
            job_command = malloc(command_len + 3); // Allocate memory for command + " &\0"
            if (job_command == NULL) {
                perror("Memory allocation error for job command");
                return;
            }
            snprintf(job_command, command_len + 3, "%s &", command);
        } else {
            job_command = strdup(command); // Use the original command
        }

        jobs[job_count].pid = pid;
        jobs[job_count].job_id = next_job_id++;
        jobs[job_count].command = job_command;
        jobs[job_count].is_running = 1;
        jobs[job_count].is_stopped = 0;
        jobs[job_count].is_background = is_background;

        printf("Added job [%d] with PID %d: %s (Background: %d)\n",
               jobs[job_count].job_id, pid, job_command, is_background);
        
        job_count++;
    } else {
        printf("Job table full. Cannot add more jobs.\n");
    }
}


void remove_job(pid_t pid) {
    for (int i = 0; i < job_count; i++) {
        if (jobs[i].pid == pid) {
            printf("Removing job [%d] with PID %d: %s\n", jobs[i].job_id, jobs[i].pid, jobs[i].command);
            free(jobs[i].command);
            jobs[i] = jobs[job_count - 1]; // Replace with the last job
            job_count--;
            return;
        }
    }
    printf("Job with PID %d not found.\n", pid);
}

void update_job_status(pid_t pid, int is_running, int is_stopped) {
    for (int i = 0; i < job_count; i++) {
        if (jobs[i].pid == pid) {
            jobs[i].is_running = is_running;
            jobs[i].is_stopped = is_stopped;
            if (!is_running && !is_stopped) {
                printf("Job [%d] with PID %d marked as Done.\n", jobs[i].job_id, pid);
            }
            return;
        }
    }
}

void print_jobs(int psd) {
    char buffer[1024] = "";
    for (int i = 0; i < job_count; i++) {
        char job_status[20];
        char job_marker = (i == job_count - 1) ? '+' : '-';

        if (jobs[i].is_running && !jobs[i].is_stopped) {
            strcpy(job_status, "Running");
        } else if (jobs[i].is_stopped) {
            strcpy(job_status, "Stopped");
        } else {
            strcpy(job_status, "Done");
        }

        char job_info[256];
        snprintf(job_info, sizeof(job_info), "[%d]%c %s %s\n",
                 jobs[i].job_id, job_marker, job_status, jobs[i].command);

        strcat(buffer, job_info);
    }
    strcat(buffer, "# ");
    send(psd, buffer, strlen(buffer), 0);
    printf("Jobs list sent to client:\n%s", buffer);
}


void fg_command(int job_id, int psd) {
    if (job_id == -1) {
        for (int i = job_count - 1; i >= 0; i--) {
            if (jobs[i].is_running && jobs[i].is_background) {
                job_id = jobs[i].job_id;
                break;
            }
        }
    }

    for (int i = 0; i < job_count; i++) {
        if (jobs[i].job_id == job_id) {
            ch_pid = jobs[i].pid;
            jobs[i].is_background = 0;
            tcsetpgrp(STDIN_FILENO, ch_pid); // Transfer terminal control
            kill(ch_pid, SIGCONT);
            printf("Bringing job [%d] to foreground: %s\n", job_id, jobs[i].command);
            send(psd, jobs[i].command, strlen(jobs[i].command), 0);

            int status;
            waitpid(ch_pid, &status, WUNTRACED);
            tcsetpgrp(STDIN_FILENO, getpid()); // Restore terminal control to the shell
            if (WIFSTOPPED(status)) {
                update_job_status(ch_pid, 0, 1);
            } else {
                remove_job(ch_pid);
            }
            ch_pid = -1;
            return;
        }
    }
    const char *errorMsg = "Job ID not found.\n# ";
    send(psd, errorMsg, strlen(errorMsg), 0);
}
void bg_command(int job_id, int psd) {
    if (job_id == -1) {
        for (int i = job_count - 1; i >= 0; i--) {
            if (jobs[i].is_stopped) {
                job_id = jobs[i].job_id;
                break;
            }
        }
    }

    for (int i = 0; i < job_count; i++) {
        if (jobs[i].job_id == job_id && jobs[i].is_stopped) {
            kill(jobs[i].pid, SIGCONT);
            jobs[i].is_running = 1;
            jobs[i].is_stopped = 0;
            jobs[i].is_background = 1;
            char msg[BUFFER_SIZE];
            snprintf(msg, sizeof(msg), "[%d]+ Running %s &\n# ", jobs[i].job_id, jobs[i].command);
            send(psd, msg, strlen(msg), 0);
            printf("Continuing job [%d] in background: %s\n", job_id, jobs[i].command);
            return;
        }
    }
    const char *errorMsg = "Job ID not found or not stopped.\n# ";
    send(psd, errorMsg, strlen(errorMsg), 0);
}
