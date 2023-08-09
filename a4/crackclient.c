/*
 * crackclient.c
 *      CSSE2310 - Assignment Four
 *
 *      Written by Alex Viller, a.viller@uqconnect.edu.au
 *
 * Usage:
 *  crackclient portnum [jobfile]
 *          optional arguments must be after the portnum
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <csse2310a3.h>
#include <csse2310a4.h>

/* Global Definitions */
// The number of arguments that there will be if there is a job file
#define HAS_JOB_FILE 3
// The number of arguments that there will be if there is no job file
#define NO_JOB_FILE 2
// The default host name for using local host
#define HOST "localhost"

/* New Type Creations */
// enum containing all the error codes
typedef enum {
    OK = 0,
    USAGE_ERR = 1,
    JOBFILE_ERR = 2,
    PORT_ERR = 3,
    CONNECTION_TERMINATED = 4
} ErrorCodes;

// enum containing the indecies of where we expect each command line arg to be
typedef enum {
    PORT_NUM = 1,
    JOB_PATH = 2
} ArgIndex;

// A struct to hold information about the socket
typedef struct {
    const char* portNum;
    const char* hostName;
    FILE* to;
    FILE* from;
} SocketInfo;

// A struct to hold all the information important for the client
typedef struct {
    SocketInfo sock;
    bool useJobFile;
    FILE* stream;
} ClientData;

/* Function Prototypes */
int main(int argc, char* argv[]);
ClientData get_args(int argc, char* argv[]);
bool process_socket(SocketInfo* socketInfo);
bool process_command(char** line);
void add_new_line(char** line);

/* main()
 * ------
 * The main functionality of the program. Handles running all the methods to 
 * check the command line arguments as well as doing the actual functionality
 * of sending and receiving messages with the server on portnum. Also closes
 * and frees all information before exit.
 *
 * Returns: OK -> 0
 * Errors: If, when the server we are connected to terminates connection from
 *          its end -> connection terminated error.
 */
int main(int argc, char* argv[]) {
    ClientData data = get_args(argc, argv);
    bool connectionTerminated = false;
    
    char* currentIn;
    while ((currentIn = read_line(data.stream))) {
        if (process_command(&currentIn)) {
            // send command to server
            fprintf(data.sock.to, "%s", currentIn);
            fflush(data.sock.to); // flush to send message immediately
            free(currentIn); // ensure no memory leakage

            // receive server output
            char* fromServer;
            fromServer = read_line(data.sock.from);
            if (fromServer == NULL) {
                connectionTerminated = true;
                break;
            }
            add_new_line(&fromServer);

            // handle server output
            if (strcmp(":invalid\n", fromServer) == 0) {
                fprintf(stdout, "Error in command\n");
            } else if (strcmp(":failed\n", fromServer) == 0) {
                fprintf(stdout, "Unable to decrypt\n");
            } else {
                fprintf(stdout, "%s", fromServer);
            }
            free(fromServer);
        }
    } 
    // close all streams
    fclose(data.sock.to);
    fclose(data.sock.from);
    if (data.useJobFile) { // only close if opened
        fclose(data.stream);
    }
    
    // Error if server terminates connection before we do
    if (connectionTerminated) {
        fprintf(stderr, "crackclient: server connection terminated\n");
        exit(CONNECTION_TERMINATED);
    }
    return OK;
}

/* get_args()
 * ----------
 * This function processes the command line arguments as well as checking
 * their validity. Any errors with the command line arguments result in an
 * appropriate error.
 *
 * argc: the number of arguments (including the program itself)
 *
 * argv: the arguments themselves in a list of char*
 *
 * Returns: A ClientData struct containing all important information for the
 *      successful running of the program
 *
 * Errors: If the number of arguments given is not 1 (just portnum) or 2 (port
 *          and jobfile provided) -> usage error. 
 *         If a jobfile is provided, and is not able to be opened for reading
 *          -> jobfile open error
 *         If the socket provided cannot be connected to -> port error
 */
ClientData get_args(int argc, char* argv[]) {
    // initialise structs
    ClientData data;
    SocketInfo sock = {.portNum = argv[PORT_NUM], .hostName = HOST};
    // check correct number of args, and process arguments accordingly
    if (argc == NO_JOB_FILE) { // just port provided
        data.useJobFile = false;
        data.stream = stdin;
    } else if (argc == HAS_JOB_FILE) { // jobfile also provided
        data.stream = fopen(argv[JOB_PATH], "r");
        // file open error
        if (data.stream == NULL) {
            fprintf(stderr, "crackclient: unable to open job file \"%s\"\n",
                    argv[2]);
            exit(JOBFILE_ERR);
        } else {
            data.useJobFile = true;
        }
    } else { // incorrect number of arguments provided
        fprintf(stderr, "Usage: crackclient portnum [jobfile]\n");
        exit(USAGE_ERR);
    }
    
    if (!process_socket(&sock)) { // portnum cannot be connected to
        fprintf(stderr, "crackclient: unable to connect to port %s\n", 
                sock.portNum);
        if (data.useJobFile) {
            fclose(data.stream);
        }
        exit(PORT_ERR);
    }
    data.sock = sock;
    return data;
}

/* process_socket()
 * ----------------
 * This function takes in a pointer to a SocketInfo struct and adds the
 * appropriate information to it and also connects the client to the socket.
 * If there was an unsuccessful connection all information is properly closed
 * and false is returned. This function raises no errors and instead by
 * returning the connection status pushes the errors up the call chain.
 *
 * socketInfo: A pointer to the SocketInfo struct to have the information
 *          placed inside of
 * 
 * Returns: boolean of whether or not there was a successful connection
 */ 
bool process_socket(SocketInfo* socketInfo) {
    // initialise all variables
    int sockfd = -1; // invalid fd, should never be able to attempt to connect
    struct addrinfo* ai = NULL;
    struct addrinfo hints;
    memset(&hints, 0, sizeof(struct addrinfo)); // initialise all members to 0
    hints.ai_family = AF_INET; // ipv4
    hints.ai_socktype = SOCK_STREAM;
    bool success = true;

    int err;
    if ((err = getaddrinfo(socketInfo->hostName, socketInfo->portNum,
            &hints, &ai)) != 0) {
        success = false; // cannot get address info
    }
    
    if (success) { // guard already failed process
        sockfd = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
        if (connect(sockfd, ai->ai_addr, ai->ai_addrlen) == -1) {
            success = false; // could not connect the socket to the server
        }
    }

    if (success) {
        socketInfo->to = fdopen(sockfd, "w");
        socketInfo->from = fdopen(dup(sockfd), "r");
        if (socketInfo->to == NULL || socketInfo->from == NULL) {
            success = false; // piping error
        }
    }

    freeaddrinfo(ai);
    return success;
}

/* process_command()
 * -----------------
 * Simple function to check if a line being read is a comment or not. And then
 * if the command is not a comment, adds a new line so that the server can
 * properly receive the command.
 *
 * line: A pointer to the current line of the command that we are processing
 *
 * Returns: Boolean about if the line is a comment or not.
 */ 
bool process_command(char** line) {
    size_t length = strlen(*line);

    if ((*line)[0] == '#') {
        return false;
    } else if (length == 0) {
        return false;
    } else {
        add_new_line(line);
        return true;
    }
}

/* add_new_line()
 * --------------
 * Simple function to add a new line at the end of a string
 *
 * line: A pointer to the string that needs a new line to be added
 *
 * Errors: If there is a realloc error
 */
void add_new_line(char** line) {
    // Increase the string size by 2 (1 for '\n' and 1 for '\0')
    size_t length = strlen(*line);
    *line = realloc(*line, length + 2); 
    if (*line == NULL) {
        // Handle realloc error
        fprintf(stderr, "Memory allocation failed\n");
    }
    (*line)[length] = '\n';
    (*line)[length + 1] = '\0';
}
