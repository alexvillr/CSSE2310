/*
 * testuqwordiply.c
 *      CSSE2310 - Assignment Two
 *
 *      Written by Alex Viller, a.viller@uqconnect.edu.au
 *
 * Usage:
 *  testuqwordiply [--quiet] [--parallel] testprogram jobfile
 *          optional arguments must be before program and jobfile
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>
#include <getopt.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <time.h>
#include <csse2310a3.h>

// global definitions
#define STARTER_LIST_SIZE 2
#define NUM_PIPES 4
#define READ_AND_WRITE 2
#define NUM_PROCESSES 4
#define PROGRAM_TO_COMPARE "demo-uqwordiply"
#define EMPTY_DIRECTORY "/dev/null"
#define ONE_OPT 2
#define TWO_OPTS 3
#define FD3 3
#define FD4 4


// enum type containing all the error codes
typedef enum {
    OK = 0,
    USAGE_ERROR = 2,
    JOBSPEC_OPEN_ERROR = 3,
    JOBSPEC_SYNTAX_ERROR = 4,
    JOBSPEC_INFILE_OPEN_ERROR = 5,
    JOBSPEC_EMPTY_ERROR = 6
} ExitStatus;

// enum type containing pipe names
typedef enum {
    STD_OUT = 0,
    STD_ERR = 1,
    FROM_TEST = 2,
    FROM_EXPECTED = 3
} PipeNames;

// enum type containing read vs write pipes
typedef enum {
    READ = 0,
    WRITE = 1
} PipeTypes;

// enum type containing all the process names
typedef enum {
    PROCESS_A = 0,
    PROCESS_B = 1,
    PROCESS_C = 2,
    PROCESS_D = 3
} Processes;

// structure type to contain all the information involving a job
typedef struct {
    int numArgs;
    char** args;
    char** testArgs;
    char* inFile;
} Job;

// structure type to hold all the jobs in one place
typedef struct {
    Job* jobs;
    int numJobs;
} JobList;

// Structure type to hold all parameters, obtained from command line
typedef struct {
    char* testPath;
    char* jobPath;
    FILE* jobFile;
    bool quiet;
    bool parallel;
    JobList jobList;
} TestParameters;

/* Funcion prototypes - see descriptions with the functions themselves */
int main(int argc, char* argv[]);
void print_usage(void);
void print_syntax_error(int, char*, char*, int);
TestParameters initialise(int argc, char* argv[]);
void opts_error_check(TestParameters params, int argc, char* originalargv[]);
JobList get_jobs(TestParameters);
void jobspec_error_check(TestParameters, JobList, char*, int);
void do_jobs(TestParameters, JobList, char*);
pid_t* do_job(Job, char*, int, bool);
void run_testee(Job, char*, int[NUM_PIPES][READ_AND_WRITE]);
void run_tester(Job, int[NUM_PIPES][READ_AND_WRITE]);
void compare_std_out(Job, char*, int, int[NUM_PIPES][READ_AND_WRITE], bool);
void compare_std_err(Job, char*, int, int[NUM_PIPES][READ_AND_WRITE], bool);

/* main()
 * ------
 * The main driver for the function, pushes all content forward and ensures
 * everything is working as it should.
 */
int main(int argc, char* argv[]) {
    TestParameters params = initialise(argc, argv);
    params.jobList = get_jobs(params);
    do_jobs(params, params.jobList, params.testPath);
    
    for (int i = 0; i < params.jobList.numJobs; i++) {
        for (int j = 0; j < params.jobList.jobs[i].numArgs; j++) {
            free(params.jobList.jobs[i].args[j]);
            free(params.jobList.jobs[i].testArgs[j]);
        }
        free(params.jobList.jobs[i].inFile);
        free(params.jobList.jobs[i].args);
        free(params.jobList.jobs[i].testArgs);
    }
    free(params.jobList.jobs);
    fclose(params.jobFile);
    return OK;
}

/* print_usage()
 * -------------
 * This function is used to print usage to the user when incorrect command line
 * arguments are used and then exit with status 2 as found in the ExitStatus
 * type.
 */
void print_usage(void) {
    fprintf(stderr, "Usage: testuqwordiply [--quiet] [--parallel] " \
            "testprogram jobfile\n");
    exit(USAGE_ERROR);
}

/* print_syntax_error()
 * --------------------
 * This function is used to print errors to do with jobfile to the user and
 * then exits with appropriate code for the issue (see ExitStatus enum).
 * 
 * lineNum: The line the error occured on.
 *
 * jobFile: The path to the job file which has an error.
 *
 * infile: An array of strings which holds the contents of the line with the
 *      error. Gets freed.
 *
 * error: The error code to be used.
 */
void print_syntax_error(int lineNum, char* jobFile, char* infile, int error) {
    switch (error) {
        case JOBSPEC_SYNTAX_ERROR:
            fprintf(stderr, "testuqwordiply: syntax error on line %d of"\
                    " \"%s\"\n", lineNum, jobFile);
            break;
        case JOBSPEC_INFILE_OPEN_ERROR:
            fprintf(stderr, "testuqwordiply: unable to open file"\
                    " \"%s\" specified on line %d of \"%s\"\n", infile, 
                    lineNum, jobFile);
            break;
        case JOBSPEC_EMPTY_ERROR:
            fprintf(stderr, "testuqwordiply: no jobs found in \"%s\"\n", 
                    jobFile);
        default:
            break;
    }
    free(infile);
    exit(error);
}

/* initialise()
 * ------------
 * This function processes any command line arguments, checks validity, and if
 * everything is correct, returns these parameters. If anything is invalide, 
 * we print a usage error and exit.
 *
 * argc: The number of command line arguments
 *
 * argv: An array of these arguments
 *
 * Returns: 0
 * Errors: With incorrect command line arguments
 */
TestParameters initialise(int argc, char* argv[]) {
    char* originalargv[argc];
    for (int i = 0; i < argc; i++) {
        originalargv[i] = argv[i];
    }
    TestParameters params = {.testPath = NULL, .jobFile = NULL, 
        .jobPath = NULL, .parallel = false, .quiet = false};
    static struct option longOpts[] = {
        {"quiet", no_argument, NULL, 'q'},
        {"parallel", no_argument, NULL, 'p'},
        {NULL, 0, NULL, 0}
    };

    int opt;
    while ((opt = getopt_long(argc, argv, ":", longOpts, NULL)) != -1) {
        switch(opt) {
            case 'q':
                if (params.quiet) {
                    print_usage();
                }
                params.quiet = true;
                break;
            case 'p':
                if (params.parallel) {
                    print_usage();
                }
                params.parallel = true;
                break;
            default:
                print_usage();
        }
    }
    params.testPath = argv[optind];
    params.jobPath = argv[optind + 1];
    params.jobFile = fopen(params.jobPath, "r");

    opts_error_check(params, argc, originalargv);

    return params;
}

/* opts_error_check()
 * ------------------
 * A function that checks all of the input and options for any errors in 
 * usage.
 *
 * params: A TestParameters struct which contains the currently filled 
 *          options.
 *
 * argc: the number of command line arguments (From main argc).
 *
 * originalargv: The original structure of argv as passed in main, before 
 *          getopt has reordered them.
 * 
 * Returns: void.
 * Errors: For any usage errors with appropriate codes.
 */
void opts_error_check(TestParameters params, int argc, char* originalargv[]) {
    if ((params.quiet && !params.parallel) || 
        (!params.quiet && params.parallel)) {
        for (int i = ONE_OPT; i < argc; i++) {
            if (originalargv[i][0] == '-') {
                print_usage();
            }
        }
    } else if (params.quiet && params.parallel) {
        for (int i = TWO_OPTS; i < argc; i++) {
            if (originalargv[i][0] == '-') {
                print_usage();
            }
        }
    }

    if (argc - optind != 2) {
        print_usage();
    }
    if (params.jobFile == NULL) {
        fprintf(stderr, "testuqwordiply: Unable to open job file \"%s\"\n", \
                params.jobPath);
        exit(JOBSPEC_OPEN_ERROR);
    }
}

/* get_jobs()
 * ----------
 * This function goes through the job file and returns a list of each job. Or 
 * an error if there are any jobs which are not syntactically correct
 *
 * jobFile: The file to be searched through for jobs
 *
 * Returns: A list of jobs
 * Errors: If any jobs are syntactically incorrect
 */
JobList get_jobs(TestParameters params) {
    JobList jobs = {.numJobs = 0};
    int bufferSize = STARTER_LIST_SIZE;
    char* currentLine;
    int lineNum = 0;
    jobs.jobs = (Job*) malloc(bufferSize);
    while ((currentLine = read_line(params.jobFile))) {
        char lineArray[strlen(currentLine) + 1];
        strcpy(lineArray, currentLine);
        lineNum++;
        bool commentLine = false;
        if (currentLine[0] == '#' || currentLine[0] == '\0') {
            commentLine = true;
        }
        if (!commentLine) {
            jobspec_error_check(params, jobs, currentLine, lineNum);
            char** splitLine = split_line(lineArray, ',');
            int numArgs;
            char** args = split_space_not_quote(splitLine[1], &numArgs);
            char** mallocedArgs = (char**) malloc((numArgs + 2) * 
                    sizeof(char*));
            char** mallocedTestArgs = (char**) malloc((numArgs + 2) *
                    sizeof(char*));
            for (int i = 1; i < numArgs + 1; i++) {
                mallocedArgs[i] = strdup(args[i - 1]);
                mallocedTestArgs[i] = strdup(args[i - 1]);
            }
            mallocedArgs[numArgs + 2] = NULL;
            mallocedTestArgs[numArgs + 2] = NULL;
            char* fileName = strdup(splitLine[0]);
            mallocedArgs[0] = strdup(fileName);
            mallocedTestArgs[0] = strdup(PROGRAM_TO_COMPARE);
            Job currentJob = {.inFile = fileName, .numArgs = numArgs + 1};
            currentJob.args = mallocedArgs;
            currentJob.testArgs = mallocedTestArgs;
            bufferSize = bufferSize + sizeof(currentJob);
            jobs.jobs = (Job*) realloc(jobs.jobs, bufferSize);
            jobs.jobs[jobs.numJobs] = currentJob;
            jobs.numJobs++;
            free(splitLine);
            free(args);
        }
        free(currentLine);
    }
    if (jobs.numJobs == 0) {
        free(jobs.jobs);
        fclose(params.jobFile);
        print_syntax_error(-1, params.jobPath, (char*) malloc(1),
                JOBSPEC_EMPTY_ERROR);
    }
    return jobs;
}

/* jobspec_error_check()
 * ---------------------
 * A function that checks the current line of the job file for any syntax
 * errors.
 *
 * params: The TestParameters struct which contains all important information
 *          for the current program.
 *
 * jobs: The current list of jobs to be filled with more.
 *
 * line: The current line of the job file that we are parsing.
 *
 * lineNum: the line number of the job file that we are parsing for use in
 *          error printing.
 *
 * Returns: void.
 * Errors: appropriately for different errors within the job file.
 */
void jobspec_error_check(TestParameters params, JobList jobs, char* line, 
        int lineNum) {
    int error = -1; // initialise no error
    char** splitLine = split_line(line, ',');
    FILE* inFile = fopen(splitLine[0], "r");
    bool noComma = splitLine[0] == NULL;
    bool validInfile = strcmp(splitLine[0], "");
    bool twoCommas = splitLine[2] == NULL;
    if (noComma || !validInfile || !twoCommas) {
        error = JOBSPEC_SYNTAX_ERROR;
    } else if (inFile == NULL) {
        error = JOBSPEC_INFILE_OPEN_ERROR;
    }
    if (error == -1) {
        fclose(inFile);
        free(splitLine);
        return;
    } else if (inFile != NULL) {
        fclose(inFile);
    }
    char* inFileName = strdup(splitLine[0]);
    fclose(params.jobFile);
    free(jobs.jobs);
    free(splitLine);
    free(line);
    print_syntax_error(lineNum, params.jobPath, inFileName, error);
}

/* do_jobs()
 * ---------
 * The function iterates through the job list and runs each one.
 *
 * params: The TestParameters struct containing all important information to
 *          be passed around
 *
 * jobs: The list of jobs to be done.
 *
 * testPath: The path to the program being tested.
 *
 * Returns: void
 * Errors: if any subsequent function calls error
*/ 
void do_jobs(TestParameters params, JobList jobs, char* testPath) {
    const struct timespec request = {
        .tv_sec = 2,
        .tv_nsec = 0,
    };
    struct timespec remaining;
    pid_t* children;
    pid_t** jobchildren;
    if (params.parallel) {
        jobchildren = malloc(sizeof(pid_t*) * jobs.numJobs);
    }

    int response;
    for (int i = 0; i < jobs.numJobs; i++) {
        printf("Starting job %d\n", i + 1);
        fflush(stdout);
        if (!params.parallel) {
            children = do_job(jobs.jobs[i], testPath, (i + 1), params.quiet);
            response = nanosleep(&request, &remaining);
            if (response != 0) {
                printf("Sleep failure\n");
            }
            for (int child = 0; child < NUM_PROCESSES; child++) {
                kill(children[child], SIGKILL);
            }
            free(children);
        } else {
            jobchildren[i] = do_job(jobs.jobs[i], testPath, (i + 1), 
                    params.quiet);
        }
    }
    if (params.parallel) {
        response = nanosleep(&request, &remaining);
        if (response != 0) {
            printf("Sleep failure\n");
        }
        for (int i = 0; i < jobs.numJobs; i++) {
            for (int child = 0; child < NUM_PROCESSES; child++) {
                kill(jobchildren[i][child], SIGKILL);
            }
            free(jobchildren[i]);
        }
        free(jobchildren);
    }
}

/* do_job()
 * --------
 * This function runs a single job by creating 4 child processes and runs the
 * appropriate programs.
 *
 * job: The job to be run.
 *
 * testPath: The path to the program to get tested.
 *
 * jobNum: What job this is.
 *
 * quiet: Whether the quiet option has been set
 *
 * Returns: The array of child pointers so that the processes can be killed
*/
pid_t* do_job(Job job, char* testPath, int jobNum, bool quiet) {
    int pipes[NUM_PIPES][READ_AND_WRITE];
    pid_t* children = malloc(sizeof(pid_t) * NUM_PROCESSES);
    // ensure fd3 and fd4 are available
    close(FD3);
    close(FD4);
    int fd3 = open(EMPTY_DIRECTORY, O_RDONLY);
    int fd4 = open(EMPTY_DIRECTORY, O_RDONLY);
    if (fd3 == -1 || fd4 == -1) {
        perror("creating file descriptors failure");
    }
    // for (int i = 0; i < NUM_PIPES; i++) {
    //     if (pipe(pipes[i]) == -1) {
    //         perror("Pipe creation error");
    //     }
    // }
    // make fd for quiet mode
    int ignorefd = open(EMPTY_DIRECTORY, O_WRONLY);
    // make prefix
    char prefixOut[32];
    sprintf(prefixOut, "\"Job %d stdout\"", jobNum);
    char prefixErr[32];
    sprintf(prefixErr, "\"Job %d stderr\"", jobNum);


    for (int child = 0; child < NUM_PROCESSES; child++) {
        pid_t childPid = fork();
        if (childPid == -1) {
            perror("Fork failed");
        }
        if (childPid == 0) { // Children
            if (child == PROCESS_A) {
                run_testee(job, testPath, pipes);
            } else if (child == PROCESS_B) {
                run_tester(job, pipes);
            } else if (child == PROCESS_C) {
                compare_std_out(job, prefixOut, ignorefd, pipes, quiet);
            } else if (child == PROCESS_D) {
                compare_std_err(job, prefixErr, ignorefd, pipes, quiet);
            }
        } else { // Parent
            children[child] = childPid;
        }
    }

    for (int i = 0; i < NUM_PIPES; i++) {
        close(pipes[i][WRITE]);
        close(pipes[i][READ]);
    }
    close(ignorefd);
    close(FD3);
    close(FD4);
    return children;
}

/* run_testee()
 * ------------
 * runs the process for the program to be tested.
 *
 * job: The current job being run
 *
 * testPath: The path to the program that needs testing
 *
 * pipes: the pipes that have been created in the do_job() method
 *
 * Return: void
*/
void run_testee(Job job, char* testPath,
        int pipes[NUM_PIPES][READ_AND_WRITE]) {
    // make new input fd
    int inputFd = open(job.inFile, O_RDONLY);
    // Redirect standard input to the input file
    dup2(inputFd, STDIN_FILENO);
    close(inputFd);
    // Redirect stdout and stderr to pipes
    // dup2(pipes[STD_OUT][WRITE], STDOUT_FILENO);
    // dup2(pipes[STD_ERR][WRITE], STDERR_FILENO);
    dup2(FD3, STDOUT_FILENO);
    dup2(FD3, STDERR_FILENO);
    // Redirect standard output and standard error to file descriptor 3
    // close(pipes[STD_OUT][READ]);
    // close(pipes[STD_ERR][READ]);
    // close(pipes[FROM_EXPECTED][READ]);
    // close(pipes[FROM_EXPECTED][WRITE]);
    // close(pipes[FROM_TEST][READ]);
    // if (dup2(pipes[FROM_TEST][WRITE], pipes[STD_OUT][WRITE]) == -1) {
        // perror("dup2 stdout fd3");
    // }
    // if (dup2(pipes[FROM_TEST][WRITE], pipes[STD_ERR][WRITE]) == -1) {
        // perror("dup2 stderr fd3");
    // }
    // Execute the program being tested
    execvp(testPath, job.args);

    // If execvp returns, there was an error
    perror("testee Execution failed");
    return;
}

/* run_tester()
 * ------------
 * runs the process for the correct program to get the correct output.
 *
 * job: The current job being run
 *
 * pipes: the pipes that have been created in the do_job() method
 *
 * Return: void
*/
void run_tester(Job job, int pipes[NUM_PIPES][READ_AND_WRITE]) {
    // make new input fd
    int inputFd = open(job.inFile, O_RDONLY);
    // Redirect standard input to the input file
    dup2(inputFd, STDIN_FILENO);
    close(inputFd);
    // Redirect stdout and stderr to pipes
    // dup2(pipes[STD_OUT][WRITE], STDOUT_FILENO);
    // dup2(pipes[STD_ERR][WRITE], STDERR_FILENO);
    dup2(FD4, STDOUT_FILENO);
    dup2(FD4, STDERR_FILENO);
    // Redirect and close unnecassary pipes
    // close(pipes[STD_OUT][READ]);
    // close(pipes[STD_ERR][READ]);
    // close(pipes[FROM_TEST][READ]);
    // close(pipes[FROM_TEST][WRITE]);
    // close(pipes[FROM_EXPECTED][READ]);
    // if (dup2(pipes[FROM_EXPECTED][WRITE], pipes[STD_OUT][WRITE]) == -1) {
    //     perror("dup2 stdout fd4");
    // }
    // if (dup2(pipes[FROM_EXPECTED][WRITE], pipes[STD_ERR][WRITE]) == -1) {
    //     perror("dup2 stderr fd4");
    // }
    // Execute the program being tested
    execvp(PROGRAM_TO_COMPARE, job.testArgs);

    // If execvp returns, there was an error
    perror("tester Execution failed");
    return;
}

/* compare_std_out()
 * ------------
 * runs an instance of uqcmp with inputs of the stdout from the tester and 
 * testee.
 *
 * job: The current job being run.
 *
 * prefix: The prefix for uqcmp to use.
 *
 * ignorefd: A file descriptor for printing empty output if the quiet option
 *          has been set.
 *
 * pipes: The pipes that have been created in the do_job() method.
 *
 * quiet: Whether the quiet option has been set or not.
 *
 * Return: void
*/
void compare_std_out(Job job, char* prefix, int ignorefd,
        int pipes[NUM_PIPES][READ_AND_WRITE], bool quiet) {
    // Redirect and close unnecassary pipes
    // close(pipes[STD_OUT][READ]);
    // close(pipes[STD_ERR][READ]);
    // close(pipes[FROM_TEST][WRITE]);
    // close(pipes[FROM_EXPECTED][WRITE]);
    // if (dup2(FD3, pipes[FROM_TEST][READ]) == -1) {
    //     perror("dup2 stdout fd3");
    // }
    // if (dup2(FD4, pipes[FROM_EXPECTED][READ]) == -1) {
    //     perror("dup2 stdout fd4");
    // }
    if (!quiet) {
        if (dup2(STDERR_FILENO, pipes[STD_ERR][WRITE]) == -1) {
            perror("process c stderr pipe error");
        }
        if (dup2(STDOUT_FILENO, pipes[STD_OUT][WRITE]) == -1) {
            perror("process c stdout pipe error");
        }
    } else {
        if (dup2(ignorefd, pipes[STD_ERR][WRITE]) == -1) {
            perror("process c stderr silent pipe error");
        }
        if (dup2(ignorefd, pipes[STD_OUT][WRITE]) == -1) {
            perror("process c stdout silent pipe error");
        }
    }
    // close(pipes[FROM_TEST][READ]);
    // close(pipes[FROM_EXPECTED][READ]);
    

    char* compare = "uqcmp";
    char* args[] = { "uqcmp", prefix, NULL };

    execvp(compare, args);
    // If execvp returns, there was an error
    perror("cmp out Execution failed");
    return;
}

/* compare_std_err()
 * ------------
 * runs an instance of uqcmp with inputs of the stderr from the tester and 
 * testee.
 *
 * job: The current job being run.
 *
 * prefix: The prefix for uqcmp to use.
 *
 * ignorefd: A file descriptor for printing empty output if the quiet option
 *          has been set.
 *
 * pipes: The pipes that have been created in the do_job() method.
 *
 * quiet: Whether the quiet option has been set or not.
 *
 * Return: void
*/
void compare_std_err(Job job, char* prefix, int ignorefd,
        int pipes[NUM_PIPES][READ_AND_WRITE], bool quiet) {
    // Redirect and close unnecassary pipes
    // close(pipes[STD_OUT][READ]);
    // close(pipes[STD_ERR][READ]);
    // close(pipes[FROM_TEST][WRITE]);
    // close(pipes[FROM_EXPECTED][WRITE]);
    // if (dup2(FD3, pipes[FROM_TEST][READ]) == -1) {
    //     perror("dup2 stderr fd3");
    // }
    // if (dup2(FD4, pipes[FROM_EXPECTED][READ]) == -1) {
    //     perror("dup2 stderr fd4");
    // }
    if (!quiet) {
        if (dup2(STDERR_FILENO, pipes[STD_ERR][WRITE]) == -1) {
            perror("process d stderr pipe error");
        }
        if (dup2(STDOUT_FILENO, pipes[STD_OUT][WRITE]) == -1) {
            perror("process d stdout pipe error");
        }
    } else {
        if (dup2(ignorefd, pipes[STD_ERR][WRITE]) == -1) {
            perror("process d stderr silent pipe error");
        }
        if (dup2(ignorefd, pipes[STD_OUT][WRITE]) == -1) {
            perror("process d stdout silent pipe error");
        }
    }
    // close(pipes[FROM_TEST][READ]);
    // close(pipes[FROM_EXPECTED][READ]);

    char* compare = "uqcmp";
    char* args[] = { "uqcmp", prefix, NULL };

    execvp(compare, args);
    // If execvp returns, there was an error
    perror("cmp err Execution failed");
    return;
}
