// This is where the server will be set up yashd.c
// Troubleshooting the server
// 1. Error binding name to stream socket: Address already in use 
// Run: lsof -i :3820 sudo lsof -i :3820
// Run: kill -9 <PID>

#include "yashd.h"

// int psd;

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
void log_command(const char *command, struct sockaddr_in from, int is_background);

void apply_redirections(char **cmd_args, int psd);
void handle_pipe(char **cmd_args_left, char **cmd_args_right, int psd);
void validatePipes(const char *command, int psd);
int validateCommand(const char *command, int psd);

// Jobs
void print_jobs(int psd);
void add_job(pid_t pid, const char *command, int is_running, int is_background);
void update_job_markers() ;
void fg_job(int job_id, int psd);
void bg_job(int job_id, int psd);
void remove_newLine(char *line);

// Signals 
void sigint_handler(int sig);
void sigtstp_handler(int sig);
void sigchld_handler(int sig);
void sig_pipe(int n) ;
void setup_signal_handlers();

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
pid_t fg_pid = -1;      // Foreground proces Id

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


  ///* Establish handlers for signals */
  if ( signal(SIGCHLD, sigchld_handler) < 0 ) {
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
      fprintf(stderr, "Can't find host %s\n", argv[1]);
      exit(-1);
    }

    hp = gethostbyname("localhost");
    if (hp == NULL) {
        perror("Error getting hostname");
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
	    perror("opening stream socket");
	    exit(-1);
    }
    
    reusePort(sd); // add this to u__listening_socket 
    //  this is also in u__lostening socket 
    if (bind(sd,(struct sockaddr *) &server, sizeof(server) ) < 0 ) {
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
        // Accept the new client connection 
        psd  = accept(sd, (struct sockaddr *)&from, &fromlen);

        if (psd < 0) {
            perror("Accepting connection");
            continue; // Retry the error 
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
void *serveClient(void *args) {
    ClientArgs *clientArgs = (ClientArgs *)args;
    int psd = clientArgs->psd;
    struct sockaddr_in from = clientArgs->from;
    char buffer[BUFFER_SIZE];
    int bytesRead;
    //pthread_t p;
    pid_t pid;
    //int pipefd_stdin[2] = {-1, -1};
    //int pipe_fd[2];

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

            // Check if command is cat > filename
            if (strncmp(command, "cat > ", 5) == 0) {
                // call the handle cat function 
                handle_cat_command(command, psd);
                continue; // Skip and wait for next command
            }

            // Handle job commands fg bg and & 

            
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

            // Handle/ Check for jobs control command s
            if (strcmp(command, "jobs") == 0) {
                print_jobs(psd); // Pass the clients socket descriptor 
                send(psd, PROMPT, strlen(PROMPT), 0);
                continue;
            }
            // Check for fg 
            if (strncmp(command, "fg", 2) == 0) {
                int job_id = (strlen(buffer) > 2) ? atoi(buffer + 3) : -1;
                fg_job(job_id, psd);
                // print_jobs(psd); // Pass the clients socket descriptor 
                send(psd, PROMPT, strlen(PROMPT), 0);
                continue;
            } 
            // Check for bg
            if (strncmp(command, "bg", 2) == 0) {
                int job_id = (strlen(buffer) > 2) ? atoi(buffer + 3) : -1;
                bg_job(job_id, psd);
                
                // print_jobs(psd); // Pass the clients socket descriptor 
                send(psd, PROMPT, strlen(PROMPT), 0);
                continue;
            } 

            int is_background = check_background(command);

            // Check for pipe
            if (strchr(command, '|') != NULL) {
                // Log the command with pipe 
                log_command(command, from, is_background);
                validatePipes(command, psd);
                send(psd, PROMPT, strlen(PROMPT), 0); // Send prompt back to the client 
                continue;
            }
            
            // First validate the command
            if (!validateCommand(command, psd)) {
                send(psd, PROMPT, strlen(PROMPT), 0);
                continue;  // Skip invalid commands
            }
            // Log the command
            log_command(command, from, is_background);

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
            // Fork the process
            pid = fork();
            if (pid == -1) {
                perror("fork");
                close(pipe_fd[0]);
                close(pipe_fd[1]);
                continue;
            }
            if (pid == 0) { // Child process
                close(pipe_fd[0]);
                if (is_background) {setsid();} // Create a new session for background process

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
            // Add the job to the job table 
            add_job(pid, strdup(command), 1, is_background);

            // Foreground job wait for it 
            if (!is_background) {
                fg_pid = pid;
                int status;
                waitpid(pid, &status, WUNTRACED);
                fg_pid = -1 ;

                // Update job status 
                if (WIFSTOPPED(status)) {
                    for (int i = 0; i < job_count; i++) {
                        if (jobs[i].pid == pid) {
                            jobs[i].is_running = 0;
                            jobs[i].is_stopped = 1; 
                            break;
                        }
                    }
                } 
            }
            close(pipe_fd[0]);
            waitpid(pid, NULL, 0);
            pid = -1;

            // Send prompt
            send(psd, PROMPT, strlen(PROMPT), 0); 
        }
        // Handle control commands
        else if (strncmp(buffer, "CTL ", 4) == 0) {
            char controlChar = buffer[4];
            // Handle Control +D
            if (controlChar == 'd') {
                printf("Recevied Ctrl+D from client. Closing connection.\n");
                // Send Message to the client
                const char *exitMsg = "Server rec. Ctrl+D. Clossing Connection.\n";
                send(psd, exitMsg, strlen(exitMsg), 0);

                // Close the client socket and exit the thread 
                close(psd);
                free(clientArgs);
                pthread_exit(NULL);
            }
            handle_control(psd, controlChar, fg_pid);
        }    
    }
    return NULL;
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
void print_jobs(int psd) {
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
    // Check to see if the job table is full 
    if (job_count >= MAX_JOBS) {
        printf("Job list is full\n");
        return;
    }
    // Set the job marker for the new job as '+' and others as '-'
    for (int i = 0; i < job_count; i++) {
        jobs[i].job_marker = '-';
    }
    // Add the new job with its attributes 
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
    update_job_markers();
}
void update_job_markers() {
    if (job_count == 0) return;

    for (int i = 0; i < job_count; i++) {
        jobs[i].job_marker = '-'; 
    }

    // Mark the most recent jobs with a + 
    jobs[job_count - 1].job_marker = '+';
    if (job_count > 1) {
        jobs[job_count -2]. job_marker = '-';
    }
  
}
void bg_job(int job_id, int psd) {
    for (int i = 0; i < job_count; i++) {
        if (jobs[i].job_id == job_id && jobs[i].is_stopped) {
            jobs[i].is_running = 1;
            jobs[i].is_stopped = 0;
            jobs[i].is_background = 1;
            
            // Send Sigcont to continue the job
            kill(-jobs[i].pid, SIGCONT);

            char buffer[BUFFER_SIZE];
            snprintf(buffer, BUFFER_SIZE, "[%d]%c %s &\n", jobs[i].job_id, jobs[i].job_marker, jobs[i].command);
            send(psd, buffer, strlen(buffer), 0 );
            dprintf(psd, "%s# ", buffer); // prints teh command when bringing to the foreground
            return;
            //break;
        }
    }
    send(psd, "Job not found\n", 15, 0);
}
void fg_job(int job_id, int psd) { // Used from from Dr.Y book Page-45-49 
    int status;

    for (int i = 0; i < job_count; i++) {
        if (jobs[i].job_id == job_id) {
            fg_pid = jobs[i].pid;// Set the foreground process ID
            jobs[i].is_running = 1; // Mark the job as running 
            dprintf(psd, "%s\n", jobs[i].command); // prints teh command when bringing to the foreground
            printf("%s\n", jobs[i].command); // Print the command and send it to the client Debugging 
            
            char buffer[BUFFER_SIZE];
            snprintf(buffer, BUFFER_SIZE, "%s\n", jobs[i].command);
            send(psd, buffer, strlen(buffer), 0);
            
            // Send SIGCONT to continue the job // brings the job to the foreground
            kill(-jobs[i].pid, SIGCONT);// Wait for the process to cont. the stopped proces
            waitpid(jobs[i].pid, &status, WUNTRACED); // wait for the process to finish or be stopped again should it be zero or wuntrance
            fg_pid = -1; // Clear foreground process , no longer a fg process 
            
            // update job status if it was stopped again 
            if (WIFSTOPPED(status)) {
                jobs[i].is_running = 0; // Mark as stopped
                jobs[i].is_stopped = 1; 
            } else {
                // Job finished remove it from the job list
                //dprintf(psd, "[%d] Done %s\n", jobs[i].job_id, jobs[i].command);  
                //printf("[%d] Done %s\n", jobs[i].job_id, jobs[i].command); 

                for (int j = i; j < job_count - 1; j++) {
                    jobs[j] = jobs[j + 1 ];
                }
                job_count--;
                update_job_markers();
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
void log_command(const char *command, struct sockaddr_in from, int is_background) {
    pthread_t log_thread;
    // IF log ares is Null
    LogRequestArgs *log_args = (LogRequestArgs *)malloc(sizeof(LogRequestArgs));
    if (log_args == NULL) {
        perror("Failed to allocate memory for log arguments");
        return;
    }
    // Prepare the command string for loggin 
    char log_command_str[BUFFER_SIZE];
    if (is_background) {
        snprintf(log_command_str, sizeof(log_command_str), "%s &", command);
    } else {
        snprintf(log_command_str, sizeof(log_command_str), "%s", command);
    }
    // Populate log arguments
    //strncpy(log_args->request, command, sizeof(log_args->request) - 1);
    strncpy(log_args->request, log_command_str, sizeof(log_args->request) - 1);
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
void sig_chld(int n)
{
    int status;
    fprintf(stderr, "Child terminated\n");
    while (waitpid(-1, NULL, WNOHANG) > 0);
    //wait(&status); /* So no zombies */
}
 //signal handler for sigtstp ctrl z
void sigint_handler(int sig)
{
    printf("SIGINT (Ctrl+C) handler called in daemon process\n");
    fflush(stdout);
    printf("sigint handler called\n");
    if (fg_pid > 0)
    {
        // if there is a foreground process , send sigint to it
        kill(fg_pid, SIGINT);
        printf("\nSent SigINT to foreground process\n# ");
    } else {
        printf("\nNo foreground process to send SIGINT.\n# ");
    }

    write(STDOUT_FILENO, PROMPT, 3);
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
            if (jobs[i].pid == fg_pid) {
                jobs[i].is_running = 0;
                jobs[i].is_stopped = 1;
                jobs[i].job_marker = '+'; // set marker for the most recent job
                // printf("\n[%d]      Stopped %s\n" ,jobs[i].job_id, jobs[i].command);
                break;
            }
        }

        fg_pid = -1; // clear the foreground process
        printf("\nSent SIGTSTP to foreground process. dprint\n");



        fflush(stdout); // ensure the prompt is displayed immediately after the message
        //const char *msg = "Type fg to resume.\n";
        //send(psd, msg, strlen(msg), 0);
    } else {
        printf("\nNo foreground process to send SIGTSTP.\n#");
    }
}
// signal handler for sigtstp ctrl z
void sigchld_handler(int sig) {
    printf("SIGCHLD handler called in daemon process\n");
    fflush(stdout);
    printf("SIGCHLD handler called\n");
    int status;
    pid_t pid;
    // Clean up zombie child processes non blocking wait
    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        for (int i = 0; i < job_count; i++) {
            if (jobs[i].pid == pid) {
                printf("/n[%d] Done %s\n", jobs[i].job_id, jobs[i].command);
                free(jobs[i].command);

                //if (WIFEXITED(status) || WIFSIGNALED(status))
                //{
                //    // print done messageif needed
                //    printf("\n[%d] Done %s\n", jobs[i].job_id, jobs[i].command);
                //    // remove the job from the job list
                //    for (int j = i; j < job_count - 1; j++)
                //    {
                //        jobs[j] = jobs[j + 1];
                //    }
                //    job_count--;
                //}
                for (int j = i; j < job_count - 1; j++) {
                    jobs[j] = jobs[j + 1];
                }
                job_count--;
                update_job_markers();
                break;
            }
        }
    }
}
void handle_control(int psd, char control, pid_t pid) {
    if (control == 'c') {
        if (pid > 0) {
            kill(pid, SIGINT);
            const char *msg = "Sent sigint to foreground process.\n# ";
            printf("Sent SIGINT to child process %d\n# ", pid);
            send(psd, msg, strlen(msg), 0);
        } else {
            const char *msg = "No foreground process to send sigint.\n# ";
            send(psd, msg, strlen(msg), 0);
            printf("No process to send SIGINT\n");
        }
    } else if (control == 'z') {
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
    } else if (control == 'd') {
        // if (pipefd_stdin[1] != -1) {
        //close(pipefd_stdin[1]);
        // pipefd_stdin[1] = -1;
        printf("received a control d ");// debugging 
        const char *msg = "Closed write end of the stdin pipe exiting thread.\n";
        send(psd, msg, strlen(msg), 0);
        printf("Closed write end of stdin pipe\n");
        close(psd);
        pthread_exit(NULL);
        //}
        // commandRunning = 0;
    } else {
        const char *errorMsg = "Error: No command is currently running.\n#";
        send(psd, errorMsg, strlen(errorMsg), 0);
    }
}
void handle_cat_command(const char *command, int psd) {
    // Extract file name after cat >
    char *filename = strtok((char *)(command + 5), " ");
    
    if (filename == NULL) {
        const char *errorMsg = " Error: no filename provided for redirection\n# ";// debugging No filename provide
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
// Check Background
int check_background(char *command) {
    int length = strlen(command);
    // check if the command ends with & 
    if (length > 0 && command[length -1 ] == '&') {
        command[length - 1] = '\0'; // Remove the & character
 
        return 1; // Indicates it is a background command 
    }
    return 0; 
}
