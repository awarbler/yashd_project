// include a guard to prevent multiple inclusions of the header file

#ifndef YASHD_H // If yashd_h is not defined
#define YASHD_H // define yashd_h

#define PORT 3820   

// Standar libraries 
#include <stdio.h> // standard input output library 
#include <stdlib.h> // standard library includes functions malloc() free() 
#include <pthread.h> // pthread header
#include <netinet/in.h> // for sockets /* inet_addr() */
#include <arpa/inet.h>      // internet operations inetpton()
#include <unistd.h>         // POSIX api for close () 
#include <signal.h>         // signal handling sigint sigtstp 
#include <errno.h>          // error handling macros

// Server headers
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/file.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <fcntl.h>
#include <netdb.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <string.h> /* memset() */

// Constants 
#define PROMPT "\n# "
#define MAX_INPUT 200
#define MAX_ARGS 100
#define MAX_JOBS 20 
#define BUFFER_SIZE 1024  
#define PATHMAX 255
#define MAXHOSTNAME 80


// function prototype for handle_client
// This function is responsible for handling communication with a connected client in a separate thread 
// this prototype declares the function so that it can be used in multiple source files where the header is included
// This fulfills the requirement of using a multi-threaded server to handle multiple clients concurrently. 
void *handle_client(void *arg);


#endif // end of the include. 
