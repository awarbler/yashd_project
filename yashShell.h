// this is the yashShell.h header file

// include a guard to prevent mutiple inclusions of the header file

#ifndef YASHSHELL_H // If yashd_h is not defined
#define YASHSHELL_H // define yashd_h

#include "yashd.h"

#include <signal.h>         // signal handling sigint sigtstp 

// Max length for input command or Max number of jobs running at the same time: 20
# define MAX_INPUT 200
# define MAX_ARGS 100
# define MAX_JOBS 20 


// Shared function Prototypes // Job Control and Piping 
void execute_command(char **cmd_args, char *original_cmd, int psd);
void handle_redirection(char **cmd_args);
void handle_pipe(char **cmd_args_left, char **cmd_args_right, int psd);//function to handle piping commands
void apply_redirections(char **cmd_args);
void remove_elements(char **args, int start, int count);
void add_job(pid_t pid, const char *command, int is_running, int is_background);
void update_job_markers(int current_job_index) ;
void print_jobs();
void fg_job(int job_id);
void bg_job(int job_id);
void yash_loop(int psd);

void setup_signal_handlers(); // function to set up signal handlers 
void sigint_handler(int sig);// signal handler for SIGINT(Ctrl - c)
void sigtstp_handler(int sig); // signal handler for sigtstp(Ctrl-Z) - copied from WeekOneCode
void sigchld_handler(int sig);
void handle_control(int psd, char control);
//void handle_command(int psd, char *command);


#endif // end of the include. 
