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

int main(int argc, char *argv[]){
    if (argc <2) {
        fprintf(stderr, "Usage: %s <server_ip>\n" , argv[0]);
        exit(1);
    }

    // placeholder for connecting to the server and sending commands
    printf("Client connecting to server %s\n", argv[1]);
    return 0;
}



