# yashd_project

Project Two 

#

# Overview

<!-- {Important!  Do not say in this section that this is college assignment.  Talk about what you are trying to accomplish as a software engineer to further your learning.}

{Provide a description the software that you wrote to demonstrate the Java language.}-->




<!--{Describe your purpose for writing this software.}-->


<!---{Provide a link to your YouTube demonstration.  It should be a 4-5 minute demo of the software running and a walkthrough of the code.  Focus should be on sharing what you learned about the language syntax.}-->

[Software Demo Video]()

# Development Environment

<!--{Describe the tools that you used to develop the software}-->
* Visual Studio Code
* Mac users Parallels
* Ubuntu
* C 
* Git / GitHub
  
# Getting Started
* Install Parallels
* Install Windows 10 or 11
* Install Visual Studio Windows 2022
* Install Ubuntu

<!--{Describe the programming language that you used and any libraries.}-->

# Useful Websites

<!--{Make a list of websites that you found helpful in this project}-->
* 
* [Visual Studio Install](https://learn.microsoft.com/en-us/visualstudio/install/install-visual-studio?view=vs-2022)
*  [Github](https://github.com/)
*   [Windows]
* [Visual Studio Code](https://code.visualstudio.com/docs/languages/java)


# File Structure

yashd_project/<br>
|___ yashd.c  # Server Code<br>
|___ yash.c   # Client Code<br>
|__yashd.h    # Header file for shared definitions<br>
|__ yashd.log # Log file for testing<br>
|--Makefile     # Makefile to compile the project <br>

|- .gitignore VS version<br>


# Collaborators

* Anita Woodford 
* Juan Gabriel Palacios Rodas quit the class in the middle of the project. 
*

# Implementation Report

# EE382V - Project 2: Yash Daemon Implementation Report

## 1. Daemon Initialization
- The server initializes with the function `daemon_init()`.
- The process forks, closes all open file descriptors, redirects standard I/O to `/dev/null`, sets up signal handlers (`SIGCHLD`, `SIGPIPE`), and changes its working directory.
- It also creates a PID file to ensure only one instance is running, as specified in the requirements.

## 2. Multi-threaded Server Using `pthreads`
- The main server loop in `main()` continuously accepts client connections using `accept()`.
- For each client, it creates a detached thread using `pthread_create()`.
- The thread function `serveClient()` handles each client independently, meeting the requirement for a responsive multi-threaded server.

## 3. Socket Communication and Port Reuse
- The function `reusePort()` is used to allow rebinding to port 3820, using the `SO_REUSEADDR` socket option.
- The server listens on port 3820 (`htons(3820)`), matching the requirement for a well-defined port.

## 4. Command Execution via `fork-exec`
- Commands are executed using the `fork()` and `execvp()` mechanisms inside `serveClient()`.
- The child process uses `execvp()` to run the specified command, following the required fork-exec model.

## 5. Logging Client Requests
- The server logs commands in `/tmp/yashd.log` using the `logRequest()` function.
- Each log entry includes the date, client IP, port, and the executed command, adhering to the required syslog-like format:
    ```
    Sep 18 10:02:55 yashd[127.0.0.1:7813]: ls -l
    ```
## 6. Protocol Adherence
- The server adheres to the specified protocol:
    - **Command Execution (`CMD`)**: The server handles messages starting with `CMD`, extracts the command string, and executes it.
    - **Control Messages (`CTL`)**: The server processes control commands (`CTL c`, `CTL z`, `CTL d`) for handling `Ctrl+C`, `Ctrl+Z`, and `Ctrl+D` using `handle_control()`.
    - **Plain Text Input**: Input from the client (e.g., for `cat > file.txt`) is handled as plain text in `handle_cat_command()`.

## 7. Job Control (`jobs`, `fg`, `bg`)
- The server implements job control with the following functions:
    - `add_job()`, `remove_job()`, and `update_job_status()` manage the job list.
    - `print_jobs()` lists all active jobs.
    - `fg_command()` brings a background job to the foreground.
    - `bg_command()` resumes a stopped job in the background.
- The job list is managed using an array of structs (`jobs`), storing details such as `pid`, `job_id`, `command`, etc.

## 8. Signal Handling (`SIGINT`, `SIGTSTP`, `SIGCHLD`)
- The server sets up signal handlers with `setup_signal_handlers()`.
    - `sigint_handler()` handles `Ctrl+C` (`SIGINT`).
    - `sigtstp_handler()` handles `Ctrl+Z` (`SIGTSTP`).
    - `sigchld_handler()` handles child process state changes (`SIGCHLD`) to reap zombie processes.

## 9. Client-Server Communication
- The server sends responses to the client using `send()`, and the client receives and displays the output.
- The prompt is sent back as `\n#` to indicate the end of output, ensuring the client displays the prompt correctly.

## 10. Testing and Debugging Statements
- Debugging print statements are included throughout the server (e.g., when jobs are added, removed, or updated).
- This helps trace the flow of execution and verify that each component functions as expected.

## 11. Makefile Requirements
- Although not directly shown here, it is planned to use a `Makefile` to automate the build process.
- The `Makefile` should compile `yashd.c` and `yash.c` into `yashd` and `yash` executables, respectively.

## 12. Unmet Requirements or Recommendations

### Daemon Initialization Call
- The call to `daemon_init()` is commented out in `main()`. Uncomment it to enable full daemon functionality.

### Memory Management
- There are potential memory leaks (e.g., `strdup()` usage in `add_job()`). Ensure that allocated memory is freed when jobs are removed.

### Edge Case Handling
- Add checks for invalid job IDs in `fg_command()` and `bg_command()` to avoid unexpected behavior when the job ID is not found.



  
# Future Work

## Daemon Initialization

- **Issue**: Lack of proper signal handling in `daemon_init()` for signals like `SIGINT`, `SIGTERM`, and `SIGQUIT`.
  - **Fix**: Add handlers for `SIGINT` and `SIGTERM` to shut down the server and close the socket cleanly.

## File Redirection

- **Issue**: The `apply_redirections()` function may have potential memory leaks due to `strdup()` usage without `free()` when files are closed.
  - **Fix**: Free `input_file`, `output_file`, and `error_file` strings after use.
- **Issue**: The current redirection implementation does not handle permission issues correctly when creating files.
  - **Fix**: Use `S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH` as file permissions when creating output files to ensure the correct access.

## Piping

- **Issue**: The implementation of `validatePipes()` and `handle_pipe()` does not handle cases where the pipe character (`|`) is the first or last character in the command string.
  - **Fix**: Add validation checks in `validatePipes()` to ensure that commands do not start or end with a pipe. Return an error message if this is the case.
- **Issue**: The current piping implementation does not handle synchronization properly between child processes.
  - **Fix**: Group child processes together with `setpgid()`. This allows signal handling (like `SIGTSTP` or `SIGINT`) to affect both processes simultaneously.

## Signal Handling

- **Issue**: Signal handlers for `SIGINT`, `SIGTSTP`, and `SIGCHLD` in the server (`yashd.c`) are not properly managing job states, especially for background jobs.
  - **Fix**: Enhance `sigtstp_handler()` and `sigint_handler()` to update the job status when the foreground process is stopped or killed.
  - **Fix**: In `sigchld_handler()`, ensure all child processes are reaped using a loop (`waitpid(-1, NULL, WNOHANG)`).
- **Issue**: Signal handling for `SIGPIPE` is not robust. Currently, `sig_pipe()` exits the process on receiving `SIGPIPE`.
  - **Fix**: Log the error and handle the broken pipe gracefully by closing the client socket and cleaning up resources instead of exiting.


## Job Control

- **Issue**: The `add_job()` function does not check if the job list is full (`job_count >= MAX_JOBS`), leading to potential overflow.
  - **Fix**: Add a check in `add_job()` to return an error if `job_count` exceeds `MAX_JOBS`.
- **Issue**: The `fg_command()` and `bg_command()` functions do not update the job markers (`+` and `-`) correctly when bringing a job to the foreground or resuming it in the background.
  - **Fix**: Reassign job markers based on the most recent jobs after updating the job status.
- **Issue**: The `print_jobs()` function does not remove completed jobs, leading to outdated job entries.
  - **Fix**: Call `remove_job()` for any exited job before printing the job list.


## Client-Server Communication

- **Issue**: The client (`yash.c`) does not handle large outputs from the server efficiently. It reads from the socket in a fixed-size buffer and might miss some output if the response is large.
  - **Fix**: Implement a loop in `recv()` to handle partial reads and accumulate the response until the prompt (`\n# `) is detected.
- **Issue**: The client does not properly handle `Ctrl+D` (EOF) to exit the input loop.
  - **Fix**: Add handling for `feof(stdin)` in the `GetUserInput()` function and send a disconnect message (`CTL d`) to the server before closing the socket.


## Logging

- **Issue**: The `logRequest()` function does not handle cases where the log file cannot be opened due to permissions issues.
  - **Fix**: Check the return value of `fopen()` and attempt to create the log file with appropriate permissions if it does not exist.
- **Issue**: Potential race condition in `logRequest()` when multiple threads try to write to the log file simultaneously.
  - **Fix**: Use `pthread_mutex_lock()` and `pthread_mutex_unlock()` around the entire logging block to ensure thread-safe access.


## Protocol Adherence

- **Issue**: The server does not always send the prompt (`\n# `) back to the client after completing a command execution.
  - **Fix**: Ensure that every command handler (including job control commands) sends the prompt to the client before returning.
- **Issue**: The client does not consistently differentiate between `CMD`, `CTL`, and plain text messages.
  - **Fix**: Add checks in `send_command_to_server()` to ensure that plain text (e.g., input for `cat > file.txt`) is handled differently from `CMD` and `CTL` messages.


## Makefile

- **Issue**: The Makefile is missing from the provided code, which is required for building the project.
  - **Fix**: Create a `Makefile` that compiles `yash.c` and `yashd.c` into `yash` and `yashd` executables, respectively. Use the `-pthread` flag for compiling with `pthreads`.


## General Code Quality

- **Issue**: Memory leaks in `strdup()` calls throughout the code (e.g., in `add_job()`, `handle_pipe()`, `apply_redirections()`).
  - **Fix**: Ensure all `strdup()` strings are freed after use.
- **Issue**: Inconsistent error handling (e.g., sometimes `perror()` is called, other times not).
  - **Fix**: Standardize error handling using `perror()` and provide meaningful error messages.


## Summary

By addressing these issues, the server and client implementations will better align with the requirements of both Project 1 and Project 2. This will help ensure a more robust and compliant solution.



Team Programming Assignment (02)

