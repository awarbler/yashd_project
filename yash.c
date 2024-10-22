// this is where the client will be set up yash.c
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

void cleanup(char *buf);
void sig_handler(int signo);
void send_command_to_server(const char *command);
void* communication_thread(void *args);

int promptMode = 0;

int clientAlive = 1;

int sockfd = 0; // declare globally to use in signal handler

/* Initialize mutex lock */
pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

void cleanup(char *buf)
{
    int i;
    for(i=0; i<BUFFER_SIZE; i++) buf[i]='\0';
}

// Signal handler to manage ctrl c and ctrl z signals 
void sig_handler(int signo) {
    char ctl_msg[BUFFER_SIZE] = {0};    // Buffer for control message
    if (signo ==SIGINT) {
        snprintf(ctl_msg, sizeof(ctl_msg), "CTL c\n"); // Send ctl c for ctrl-c
    } else if (signo == SIGTSTP) {
        snprintf(ctl_msg, sizeof(ctl_msg), "CTL z\n"); // Send ctl z for ctrl-z
    }

    // Send the control message to the server
    if (send(sockfd, ctl_msg, strlen(ctl_msg), 0) < 0) {
        perror("Failed to send control signal");
    }
    printf(" Control signal sent to server. \n");
}

// A Test function to send a command to the server 
void send_command_to_server(const char *command) {

    pthread_mutex_lock(&lock); // wrap CS in lock ...
    promptMode = 0;
    pthread_mutex_unlock(&lock); // ... unlock

    //printf("before send command: %d\n", promptMode); // display prompt
    fflush(stdout);
    char message[BUFFER_SIZE] = {0}; // Buffer for the message to be sent 

    cleanup(message); // clear the buffer

    // debuggin output: check what is being sent
    // printf("client sending command: %s\n", command);

    // split the command by spaces to handle redirection and pipes 
    // Use strtok to split the command
    char * command_copy = strdup(command);
    char *token = strtok(command_copy, " ");
   

    // Format the message to be sent to the server 
    snprintf(message, sizeof(message), "CMD %s\n", command);

    // Send the command to the server
    if (send(sockfd, message, strlen(message), 0) <0) {
        perror("Failed to send command");
        return;
    }

    //printf("after send command: %d\n", promptMode); // display prompt
    fflush(stdout);
}

void* communication_thread(void *args){
    // buffer for receiving response
    char buf[BUFFER_SIZE] = {0};
    char msg_buf[BUFFER_SIZE*10] = {0}; // Increased size to handle larger messages
    int bytesRead;

    while(1) {

        // read response from the server
        bytesRead = recv(sockfd, buf, BUFFER_SIZE -1, 0);
        if (bytesRead < 0) {
            perror("Error receiving response from server");
            pthread_exit(NULL);
        } else if (bytesRead == 0) {
            // server has closed the connection 
            printf("server closed the connection \n");
            pthread_exit(NULL);

        } else {
            buf[bytesRead] = '\0'; // Null-terminate the received data

            // Append the new data to the accumulated message buffer
            strncat(msg_buf, buf, bytesRead);

            // Process complete messages
            char *delimiter_pos;
            while ((delimiter_pos = strstr(msg_buf, "\n# ")) != NULL) {
                
                // We found a complete message
                size_t message_length = delimiter_pos - msg_buf + 3;

                // Print the message
                printf("%.*s", (int)message_length, msg_buf);
                fflush(stdout);

                // Remove the processed message from msg_buf
                size_t remaining_length = strlen(delimiter_pos + 3); // 3 for "\n# "
                memmove(msg_buf, delimiter_pos + 3, remaining_length + 1); // +1 to copy null terminator
                msg_buf[remaining_length] = '\0';
            }
        } 
    }
    pthread_exit(NULL);
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

    if ((threadStatus = pthread_create(&thread, NULL, communication_thread, (void *)NULL))) {
      fprintf(stderr, "error: pthread_create, rc: %d\n", threadStatus);
      return EXIT_FAILURE;
    }

    // set up signal handlers for ctrl c and ctrl z 
    signal(SIGINT, sig_handler); // catch ctrl c sigint
    signal(SIGTSTP, sig_handler); // catch ctrl z sigtstp

    printf("Connected to server at %s:%d\n", argv[1], PORT);
    // printf("# "); // display prompt

    /* code */
    char command[BUFFER_SIZE] = {0}; // buffer for user input 

    // main loop to send commands and receive responses
    while (1) {
        
        /*pthread_mutex_lock(&lock);
        if(promptMode) {
            printf("# ");
            //printf("1promptMode: %d\n", promptMode);
            fflush(stdout);
            promptMode = 0;
        }
        pthread_mutex_unlock(&lock);*/
        cleanup(command); // clear the 
        

        // read command input from the user 
        if (fgets(command, sizeof(command), stdin) ==  NULL) {
            if (feof(stdin)) {
                printf("Client terminating....\n");
                if(send(sockfd, "CTL d\n", strlen("CTL d\n"), 0) < 0) {
                    perror("Failed to send control-d signal");
                }
                shutdown(sockfd, SHUT_RDWR);
                break;
            } else {
                perror("error reading command");
                break;
            }
        }
        fflush(stdout);
        send_command_to_server(command);
    }

    close(sockfd);
    pthread_join(thread, NULL);
    
    printf("yash client terminated.\n");
    
    return 0;
}

#endif
