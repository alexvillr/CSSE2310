/*
 * uqwordiply.c
 *	CSSE2310/7231 - Assignment One - 2023 - Semester One
 *
 *	Written by Peter Sutton, p.sutton@uq.edu.au
 *
 * Usage:
 *   uqwordiply [--start starter-word | --len length] [--dictionary filename]
 *		Only one of --start or --len can be specified
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdbool.h>
#include <csse2310a1.h>
#include <unistd.h>
#include <getopt.h>

// The maximum length of any dictionary word can be assumed to be 50 chars
#define MAX_DICTIONARY_WORD_LENGTH 50

// When reading dictionary words into a buffer, we need to allow space for
// the word + a newline + a terminating null
#define WORD_BUFFER_SIZE (MAX_DICTIONARY_WORD_LENGTH + 2)

// Default dictionary that we search
#define DEFAULT_DICTIONARY "/usr/share/dict/words"

// Limits on size of starter word
#define MIN_STARTER_WORD_LENGTH 3
#define MAX_STARTER_WORD_LENGTH 4

// Maximum number of guesses that will be permitted
#define MAX_GUESSES 5

// Enumerated type with our argument types - used for the getopt() version
// of command line argument parsing
typedef enum {
    START_ARG = 1,
    LEN_ARG = 2,
    DICTIONARY_ARG = 3
} ArgType;

// Enumerated type with our exit statuses
typedef enum {
    OK = 0,
    USAGE_ERROR = 1,
    STARTER_WORD_ERROR = 2,
    DICTIONARY_ERROR = 3,
    NO_GUESSES_MADE = 4
} ExitStatus;

// Structure type to hold our game parameters - obtained from the command line
typedef struct {
    const char* starterWord;
    int starterWordLen;
    char* dictionaryFileName;
} GameParameters;

// Structure type to hold a list of words - used for the dictionary, as well
// as the list of guesses and lists of longest words
typedef struct {
    int numWords;
    char** wordArray;
} WordList;

/* Function prototypes - see descriptions with the functions themselves */
bool check_guess(const char* guess, const char* starterWord, WordList words,
	WordList previousGuesses);
GameParameters process_command_line(int argc, char* argv[]);
GameParameters process_command_line_getopt(int argc, char* argv[]);
void usage_error(void);
void dictionary_error(char* fileName);
void check_starter_word_is_valid(const char* word);
WordList read_dictionary(FILE* fileStream, const char* starterWord);
bool word_contains(const char* within, const char* wordToFind);
WordList add_word_to_list(WordList words, char* word);
void free_word_list(WordList words);
bool word_contains_only_letters(const char* word);
char* convert_word_to_upper_case(char* word);
bool is_word_in_list(const char* word, WordList words);
char* read_line(void);
ExitStatus play_game(const char* starterWord, WordList words);

/*****************************************************************************/
int main(int argc, char* argv[]) {
    GameParameters gameDetails;
    WordList validWords;
    FILE* dictionaryFileStream;

    // Process the command line arguments - and get supplied game parameters.
    // Will not return if arguments are invalid - prints message and exits. 
#ifdef USE_GETOPT
    gameDetails = process_command_line_getopt(argc, argv);
#else
    gameDetails = process_command_line(argc, argv);
#endif

    // If a starter word is supplied on the command line, check it is valid.
    // (We print an error message and exit if not.)
    // If no word was supplied, choose a random word
    if (gameDetails.starterWord) {
	check_starter_word_is_valid(gameDetails.starterWord);
    } else {
	gameDetails.starterWord = 
		get_wordiply_starter_word(gameDetails.starterWordLen);
    }

    // Set a default dictionary name if none given on command line
    if (!gameDetails.dictionaryFileName) {
	gameDetails.dictionaryFileName = DEFAULT_DICTIONARY;
    }

    // Try opening the dictionary file - print an error message 
    // and exit on failure 
    dictionaryFileStream = fopen(gameDetails.dictionaryFileName, "r");
    if (!dictionaryFileStream) {
	dictionary_error(gameDetails.dictionaryFileName);
    }

    // Read the dictionary and populate our list of words. Close the file
    // when done.
    validWords = 
	    read_dictionary(dictionaryFileStream, gameDetails.starterWord);
    fclose(dictionaryFileStream);

    // Play the game and output the result
    ExitStatus status = play_game(gameDetails.starterWord, validWords);

    // Tidy up and exit
    free_word_list(validWords);
    return status;
}

/* 
 * process_command_line()
 * 	Go over the supplied command line arguments, check their validity, and
 *	if OK return the game parameters. (The starter word, if given, is 
 *	converted to upper case.) If the command line is invalid, then 
 *	we print a usage error message and exit. 
 */
GameParameters process_command_line(int argc, char* argv[]) {
    // No parameters to start with (these values will be updated with values
    // from the command line, if specified)
    GameParameters param = { .starterWord = NULL, .starterWordLen = 0, 
	    .dictionaryFileName = NULL };

    // Skip over the program name argument
    argc--;
    argv++;

    // Check for option arguments.
    while (argc >= 2 && argv[0][0] == '-') {
	if (strcmp(argv[0], "--start") == 0 && !param.starterWord) {
	    param.starterWord = convert_word_to_upper_case(argv[1]);
	} else if (strcmp(argv[0], "--len") == 0 && !param.starterWordLen &&
		isdigit(argv[1][0]) && strlen(argv[1]) == 1) {
	    param.starterWordLen = atoi(argv[1]);
	    if (param.starterWordLen < MIN_STARTER_WORD_LENGTH ||
		    param.starterWordLen > MAX_STARTER_WORD_LENGTH) {
		usage_error();
	    }
	} else if (strcmp(argv[0], "--dictionary") == 0 
		&& !param.dictionaryFileName) {
	    param.dictionaryFileName = argv[1];
	} else {
	    // Unexpected argument (covers the case of a repeated argument
	    // also)
	    usage_error();
	}
	// If we got here, we processed an option argument and value - skip
	// over those, then return to the top of the loop to check for more
	argc -= 2;
	argv += 2;
    }
    // If any arguments now remain OR both a length and starter word argument
    // were given, then this is a usage error, otherwise we have our parameters
    if (argc || (param.starterWordLen && param.starterWord)) {
	usage_error();
    }

    return param;
}

/* 
 * process_command_line_getopt()
 * 	Go over the supplied command line arguments, check their validity, and
 *	if OK return the game parameters. (The starter word, if given, is 
 *	converted to upper case.) If the command line is invalid, then 
 *	we print a usage error message and exit. 
 *	This is a getopt version of the command line processing.
 */
GameParameters process_command_line_getopt(int argc, char* argv[]) {
    // No parameters to start with (these values will be updated with values
    // from the command line, if specified)
    GameParameters param = { .starterWord = NULL, .starterWordLen = 0, 
	    .dictionaryFileName = NULL };

    // REF: Code based on the example in the getopt(3) man page
    struct option longOptions[] = {
	    { "start", required_argument, 0, START_ARG},
	    { "len", required_argument, 0, LEN_ARG},
	    { "dictionary", required_argument, 0, DICTIONARY_ARG},
	    { 0, 0, 0, 0}};
    int optionIndex = 0;

    while (true) {
	int opt = getopt_long(argc, argv, ":", longOptions, 
		&optionIndex);
	if (opt == -1) { // Ran out of option arguments
	    break;
	} else if (opt == START_ARG && !param.starterWord) {
	    param.starterWord = convert_word_to_upper_case(optarg);
	} else if (opt == LEN_ARG && !param.starterWordLen && 
		isdigit(optarg[0]) && strlen(optarg) == 1) {
	    param.starterWordLen = atoi(optarg);
	    if (param.starterWordLen < MIN_STARTER_WORD_LENGTH ||
		    param.starterWordLen > MAX_STARTER_WORD_LENGTH) {
		usage_error();
	    }
	} else if (opt == DICTIONARY_ARG && !param.dictionaryFileName) {
	    param.dictionaryFileName = optarg;
	} else {
	    usage_error();
	}
    }

    // If any arguments now remain OR both a length and starter word argument
    // were given, then this is a usage error, otherwise we have our parameters
    if (optind < argc || (param.starterWordLen && param.starterWord)) {
	usage_error();
    }

    return param;
}

/*  
 * usage_error()
 *	Print the usage error message, then exit with a non-zero exit status.
 */
void usage_error(void) {
    fprintf(stderr, "Usage: uqwordiply [--start starter-word | --len length] "
	    "[--dictionary filename]\n");
    exit(USAGE_ERROR);
}

/*  
 * dictionary_error()
 *	Print the error message about being unable to open dictionary
 *	(including the supplied filename in the message). Exit
 *	with appropriate exit code.
 */
void dictionary_error(char* fileName) {
    fprintf(stderr, "uqwordiply: dictionary file \"%s\" cannot be opened\n",
	    fileName);
    exit(DICTIONARY_ERROR);
}

/*
 * check_starter_word_is_valid()
 * 	Checks if the given word is a valid starter word (length
 * 	OK and contains only letters). If not, prints error
 * 	message and exits. Otherwise, just returns.
 */
void check_starter_word_is_valid(const char* word) {
    int len = strlen(word);
    if (len < MIN_STARTER_WORD_LENGTH || len > MAX_STARTER_WORD_LENGTH || 
	    !word_contains_only_letters(word)) {
	fprintf(stderr, "uqwordiply: invalid starter word\n");
	exit(STARTER_WORD_ERROR);
    }
}

/*
 * read_dictionary()
 *	Read all words from the given dictionary file stream (fileStream)
 *	that contain the given word (starterWord), are longer than the
 *	given word, and contain only letters. Words are converted to upper
 *	case to be stored. The starterWord is known to be upper case.
 */
WordList read_dictionary(FILE* fileStream, const char* starterWord) {
    WordList validWords;
    char currentWord[WORD_BUFFER_SIZE]; 	// Buffer to hold word.
    int starterWordLen = strlen(starterWord);

    // Initialise our list of matches - nothing in it initially.
    validWords.numWords = 0;
    validWords.wordArray = 0;

    // Read lines of file one by one 
    while (fgets(currentWord, WORD_BUFFER_SIZE, fileStream)) {
	// Word has been read - remove any newline at the end
	// if there is one. Convert the word to uppercase.
	int wordLen = strlen(currentWord);
	if (wordLen > 0 && currentWord[wordLen - 1] == '\n') {
	    currentWord[wordLen - 1] = '\0';
	    wordLen--;
	}
	convert_word_to_upper_case(currentWord);
	// If the word is longer than our starter word and contains the 
	// starter word, then add it to our list
	if (wordLen > starterWordLen && 
		word_contains(currentWord, starterWord) &&
		word_contains_only_letters(currentWord)) {
	    validWords = add_word_to_list(validWords, currentWord);
	}
    }
    return validWords;
}

/*
 * word_contains()
 * 	Returns true if wordToFind is found inside the word within,
 * 	false otherwise. The words are assumed to have the same case,
 * 	i.e. both upper case.
 */
bool word_contains(const char* within, const char* wordToFind) {
    return strstr(within, wordToFind);
}

/* 
 * add_word_to_list()
 *	Copy the given word and add it to the given list of matches (words).
 */
WordList add_word_to_list(WordList words, char* word) {
    char* wordCopy;

    // Make a copy of the word into newly allocated memory
    wordCopy = strdup(word);

    // Make sure we have enough space to store our list (array) of
    // words.
    words.wordArray = realloc(words.wordArray,
	    sizeof(char*) * (words.numWords + 1));
    words.wordArray[words.numWords] = wordCopy;	// Add word to list
    words.numWords++;	// Update count of words
    return words;
}

/*
 * free_word_list()
 * 	Deallocates all memory associated with the given list of words.
 */
void free_word_list(WordList words) {
    for (int i = 0; i < words.numWords; i++) {
	free(words.wordArray[i]);
    }
    free(words.wordArray);
}

/*
 * find_longest_words_in_list()
 * 	Search the given list of words and return a list of the longest
 * 	word(s) in that list.
 */
WordList find_longest_words_in_list(WordList words) {
    int maxLen = 0;
    WordList longestWords = {0, NULL};
    for (int i = 0; i < words.numWords; i++) {
	int wordLen = strlen(words.wordArray[i]);
	if (wordLen == maxLen) {
	    // We've found a word equal in length to our longest word - 
	    // add it to the list of longest words
	    longestWords = add_word_to_list(longestWords, words.wordArray[i]);
	} else if (wordLen > maxLen) {
	    // We've found a word longer than our current maximum. Destroy
	    // our previous list of longest words and start a new one with
	    // this word as a member (and update the max length).
	    free_word_list(longestWords);
	    longestWords.numWords = 0;
	    longestWords.wordArray = NULL;
	    longestWords = add_word_to_list(longestWords, words.wordArray[i]);
	    maxLen = wordLen;
	}
    }
    return longestWords;
}

/*
 * print_list_of_words_with_lengths()
 * 	Prints the given list of words to standard output - one per line. Each
 * 	word is followed by the length of that word in parentheses. We
 * 	free the list of words when done.
 */
void print_list_of_words_with_lengths(WordList words) {
    for (int i = 0; i < words.numWords; i++) {
	printf("%s (%d)\n", words.wordArray[i], 
		(int)strlen(words.wordArray[i]));
    }
    free_word_list(words);
}

/*
 * convert_word_to_upper_case()
 * 	Traverses the supplied word and converts each lower case letter
 * 	into the upper case equivalent. We return a pointer to the string
 * 	we're given.
 */
char* convert_word_to_upper_case(char* word) {
    char* cursor = word;
    while (*cursor) {
	*cursor = toupper(*cursor);
	cursor++;
    }
    return word;
}

/*
 * word_contains_only_letters()
 * 	Traverses the word and returns true if the string contains only
 * 	letters (upper or lower case), false otherwise.
 */
bool word_contains_only_letters(const char* word) {
    while (*word) {
	if (!isalpha(*word)) {
	    return false;
	} 
	word++;
    }
    return true;
}

/*
 * is_word_in_list()
 * 	Returns true if the given word is in the given list of words (words),
 * 	false otherwise. The word and all words in the list are known to be 
 * 	upper case.
 */
bool is_word_in_list(const char* word, WordList words) {
    for (int i = 0; i < words.numWords; i++) {
	if (strcmp(words.wordArray[i], word) == 0) {
	    return true;
	}
    }
    // Word not found
    return false;
}

/*
 * read_line()
 *	Read a line of indeterminate length from stdin (i.e. we read
 * 	characters until we reach a newline or EOF. If we hit EOF at 
 *	the start of the line then we return NULL, otherwise we 
 *	return the line of text (without any newline) in dynamically 
 *	allocated memory.
 */
char* read_line(void) {
    char* buffer = NULL;
    int bufSize = 0;
    int character;

    while (character = fgetc(stdin), (character != EOF && character != '\n')) {
	// Got a character - grow the buffer and put it in the buffer
	buffer = realloc(buffer, bufSize + 1);
	buffer[bufSize] = character;
	bufSize++;
    }
    // Got EOF or newline
    if (character == '\n' || bufSize > 0) {
	// Have a line to return - grow the buffer and null terminate it
	buffer = realloc(buffer, bufSize + 1);
	buffer[bufSize] = '\0';
    } // else got EOF at start of line - we'll be returning NULL
    return buffer;
}

/*
 * play_game()
 *	Play the uqwordiply game with the given starter word and list
 *	of valid guesses from the dictionary.
 *	If no guesses are made, we return with status NO_GUESSES_MADE, 
 *	otherwise we return OK.
 */
ExitStatus play_game(const char* starterWord, WordList dictionary) {
    int numValidGuesses = 0;
    int totalLen = 0;	// Total length of all valid guesses
    WordList previousGuesses = {0, NULL};

    printf("Welcome to UQWordiply!\n");
    printf("The starter word is: %s\n", starterWord);
    printf("Enter words containing this word.\n");

    while (numValidGuesses < MAX_GUESSES) {
	// Prompt for word
	printf("Enter guess %d:\n", numValidGuesses + 1);
	// Read line of text from user. Abort if EOF.
	char* guess = read_line();
	if (!guess) {
	    break;
	}

	// Convert the guess to upper case, and make sure it is valid
	// If it is valid, update our stats and add it to the list of guesses
	convert_word_to_upper_case(guess);
	if (check_guess(guess, starterWord, dictionary, previousGuesses)) {
	    int guessLen = strlen(guess);
	    totalLen += guessLen;
	    previousGuesses = add_word_to_list(previousGuesses, guess);
	    numValidGuesses++;
	}

	free(guess);
    }

    // Have detected EOF or run out of guesses - game is over
    if (numValidGuesses == 0) {
	return NO_GUESSES_MADE;
    } else {
	printf("\nTotal length of words found: %d\n", totalLen);
	printf("Longest word(s) found:\n");
	WordList longestWords = find_longest_words_in_list(previousGuesses);
	print_list_of_words_with_lengths(longestWords);
	printf("Longest word(s) possible:\n");
	longestWords = find_longest_words_in_list(dictionary);
	print_list_of_words_with_lengths(longestWords);
	free_word_list(previousGuesses);
	return OK;
    }
}

/*
 * check_guess()
 *	Check that the given guess is valid, i.e. that it contains only	
 *	letters, contains the starter word (starterWord) - but isn't just 
 *	the starter word), that it hasn't been previously guessed (i.e. not
 *	in previousGuesses) and is a valid word (i.e. is in validWords).
 *	Return true if OK, false otherwise (with a suitable message printed)..
 */
bool check_guess(const char* guess, const char* starterWord, 
	WordList validWords, WordList previousGuesses) {
    if (!word_contains_only_letters(guess)) {
	printf("Guesses must contain only letters - try again.\n");
	return false;
    }
    if (!word_contains(guess, starterWord)) {
	printf("Guesses must contain the starter word - try again.\n");
	return false;
    }
    if (strcasecmp(guess, starterWord) == 0) {
	printf("Guesses can't be the starter word - try again.\n");
	return false;
    }
    if (!is_word_in_list(guess, validWords)) {
	printf("Guess not found in dictionary - try again.\n");
	return false;
    }
    if (is_word_in_list(guess, previousGuesses)) {
	printf("You've already guessed that word - try again.\n");
	return false;
    }
    return true;	// The guess is OK
}
