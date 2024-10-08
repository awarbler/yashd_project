// This is where the server will be set up 
// pseudo code key components

// main() { 
// daemonize(); // convert to a daemon
// setup_socket(); // bind and listen on port 3820
// while(1){client_fd = accept_connection(); // accept client connection
// pthread_create(thread, handle_client, client_fd)// handle each client in a new thread
//}
//}
// handle_client(client_fd){
// read command from client, fork exec the command, send output back to client
// log command to /tmp/yashd.log }


// basic functionality for connecting to the server

#include "yashd.h"

extern int errno;

#define PATHMAX 255

/* Initialize path variables */
static char* server_path = "/tmp/yashd";
static char* log_path = "/tmp/yashd.log";
static char* pid_path = "/tmp/yashd.pid";

/* create thread argument struct for logRequest() */
typedef struct _thread_log_t {
  char * request;
  struct sockaddr_in from;
} _thread_log_t;

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
  static FILE *log; /* for the log */
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

  /* Redirecting stdin and stdout to /dev/null */
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


#define MAXHOSTNAME 80
void reusePort(int sock);
void Serve(int psd, struct sockaddr_in from);


int main(int argc, char **argv ) {
    int   sd, psd;
    struct   sockaddr_in server;
    struct  hostent *hp;
    struct sockaddr_in from;
    int fromlen;
    int length;
    int childpid;

    char* ThisHost = "localhost";

    // initialize the daemon
    // daemon_init(server_path, 0);
    
    printf("----TCP/Server running at host NAME: %s\n", ThisHost);
    if  ( (hp = gethostbyname(ThisHost)) == NULL ) {
      fprintf(stderr, "Can't find host %s\n", argv[1]);
      exit(-1);
    }
    bcopy ( hp->h_addr, &(server.sin_addr), hp->h_length);
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
	perror("binding name to stream socket");
	exit(-1);
    }
    
    /** get port information and  prints it out */
    length = sizeof(server);
    if ( getsockname (sd, (struct sockaddr *)&server,&length) ) {
	perror("getting socket name");
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
}

void logRequest(char *request, struct sockaddr_in from) {
    time_t rawtime;
    struct tm *timeinfo;
    char timeString[80];

    time(&rawtime);
    timeinfo = localtime(&rawtime);
    strftime(timeString, 80, "%b %d %H:%M:%S", timeinfo);

    printf("%s yashd[%s:%d]: %s\n", timeString, inet_ntoa(from.sin_addr), ntohs(from.sin_port), request);
    // pthread_exit(NULL);
 }

void Serve(int psd, struct sockaddr_in from) {
    char buf[512];
    int rc;
    struct  hostent *hp;
    pthread_t p;

    
    // *gethostbyname();
    
    // printf("Serving %s:%d\n", inet_ntoa(from.sin_addr),
	//    ntohs(from.sin_port));
 
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
            // pthread_create(&p, NULL, logRequest, (void *) argin); // passing an int but create expects void* so cast it
            // pthread_join(p, (void **) &m); 
            logRequest(buf, from); 
            if (send(psd, buf, rc, 0) <0 )
            perror("sending stream message");
        }
        else { // connection closed by client 
            printf("TCP/Client: %s:%d\n", inet_ntoa(from.sin_addr),
            ntohs(from.sin_port));
            printf("(Name is : %s)\n", hp->h_name);
            printf("Disconnected..\n");
            close (psd);
            exit(0);
        }
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