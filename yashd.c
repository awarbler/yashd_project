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
int psd; 
/* Initialize path variables */
static char* server_path = "./tmp/yashd";
char* log_path = "./tmp/yashd.log";
static char* pid_path = "./tmp/yashd.pid";

/* Initialize mutex lock */
pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;


void reusePort(int sock);
void* serveClient(void *args);
void *logRequest(void *args);
void execute_command(char **cmd_args, char *original_cmd, int psd);
void handle_redirection(char **cmd_args);
void apply_redirections(char **cmd_args, int psd);
void handle_pipe(char **cmd_args_left, char **cmd_args_right, int psd);
int checkPipe(char *command, int psd);



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
}

/**
 * @brief Handler for SIGCHLD signal 
 */
void sig_chld(int n)
{

  fprintf(stderr, "Child terminated\n");
  while (waitpid(-1, NULL, WNOHANG) > 0);
  //wait(&status); /* So no zombies */
}

void daemon_init(const char * const path, uint mask)
{
  pid_t pid;
  char buff[256];
  int fd;
  int k;

  /* put server in background (with init as parent) */
  if ( ( pid = fork() ) < 0 ) {
    perror("daemon_init: cannot fork");
    exit(0);
  } else if (pid > 0) /* The parent */
    exit(0);

  /* the child */

  /* Close all file descriptors that are open */
  for (k = getdtablesize()-1; k>0; k--)
      close(k);

  /* Redirecting stdin, stdout, and stdout to /dev/null */
  if ( (fd = open("/dev/null", O_RDWR)) < 0) {
    perror("Open");
    exit(0);
  }
  dup2(fd, STDIN_FILENO);      /* detach stdin */
  dup2(fd, STDOUT_FILENO);     /* detach stdout */
  dup2(fd, STDERR_FILENO);     /* detach stderr */
  close (fd);
  /* From this point on printf and scanf have no effect */

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
  if ( ( k = open(pid_path, O_RDWR | O_CREAT, 0666) ) < 0 )
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

    // TO-DO: Initialize the daemon
    // daemon_init(server_path, 0);
    
    hp = gethostbyname("localhost");
    bcopy(hp->h_addr, &(server.sin_addr), hp->h_length);
    printf("(TCP/Server INET ADDRESS is: %s )\n", inet_ntoa(server.sin_addr));
    
    /** Construct name of socket */
    server.sin_family = AF_INET;    
    server.sin_addr.s_addr = htonl(INADDR_ANY);
    server.sin_port = htons(3820);  
    
    // Create the socket to send and rece
    sd = socket (AF_INET, SOCK_STREAM, IPPROTO_TCP); 
    if (sd < 0) {
	    perror("opening stream socket");
	    exit(-1);
    }

    //this allow the server to re-start quickly instead of waiting

    reusePort(sd);

    if ( bind(sd, (struct sockaddr *) &server, sizeof(server) ) < 0 ) {
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

void* serveClient(void *args) {
    ClientArgs *clientArgs = (ClientArgs *)args;
    int psd = clientArgs->psd;
    struct sockaddr_in from = clientArgs->from;
    char buffer[BUFFER_SIZE]; // Maybe 205 as the command can only have 200 characters
    int bytesRead;
    int pipefd_stdout[2] ={-1, -1};
    int pipefd_stdin[2] = {-1, -1};
    pthread_t p;
    pid_t pid = -1;
    //int commandRunning = 0;

    // Send initial prompt to the client
    if (send(psd, PROMPT, sizeof(PROMPT), 0) <0 ){
      perror("Sending Prompt # 1");
      close(psd);
      free(clientArgs);
      pthread_exit(NULL);
    } else {
      printf("sent over to client prompt from s1");
    }
        
    /**  get data from  client and send it back */
    for(;;){ // infinite loop to keep the server running
        printf("\n...server is waiting...\n ");

        cleanup(buffer); // clear the buffer

        bytesRead = recv(psd, buffer, sizeof(buffer), 0);

        // Receive command from client
        if (bytesRead <= 0){ // read data from the client
            if (bytesRead == 0) {
              // connection closed by client 
              printf("Connection closed by client\n");
            } else {
              perror("Error receiving stream message\n");
            }
            close(psd);
            free(clientArgs);
            pthread_exit(NULL);
        }

        buffer[bytesRead] = '\0'; // NULL terminate the received data

        printf("Received Buffer: %s",buffer);

        // Handle: CMD<blank><Command_String>\n
        if (strncmp(buffer, "CMD ", 4) == 0) {
            // Extract the command string
            char *command = buffer + 4;
            command[strcspn(command, "\n")] = '\0'; // Remove the newline character
            printf("Command Received: %s\n", command);
            // check for invalid combinations lif fg & and fg | echo
            if ((strstr(command, "fg &") !=NULL) || (strstr(command, "fg | ") != NULL )|| (strstr(command, "bg &") !=NULL) || (strstr(command, "bg | ") != NULL)){ 
                //invalid combination, move to next line
                const char *errorMsg = "Invalid command combination fg &, fg |, bg &, bg |\n# ";
                send(psd, errorMsg, strlen(errorMsg), 0);
                continue; // skp to the next iteration if no command was entered
            }

            // check if the user input is empty pressing enter without types anything 
            if(strlen(command) == 0){
                const char *errorMsg = "No Command \n#";
                send(psd, errorMsg, strlen(errorMsg), 0);
                continue; // skp to the next iteration if no command was entered
              }

            // handle jobs command
            if ( strcmp(command, "jobs") == 0) {
                  print_jobs();
                  send(psd, "\n# ", strlen("\n# "), 0);
                  continue;
            }

            // check if input starts with a special character (<|> & and skip it if it does 
            if (command[0] == '<' || command[0] == '>' || command[0] == '|' || command[0] == '&'){
                const char *errorMsg = "Invalid command: Commands cannot start with <>| or &\n# ";
                send(psd, errorMsg, strlen(errorMsg), 0);
                printf("\n");
                continue;
            }
            
            // Tokenize the command into arguements
            char *cmd_args[10];
            int i = 0;

            cmd_args[i] = strtok(command, " ");
            while (cmd_args[i] != NULL && i < 9)
            {
                i++;
                cmd_args[i] = strtok(NULL, " ");
            }
            cmd_args[i] = NULL;
        
            // Execute the command, piping handled inside execute_commnand or 
            // handle_pipe

            // apply redirection
            apply_redirections(cmd_args, psd);
            execute_command(cmd_args, command, psd);


          // Allocate memory for LogRequestArgs
          LogRequestArgs *args = (LogRequestArgs *)malloc(sizeof(LogRequestArgs));
          if (args == NULL) {
            perror("Error allocating memory for log request");
            continue;
          }
          // Copy the request and client information to the struct
          strncpy(args->request, command, sizeof(args->request) - 1);
          args->from = from;
          
          // Create a new thread for logging
          if (pthread_create(&p, NULL, logRequest, (void *)args) != 0) {
            perror("Error creating log request thread");
            free(args); // Free the allocated memory if thread creation fails
          } else {
            pthread_detach(p); // Detach the thread to allow it to run independently
          }

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
        } else {
          // Handle plain text input
          //if (commandRunning) {
              // Write the plain text to the stdin of the running command
              //write(pipefd_stdin[1], buffer, bytesRead);
          //} 
          //else {
              // If no command is running, send an error message
              const char *errorMsg = "Error: No command is currently running.\n#";
              send(psd, errorMsg, strlen(errorMsg), 0);
          }
        }
        pthread_exit(NULL);
    }
    
//}

void reusePort(int s)
{
    int one=1;
    
    if ( setsockopt(s,SOL_SOCKET,SO_REUSEADDR,(char *) &one,sizeof(one)) == -1 )
	{
	    printf("error in setsockopt,SO_REUSEPORT \n");
	    exit(-1);
	}
}


// function to execute the command by creating a child process using fork()
void execute_command(char **cmd_args, char *original_cmd, int psd) {

    pid_t pid;
    //pid_t pid = fork(); // create a new process 
    int status;
    int pipefd[2];
    ssize_t bytesRead;
    char buffer[BUFFER_SIZE];
    char results[BUFFER_SIZE];
    int is_background = 0;
    int pipefd_stdout[2] ={-1, -1};
    int pipefd_stdin[2] = {-1, -1};

    /*for (int i = 0; cmd_args[i]!= NULL; i++) {
        if (strcmp(cmd_args[i], "&") == 0)
        {
            is_background = 1;
            cmd_args[i] = NULL; // Remove & from the argument
            break;
        }
    }

    // Check if the last argument is & indicating a background job
    for (int i = 0; cmd_args[i] != NULL; i++) {
        if (strcmp(cmd_args[i], "&") == 0) {
            is_background = 1;
            cmd_args[i] = NULL; // Remove & from the argument list
            break;
        }
    }

    if (cmd_args == NULL || cmd_args[0] == NULL) {
        perror("command arguments are invalid");
        return;
    }

    if (cmd_args[0] == NULL)
    {
        perror("invalid command");
        return;
    }
*/
    // server Context
   /*if (psd >= 0)
    {
        if (pipe(pipefd) == -1)
        {
            perror("PIPE error ");
            return;
        }
    }*/

    pid = fork();
    if (pipe(pipefd) == -1)
    {
      perror("PIPE error ");
      return;
    }
    
    if (pid == 0) {

        // Child process
        // Redirect stdout and stderr to the pile
        //apply_redirections(cmd_args);

        // Child process
        close(pipefd_stdout[0]); // Close the read end of the stdout pipe
        close(pipefd_stdin[1]);  // Close the write end of the stdin pipe
        
        dup2(pipefd_stdout[1], STDOUT_FILENO); // Redirect stdout to the write end of the stdout pipe
        dup2(pipefd_stdout[1], STDERR_FILENO); // Redirect stderr to the write end of the stdout pipe
        dup2(pipefd_stdin[0], STDIN_FILENO);   // Redirect stdin to the read end of the stdin pipe
        
        close(pipefd_stdout[1]);
        close(pipefd_stdin[0]);
        // socket if psd > 0
        if (psd >= 0)
        {
            close(pipefd[0]);
            dup2(pipefd[1], STDOUT_FILENO);
            dup2(pipefd[1], STDERR_FILENO);
            close(pipefd[1]);
        }

        setpgid(0,0); // Set a new process group with the child pid

        //handle_redirection(cmd_args);
        apply_redirections(cmd_args, psd);
        
        if (execvp(cmd_args[0], cmd_args) == -1){
            perror("Invalid command or options");
            printf("\n# "); //  print a newline for invalide commands
            
            exit(EXIT_FAILURE);
        } 

    } else if (pid < 0) {
        // fork failed 
        perror("Fork failed");
        return;
    } else if (pid > 0) {
        // parent process 
         // Fork failed
        if (psd >= 0)
        {
            close(pipefd[0]);
            close(pipefd[1]);
        }
        //setpgid(pid, pid); // set child process group id to its own pid 
        int status;
        waitpid(pid, &status, 0);

        // Add job to job list and run in background 
        if (is_background) {
            
            add_job(pid, original_cmd, 1, 1);
            printf("[%d] %d\n", jobs[job_count - 1].job_id, jobs[job_count - 1].pid);
        } else {
            // Foreground job 

            //  parent process
            if (psd >= 0) {
                close(pipefd[1]);

                // Read from pipe and send to client
                while ((bytesRead = read(pipefd[0], buffer, sizeof(buffer))) > 0)
                {
                    send(psd, buffer, bytesRead, 0);
                }
                close(pipefd[1]);
                waitpid(pid, &status, 0);
            } else {
                fg_pid = pid;
            add_job(pid, original_cmd, 1, is_background);
            if (is_background){
                // add_job(pid, original_cmd, 1, 1);
                printf("[%d] %d\n", jobs[job_count - 1].job_id, jobs[job_count - 1].pid);
            } else {
                //fg_pid = pid;
                //add_job(pid, original_cmd, 1, 0);
                // Wait for the job to finish or be stopped
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
    }
}

// function to handle file redirection 
void handle_redirection(char **cmd_args){
    apply_redirections(cmd_args, psd);
}

// function to handle piping
void handle_pipe(char **cmd_args_left, char **cmd_args_right, int psd){
    int pipe_fd[2];
    pid_t pid1, pid2;
    
    if (pipe(pipe_fd) == -1){
        //perror("yash");
        return;
    }

    
    pid1 = fork(); // left child process
    if (pid1 == 0) {
        // first child left side command like ls
        dup2(pipe_fd[1], STDOUT_FILENO); // redirect stdout to the pipe
        
        close(pipe_fd[0]);
        close(pipe_fd[1]);

        // apply redirection for the left 
        apply_redirections(cmd_args_left, psd);

        if (execvp(cmd_args_left[0], cmd_args_left == -1)) {
            perror("execvp failed for the left pipe"); // debugging
            exit(EXIT_FAILURE);
            //return;
        } 
    }
    
    pid2 = fork(); // right sid of the pipe child process
    if (pid2 == 0) {
        
        dup2(pipe_fd[0], STDIN_FILENO); // redirect stdout to the pipe
        // close both ends of hte pipe in the child 
        close(pipe_fd[1]);
        close(pipe_fd[0]);

        // apply redirection for right command
        apply_redirections(cmd_args_right, psd);

        if (execvp(cmd_args_right[0], cmd_args_right == -1)) {
            perror("execvp failed for the right pipe");
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
        // if there is a foreground process , send sigint to it 
        kill(fg_pid, SIGINT);

    }

    //write(STDOUT_FILENO, "\n", 1);

    //write(STDOUT_FILENO, "# ", 2);
        // handle ctrz z prevent stopping the shell
        //printf("\nCaught SIGINT(Ctrl-C), but the shell is not stopped.\n#" );
    write(STDOUT_FILENO, "\n# ", 3);
    //tcflush(STDIN_FILENO, TCIFLUSH);
    fflush(stdout); // ensure the prompt is displayed immediately after the message

}

// signal handler for sigtstp ctrl z 
void sigtstp_handler(int sig){
    if (fg_pid > 0) {
        // if there is a foreground process , send sigint to it 
        kill(fg_pid, SIGTSTP);

        // find the command associat with the foreground process and add it 
        for (int i = 0; i < job_count; i++) {
            if (jobs[i].pid == fg_pid) {
                jobs[i].is_running = 0;
                //printf("\n[%d]      Stopped %s\n" ,jobs[i].job_id, jobs[i].command);
                break;
            }
        }

        fg_pid = -1; // clear the foreground process 

        // reprint the print
        // took these out becaue I didnt need a two new prompt lines 
        //write(STDOUT_FILENO, "# ", 2);
        //tcflush(STDIN_FILENO, TCIFLUSH); 
        //printf("# ");
        fflush(stdout); // ensure the prompt is displayed immediately after the message
    }
        
}

// signal handler for sigtstp ctrl z 
void sigchld_handler(int sig){
    int status;
    pid_t pid;
    // Clean up zombie child processes non blocking wait
    while ((pid = waitpid(-1, &status, WNOHANG)) > 0){ 
        for(int i =0; i < job_count; i++) {
            if (jobs[i].pid == pid) {
                if (WIFEXITED(status) || WIFSIGNALED(status)) {
                    // print done messageif needed
                    printf("\n[%d] Done %s\n", jobs[i].job_id, jobs[i].command);
                    // remove the job from the job list 
                    for (int j = i; j < job_count - 1; j++) {
                        jobs[j] = jobs[j + 1];
                    }
                    job_count--;
                }
                break;
            }
        }
    }
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

void apply_redirections(char **cmd_args, int psd){
    int i = 0;
    int in_fd = -1, out_fd = -1, err_fd = -1;

    while (cmd_args[i] != NULL){
        if (strcmp(cmd_args[i], "<") == 0){

            in_fd = open(cmd_args[i + 1], O_RDONLY); // input redirection 
            if (in_fd < 0 ){
                cmd_args[i] = NULL;
                //cmd_args[i - 1] = NULL;
                perror("yash");
                exit(EXIT_FAILURE); // exit child process on failure
                //return;
            }
            
            dup2(in_fd, STDIN_FILENO); // replace stdin with the file // socket file descriptor 
            close(in_fd);

            // shift arguements left to remove redirecton operator and file name 
            // doing this because err1.txt and err2.txt are not getting created for redirection
            for (int j = i; cmd_args[j + 2] != NULL; j++){
                cmd_args[j] = cmd_args[j+ 2];
            }
            // shift remaing arguments 
            cmd_args[i] = NULL; // remove < 
            //cmd_args[i + 1] = NULL; // remove the file name 
            i--; // adjust index to recheck this position 

        } 
        else if (strcmp(cmd_args[i], ">") == 0) { 
            // output direction 
            out_fd = open(cmd_args[i + 1], O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH );
            if (out_fd < 0) {
                perror("yash");
                exit(EXIT_FAILURE);
                //return;
            }
            // debugging 
            printf("redirection output file : %s\n" , cmd_args[i+1]);

            dup2(out_fd,STDOUT_FILENO);
            close(out_fd);

            // shift arguements left to remove redirecton operator and file name 
            // doing this because err1.txt and err2.txt are not getting created for redirection
            for (int j = i; cmd_args[j + 2] != NULL; j++){
                cmd_args[j] = cmd_args[j+ 2];
            }

            cmd_args[i] = NULL;
            // cmd_args[i + 1] = NULL;
            i--;
        } 
        else if (strcmp(cmd_args[i], "2>") == 0){
            // error redirection 
            err_fd = open(cmd_args[i + 1],O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH );
            if (err_fd < 0) {
                perror("yash");
                exit (EXIT_FAILURE);
                //return;
            }
            //printf("redirection output file : %s\n" , cmd_args[i+1]);
            dup2(err_fd,STDERR_FILENO);
            close(err_fd);

            // shift arguements left to remove redirecton operator and file name 
            // doing this because err1.txt and err2.txt are not getting created for redirection
            for (int j = i; cmd_args[j + 2] != NULL; j++){
                cmd_args[j] = cmd_args[j+ 2];
            }
            cmd_args[i] = NULL;
            //cmd_args[i + 1] = NULL;
            i--;
        }
        i++;
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