# Project Two Systems Programming

Project Two 

# Collaborators

* Anita Woodford 
* Juan Gabriel Palacios Rodas quit the class in the middle of the project. 

# Overview
This project covers key concepts in Unix systems programming, including process creation, inter-process communication, signal handling, job control, and socket programming. It is divided into two parts:

- **Project 1:** Implementation of a command-line shell (`yash`).
- **Project 2:** Extension of the shell into a network service daemon (`yashd`).


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

# Objective 
## Project 1 and Project 2

Create a command-line interpreter called `yash` that mimics basic functionalities of Unix shells like Bash. Implement key features such as file redirection, piping, signal handling, and job control. 
Extend your shell into a network service daemon (`yashd`) that mimics the behavior of an SSH daemon (`sshd`). Implement a client (`yash`) that communicates with the daemon over a TCP/IP socket.


### Key Features

1. **File Redirection:**
   - Support input (`<`) and output (`>`) redirection.
   - Automatically create output files if they do not exist.
   - Fail if the input file does not exist.
   - Handle redirection with commands, e.g., `ls > output.txt`.

2. **Piping:**
   - Implement single-level piping using the `|` operator.
   - Redirect the output of the left command to the input of the right command.
   - Ensure that the commands in a pipeline start and stop together.

3. **Signal Handling:**
   - Handle `SIGINT` (Ctrl+C) to terminate the foreground process without killing the shell.
   - Handle `SIGTSTP` (Ctrl+Z) to stop the current foreground process.
   - Reap child processes using `SIGCHLD` to prevent zombie processes.

4. **Job Control:**
   - Support background processes using `&`.
   - Implement `fg`, `bg`, and `jobs` commands for job management.
   - Manage a list of active jobs and their states (running, stopped).
   - Display jobs similar to Bash, including job IDs and status indicators (`+`, `-`).

5. **Miscellaneous:**
   - Inherit environment variables from the parent shell.
   - Search for executables in the `PATH` environment variable.
   - Provide a prompt (`# `) to indicate readiness for input.
   **Daemon Initialization:**
   - Implement `yashd` as a proper Unix daemon process.
   - Follow guidelines for daemon processes, including detaching from the terminal, handling signals, and creating a PID file to prevent multiple instances.

6. **Multi-threaded Server:**
   - Use `pthreads` to create a multi-threaded server.
   - Handle each client connection in a separate thread for responsiveness.
   - Execute commands using the `fork-exec` model, returning the output to the client over the socket.

7. **Client-Server Communication:**
   - The client connects to the server using a well-defined port (3820).
   - The client sends commands using a simple protocol:
     - `CMD <command>\n` for command execution.
     - `CTL <c|z|d>\n` for control signals (Ctrl+C, Ctrl+Z, Ctrl+D).
     - Plain text input (e.g., for `cat > file.txt`) is sent directly to the server.
   - The server responds with plain ASCII text and a prompt (`\n# `) to indicate the end of output.

8. **Logging:**
   - Log client requests in `/tmp/yashd.log` in a syslog-like format, including the date, client IP, port, and executed command.

9. **Signal Handling and Job Control:**
   - Handle `SIGINT` and `SIGTSTP` signals sent by the client to control the server’s running commands.
   - Implement job control commands (`jobs`, `fg`, `bg`) on the server side, allowing clients to manage processes remotely.



### Requirements and Restrictions

- Implemented in ANSI C (C99 or GNU99).
- Must include a `Makefile` for building the project.
- Line length is limited to 200 characters.
- The shell should not use the `system()` function for command execution.
- The server must bind to port 3820 using `SO_REUSEADDR` for port reuse.
- Follow a standardized protocol for cross-compatibility with other implementations.
- Include a `Makefile` to build the server (`yashd`) and client (`yash`).



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

## Deliverables

- Source code for `yash` (shell), `yashd` (daemon), and `yash` (client).
- `Makefile` for building the project.
- Log file (`/tmp/yashd.log`) generated during server operation.
- For submission, include all source files and the `Makefile` in an archive (`yashd.tgz`).

---

# Test Cases for Project 2 (Yash Daemon)

## Basic Functionality - 20 Points

- **Example Test Cases:**
  - **Basic Command:** `# date`
  - **Command with Arguments:** `# ls -a -l`
  - **Prompt:** Does it prompt with `# `?
  - **Termination:** Does it terminate with Ctrl-D?
    - Ctrl-C can print `^C` or `^C#`
    - Ctrl-Z can print `^Z` or `^Z#`
  - **All Basic Commands - PASS:**
    - Commands: `ls`, `ls -a -l`, `date`, `echo` - **PASS**
    - Prompting with `# ` - **PASS**
    - Ctrl+D - Terminating - **PASS**

- **Invalid Commands:**
  - `# < ls`
  - `# > ls`
  - `# | ls`
  - `# & ls`

---

## File Redirection - 20 Points

- **Example Test Cases:**
  - **Output Redirection:** `# ls > foo.txt`
  - **Input Redirection:** `# wc < foo.txt`
  - **Output File Creation:** `# ls > out.txt`
  - **Input File Reading:** `# wc < out.txt`
  - **Error Redirection:** `# cat fake_file 2> bar.txt`
    - Output: `#`
  - **Combined Redirection:** `# cat in1.txt > out1.txt | cat < in2.txt`
    - Output: Content from `in2.txt`
  - **Piping with File Input:** `# cat in1.txt | cat`
    - Output: Content from `in1.txt`
  - **Invalid Input File:** `# cat < fake-file.bar`
  - **Hello World Check:** `# ./hello`
    - Output: `Hello World, this is Anita Woodford ajw 4987`
  - **Valid Redirections:**
    - `echo "hello world" > temp.txt` - **PASS**
    - `cat < temp.txt` - **PASS**
    - `ls > foo.txt` - Output redirection - **PASS**
    - `wc < foo.txt` - Input redirection - **PASS**
    - Multiple redirections: `cat < my-file > your_file` - **FAIL**
    - `cat < temp.txt > output.txt` - **Partial PASS**

---

## Piping - 20 Points

- **Example Test Cases:**
  - **Basic Pipe:** `# ls | wc`
  - **Multiple Arguments:** `# ls -a -l -t -r . | wc -c -L`
  - **Complex Redirection with Pipe:**
    - `# left_child < in.txt > err1.txt | right_child > out.txt 2> err2.txt`
  - **Results:**
    - Basic pipe commands: `ls | wc` - **PASS**
    - Multiple arguments: `ls -a -l -t -r . | wc -c -L` - **PASS**
    - Pipe with file redirection: `cat in1.txt > out1.txt | cat < in2.txt` - **Partial PASS**

---

## Signal Handling - 20 Points

- **Example Test Cases:**
  - **Ctrl-C:** Terminates the command
  - **Ctrl-C:** Terminates piped command
  - **Ctrl-Z:** Stops the command
  - **Ctrl-Z:** Stops piped command

---

## Job Control - 20 Points

- **Example Test Cases:**
  - **Basic Job Control:**
    - Run a command, then Ctrl-Z, then `bg`, then `fg`.
    - Create long-running background jobs and use `fg`, `bg`, and Ctrl-Z to switch between foreground, background, and stopped states.
    - Kill some jobs by bringing them to the foreground and using Ctrl-C.
    - Run the `jobs` command to display job states.
  - **Example Sequence:**
    - `# sleep 4`
      - Output: `^Z# fg`
    - `# sleep 4`
      - Output: `^Z# bg`
      - `[1]+ sleep 4 &`
    - `# sleep 4 &`
    - `# jobs`
      - `[1]+ Running sleep 4 &`
    - `# sleep 2 &`
    - `# ls`
      - `[1]+ Done sleep 2 &`
    - `# sleep 5`
      - Output: `^Z# sleep 10`
      - `^Z sleep 15`
    - `# jobs`
      - `[2]- Stopped sleep 10`
      - `[3]- Stopped sleep 15`
      - `[4]+ Stopped sleep 20`
    - `# fg`
      - `[4]+ Stopped sleep 20`
    - `# sleep 10`
      - Output: `^Z# sleep 15`
      - `^Z sleep 5 &`
    - `# jobs`
      - `[1]- Stopped sleep 10`
      - `[2]- Stopped sleep 15`
      - `[3]+ Stopped sleep 5 &`
      - `[4]+ Stopped sleep 20`

- **Allowed Redirections:**
  - `echo "hello world" > temp.txt` - **PASS**
  - `cat < temp.txt` - **PASS**
  - `cat < temp.txt > output.txt` - **PASS**
  - `cat < in1.txt > out1.txt | cat < in2.txt > out2.txt` - **PASS**

Team Programming Assignment (02)

