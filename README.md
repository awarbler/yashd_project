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

## Conclusion
The current implementation satisfies most of the requirements specified in the project documentation. Address the minor issues noted above for a complete and robust submission.


  
# Instructions


# Requirements

# Rubric

Team Programming Assignment (02)

