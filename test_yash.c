
#include <check.h>      // include the check framework for unit testing 
#include <stdio.h>      // standar input output functions 
#include <string.h>     // string handling functions 
#include <stdlib.h>     // include for exit success and exit failure 

#include <arpa/inet.h>      // internet operations inetpton()
#include <unistd.h>         // posix api for close () 
#include <signal.h>         // signal handling sigint sigtstp 
#include <sys/socket.h>
#include <netinet/in.h> 

#define PORT 3820
#define BUFFER_SIZE 1024

// create test 

// client code to be tested 
extern void send_command_to_server(const char *command);

extern int sockfd; // declare the global socket file descriptor

// set up the socket connection for the test 
int setup_socket_connection(const char *server_ip) {
    struct sockaddr_in servaddr; // structure for server address 

    // create a sockt for communication ipv4, tcp 
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) <0) {
        perror("Socket creation failed");
        return EXIT_FAILURE;
    }

    // setup server address structure 
    servaddr.sin_family = AF_INET; // ipv4 protocol 
    servaddr.sin_port = htons(PORT); // convert port number to network byte

    // convert ip address from text to binary format 
    if (inet_pton(AF_INET, server_ip, &servaddr.sin_addr) <= 0) {
        perror("Invalid ip address / address not supported");
        return EXIT_FAILURE;
    }

    // connect socket to server address
    if(connect(sockfd, (struct sockaddr *) &servaddr, sizeof(servaddr)) <0) {
        perror("Connectio to server failed");
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;

}


// mock server resonse for the test 
//char mock_server_response[] = " mock server response output\n";

// test case for sending a command 
START_TEST(test_send_command_to_server) {

    // setup the socket connection to the mock server
    if (setup_socket_connection("127.0.0.1") != EXIT_SUCCESS) {
        printf("Failed to setup socket connection. \n");
    }

    // example of what to expect the command to look like 
    char test_command [] = " ls -l";

    // call the function i am testing 
    send_command_to_server(test_command);

    // we dont have to mock the server here because we are using a real mock server
    printf("Command sent and response recieved.\n"); 

    // check if the mock response matches what is expected 
    //ck_assert_str_eq(mock_server_response, "mocked server response output\n");

    // close the socket after the test is done
    close(sockfd);
}

END_TEST

// create a suite that groups togehter one or more test cases
Suite* client_suite(void) {
    Suite *s;
    TCase *tc_core;

    s = suite_create("Client");

    // core test case
    tc_core = tcase_create("Core");

    // add the test case for command sending 
    tcase_add_test(tc_core, test_send_command_to_server);
    suite_add_tcase(s, tc_core);

    return s;
}

int main(void) {
    int number_failed;
    Suite *s;
    SRunner *sr;

    s = client_suite();
    sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;


}