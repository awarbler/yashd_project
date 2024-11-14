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
void log_command(const char *command, struct sockaddr_in from);

void apply_redirections(char **cmd_args, int psd);
void handle_pipe(char **cmd_args_left, char **cmd_args_right, int psd);
void validatePipes(const char *command, int psd);
int validateCommand(const char *command, int psd);

// jobs 

void bg_command(int job_id, int psd);
void fg_command(int job_id, int psd);
void print_jobs(int psd);
void update_job_status(pid_t pid, int is_running, int is_stopped);
void remove_job(pid_t pid);
void add_job(pid_t pid, const char *command, int is_background);
// Signals 

void sig_pipe(int n) ;
void setup_signal_handlers();
void sigint_handler(int sig);
void sigtstp_handler(int sig);
void sigchld_handler(int sig);
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
    perror("Error daemon_init: cannot fork");
    exit(0);
  } else if (pid > 0) /* The parent */
    exit(0);

  /* the child */

  /* Close all file descriptors that are open */
  for (k = getdtablesize()-1; k>0; k--)
      close(k);

  /* Redirecting stdin, stdout, and stdout to /dev/null */
  if ((fd = open("/dev/null", O_RDWR)) < 0) {
    perror("Error daemon_init: Open");
    exit(0);
  }
  dup2(fd, STDIN_FILENO);      /* detach stdin */
  dup2(fd, STDOUT_FILENO);     /* detach stdout */
  dup2(fd, STDERR_FILENO);     /* detach stderr */
  close (fd);

  ///* Establish handlers for signals */
  if ( signal(SIGCHLD, sigchld_handler) < 0 ) {
    perror("Error daemon_init: Signal SIGCHLD");
    exit(1);
  }
  if ( signal(SIGPIPE, sig_pipe) < 0 ) {
    perror("Error daemon_init: Signal SIGPIPE");
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
        perror("Error serverClietn Sending Prompt");
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
                perror("Error serverClient receiving stream message\n");
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
            // Check for pipe
            if (strchr(command, '|') != NULL) {
                // If command starts with a pipe, return an error message
                if (command[0] == '|') {
                    const char *errorMsg = "Error serverClient Invalid command: commands cannot start with a pipe (|)\n# ";
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


            if (strcmp(command, "jobs") == 0) {
                print_jobs(psd);
                if (!is_background) {
                    send(psd, PROMPT, strlen(PROMPT), 0);
                }
                // send(psd, PROMPT, strlen(PROMPT), 0);
                continue;
            } 
            
            if (strncmp(command, "fg ", 3) == 0) {
                int job_id = atoi(command + 3);
                fg_command(job_id, psd);
                if (!is_background) {
                    send(psd, PROMPT, strlen(PROMPT), 0);
                }
                //send(psd, PROMPT, strlen(PROMPT), 0);
                continue;
            } 
            
            if (strncmp(command, "bg ", 3) == 0) {
                int job_id = atoi(command + 3);
                bg_command(job_id, psd);
                if (!is_background) {
                    send(psd, PROMPT, strlen(PROMPT), 0);
                }
                //send(psd, PROMPT, strlen(PROMPT), 0);
                continue;
            }

            // Check if command should be n background 
            // Trim trailing whitespace
            while (strlen(command) > 0 && isspace(command[strlen(command) - 1])) {
                command[strlen(command) - 1] = '\0';
            }

            // Check if the command ends with '&'
            if (strlen(command) > 0 && command[strlen(command) - 1] == '&') {
                is_background = 1;
                command[strlen(command) - 1] = '\0'; // Remove '&' character

                // Trim any trailing whitespace again
                while (strlen(command) > 0 && isspace(command[strlen(command) - 1])) {
                    command[strlen(command) - 1] = '\0';
                }
            } else {
                is_background = 0;
            }

            // First validate the command
            if (!validateCommand(command, psd)) {
                //send(psd, PROMPT, strlen(PROMPT), 0);
                if (!is_background) {
                    send(psd, PROMPT, strlen(PROMPT), 0);
                }
                continue;  // Skip invalid commands
            }
            // Log the command
            log_command(command, from);

            // Parse command and check for redirections
            //char *cmd_args[MAX_ARGS];
            int i = 0;
            char *input_file = NULL;
            char *output_file = NULL;
            
            // Tokenize the command
            char *cmd_args[MAX_ARGS];
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
                perror("Error serverClient pipe");
                continue;
            }
            // Fork the process
            pid = fork();
            if (pid == -1) {
                perror("Error serverClient fork");
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
                        perror("Error serverClient  open input file");
                        exit(EXIT_FAILURE);
                    }
                    dup2(fd, STDIN_FILENO);
                    close(fd);
                }

                // Handle output redirection
                if (output_file != NULL) {
                    int fd = open(output_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
                    if (fd == -1) {
                        perror("Error serverClient  open output file");
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
                perror("Error serverClient  execvp failed");
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

            if (is_background) {
                pid = fork();
                if (pid == 0) { // Child process
                    setpgid(0, 0); // Set the process group
                    execvp(cmd_args[0], cmd_args);
                    perror("Error serverClient child process execvp failed");
                    exit(EXIT_FAILURE);
                } else if (pid > 0) {
                    setpgid(pid, pid);
                    add_job(pid, command, 1);
                    printf("Started background job [%d] with PID %d: %s\n", next_job_id - 1, pid, command);
                } else {
                    perror("fork");
                }
            }

            // Wait for the foreground process to change state (terminated, stopped, or continued)
            if (is_background) {
                // Start the background job without waiting
                add_job(pid, command, 1);
                printf("Started background job [%d] with PID %d: %s\n", next_job_id - 1, pid, command);

                // Send prompt back to the client immediately
                send(psd, PROMPT, strlen(PROMPT), 0);
            } else {
                // Wait for the foreground process to change state (terminated, stopped, or continued)
                fg_pid = pid;
                int status;
                pid_t wait_result = waitpid(fg_pid, &status, WUNTRACED | WCONTINUED);

                if (wait_result == -1) {
                    perror("Error serverClient waitpid");
                } else if (WIFSTOPPED(status)) {
                    printf("Foreground process [%d] was stopped.\n", fg_pid);
                    update_job_status(fg_pid, 0, 1);
                    fg_pid = -1;
                } else if (WIFSIGNALED(status)) {
                    printf("Foreground process [%d] was terminated by signal %d.\n", fg_pid, WTERMSIG(status));
                    remove_job(fg_pid);
                    fg_pid = -1;
                } else if (WIFEXITED(status)) {
                    printf("Foreground process [%d] exited with status %d.\n", fg_pid, WEXITSTATUS(status));
                    remove_job(fg_pid);
                    fg_pid = -1;
                }

            close(pipe_fd[0]);
            waitpid(pid, NULL, 0);
            pid = -1;
            }

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
        perror("Error handle_pipe pipe");
        send(psd, "Error handle_pipe creating pipe\n", 21, 0);
        return;
    }

    // First child for left command
    if ((pid1 = fork()) == -1) {
        perror("Error handle_pipe fork");
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
                    perror("Error handle_pipe open output file");
                    exit(EXIT_FAILURE);
                }
                dup2(out_fd, STDOUT_FILENO);
                cmd_args_left[i] = NULL; // Remove redirection operator
            } else if (strcmp(cmd_args_left[i], "<") == 0) {
                in_fd = open(cmd_args_left[i + 1], O_RDONLY);
                if (in_fd == -1) {
                    perror("Error handle_pipe open input file");
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
        perror("Error handle_pipe execvp failed for left command");
        exit(EXIT_FAILURE);
    }

    // Second child for right command
    if ((pid2 = fork()) == -1) {
        perror("Error handle_pipe child right fork");
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
                    perror("Error handle_pipe open input file");
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
        perror("Error handle_pipe execvp failed for right command");
        exit(EXIT_FAILURE);
    }

    // Parent process closes both ends of the pipe and waits for children
    close(pipe_fd[0]);
    close(pipe_fd[1]);
    waitpid(pid1, NULL, 0);
    waitpid(pid2, NULL, 0);

    //send(psd, PROMPT, 3, 0); // Send prompt to client
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
            perror("Error apply redirection  opening input file Redirection");
            send(psd, "Error apply redirection opening input file \n#", 30, 0);
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
            perror("Error apply redirection  2> error opening error file");
            send(psd, "Error apply redirection opening error file \n#", 25, 0);
            exit(EXIT_FAILURE);
            // return;
        }
        // printf("redirection output file : %s\n" , cmd_args[i+1]);
        dup2(err_fd, STDERR_FILENO);
        close(err_fd);

    }

    //execvp(cmd_args[0], cmd_args);
    if (execvp(cmd_args[0], cmd_args) == -1) {
        perror("Error apply redirection execvp failed");
        exit(EXIT_FAILURE);
    }


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
    if (strncmp(command, "fg ", 3) == 0 || strncmp(command, "bg ", 3) == 0 || strcmp(command, "jobs") == 0) {
        char *cmd_copy = strdup(command);
        if (!cmd_copy) {
            perror("Error validate command Memory allocation error");
            return 0;
        }

        char *token = strtok(cmd_copy, " ");
        token = strtok(NULL, " "); // Get the next token (e.g., job id or arguments)

        // Check for invalid usage of & or |
        if (token && (strcmp(token, "&") == 0 || strcmp(token, "|") == 0)) {
            const char *errorMsg = "Invalid usage: job control commands cannot be used with & or |\n";
            send(psd, errorMsg, strlen(errorMsg), 0);
            free(cmd_copy);
            return 0;
        }

        free(cmd_copy);
        return 1;
    }

    return 1; // Command is valid
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
        send(psd, "Error validatePipes Memory allocation error\n# ", 25, 0);
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
void reusePort(int s)
{
    int one = 1;

    if (setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (char *)&one, sizeof(one)) == -1)
    {
        printf("Error reusePort in setsockopt,SO_REUSEPORT \n");
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
        perror("Error Logrequest opening log file");
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
        perror("Error log command Failed to allocate memory for log arguments");
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
        perror("Error log command:Failed to create a logging thread");
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
    // Set up signal handlers for job control 
    signal(SIGINT, sigint_handler); // Handle ctrl c 
    printf("Signal int handlers initialized.\n");

    signal(SIGTSTP, sigtstp_handler);// Handle ctrl z
    printf("Signal tsp handlers initialized.\n");

    signal(SIGCHLD, sigchld_handler); // Handle child process state changes 
    printf("Signal child handlers initialized.\n");
}
/**
 * @brief  If we are waiting reading from a pipe and
 *  the interlocutor dies abruptly (say because
 *  of ^C or kill -9), then we receive a SIGPIPE
 *  signal. Here we handle that.
 */
void sig_pipe(int n) 
{
   perror("Error sig pipe Broken pipe signal");
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
        const char *errorMsg = " Error:cat command no filename provided for redirection\n# ";// debugging No filename provide
        send(psd, errorMsg, strlen(errorMsg), 0);
        return;
    }
    // Open the specified file in write mode or create one
    int fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd == -1) {
        perror("Error cat command open output file");
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
            perror("Error cat command: writing to file");
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
// Handler for SIGINT (Ctrl+C)
void sigint_handler(int sig) {
    if (fg_pid > 0) {
        kill(fg_pid, SIGINT);
        printf("Sent SIGINT to foreground process %d\n", fg_pid);
        //fg_pid = -1;
    } else {
        printf("No foreground process to send SIGINT.\n");
    }
}
// Handler for SIGTSTP (Ctrl+Z)
void sigtstp_handler(int sig) {
    if (fg_pid > 0) {
        kill(fg_pid, SIGTSTP);
        update_job_status(fg_pid, 0, 1);
        printf("Sent SIGTSTP to foreground process %d\n", fg_pid);
        fg_pid = -1;
    } else {
        printf("No foreground process to send SIGTSTP.\n");
    }
    //printf("# ");
    //fflush(stdout);
}
// Handler for SIGCHLD (child process state change)
void sigchld_handler(int sig) {
    int status;
    pid_t pid;

    while ((pid = waitpid(-1, &status, WNOHANG | WUNTRACED | WCONTINUED)) > 0) {
        if (WIFEXITED(status)) {
            printf("Child process [%d] exited with status %d.\n", pid, WEXITSTATUS(status));
            remove_job(pid);
        } else if (WIFSIGNALED(status)) {
            printf("Child process [%d] was terminated by signal %d.\n", pid, WTERMSIG(status));
            remove_job(pid);
        } else if (WIFSTOPPED(status)) {
            printf("Child process [%d] was stopped.\n", pid);
            update_job_status(pid, 0, 1);
        } else if (WIFCONTINUED(status)) {
            printf("Child process [%d] continued.\n", pid);
            update_job_status(pid, 1, 0);
        }
    }
    // Restore terminal control to the shell if needed
    tcsetpgrp(STDIN_FILENO, getpid());
}

void add_job(pid_t pid, const char *command, int is_background) {
    // Check if the job table is full
    if (job_count >= MAX_JOBS) {
        printf("Error: Job list is full. Cannot add more jobs.\n");
        return;
    }

    // Check if the job with the same PID already exists
    for (int i = 0; i < job_count; i++) {
        if (jobs[i].pid == pid) {
            printf("Error: Job with PID %d already exists.\n", pid);
            return;
        }
    }

    // Add the new job
    jobs[job_count].pid = pid;
    jobs[job_count].job_id = next_job_id++;
    jobs[job_count].command = strdup(command);
    jobs[job_count].is_running = 1;
    jobs[job_count].is_stopped = 0;
    jobs[job_count].is_background = is_background;
    jobs[job_count].job_marker = ' ';

    // Wrap around `next_job_id` if it exceeds MAX_JOBS
    if (next_job_id > MAX_JOBS) next_job_id = 1;

    printf("DEBUG: Added job [%d] with PID %d: %s\n", jobs[job_count].job_id, pid, command);
    job_count++;
}

void remove_job(pid_t pid) {
    for (int i = 0; i < job_count; i++) {
        if (jobs[i].pid == pid) {
            printf("Removing job [%d] with PID %d: %s\n", jobs[i].job_id, jobs[i].pid, jobs[i].command);

            // Free the allocated command string
            free(jobs[i].command);

            // Replace the removed job with the last job in the list to maintain continuity
            jobs[i] = jobs[job_count - 1];
            job_count--;

            printf("DEBUG: Job removed. Remaining job count: %d\n", job_count);
            return;
        }
    }
    printf("DEBUG: Job with PID %d not found.\n", pid);
}

void update_job_status(pid_t pid, int is_running, int is_stopped) {
    for (int i = 0; i < job_count; i++) {
        if (jobs[i].pid == pid) {
            jobs[i].is_running = is_running;
            jobs[i].is_stopped = is_stopped;

            // Update the job marker based on the status
            jobs[i].job_marker = (i == job_count - 1) ? '+' : '-';

            printf("DEBUG: Updated job [%d] - Running: %d, Stopped: %d\n",
                   jobs[i].job_id, jobs[i].is_running, jobs[i].is_stopped);
            return;
        }
    }
    printf("DEBUG: Job with PID %d not found. Cannot update status.\n", pid);
}

/**
 * @brief Prints the list of jobs to the client.
 * @param psd Socket descriptor for sending output to the client.
 */
void print_jobs(int psd) {
    char buffer[4096] = "";  // Increased buffer size for safety
    printf("DEBUG: Active job count: %d\n", job_count);

    for (int i = 0; i < job_count; i++) {
        const char *status = jobs[i].is_running ? "Running" : "Stopped";
        char job_marker = (i == job_count - 1) ? '+' : '-';

        // Format job information
        char job_info[256];
        snprintf(job_info, sizeof(job_info), "[%d]%c %s %s\n",
                 jobs[i].job_id, job_marker, status, jobs[i].command);

        // Append job information to the buffer
        strncat(buffer, job_info, sizeof(buffer) - strlen(buffer) - 1);
    }

    // Append the prompt to ensure the client receives it
    strncat(buffer, "# ", sizeof(buffer) - strlen(buffer) - 1);

    // Send the formatted jobs list to the client
    send(psd, buffer, strlen(buffer), 0);
    printf("Jobs list sent to client:\n%s", buffer);
}



void fg_command(int job_id, int psd) {
    for (int i = 0; i < job_count; i++) {
        if (jobs[i].job_id == job_id) {
            fg_pid = jobs[i].pid;
            jobs[i].is_background = 0;

            // Transfer terminal control to the foreground process
            tcsetpgrp(STDIN_FILENO, fg_pid);

            // Resume the process
            kill(fg_pid, SIGCONT);
            printf("Bringing job [%d] to foreground: %s\n", job_id, jobs[i].command);

            // Wait for the foreground process to complete or stop
            int status;
            waitpid(fg_pid, &status, WUNTRACED);

            // Restore terminal control to the shell
            tcsetpgrp(STDIN_FILENO, getpid());

            // Update job status based on the wait result
            if (WIFSTOPPED(status)) {
                update_job_status(fg_pid, 0, 1);
            } else {
                remove_job(fg_pid);
            }

            fg_pid = -1;
            if (!is_background) {
                    send(psd, PROMPT, strlen(PROMPT), 0);
            }
            //send(psd, PROMPT, strlen(PROMPT), 0);
            return;
        }
    }
    const char *errorMsg = "Error: Job ID not found.\n# ";
    send(psd, errorMsg, strlen(errorMsg), 0);
}

void bg_command(int job_id, int psd) {
    for (int i = 0; i < job_count; i++) {
        if (jobs[i].job_id == job_id && jobs[i].is_stopped) {
            kill(jobs[i].pid, SIGCONT);
            jobs[i].is_running = 1;
            jobs[i].is_stopped = 0;
            jobs[i].is_background = 1;

            char msg[BUFFER_SIZE];
            snprintf(msg, sizeof(msg), "[%d]+ Running %s &\n# ", jobs[i].job_id, jobs[i].command);
            send(psd, msg, strlen(msg), 0);

            printf("Resumed job [%d] in background: %s\n", job_id, jobs[i].command);
            return;
        }
    }

    const char *errorMsg = "Error: Job ID not found or not stopped.\n# ";
    send(psd, errorMsg, strlen(errorMsg), 0);
}
