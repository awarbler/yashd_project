// this is where the client will be set up 

/*
  Step 1 Include necessar headers and global variables from book 
  Step 2: Set up Test function to send a command to the server 
  step 3: set up the signal handlers - ctrl c sigint control z sigtstp
  step 4: Establish connewction to the server 
  step 5: set up command input loop 
  step 6 : close the connection 
  step 7 main function 


*/


// Step 1 Include necessar headers and global variables from book
#include "yashd.h"
#include <stdio.h>          // standard i/o functions
#include <stdlib.h>         // standard library functions exit
#include <string.h>         // string handling functions eg strlen
#include <arpa/inet.h>      // internet operations inetpton()
#include <unistd.h>         // posix api for close () 
#include <signal.h>         // signal handling sigint sigtstp 

#define PORT 3820           // port number to connect to the server
#define BUFFER_SIZE 1024    // buffer size for communication

// pseduo code for key components
// main server(server_ip){
// connect_to_server(server_ip, 3820);
// while(1){
// read_user_input(); // read command from user
// send_command(); // send to server
// handle_response(); // display server response.
//}
//}

int sockfd = 0; // declare globally to use in signal handler

void sig_handler(int signo) {
    char ctl_msg[BUFFER_SIZE] = {0};    // buffer for control message
    if (signo ==SIGINT) {
        snprintf(ctl_msg, sizeof(ctl_msg), "CTL c\n"); // send ctl c for ctrl-c
    } else if (signo == SIGTSTP) {
        snprintf(ctl_msg, sizeof(ctl_msg), "CTL z\n"); // send ctl z for ctrl-z
    }

    // send control message to the server
    if ( send(sockfd, ctl_msg, strlen(ctl_msg), 0) < 0) {
        perror("Failed to send control signal");
    }
    printf("Control signal sent to server. \n");
}

// Test function to send a command to the server 
void send_command_to_server(const char *command) {
    char message[BUFFER_SIZE] = {0}; // buffer for the message to be sent 

    // format the message to be sent to the server 
    snprintf(message, sizeof(message), "cmd %s\n", command);

    // send the command to the server
    if (send(sockfd, message, strlen(message), 0) <0) {
        perror("Failed to send command");
        return;
    }

    // buffer for receiving response
    char buf[BUFFER_SIZE] = {0};

    // rec response from server 
    int valread = recv(sockfd, buf, BUFFER_SIZE, 0);
    if (valread < 0) {
        perror("error receiving response");
    } else if (valread > 0) {
        printf("%s", buf); // print the servers response
    }
}



int main(int argc, char *argv[]){
    // code 
    printf("yash client running....\n");
    return 0;
}
