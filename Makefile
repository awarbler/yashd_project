# Compiler to be used for compiling the code 
CC = gcc

# compiler flags -Wall: enanbles all compilers warning messages
# pthread links the pthread library for multithreading support 
CFLAGS = -Wall -pthread
# directories for source files, headder files and object file
SRC_DIR = src # directory containing the source files(.c)
INC_DIR = include # directory contain the header files (.h)
OBJ_DIR = obj # directory for storing the compiled object files(.o)

# target to build the server (yashd) and the client(yash)
TARGETS = yashd yash 

# default target that will be built if 'make' is run without arguments
all: $(TARGETS) # BUILD BOTH YASHD AND YASH executables

# ensure that the object directory exisits
$(OBJ_DIR) : 
	mkdir -p $(OBJ_DIR)

# RULE to build the server executable(yashd)
yashd: $(OBJ_DIR)/yashd.o
	$(CC) $(CFLAGS) -o yashd $(OBJ_DIR)/yashd.o 
# compile and link the yashd object file to create the executable yashd

# Rule to buile the client executable (yash)
# second line compiles and link the yash object file to create the executable yash
yash: $(OBJ_DIR)/yash.o
	$(CC) $(CFLAGS) -o yash $(OBJ_DIR)/yash.o 


# Compile the yashd source file into an object file 
# make sure obj directory exisit
# compile yash.c into yash.o, include the headers from the include directory
$(OBJ_DIR)/yashd.o: $(SRC_DIR)/yashd.c $(INC_DIR)/yashd.h 
	$(CC) $(CFLAGS) -I$(INC_DIR) -c $(SRC_DIR)/yashd.c -o $(OBJ_DIR)/yashd.o 


# Compile the yash source file into an object file 
# compile yash.c into yash.o, include the headers from the included directory
$(OBJ_DIR)/yash.o: $(SRC_DIR)/yash.c $(INC_DIR)/yashd.h 
	$(CC) $(CFLAGS) -I$(INC_DIR) -c $(SRC_DIR)/yash.c -o $(OBJ_DIR)/yash.o 

# clean up rule to remove compiled object files and executables 
clean:
	@echo "cleaning up object files and executables....."
	rm -rf $(OBJ_DIR)/*.o
	rm -f yashd yash


