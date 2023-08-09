/*
 * crackserver.c
 *      CSSE2310 - Assignment Four
 *
 *      Written by Alex Viller, a.viller@uqconnect.edu.au
 *
 * Usage:
 *  crackserver [--maxconn connections] [--port portnum]
 *          [--dictionary filename]
 *
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <getopt.h>
#include <ctype.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <semaphore.h>
#include <crypt.h>
#include <csse2310a3.h>
#include <csse2310a4.h>

/* Global Definitions */
// The maximum value a valid port number can be
#define MAX_PORTNUM 65535
// The minimum value a valid port number can be
#define MIN_PORTNUM 1024
// Use 0 to look at the ephemeral port
#define ANY_PORTNUM "0"
// crypt can only encrypt the first 8 characters of a word, so ignore words
//      that are longer
#define MAX_WORD_LEN 8
// The default dictionary location for unix
#define DEFAULT_DICT "/usr/share/dict/words"
// The value associated with TCP for socket
#define TCP 0
// Use 0 to represent when we are not checking for the maximum number of
//      connections
#define UNLIMITED_CONNECTIONS 0
// The maximum number of commands that the server can accept
#define MAX_COMMAND_ARGS 3
// The length of the salt string for crypt
#define SALT_LENGTH 2
// The maximum number of threads that a client can make
#define MAX_THREADS 50
// The length of encrypted text
#define CRYPT_LEN 13
// Plain text characters that each character of the salt string must
//      exclusively contain
#define PLAINTEXT_CHARS "abcdefghijklmnopqrstuvwxyz"\
                        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"\
                        "0123456789./"

/* New Type Creations */
// enum containing the values to be used for getopt_long
typedef enum {
    MAXCONN_ARG = 1,
    PORT_ARG = 2,
    DICT_ARG = 3
} ArgType;

// enum containing the exit codes
typedef enum {
    OK = 0,
    USAGE_ERR = 1,
    DICT_OPEN_ERR = 2,
    EMPTY_DICT = 3,
    PORTNUM_ERR = 4
} ErrorCodes;

// struct for containing the dictionary
typedef struct {
    char** words;
    int numWords;
    pthread_mutex_t* dictMutex;
} Dictionary;

// struct for containing thread information for client threads
typedef struct {
    int fd;
    int* currCount;
    sem_t* semaphore;
    Dictionary dict;
} ThreadParams;

// struct for containing thread information for crack requests
typedef struct {
    char* encrypted;
    char* salt;
    int threadId;
    int numThreads;
    Dictionary dict;
    char* result;
    volatile int* stopFlag;
} CrackThreadData;

// struct for containing all parameters for proper running of the server
typedef struct {
    char* dictPath;
    Dictionary dict;
    const char* port;
    int socketfd;
    int maxConnections;
    int currentNumConns;
    int totalConns;
    sem_t countSemaphore;
} ServerParams;

/* Function Prototypes */
void print_usage();
ServerParams initialise(int argc, char* argv[]);
bool is_digits(char* input);
Dictionary process_dict(char* dictPath);
void free_dict(Dictionary dict);
int process_port(const char* portNum);
void process_connections(int fdServer, ServerParams* params);
void* client_thread(void* fdPtr);
void add_new_line(char** line);
char* do_command(char* command, Dictionary dict);
char* crack(char* encrypted, int numThreads, Dictionary dict);
void* crack_thread(void* arg);

/* main()
 * ------
 * The function to be run at the begining of operation. Executes all important
 * methods for the program to actually be run.
 *
 * Returns: OK -> 0
 * Errors: If the socket was not openable -> PORTNUM_ERR -> 4
 *          or if any of the called methods error
 */
int main(int argc, char* argv[]) {
    ServerParams params = initialise(argc, argv);
    sem_init(&(params.countSemaphore), 0, 1);
    params.currentNumConns = 0;
    params.totalConns = 0;
    params.dict = process_dict(params.dictPath);
    params.socketfd = process_port(params.port);
    if (params.socketfd == -1) {
        free_dict(params.dict);
        fprintf(stderr, "crackserver: unable to open socket for listening\n");
        exit(PORTNUM_ERR);
    }
    process_connections(params.socketfd, &params);

    free_dict(params.dict);
    return OK;
}

/* print_usage()
 * -------------
 * A simple method to print the usage to the user for modularity reasons.
 *
 * Returns: void
 * Errors: with USAGE_ERR
 */
void print_usage() {
    fprintf(stderr, "Usage: crackserver [--maxconn connections] "\
            "[--port portnum] [--dictionary filename]\n");
    exit(USAGE_ERR);
}

/* free_dict()
 * -----------
 * A program that goes through and frees the dictionary. Although server
 * shouldn't close under normal circumstances, makes sure that memory is freed
 * if it does.
 *
 * dict: The dictionary to be freed
 *
 * Returns: void
 */
void free_dict(Dictionary dict) {
    for (int i = 0; i < dict.numWords; i++) {
        free(dict.words[i]);
    }
    free(dict.words);
}

/* initialise()
 * ------------
 * The function which gets all the arguments from the command line and ensures
 * proper usage as wel as the validity of these parameters.
 *
 * argc: The number of arguments, acquired from main
 *
 * argv: The list of arguments themselves, acquired from main
 *
 * Returns: ServerParams struct containing all the command line arguments
 * Errors: For any usage error, calls print_usage() which will error
 */
ServerParams initialise(int argc, char* argv[]) {
    bool maxconnFlag = false, portFlag = false, dictFlag = false;
    ServerParams params = {.port = ANY_PORTNUM, .dictPath = DEFAULT_DICT,
            .maxConnections = UNLIMITED_CONNECTIONS};
    static struct option longOpts[] = {
        {"maxconn", required_argument, NULL, MAXCONN_ARG},
        {"port", required_argument, NULL, PORT_ARG},
        {"dictionary", required_argument, NULL, DICT_ARG},
        {0, 0, 0, 0}
    };

    while (true) {
        int opt = getopt_long(argc, argv, ":", longOpts, NULL);
        if (opt == -1) { // no more option args
            break;
        } else if (opt == MAXCONN_ARG && !maxconnFlag) {
            maxconnFlag = true;
            if (is_digits(optarg)) {
                int maxConn = atoi(optarg);
                if (maxConn >= 0) { // max connections non negative
                    params.maxConnections = maxConn;
                    continue;
                }
            }
            print_usage();
        } else if (opt == PORT_ARG && !portFlag) {
            portFlag = true;
            if (is_digits(optarg)) {
                int portNum = atoi(optarg);
                if (portNum == atoi(ANY_PORTNUM) || (portNum >= MIN_PORTNUM && 
                        portNum <= MAX_PORTNUM)) { // valid port numbers
                    params.port = optarg;
                    continue;
                }
            }
            print_usage();
        } else if (opt == DICT_ARG && !dictFlag) {
            dictFlag = true;
            params.dictPath = optarg;
            continue;
        } else {
            print_usage();
        }
    }
    if (optind < argc) {
        print_usage();
    }

    return params;
}

/* is_digits()
 * -----------
 * A method which goes over a string and ensures that every character is a
 * digit (value 0 through 9) to ensure no side effects from atoi
 *
 * input: The string to be checked
 *
 * Returns: boolean value of if a string is all digits or not
 */
bool is_digits(char* input) {
    if (strlen(input) == 0) {
        return false; // handle empty string case
    }
    while (*input) { // for each character in string
        if (isdigit(*input++) == 0) return false;
    }
    return true;
}

/* process_dict()
 * --------------
 * A method which goes line by line through the provided dictionary file and
 * checks each word to be less than or equal to 8 characters in length, then
 * adds the words to a dictionary struct which is then returned.
 *
 * dictPath: The path to the dictionary to be read
 *
 * Returns: A dictionary struct containing only valid words
 * Errors: If dictionary is unopenable DICT_OPEN_ERR -> 2
 *         If after processing dictionary is empty EMPTY_DICT -> 3
 */
Dictionary process_dict(char* dictPath) {
    Dictionary dictionary = {.numWords = 0};
    FILE* dictFile = fopen(dictPath, "r");
    if (dictFile == NULL) {
        fprintf(stderr, "crackserver: unable to open dictionary file \"%s\"\n",
                dictPath);
        exit(DICT_OPEN_ERR);
    }
    dictionary.words = malloc(sizeof(char*));
    char* currentLine;
    while ((currentLine = read_line(dictFile))) {
        int wordLen = strlen(currentLine);
        if (wordLen > MAX_WORD_LEN || wordLen == 0) {
            continue; // skip over this word if not the right length
        }
        // increase dictionary size for each word. Not very efficient but only
        // called once at the start, so not too important at run time.
        dictionary.words = realloc(dictionary.words, sizeof(char*) *
                dictionary.numWords + 1);
        dictionary.words[dictionary.numWords] = currentLine;
        dictionary.numWords++;
    }
    fclose(dictFile);
    // either no words in dict or none the right length.
    if (dictionary.numWords == 0) {
        free(dictionary.words);
        fprintf(stderr, "crackserver: no plain text words to test\n");
        exit(EMPTY_DICT);
    }

    return dictionary;
}

/* process_port()
 * --------------
 * A method to process the given port number and ensure its validity as well as
 * the ability to actually connect to it
 * 
 * portNum: The string value of the port number to listen on
 *
 * Returns: A file descripter of the port after being opened for listening
 * Errors: If there are any socketing or address errors. Error is passed up to
 *      The function which called it.
 */
int process_port(const char* portNum) {
    struct addrinfo* ai = 0;
    struct addrinfo hints;

    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    int err;
    if ((err = getaddrinfo(NULL, portNum, &hints, &ai))) {
        freeaddrinfo(ai);
        return -1;
    }

    int listenfd = socket(AF_INET, SOCK_STREAM, TCP);

    int optVal = 1;
    if (setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, 
            &optVal, sizeof(int)) < 0) {
        freeaddrinfo(ai);
        return -1;
    }

    if (bind(listenfd, ai->ai_addr, sizeof(struct sockaddr)) < 0) {
        freeaddrinfo(ai);
        return -1;
    }

    struct sockaddr_in addr;
    socklen_t addrLen = sizeof(addr);
    if (getsockname(listenfd, (struct sockaddr*)&addr, &addrLen) == -1) {
        freeaddrinfo(ai);
        return -1;
    }

    if (listen(listenfd, 0) < 0) {
        freeaddrinfo(ai);
        return -1;
    }

    int port = ntohs(addr.sin_port); // find actual port number
    fprintf(stderr, "%d\n", port);
    fflush(stderr);
    return listenfd;
}

/* process_connections()
 * ---------------------
 * A function which listens and waits for clients to attempt to connect. If we
 * have reached maxConnections, then the client is to be held potentially
 * indefinitely until a space becomes free for it.
 *
 * fdServer: The file descripter that the server is listening on
 *
 * params: The server parameters containing important information like the
 *          dictionary and maximum connections
 * 
 * Returns: void
 * Errors: If there are any connection errors
 */
void process_connections(int fdServer, ServerParams* params) {
    int fd;
    struct sockaddr_in fromAddr;
    socklen_t fromAddrSize;

    while (1) {
        fromAddrSize = sizeof(struct sockaddr_in);
        fd = accept(fdServer, (struct sockaddr*)&fromAddr, &fromAddrSize);
        if (fd < 0) {
            perror("Error accepting connection");
            exit(1);
        }

        sem_wait(&(params->countSemaphore));
        if (params->maxConnections != 0) { // check for and wait if > max conns
            while (params->currentNumConns >= params->maxConnections) {
                sem_post(&(params->countSemaphore));
                sem_wait(&(params->countSemaphore));
            }
        }
        (params->currentNumConns)++;
        (params->totalConns)++;
        sem_post(&(params->countSemaphore));

        ThreadParams threadParams;
        threadParams.fd = fd;
        threadParams.semaphore = &params->countSemaphore;
        threadParams.currCount = &params->currentNumConns;
        threadParams.dict = (*params).dict;

        pthread_t threadId;
	    pthread_create(&threadId, NULL, client_thread, &threadParams);
        pthread_detach(threadId);
    }
}

/* client_thread()
 * ---------------
 * A method to handle the processes to be run and processing of requests from a
 * client. This is the method called by the server to create a thread for each
 * incoming connection.
 * 
 * arg: The thread parameters structure containing important information for
 *      each client
 *
 * Returns: void*
 * Errors: If any subsequent calls error.
 */
void* client_thread(void* arg) {
    ThreadParams* params = (ThreadParams*)arg;
    int fd = params->fd;
    FILE* to = fdopen(fd, "w");
    FILE* from = fdopen(dup(fd), "r");

    char* currentIn;
    while ((currentIn = read_line(from)) != NULL) {
        char* response;
        response = do_command(currentIn, params->dict);
        if (response[0] != ':') {
            response = strdup(response);
            add_new_line(&response);
        }
        fprintf(to, "%s", response);
        fflush(to);
        free(currentIn);
    }

    sem_wait(params->semaphore);
    (*params->currCount)--;
    sem_post(params->semaphore);
    close(fd);
    fclose(to);
    fclose(from);
    return NULL;
}

/* add_new_line()
 * --------------
 * A helper function to add an extra new line at the end of a malloced string
 * in place. Important for sending messages to the client as they are expecting
 * a new line to terminate each response.
 *
 * line: A pointer to the string requiring a new line
 * 
 * Returns: void, as edits the string in place
 * Errors: for any mallocing errors
 */
void add_new_line(char** line) {
    // Increase the string size by 2 (1 for '\n' and 1 for '\0')
    size_t length = strlen(*line);
    *line = (char*) realloc(*line, length + 2); 
    if (*line == NULL) {
        // Handle realloc error
        fprintf(stderr, "Memory allocation failed\n");
    }
    (*line)[length] = '\n';
    (*line)[length + 1] = '\0';
}

/* num_places()
 * ------------
 * A recursive function to calculate the number of decimal places a value takes
 *
 * n: the input number to see how many places it takes up
 *
 * Returns: The number of places taken by the number
 */
int num_places(int n) {
    if (n < 10) {
        return 1;
    } else {
        return 1 + num_places(n / 10);
    }
}

/* do_command()
 * ------------
 * A method to be called by the client thread which handles the incoming
 * command and acts appropriately.
 *
 * command: The line of input received from the client.
 *
 * dict: The server dictionary to use for crack
 *
 * Returns: The response to send back to the client
 * Errors: If any subsequent calls error
 */
char* do_command(char* command, Dictionary dict) {
    char** arguments = split_by_char(command, ' ', MAX_COMMAND_ARGS);
    char* result;
    if (arguments[2] == NULL ) { // less than 2 commands found
        return ":invalid\n";
    }
    if (strcmp(arguments[0], "crack") == 0) {
        // checking num threads is a valid number and that the number is not a
        // greater order of magnitude
        if (strlen(arguments[2]) > num_places(MAX_THREADS)||
                !is_digits(arguments[2])) {
            return ":invalid\n";
        }
        int crackThreads = atoi(arguments[2]);
        if (crackThreads > MAX_THREADS || crackThreads <= 0) {
            return ":invalid\n"; // invalid value for num threads
        }
        return crack(arguments[1], crackThreads, dict);
    } else if (strcmp(arguments[0], "crypt") == 0) {
        if (strlen(arguments[2]) != 2) {
            return ":invalid\n"; // invalid salt length
        } else if (strspn(arguments[2], PLAINTEXT_CHARS) != 2) {
            return ":invalid\n"; // salt not exclusively plaintext
        } else {
            struct crypt_data data;
            return crypt_r(arguments[1], arguments[2], &data);
        }
    } else { // not a valid command
        result = ":invalid\n";
    }
    return result;
}

/* crack()
 * -------
 * A method which implements the brute force cracking technique in a
 * multithreaded way.
 *
 * encrypted: The value we are checking each encryption against to see if we
 *          have found our word
 * 
 * numThreads: The number of threads to be created for cracking, specified by
 *          client
 * 
 * Returns: The result of cracking the password:
 *              :invalid if the command is found to be invalid
 *              :failed if the encryption cannot be found in our dictionary
 *              else, the word which correlates to the given encryption
 * Errors: Potential malloc errors
 */
char* crack(char* encrypted, int numThreads, Dictionary dict) {
    if (strlen(encrypted) != CRYPT_LEN) {
        return ":invalid\n"; // must be 13 characters long
    }
    char salt[SALT_LENGTH + 2];
    for (int i = 0; i < SALT_LENGTH; i++) {
        salt[i] = encrypted[i];
    }
    salt[SALT_LENGTH] = '\0';
    if (strspn(salt, PLAINTEXT_CHARS) != 2) {
        return ":invalid\n"; // check if salt substring exclusively plaintext
    }

    pthread_t* threads = malloc(sizeof(pthread_t) * numThreads);
    CrackThreadData* threadData = malloc(sizeof(CrackThreadData) * numThreads);
    char* result = NULL;
    
    volatile int stopFlag = 0;
    pthread_mutex_t dictMutex = PTHREAD_MUTEX_INITIALIZER;
    
    for (int i = 0; i < numThreads; i++) {
        threadData[i].encrypted = encrypted;
        threadData[i].salt = salt;
        threadData[i].threadId = i;
        threadData[i].numThreads = numThreads;
        threadData[i].dict = dict;
        threadData[i].result = NULL;
        threadData[i].stopFlag = &stopFlag;
        threadData[i].dict.dictMutex = &dictMutex;
        
        pthread_create(&threads[i], NULL, crack_thread, &threadData[i]);
    }
    
    for (int i = 0; i < numThreads; i++) {
        pthread_join(threads[i], NULL);
        
        if (threadData[i].result != NULL && threadData[i].result[0] != ':') {
            result = threadData[i].result;
            break;
        }
    }
    
    free(threads);
    free(threadData);
    // ensure null pointer safety
    return result != NULL ? result : ":failed\n";
}

/* crack_thread()
 * --------------
 * The thread method to be called by crack which does the actual cracking of
 * the encryption.
 *
 * arg: The CrackThreadData struct which contains all the important information
 *      for cracking a password, as well as their id for calculating where in
 *      the dictionary they should search.
 * 
 * Returns: void*
 * Errors: should not produce any errors.
 */
void* crack_thread(void* arg) {
    CrackThreadData* data = (CrackThreadData*)arg;
    // get the start of where to look with floor (numDict / numThreads)
    int start = data->threadId * (data->dict.numWords / data->numThreads);
    int end;
    if (data->threadId == data->numThreads - 1) {
        end = data->dict.numWords; // last thread gets rest of the words
    } else { // basically gets the next start
        end = (data->threadId + 1) * (data->dict.numWords / data->numThreads);
    }

    struct crypt_data cryptData;
    cryptData.initialized = 0;
    
    for (int i = start; i < end; i++) {
        if (*data->stopFlag) {
            data->result = NULL;
            return NULL;
        }
        
        char* encryptedWord;
        pthread_mutex_lock(data->dict.dictMutex);
        encryptedWord = crypt_r(data->dict.words[i], data->salt, &cryptData);
        pthread_mutex_unlock(data->dict.dictMutex);
        
        if (strcmp(encryptedWord, data->encrypted) == 0) {
            *data->stopFlag = 1;
            data->result = data->dict.words[i];
            return NULL;
        }
    }
    
    data->result = ":failed\n";
    return NULL;
}

