// This is where the server will be set up 
// Troubleshooting the server
// 1. Error binding name to stream socket: Address already in use 
// Run: sudo lsof -i :3820
// Run: sudo kill -9 <PID>
// basic functionality for connecting to the server

#include "yashd.h"

/* Initialize path variables */
static char* server_path = "./tmp/yashd";
static char* log_path = "./tmp/yashd.log";
static char* pid_path = "./tmp/yashd.pid";

/* Initialize mutex lock */
pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;


void reusePort(int sock);
void Serve(int psd, struct sockaddr_in from);

/* create thread argument struct for logRequest() thread */
typedef struct LogRequestArgs {
  char request[200];
  struct sockaddr_in from;
} LogRequestArgs;

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
    int fromlen;
    int length;
    int childpid;

    char* ThisHost = "localhost";

    // TO-DO: Initialize the daemon
    // daemon_init(server_path, 0);
    
    printf("----TCP/Server running at host NAME: %s\n", ThisHost);
    if  ( (hp = gethostbyname(ThisHost)) == NULL ) {
      fprintf(stderr, "Can't find host %s\n", argv[1]);
      exit(-1);
    }
    bcopy( hp->h_addr, &(server.sin_addr), hp->h_length);
    printf("(TCP/Server INET ADDRESS is: %s )\n", inet_ntoa(server.sin_addr));
    
    /** Construct name of socket */
    server.sin_family = AF_INET;
    /* OR server.sin_family = hp->h_addrtype; */
    
    server.sin_addr.s_addr = htonl(INADDR_ANY);
    server.sin_port = htons(3820);  
    
    /** Create socket on which to send  and receive */
    
    sd = socket (AF_INET, SOCK_STREAM, IPPROTO_TCP); 
    /* OR sd = socket (hp->h_addrtype,SOCK_STREAM,0); */
    if (sd<0) {
	    perror("opening stream socket");
	    exit(-1);
    }
    /** this allow the server to re-start quickly instead of waiting
	  for TIME_WAIT which can be as large as 2 minutes */
    reusePort(sd);
    if ( bind( sd, (struct sockaddr *) &server, sizeof(server) ) < 0 ) {
      close(sd);
      perror("Error binding name to stream socket");
      exit(-1);
    }
    
    /** get port information and prints it out */
    length = sizeof(server);
    if ( getsockname (sd, (struct sockaddr *)&server,&length) ) {
      perror("Error getting socket name");
      exit(0);
    }
    printf("Server Port is: %d\n", ntohs(server.sin_port));
    
    /** accept TCP connections from clients and fork a process to serve each */
    listen(sd,4);
    fromlen = sizeof(from);
    for(;;){
      psd  = accept(sd, (struct sockaddr *)&from, &fromlen);
      childpid = fork();
      if ( childpid == 0) {
          close (sd);
          Serve(psd, from);
      }
      else{
          printf("My new child pid is %d\n", childpid);
          close(psd);
      }
    }
    // Destroy the mutex
    pthread_mutex_destroy(&lock);
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


    fprintf(logFile,"%s yashd[%s:%d]: %s", timeString, inet_ntoa(from.sin_addr), ntohs(from.sin_port), request);
    fclose(logFile);
    pthread_mutex_unlock(&lock); // ... unlock
    
    pthread_exit(NULL);
 }

void Serve(int psd, struct sockaddr_in from) {
    char buf[512];
    int rc;
    struct  hostent *hp;
    pthread_t p;
    
    // printf("Serving %s:%d\n", inet_ntoa(from.sin_addr),
	  // ntohs(from.sin_port));
 
    if ((hp = gethostbyaddr((char *)&from.sin_addr.s_addr, sizeof(from.sin_addr.s_addr),AF_INET)) == NULL)
	    fprintf(stderr, "Can't find host %s\n", inet_ntoa(from.sin_addr));
    else
	    printf("(Name is : %s)\n", hp->h_name);
    
    /**  get data from  client and send it back */
    for(;;){ // infinite loop to keep the server running
        printf("\n...server is waiting...\n");
        if( (rc=recv(psd, buf, sizeof(buf), 0)) < 0){ // read data from the client
            perror("receiving stream  message");
            exit(-1);
        }
        if (rc > 0){ // if there is data to read
            buf[rc]='\0';

            // Allocate memory for LogRequestArgs
            LogRequestArgs *args = (LogRequestArgs *)malloc(sizeof(LogRequestArgs));
            if (args == NULL) {
            perror("Error allocating memory for log request");
            continue;
            }
            
            printf("1. command: %s", buf);
            // Copy the request and client information to the struct
            strncpy(args->request, buf, sizeof(args->request) - 1);
            // args->request[sizeof(args->request) - 1] = '\0'; // Ensure null-termination
            args->from = from;

            // Create a new thread for logging
            if (pthread_create(&p, NULL, logRequest, (void *)args) != 0) {
              perror("Error creating log request thread");
              free(args); // Free the allocated memory if thread creation fails
            } else {
              pthread_detach(p); // Detach the thread to allow it to run independently
            }
            
            // TO-DO: Handle the command from the client
        }
        else { // connection closed by client 
          printf("TCP/Client: %s:%d\n", inet_ntoa(from.sin_addr),
          ntohs(from.sin_port));
          printf("(Name is : %s)\n", hp->h_name);
          printf("Disconnected..\n");
          close (psd);
          exit(0);
        }
        printf("\n...another round...\n");
    }
}

void reusePort(int s)
{
    int one=1;
    
    if ( setsockopt(s,SOL_SOCKET,SO_REUSEADDR,(char *) &one,sizeof(one)) == -1 )
	{
	    printf("error in setsockopt,SO_REUSEPORT \n");
	    exit(-1);
	}
}