#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <stdio.h>
#include <stdbool.h>
#include <getopt.h>
#include <csse2310a1.h>

#define MAX_LINE_LENGTH 52
#define MAX_GUESSES 5
#define NEWLINE '\n'
#define NULLTERMINATOR '\0'

// Typedefs
// Dictionary to contain the words as well as it's current size
typedef struct {
    bool isInitialised;
    char** words;
    int size;
    int maxLength;
} Dictionary;

// struct to store important game variables like the dictionary and the starter
// word
typedef struct {
    const char* starterWord;
    Dictionary dictionary;
    Dictionary guesses;
} GameVariables;

// Method declarations
void print_usage();
void print_results(GameVariables);
void print_longest_in_dict(Dictionary, const char*);
void clean_up(GameVariables);
int len_processing(char*, GameVariables);
bool validate_guess(char*, GameVariables, Dictionary);
bool str_all_alpha(char*);
bool in_dictionary(Dictionary, char*);
char* str_to_upper(char*);
char* starter_word_processing(char*);
Dictionary get_guesses(GameVariables);
Dictionary dictionary_processing(char*, const char*);
GameVariables initialise_game(int argc, char* argv[]);

/* main()
 * ------
 * This function is what is run initially by the c program. It will call all
 * the helper methods defined so that the game will properly run with the given
 * command line arguments
 *
 * argc: the number of inputs at command line
 *
 * argv: the inputs provided at command line
 *
 * Returns: 0
 * Errors: Silently exits with code 4. For all other potential errors, see any
 * used helper functions (in particular initialise_game())
*/ 
int main(int argc, char* argv[]) {
    GameVariables gameValues;
    gameValues = initialise_game(argc, argv);

    // print the welcome message
    printf("Welcome to UQWordiply!\n" \
            "The starter word is: %s\n" \
            "Enter words containing this word.\n",
            gameValues.starterWord);

    gameValues.guesses = get_guesses(gameValues);

    if (gameValues.guesses.size == 0) { // in case of silent exit
        clean_up(gameValues);
        exit(4);
    }

    print_results(gameValues);

    clean_up(gameValues);
    return 0;
}

/* print_usage()
 * -------------
 * This function prints to standard error the proper usage of the built code 
 * and exits the program with code 1
 *
 * Returns: void
 * Errors: Will always exit with code 1 when called by design
*/
void print_usage() {
    // prints standard usage to std error
    fprintf(stderr, "Usage: uqwordiply [--start starter-word | --len length"\
            "] [--dictionary filename]\n");
    exit(1);
}

/* clean_up()
 * ----------
 * This function frees all malloced values, avoiding memory leaks
 *
 * game: The game variables which store the dictionary and guesses
 *
 * Returns: void
*/
void clean_up(GameVariables game) {
    // Freeing up the dictionary values
    if (game.dictionary.isInitialised) {
        for (int i = 0; i < game.dictionary.size; i++) {
            free(game.dictionary.words[i]);
        }
        free(game.dictionary.words);
    }
    if (game.guesses.isInitialised) {
        if (game.guesses.size != 0) {
            for (int i = 0; i < game.guesses.size; i++) {
                free(game.guesses.words[i]);
            }
        }
        free(game.guesses.words);
    }
}

/* print_results()
 * ---------------
 * This function prints the results at the end of the game. Including the total
 * length of the words guessed, the longest guesses with how long they are, and
 * the longest words in the dictionary that could be guessed
 *
 * game: The game variables that store the guesses and dictionary used
 *
 * Returns: void
*/
void print_results(GameVariables game) {
    int totalLength = 0;
    for (int i = 0; i < game.guesses.size; i++) {
        int currentLength = strlen(game.guesses.words[i]);
        totalLength += currentLength;
    }
    printf("\nTotal length of words found: %d\n", totalLength);
    printf("Longest word(s) found:\n");
    print_longest_in_dict(game.guesses, game.starterWord);

    printf("Longest word(s) possible:\n");
    print_longest_in_dict(game.dictionary, game.starterWord);
}

/* print_longest_in_dict()
 * -----------------------
 * This function travels through the dictionary and compares the length of each
 * word to the stored max length for words that contain the starter word. Then
 * if the word also contains the starter word, it is printed.
 * 
 * dict: The dictionary to be searched through
 *
 * starter: The starter word, required to be in current word to print
 *
 * Returns: void
*/
void print_longest_in_dict(Dictionary dict, const char* starter) {
    for (int i = 0; i < dict.size; i++) {
        if ((int) strlen(dict.words[i]) == dict.maxLength) {
            if (strstr(dict.words[i], starter) != NULL) {
                printf("%s (%d)\n", dict.words[i], dict.maxLength);
            }
        }
    }
}

/* str_to_upper()
 * --------------
 * This function takes an input string and converts all characters to upper 
 * case
 *
 * word: The string to be converted to upper case
 *
 * Returns: The string in all upper case
*/
char* str_to_upper(char* word){
    for (int i = 0; i < strlen(word); i++) {
        word[i] = toupper(word[i]);
    }
    return word;
}

/* starter_word_processing()
 * -------------------------
 *  This function processes a user inputted starter word in order to ensure 
 *  the word is valid given the rules provided. i.e. word is 3 or 4 letters
 *  long, and the word only contains alpha characters. The word is then 
 *  converted to be all upper case
 *
 *  word: the word to be processed and checked for validity
 *
 *  Returns: The word after proper processing
 *  Errors: exits with code 2 and prints to standard error if the word is not
 *  valid
*/
char* starter_word_processing(char* word) {
    
    bool validWord = true;
    // starter word must be either 3 or 4 letters long
    if (strlen(word) == 3 || strlen(word) == 4) {
        validWord = str_all_alpha(word);
    } else {
        validWord = false;
    }
    
    if (validWord) {
        word = str_to_upper(word);
        return word;
    } else { 
        fprintf(stderr, "uqwordiply: invalid starter word\n");
        exit(2);
    }
}

/* len_processing()
 * ----------------
 *  This function processes a user inputted starter word length in order to
 *  ensure the length is a valid input, then returns the input as an int
 *
 *  lenInput: The string version of the length provided at command line
 *
 *  Returns: an integer version of this input
 *  Errors: if the input is not 3, 4 or all digits, the program exits with code
 *  1 and prints to std error proper usage
*/
int len_processing(char* lenInput, GameVariables game) {
    int newLen = atoi(lenInput);
    // length must be 3 or 4
    if (!(newLen == 3 || newLen == 4)) {
        print_usage();
    } else {
        // length must be a digit
        for (int i = 0; i < strlen(optarg); i++) {
            if (!(isdigit(lenInput[i]))) {
                print_usage();
            }
        }
    }
    return newLen;
}

/* dictionary_processing()
 * -----------------------
 *  This function processes the dictionary and returns an array of all the 
 *  words within the dictionary and then closes the file
 *
 *  path: The path to the dictionary to be used
 *
 *  Returns: an array of each word contained within the dictionary
*/
Dictionary dictionary_processing(char* path, const char* starter) {
    FILE* dictionaryFile = fopen(path, "r");
    // Dictionary not openable
    if (dictionaryFile == NULL) {
        fprintf(stderr, "uqwordiply: dictionary file \"%s\" cannot be opened"\
                "\n", path);
        exit(3);
    } else {
        Dictionary dict;
        dict.isInitialised = true;
        dict.size = 0;
        dict.maxLength = -1;
        char lineBuffer[MAX_LINE_LENGTH];
        int bufferSize = sizeof(char*) * 32;
        dict.words = (char**) malloc(bufferSize);
        while (fgets(lineBuffer, MAX_LINE_LENGTH, dictionaryFile)) {
            if ((dict.size * sizeof(char*)) == bufferSize - 8) {
                bufferSize = bufferSize * 2;
                dict.words = realloc(dict.words, bufferSize);
            }
            int length = strlen(lineBuffer);
            if (lineBuffer[length - 1] == NEWLINE) {
                lineBuffer[length - 1] = NULLTERMINATOR;
            }
            if (str_all_alpha(lineBuffer)) {
                length--;
                dict.words[dict.size] = strdup(lineBuffer);
                dict.words[dict.size] = str_to_upper(dict.words[dict.size]);
                if (strstr(dict.words[dict.size], starter) != NULL) {
                    if ((int) strlen(dict.words[dict.size]) > dict.maxLength) {
                        dict.maxLength = (int) length;
                    }
                }
                dict.size++;
            }
        }
        fclose(dictionaryFile);
        return dict;
    }
}

/* initialise_game()
 * -----------------
 *  This function goes through all command line inputs and checks the validity
 *  of all inputs and ensuring the xor on --len and --start inputs.
 *
 *  argc: argc from main, the number of input arguments at command line
 *
 *  argv: argv from main, the contents of input arguments at command line
 *
 *  Returns: struct GameVariables containing all variables important for the
 *           game to be played
 *  Errors: if any of the conditions required for proper usage are failed, 
 *  program will exit and print to std error.
 *  REF: The code relating to command line arguments is inspired by code at
 *  REF: https://stackoverflow.com/questions/17877368
*/
GameVariables initialise_game(int argc, char* argv[]) {
    bool startFlag = false;
    bool lenFlag = false;
    int starterLen = 0; // can be 3 or 4, randomised when 0
    char* dictionaryPath = "/usr/share/dict/words";
    GameVariables values;
    values.dictionary.isInitialised = false;
    int opt;
    struct option longOpt[] = { // struct for options for getopt_long
        {"start", required_argument, NULL, 's'},
        {"len", required_argument, NULL, 'l'},
        {"dictionary", required_argument, NULL, 'd'},
        {NULL, 0, NULL, 0}
    };
    if (argc % 2 != 1) { // error for option without argument
        print_usage();
    }
    while ((opt = getopt_long(argc, argv, ":", longOpt, NULL)) != -1) {
        switch (opt) {
            case 's':
                if (startFlag || lenFlag) {
                    print_usage();
                }
                values.starterWord = starter_word_processing(optarg);
                startFlag = true;
                break;
            case 'l':
                if (startFlag || lenFlag) {
                    print_usage();
                } else {
                    starterLen = len_processing(optarg, values);
                    lenFlag = true;
                    break;
                }
            case 'd':
                dictionaryPath = optarg;
                break;
            default:
                print_usage();
        }
    }
    if (!startFlag) {
        values.starterWord = get_wordiply_starter_word(starterLen);
    }
    values.dictionary = dictionary_processing(dictionaryPath, 
            values.starterWord);
    return values;
}

/* str_all_alpha()
 * ---------------
 * This function validates a guess, making sure it is a valid word
 *
 * guess: The current guess that we validate that all characters are letters
 *
 * Returns: boolean value true if word is valid, false otherwise
*/
bool str_all_alpha(char* guess) {
    for (int i = 0; i < strlen(guess) - 1; i++) {
        // every character must be a letter
        if (!isalpha(guess[i])) {
            return false;
        }
    }
    return true;
}

/* in_dictionary()
 * ---------------
 *
 * dictionary: The dictionary to be checked with
 *
 * word: The word to be checked
 *
 * Returns: Boolean true if in the dictionary, false otherwise
*/
bool in_dictionary(Dictionary dictionary, char* word) {
    for (int i = 0; i < dictionary.size; i++) {
        if (strcmp(dictionary.words[i], word) == 0) {
            return true;
        }
    }
    return false;
}

/* validate_guess()
 * ----------------
 * guess: current guess for a word to validate
 * game: the game containing the library and other values
 *
 * Returns: boolean true if a valid guess, false otherwise
*/
bool validate_guess(char* guess, GameVariables game, Dictionary guesses) {
    guess = str_to_upper(guess);
    char* contained;
    bool allAlpha;
    bool inDict = in_dictionary(game.dictionary, guess);
    bool alreadyGuessed = in_dictionary(guesses, guess);
    contained = strstr(guess, game.starterWord);
    allAlpha = str_all_alpha(guess);

    if (!allAlpha) { // Must be only letters
        printf("Guesses must contain only letters - try again.\n");
        return false;
    }
    if (contained == NULL) { // Must contain starter word
        printf("Guesses must contain the starter word - try again.\n");
        return false;
    }
    if (strcmp(guess, game.starterWord) == 0) { // Cannot be the starter word
        printf("Guesses can't be the starter word - try again.\n");
        return false;
    }
    if (!inDict) { // Must be in the dictionary
        printf("Guess not found in dictionary - try again.\n");
        return false;
    }
    if (alreadyGuessed) { // Cannot use already guessed word
        printf("You've already guessed that word - try again.\n");
        return false;
    }
    return true;
}

/* get_guesses()
 * -------------
 * This function retrieves the guess from the user and validates if it is a 
 * usable word
 *
 * game: The game variables being used for this instance of play
 *
 * dict: The dictionary to make sure word is within
 *
 * Returns: Boolean, if the guessing steps are finished
*/ 
Dictionary get_guesses(GameVariables game) {
    char buffer[MAX_LINE_LENGTH];
    Dictionary guesses;

    guesses.isInitialised = true;
    guesses.maxLength = -1;
    guesses.size = 0;
    guesses.words = malloc(sizeof(char*) * MAX_GUESSES);

    while (guesses.size < MAX_GUESSES) {
        bool validGuess;
        char* currentGuess;
        do {
            printf("Enter guess %d:\n", guesses.size + 1);
            currentGuess = fgets(buffer, MAX_LINE_LENGTH, stdin);
            // Check for guessing finished early with ctrl-d
            if (currentGuess == NULL) {
                return guesses;
            }
            int length = strlen(currentGuess);
            if (currentGuess[0] == NEWLINE) {
                printf("Guesses must contain the starter word - try again.\n");
                validGuess = false;
                continue;
            }
            if (currentGuess[length - 1] == NEWLINE) {
                currentGuess[length - 1] = NULLTERMINATOR;
            } 
            validGuess = validate_guess(currentGuess, game, guesses);
        } while (!validGuess || !currentGuess);
        guesses.words[guesses.size] = strdup(currentGuess);
        if ((int) strlen(guesses.words[guesses.size]) > guesses.maxLength) {
            guesses.maxLength = (int) strlen(guesses.words[guesses.size]);
        }
        guesses.size++;
    }
    return guesses;
}

