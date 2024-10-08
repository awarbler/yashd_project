// This is where the server will be set up
// Troubleshooting the server
// 1. Error binding name to stream socket: Address already in use
// Run: lsof -i :3820
// Run: kill -9 <PID>

// basic functionality for connecting to the server

#include "yashd.h"
#include "pipes.h"

#define MAX_ARGS 200

/* Initialize path variables */
static char *server_path = "./tmp/yashd";
static char *log_path = "./tmp/yashd.log";
static char *pid_path = "./tmp/yashd.pid";

/* Initialize mutex lock */
pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

void reusePort(int sock);
void *serveClient(void *args);

/* create thread argument struct for logRequest() thread */
typedef struct LogRequestArgs
{
  char request[200];
  struct sockaddr_in from;
} LogRequestArgs;

/* create thread argument struct for client thread */
typedef struct
{
  int psd;
  struct sockaddr_in from;
} ClientArgs;

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
  int status;

  fprintf(stderr, "Child terminated\n");
  wait(&status); /* So no zombies */
}

/**
 * @brief Initializes the current program as a daemon, by changing working
 *  directory, umask, and eliminating control terminal,
 *  setting signal handlers, saving pid, making sure that only
 *  one daemon is running. Modified from R.Stevens.
 * @param[in] path is where the daemon eventually operates
 * @param[in] mask is the umask typically set to 0
 */
void daemon_init(const char *const path, uint mask)
{
  pid_t pid;
  char buff[256];
  int fd;
  int k;

  /* put server in background (with init as parent) */
  if ((pid = fork()) < 0)
  {
    perror("daemon_init: cannot fork");
    exit(0);
  }
  else if (pid > 0) /* The parent */
    exit(0);

  /* the child */

  /* Close all file descriptors that are open */
  for (k = getdtablesize() - 1; k > 0; k--)
    close(k);

  /* Redirecting stdin, stdout, and stdout to /dev/null */
  if ((fd = open("/dev/null", O_RDWR)) < 0)
  {
    perror("Open");
    exit(0);
  }
  dup2(fd, STDIN_FILENO);  /* detach stdin */
  dup2(fd, STDOUT_FILENO); /* detach stdout */
  dup2(fd, STDERR_FILENO); /* detach stderr */
  close(fd);
  /* From this point on printf and scanf have no effect */

  /* Establish handlers for signals */
  if (signal(SIGCHLD, sig_chld) < 0)
  {
    perror("Signal SIGCHLD");
    exit(1);
  }
  if (signal(SIGPIPE, sig_pipe) < 0)
  {
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
  if ((k = open(pid_path, O_RDWR | O_CREAT, 0666)) < 0)
    exit(1);
  if (lockf(k, F_TLOCK, 0) != 0)
    exit(0);

  /* Save server's pid without closing file (so lock remains)*/
  sprintf(buff, "%6d", pid);
  write(k, buff, strlen(buff));

  return;
}

int main(int argc, char **argv)
{
  int sd, psd;
  struct sockaddr_in server;
  struct hostent *hp;
  struct sockaddr_in from;
  int fromlen;
  int length;
  int childpid;

  // TO-DO: Initialize the daemon
  // daemon_init(server_path, 0);

  hp = gethostbyname("localhost");
  bcopy(hp->h_addr, &(server.sin_addr), hp->h_length);
  printf("(TCP/Server INET ADDRESS is: %s )\n", inet_ntoa(server.sin_addr));

  /** Construct name of socket */
  server.sin_family = AF_INET;
  server.sin_addr.s_addr = htonl(INADDR_ANY);
  server.sin_port = htons(3820);

  /** Create socket on which to send and receive */
  sd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (sd < 0)
  {
    perror("opening stream socket");
    exit(-1);
  }

  /** this allow the server to re-start quickly instead of waiting
  for TIME_WAIT which can be as large as 2 minutes */
  reusePort(sd);

  /** Bind the socket to the server address */
  if (bind(sd, (struct sockaddr *)&server, sizeof(server)) < 0)
  {
    close(sd);
    perror("Error binding name to stream socket");
    exit(-1);
  }

  /** get port information and prints it out */
  length = sizeof(server);
  if (getsockname(sd, (struct sockaddr *)&server, &length))
  {
    perror("Error getting socket name");
    exit(0);
  }
  printf("Server Port is: %d\n", ntohs(server.sin_port));

  /** accept TCP connections from clients and thread a process to serve each request */
  listen(sd, 4);
  fromlen = sizeof(from);
  printf("Server is ready to receive\n");

  /** Infinite loop to accept and server clients */
  for (;;) {
    psd = accept(sd, (struct sockaddr *)&from, &fromlen);
    if (psd < 0) {
      perror("accepting connection");
      continue;
    }

    // Allocate memory for ClientArgs
    ClientArgs *clientArgs = malloc(sizeof(ClientArgs));
    if (clientArgs == NULL) {
      perror("Error allocating memory for client arguments");
      close(psd);
      continue;
    }

    printf("Memore allocated for client arguments\n");

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
    printf("Thread created to serve client\n");

    pthread_detach(thread); // Detach the thread to unblock it
  }
  // Destroy the mutex
  pthread_mutex_destroy(&lock);
  return 0;
}

/**
 * @brief this function handles logging the client requests into a log file.
 */

void *logRequest(void *args)
{
  LogRequestArgs *logArgs = (LogRequestArgs *)args;
  char *request = logArgs->request;
  struct sockaddr_in from = logArgs->from;

  // log time and client information
  time_t rawtime;
  struct tm *timeinfo;
  char timeString[80];
  FILE *logFile;

  time(&rawtime);
  timeinfo = localtime(&rawtime);
  strftime(timeString, 80, "%b %d %H:%M:%S", timeinfo);

  pthread_mutex_lock(&lock); // wrap CS in lock ...this prevents race conditions

  printf("Opening log file at path: %s\n", log_path);

  logFile = fopen(log_path, "a");
  if (logFile == NULL)
  {
    perror("Error opening log file");
    pthread_mutex_unlock(&lock); // Unlock the mutex before returning
    return NULL;
  }

  // Debugging statement to check if writing to the file
  printf("Writing to log file: %s\n", request);

  fprintf(logFile, "%s yashd[%s:%d]: %s", timeString, inet_ntoa(from.sin_addr), ntohs(from.sin_port), request);
  fclose(logFile);
  pthread_mutex_unlock(&lock); // ... unlock

  pthread_exit(NULL);
}


/**
 * @brief Function that handles serving a single client.
 */
void *serveClient(void *args)
{
  ClientArgs *clientArgs = (ClientArgs *)args;
  int psd = clientArgs->psd;
  struct sockaddr_in from = clientArgs->from;
  char buf[512];
  int rc;
  struct hostent *hp;
  pthread_t p;

  if ((hp = gethostbyaddr((char *)&from.sin_addr.s_addr, sizeof(from.sin_addr.s_addr), AF_INET)) == NULL)
    fprintf(stderr, "Can't find host %s\n", inet_ntoa(from.sin_addr));
  else
    printf("(Name is : %s)\n", hp->h_name);

  /**  get data from  client and send it back */
  for (;;) { 
    printf("\n...server is waiting...\n"); // infinite loop to keep the server running
    if ((rc = recv(psd, buf, sizeof(buf), 0)) < 0) { 
      perror("receiving stream  message");// read data from the client
      exit(-1);
    }

    printf("Received: %s\n", buf);

    if (rc > 0) { 
      buf[rc] = '\0'; // if there is data to read
      printf("Server received command: %s\n", buf); // debugging output

      // remove the cmd prefix from the command before processing
      if (strncmp(buf, "CMD ", 4) == 0) {
        memmove(buf, buf + 4, strlen(buf) - 3); // shift the buffer to remove cmd
      }

      // Allocate memory for LogRequestArgs
      LogRequestArgs *args = (LogRequestArgs *)malloc(sizeof(LogRequestArgs));
      if (args == NULL)
      {
        perror("Error allocating memory for log request");
        continue;
      }

      // Copy the request and client information to the struct
      strncpy(args->request, buf, sizeof(args->request) - 1);
      // args->request[sizeof(args->request) - 1] = '\0'; // Ensure null-termination
      args->from = from;

      // Create a new thread for logging
      if (pthread_create(&p, NULL, logRequest, (void *)args) != 0)
      {
        perror("Error creating log request thread");
        free(args); // Free the allocated memory if thread creation fails
      }
      else
      {
        pthread_detach(p); // Detach the thread to allow it to run independently
      }


      // create pipe to capture stdout
      int pipe_fd[2];

      if (pipe(pipe_fd) == -1) {
        perror("Pipe failed");
        continue;
      }

      // TO-DO: Handle the command from the client


      printf("Handling client command: %s\n", buf);

      // Create a new process to execute the command 
      int pid = fork(); 
      if (pid == 0) {
        // Child process// Redirect stdout and stderr to the client socket
        dup2(psd, STDOUT_FILENO);
        dup2(psd, STDERR_FILENO);
        close(psd); // Close the socket descriptor

        // flush stdout and stderr to ensure the oput is sent to the client immediately 
        fflush(stdout);
        fflush(stderr);

        // split the received command into argument using strtok() 
        char *cmd_args[10]; // used cmd_args instead of args to avoid conflict 
        cmd_args[0] = strtok(buf, " \n");

        int i = 1;
        while ((cmd_args[i] = strtok(NULL, " \n")) != NULL)i++;

        cmd_args[i] = NULL; // NULL terminate the array for execvp
        
        // executing the command execvp that we used in yash shell
        if (execvp(cmd_args[0], cmd_args) < 0) { 
          perror("Execute failed");
        }// child process should exit after executing 
        exit(0); 
      } else if (pid > 0) { 
        wait(NULL); // parent process // wait for child process to finish 
        if (send(psd, buf, rc, 0) <0 )
		          perror("sending stream message");
      } else { // connection closed by client 
          close(psd);
          // exit(0);
          pthread_exit(0);
        }
        printf("\n...another round...\n");
    }
  }
    //pthread_exit(NULL);
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
