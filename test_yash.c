
#include <check.h>      // include the check framework for unit testing 
#include <stdio.h>      // standar input output functions 
#include <string.h>     // string handling functions 
#include <stdlib.h>     // include for exit success and exit failure 




#define PORT 3820

// create test 

// client code to be tested 
extern void send_command_to_server(const char *command);

// mock server resonse for the test 
char mock_server_response[] = " mock server response output\n";

// test case for sending a command 
START_TEST(test_send_command_to_server) {
    // example of what to expect the command to look like 
    char test_command [] = " ls -l";

    // call the function i am testing 
    send_command_to_server(test_command);

    // check if the mock response matches what is expected 
    ck_assert_str_eq(mock_server_response, "mocked server response output\n");
}

END_TEST

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