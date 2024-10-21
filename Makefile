# Compiler to be used for compiling the code 
CC = gcc

# compiler flags -Wall: enables all compiler's warning messages
# pthread links the pthread library for multithreading support 
CFLAGS = -Wall -pthread

# Target to build the server (yashd) and the client(yash)
TARGETS = yashd yash yashShell

# default target that will be built if 'make' is run without arguments
all: $(TARGETS) # BUILD BOTH YASHD AND YASH executables


# RULE to build the server executable(yashd)
yashd: yashd.o yashShellHelpers.o
	$(CC) $(CFLAGS) yashd.o yashShellHelpers.o -o yashd 
# compile and link the yashd object file to create the executable yashd

# Rule to build the client executable (yash)
# The second line compiles and links the yash object file to create the executable yash
yash: yash.o
	$(CC) $(CFLAGS) yash.o -o yash

yashShell: yashShell.o
	$(CC) $(CFLAGS) yashShell.o -o yashShell
# Compile the yashd source file into an object file 
# Make sure the obj directory exists
# compile yash.c into yash.o, include the headers from the include directory
yashd.o: yashd.c yashd.h
	$(CC) $(CFLAGS) -c yashd.c -o yashd.o 

# Compile the yash source file into an object file 
# Compile yash.c into yash.o, include the headers from the included directory
yash.o: yash.c yashd.h 
	$(CC) $(CFLAGS) -c yash.c -o yash.o 

yashShell.o: yashShell.c yashd.h 
	$(CC) $(CFLAGS) -c yashShell.c -o yashShell.o 

yashShellHelpers.o: yashShellHelpers.c yashShell.h 
	$(CC) $(CFLAGS) -c yashShellHelpers.c -o yashShellHelpers.o 
 
# clean up rule to remove compiled object files and executables 
clean:
	@echo "cleaning up object files and executables....."
	rm -f *.o yashd yash mock_server test_yash pipes yashTestd yashShell yashShellHelpers



