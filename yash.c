// this is where the client will be set up 
/*
 Step 1 Include necessary headers and global variables from the book 
 Step 2: Set up the Test function to send a command to the server 
 step 3: set up the signal handlers - ctrl c sigint control z sigtstp
 step 4: Establish a connection to the server 
 step 5: set up the command input loop 
 step 6: close the connection 
 step 7 main function 
*/
// Step 1 Include necessary headers and global variables from book
#include "yashd.h"


#define PORT 3820           // port number to connect to the server
#define BUFFER_SIZE 1024    // buffer size for communication

int promptMode = 0;

int clientAlive = 1;

int sockfd = 0; // declare globally to use in signal handler


// Signal handler to manage ctrl c and ctrl z signals 
void sig_handler(int signo) {
    char ctl_msg[BUFFER_SIZE] = {0};    // Buffer for control message
    if (signo ==SIGINT) {
        snprintf(ctl_msg, sizeof(ctl_msg), "CTL c\n"); // Send ctl c for ctrl-c
    } else if (signo == SIGTSTP) {
        snprintf(ctl_msg, sizeof(ctl_msg), "CTL z\n"); // Send ctl z for ctrl-z
    }

    // Send the control message to the server
    if ( send(sockfd, ctl_msg, strlen(ctl_msg), 0) < 0) {
        perror("Failed to send control signal");
    }
    printf("Control signal sent to server. \n");
}

// A Test function to send a command to the server 
void send_command_to_server(const char *command) {
    char message[BUFFER_SIZE] = {0}; // Buffer for the message to be sent 

    // debuggin output: check what is being sent
    // printf("client sending command: %s\n", command);

    // Format the message to be sent to the server 
    snprintf(message, sizeof(message), "CMD %s\n", command);

    // Send the command to the server
    if (send(sockfd, message, strlen(message), 0) <0) {
        perror("Failed to send command");
        return;
    }
}

// handle plain text input after issuing commands like cat 
void handle_plain_text() {
    char line[BUFFER_SIZE] = {0};

    // the user can input multiple lines of text until the type eof (ctrl-d)
    while (fgets(line, sizeof(line), stdin))
    {
        /* code */
        if (send(sockfd, line, strlen(line), 0) < 0) {
            perror("Failed to send text to the server");
            break;
        }
    }
    // end of input to the server by closing the connection 
    shutdown(sockfd, SHUT_WR); // close write channel 
    
}

// Communication thread to handle communication with the server
void* commmunication_thread(void *args){
    // buffer for receiving response
    char buf[BUFFER_SIZE] = {0};

    while(clientAlive) {
        // read response from the server
        int bytesRead = recv(sockfd, buf, BUFFER_SIZE, 0);
        if (bytesRead < 0) {
            perror("Error receiving response");
        } else if (bytesRead > 0) {
            if (strcmp(buf, "\n# ") ==0) {
                promptMode = 1;
                // printf("%s", buf); // print the servers response
            }
            printf("%s", buf); // print the servers response
            fflush(stdout);
        } else {
            // server has closed the connection
            printf("Server closed the connection\n");
            // kill(getppid(), SIGINT); // kill the parent process
            break;
        }
    }
    pthread_exit(NULL);
}

void cleanup(char *buf)
{
    int i;
    for(i=0; i<BUFFER_SIZE; i++) buf[i]='\0';
}


#ifndef TESTING

int main(int argc, char *argv[]){

    pthread_t thread;
    int threadStatus;

    struct sockaddr_in servaddr; // structure for server address 

    // check for correct number of arguments expect server ip 
    if ( argc !=2) {
        fprintf(stderr, "Usage: %s <server_ip>\n", argv[0]);
        return EXIT_FAILURE;
    }

    // create a sockt for communication ipv4, tcp 
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) <0) {
        perror("Socket creation failed");
        return EXIT_FAILURE;
    }

    // setup server address structure 
    servaddr.sin_family = AF_INET; // ipv4 protocol 
    servaddr.sin_port = htons(PORT); // convert port number to network byte

    // convert ip address from text to binary format 
    if (inet_pton(AF_INET, argv[1], &servaddr.sin_addr) <= 0) {
        perror("Invalid ip address / address not supported");
        return EXIT_FAILURE;
    }

    // connect socket to server address
    if(connect(sockfd, (struct sockaddr *) &servaddr, sizeof(servaddr)) <0) {
        perror("Connectio to server failed");
        return EXIT_FAILURE;
    }

    // set up signal handlers for ctrl c and ctrl z 
    signal(SIGINT, sig_handler); // catch ctrl c sigint
    signal(SIGTSTP, sig_handler); // catch ctrl z sigtstp

    if ((threadStatus = pthread_create(&thread, NULL, commmunication_thread, (void *)NULL))) {
      fprintf(stderr, "error: pthread_create, rc: %d\n", threadStatus);
      return EXIT_FAILURE;
    }

    printf("Connected to server at %s:%d\n", argv[1], PORT);
    // printf("# "); // display prompt

    /* code */
    char command[BUFFER_SIZE] = {0}; // buffer for user input 

    // main loop to send commands and receive responses
    while (1) {
        cleanup(command); // clear the buffer

        // read command input from the user 
        if (fgets(command, sizeof(command), stdin) ==  NULL) {
            if (feof(stdin)) {
                if(promptMode){
                    printf("Client terminating....\n");
                    clientAlive = 0;
                    // return 0;
                    break; // exit on eof ctrl d 
                }
                if(send(sockfd, "CTL d\n", strlen("CTL d\n"), 0) < 0) {
                    perror("Failed to send control-d signal");
                }
                continue;
            } else {
                perror("error reading command");
                continue;
            }
        }

        // remove the newline character from the input 
        command[strcspn(command, "\n")] = 0;

        // // check if hte command is a file redirecton command cat>file.txt
        // if (strstr(command, ">") != NULL) {
        //     send_command_to_server(command); // send command to the server
        //     printf("Enter plain text (CTRL-D to end): \n");
        //     handle_plain_text(); // hnadle plain text for cat 
        // } else { 
        //     // Use the function Send_command_to_server 
        //     send_command_to_server(command);

        send_command_to_server(command);

        // }
        
    }
    // close the socket 
    // pthread_join(thread, NULL);
    
    printf("yash client terminated.\n");
    close(sockfd);
    return 0;
}

#endif
