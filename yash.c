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
#define MAXHOSTNAME 80
#define PORT 3820           // port number to connect to the server
#define BUFFER_SIZE 1024    // buffer size for communication

//void sig_handler(int signo);
void GetUserInput();
void send_command_to_server(const char *command);
void* communication_thread(void *args);
void client_signal_handlers();
void handle_sigint(int sig);
void handle_sigtstp(int sig);
void handle_sigquit(int sig);
void cleanup(char *buf);

char buffer[BUFFER_SIZE];
char rbuf[BUFFER_SIZE];

int sockfd = 0; // declare globally to use in signal handler
int rc, cc;
int   sd; // Socket File Descriptor 
/* Initialize mutex lock */
pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;


int main(int argc, char *argv[]){
    
    pthread_t thread;
    int threadStatus;
    client_signal_handlers(); // Set up signal handlers

    struct sockaddr_in servaddr; // structure for server address 

    // check for correct number of arguments expect server ip 
    if ( argc !=2) {
        fprintf(stderr, "Usage: %s <server_ip>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    // setup server address structure 
    servaddr.sin_family = AF_INET; // ipv4 protocol 
    servaddr.sin_port = htons(PORT); // convert port number to network byte

    // create a sockt for communication ipv4, tcp 
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) <0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    // convert ip address from text to binary format 
    if (inet_pton(AF_INET, argv[1], &servaddr.sin_addr) <= 0) {
        perror("Invalid ip address / address not supported");
        exit(EXIT_FAILURE);
    }

    // connect socket to server address
    if(connect(sockfd, (struct sockaddr *) &servaddr, sizeof(servaddr)) <0) {
        perror("Error: Connection to server failed");
        close(sockfd);
        exit(EXIT_FAILURE);
    }
  
    if ((threadStatus = pthread_create(&thread, NULL, communication_thread, (void *)NULL))) {
      fprintf(stderr, "error: pthread_create, rc: %d\n", threadStatus);
      exit(EXIT_FAILURE);
    }

    printf("Connected to server at %s:%d\n", argv[1], PORT);
    // printf("# "); // display prompt

    // Get input
    GetUserInput();
   
    //char command[BUFFER_SIZE] = {0}; // buffer for user input 
    //// Main loop to send commands and receive responses
    //while (1) {
    //cleanup(command); // Clear the command buffer
    //// Read command input from the user 
    //if (fgets(command, sizeof(command), stdin) == NULL) {
    //    if (feof(stdin)) {
    //        printf("Client terminating....\n");
    //        // Send a disconnect signal to the server
    //        if(send(sockfd, "CTL d\n", strlen("CTL d\n"), 0) < 0) {
    //            perror("Failed to send control-d signal");
    //        }
    //        shutdown(sockfd, SHUT_RDWR); // Graceful shutdown
    //        break;
    //    } else {
    //        perror("Error reading command");
    //        break;
    //    }
    //}
    //fflush(stdout);
    //send_command_to_server(command); // Send the command to the server
    //}
    close(sockfd);
    pthread_join(thread, NULL);
    
    printf("The client has terminated.\n");
    
    return 0;
}

void GetUserInput()
{
    char *input = NULL;
    size_t len = 0;
    printf("\nType anything followed by RETURN, or type CTRL-D to exit\n");

    for(;;) {

    cleanup(buffer);
	rc=read(0,buffer, sizeof(buffer));

	if (rc == 0) {
        // Control D (EOF detected)
        printf("\nSending Ctrl+D to server........\n");
        pthread_mutex_lock(&lock);
        if (send(sd, "CTL d\n", 6, 0) <0) {
            perror("Sending Ctrl D message");
        }
        pthread_mutex_unlock(&lock);
        break;
    }
    buffer[rc -1] = '\0'; // Remove newline character 
    // Use send command to server for sending input
    send_command_to_server(buffer);
    }

    printf("EOF... exit\n");
    close(sd);
    //kill(getppid(), 9);
    pthread_exit(NULL);
    exit (0);
}
// A Test function to send a command to the server 
void send_command_to_server(const char *command) {

    pthread_mutex_lock(&lock); // wrap CS in lock ...
    fflush(stdout);
    char message[BUFFER_SIZE] = {0}; // Buffer for the message to be sent 

    cleanup(message); // clear the buffer
    // debuggin output: check what is being sent
    printf("Client sending command: %s\n", command);

    // split the command by spaces to handle redirection and pipes 
    // Use strtok to split the command
    char * command_copy = strdup(command);
    char *token = strtok(command_copy, " ");
   
    // Format the message to be sent to the server 
    snprintf(message, sizeof(message), "CMD %s\n", command);

    // Send the command to the server
    if (send(sockfd, message, strlen(message), 0) <0) {
        perror("Failed to send command");
        pthread_mutex_unlock(&lock); //  unlock before returning 
        return; // exit the current function without affecting the rest of the client loop 
        // if the send fails we would want the user to re-ejter the command or shut down.
    }
    fflush(stdout);
    free(command_copy);
    pthread_mutex_unlock(&lock); // ... unlock
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
            printf("Server closed the connection \n");
            close(sockfd);
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
void cleanup(char *buf)
{
    int i;
    for(i=0; i<BUFFER_SIZE; i++) buf[i]='\0';
}
void client_signal_handlers() {
    signal(SIGINT, handle_sigint);
    printf("Signal int handlers initialized.\n");

    signal(SIGTSTP, handle_sigtstp);
    printf("Signal tsp handlers initialized.\n");

    signal(SIGQUIT, handle_sigquit);
    printf("Signal quit handlers initialized.\n");

}
// Signal handler for Ctrl+C (SIGINT)
void handle_sigint(int sig) {
    printf("\nSending Ctrl+C (SIGINT) to server...\n");
    pthread_mutex_lock(&lock);
    if (send(sockfd, "CTL c\n", 6, 0) < 0) {
        perror("sending Ctrl+C message");
    }
    pthread_mutex_unlock(&lock);
}

// Signal handler for Ctrl+Z (SIGTSTP)
void handle_sigtstp(int sig) {
    printf("\nSending Ctrl+Z (SIGTSTP) to server...\n");
    pthread_mutex_lock(&lock);
    if (send(sockfd, "CTL z\n", 6, 0) < 0) {
        perror("sending Ctrl+Z message");
    }
    pthread_mutex_unlock(&lock);
}
// Signal handler for Ctrl+D (SIGQUIT)
void handle_sigquit(int sig) {
    printf("\nSending Ctrl+D (SIGQUIT) to server...\n");
    pthread_mutex_lock(&lock);
    if (send(sockfd, "CTL d\n", 6, 0) < 0) {
        perror("sending Ctrl+D message");
    }
    pthread_mutex_unlock(&lock);
    close(sockfd);
    exit(0);
}