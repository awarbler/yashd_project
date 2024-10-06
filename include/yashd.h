// this is the header file

// include a gurad to prevent mutiple inclusions of the header file

#ifndef YASHD_H // If yashd_h is not defined
#define YASHD_H // define yashd_h

// including standard libraries for various functionalitites
#include <stdio.h> // standard input output library 
#include <stdlib.h> // standard library includes functions malloc() free() 
#include <pthread.h> // pthread header
#include <netinet/in.h> // for sockets /* inet_addr() */


// function prototype for handle_client
// this function is responsible for handling communication with a connected client in a seperate thread 
// this protoype declares teh function so that it can be used in mutiple source files where the header is included
// this fullfills the requirement of using a mult threaded server to handle multiple clients concurrently. 
void *handle_client(void *arg);

#endif // end of the include. 
