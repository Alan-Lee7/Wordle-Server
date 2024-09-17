#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <signal.h>
#include <errno.h>
#include <ctype.h>

extern int total_guesses;
extern int total_wins;
extern int total_losses;
extern char **words;

#define MAX_GUESSES 6
#define WORD_LENGTH 5
#define BUFFER_SIZE 9

pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t active_games_mutex = PTHREAD_MUTEX_INITIALIZER;
int server_active = 1;
int word_length = 5;
int max_guesses = 5;
int active_games = 0;
char** all_words = NULL;
int size = 0;


//Convert to lower case
void to_lowercase(char *str) {
    char *p = str;
    while (*p) {
        *p = tolower((unsigned char)*p);
        p++;
    }
}

//Convert to upper case
void to_uppercase(char *str) {
    char *p = str;
    while (*p) {
        *p = toupper((unsigned char)*p);
        p++;
    }
}

void insert_word(const char *new_word) {
    char **new_array = realloc(all_words, (size + 1) * sizeof(char *));
    if (new_array == NULL) {
        perror("Failed to reallocate memory");
        exit(EXIT_FAILURE);
    }
    all_words = new_array;
    //Allocate memory for the new string and copy it into the array
    all_words[size] = strdup(new_word);
    //Update the size
    size++;
}

//Compares strings
int word_compare(const char *str1, const char *str2) {
    while (*str1 && *str2) {
        if (tolower((unsigned char)*str1) != tolower((unsigned char)*str2)) {
            return 0;
        }
        str1++;
        str2++;
    }
    return (*str1 == '\0' && *str2 == '\0');
}

//Compares the guess with the actual word and adjust result
void check_guess(const char *actual_word, const char *guess, char *result) {
    int i;
    int length = strlen(actual_word);

    //Frequency of letters in hidden word
    int *actual_freq = (int *)calloc(26, sizeof(int));
    //Frequency of letters in guess
    int *guess_freq = (int *)calloc(26, sizeof(int));

    if (actual_freq == NULL || guess_freq == NULL) {
        perror("calloc");
        exit(EXIT_FAILURE);
    }

    //Find matches between words and count frequencies
    for (i = 0; i < length; i++) {
        if (*(guess + i) == *(actual_word + i)) {
            //Correct position
            *(result + i) = toupper((unsigned char)*(guess + i)); 
        } else {
            *(result + i) = '-';
            //Letter frequencies incrementing
            (*(actual_freq + (*(actual_word + i) - 'a')))++;
            (*(guess_freq + (*(guess + i) - 'a')))++;
        }
    }

    //Clean up string with the letters guessed
    for (i = 0; i < length; i++) {
        //Check letters that aren't in hidden word
        if (*(result + i) == '-') {
            //Check if the letter in guess is present in actual_word
            //Subtract 'a' to find index of letters in an array
            if ((*(guess_freq + (*(guess + i) - 'a'))) > 0 && (*(actual_freq + (*(guess + i) - 'a'))) > 0) {
                 //Letters in wrong position
                *(result + i) = tolower((unsigned char)*(guess + i));
                //Decrement to reduce frequency of letter since it was used
                (*(actual_freq + (*(guess + i) - 'a')))--;
            }
        }
    }

    //Null-termination
    *(result + length) = '\0';

    //Free memory
    free(actual_freq);
    free(guess_freq);
}

//Check if word is in dictionary
int in_dictionary(const char *guess, char **words) {
    char *lower_guess = (char *)calloc(word_length + 1, sizeof(char));
    if (!lower_guess) {
        perror("calloc");
        return 0;
    }
    strncpy(lower_guess, guess, word_length);
    *(lower_guess + word_length) = '\0';
    to_lowercase(lower_guess);

    char **ptr = words;
    //Loop through words to see if the guess is in the dictionary
    while (*ptr) {
        char *lower_word = (char *)calloc(word_length + 1, sizeof(char));
        if (!lower_word) {
            perror("calloc");
            free(lower_guess);
            return 0;
        }
        strncpy(lower_word, *ptr, word_length);
        *(lower_word + word_length) = '\0';
        to_lowercase(lower_word);

        if (word_compare(lower_word, lower_guess)) {
            free(lower_word);
            free(lower_guess);
            return 1;
        }
        free(lower_word);
        ptr++;
    }
    free(lower_guess);
    return 0;
}

//Free once server ends
void free_everything() {
    printf(" SIGUSR1 rcvd; Wordle server shutting down...\n");
    printf("MAIN: valid guesses: %d\n", total_guesses);
    printf("MAIN: win/loss: %d/%d\n", total_wins, total_losses);
    for (int i = 0; i < size; i++) {
        to_uppercase(all_words[i]);      
        printf("MAIN: word #%d %s\n", i+1, all_words[i]);
        free(all_words[i]);
    }
    free(all_words);

    char **ptr = words;
    while (*ptr) {
        free(*ptr);
        ptr++;
    }
    free(words);
    pthread_mutex_destroy(&lock);
    pthread_mutex_destroy(&active_games_mutex);
    server_active = 0;
}

void *handle_client(void *arg) {
    //Socket
    int sock = *((int *)arg);
    //The hidden word
    char *hidden_word = *((char **)(arg + sizeof(int)));
    free(arg);

    pthread_mutex_lock(&active_games_mutex);
    active_games++;
    pthread_mutex_unlock(&active_games_mutex);

    char *buffer = (char *)calloc(BUFFER_SIZE, sizeof(char));
    char *response = (char *)calloc(8, sizeof(char));
    int guesses_remaining = MAX_GUESSES;

    if (!buffer || !response) {
        perror("calloc");
        close(sock);
        pthread_mutex_lock(&active_games_mutex);
        active_games--;
        pthread_mutex_unlock(&active_games_mutex);
        pthread_exit(NULL);
    }
    //Loop until guess is correct or until no more guesses
    while (guesses_remaining > 0) {
        printf("THREAD %lu: waiting for guess\n", pthread_self());
        memset(buffer, 0, BUFFER_SIZE);
        int n = read(sock, buffer, BUFFER_SIZE);
        
        if (n <= 0) {
            if (n == 0) {
                printf("THREAD %lu: gave up; closing TCP connection...\n", pthread_self());
                to_uppercase(hidden_word);
                printf("Thread %lu: game over; word was %s!\n", pthread_self(), hidden_word);
                total_losses++;
            } else {
                perror("read");
            }
            close(sock);
            pthread_mutex_lock(&active_games_mutex);
            active_games--;
            pthread_mutex_unlock(&active_games_mutex);
            pthread_exit(NULL);
        }

        //Null-termination
        *(buffer + WORD_LENGTH) = '\0';
        printf("THREAD %lu: rcvd guess: %s\n", pthread_self(), buffer);

        //Invalid guess
        if (strlen(buffer) != WORD_LENGTH) {
            *(response) = 'N';
            *(short *)(response + 1) = htons(guesses_remaining);
            //Set result as ?????
            memset(response + 3, '?', 5);
            if (guesses_remaining == 1) {
                printf("THREAD %lu: invalid guess; sending reply: ????? (%d guess left)\n", pthread_self(), guesses_remaining);
            } else {
                printf("THREAD %lu: invalid guess; sending reply: ????? (%d guesses left)\n", pthread_self(), guesses_remaining);
            }
            write(sock, response, 8);
            continue;
        }

        //Check guess
        char *result = (char *)calloc(WORD_LENGTH + 1, sizeof(char));
        if (!result) {
            perror("calloc");
            close(sock);
            pthread_mutex_lock(&active_games_mutex);
            active_games--;
            pthread_mutex_unlock(&active_games_mutex);
            pthread_exit(NULL);
        }

        //Check if guess is in dictionary
        if (!in_dictionary(buffer, words)) {
            if (guesses_remaining == 1) {
                printf("THREAD %lu: invalid guess; sending reply: ????? (%d guess left)\n", pthread_self(), guesses_remaining);
            } else {
                printf("THREAD %lu: invalid guess; sending reply: ????? (%d guesses left)\n", pthread_self(), guesses_remaining);
            }
            //Invalid guess
            *(response) = 'N';
            *(short *)(response + 1) = htons(guesses_remaining);
            //Set as ?????
            memset(response + 3, '?', 5);
            write(sock, response, 8); 
            continue;
        }
            
        check_guess(hidden_word, buffer, result);
        //If guess is correct
        if (strcmp(buffer, hidden_word) == 0) {
            guesses_remaining--;
            pthread_mutex_lock(&lock);
            total_wins++;
            pthread_mutex_unlock(&lock);
            *(response) = 'Y';
            *(short *)(response + 1) = htons(guesses_remaining);
            strncpy(response + 3, result, 5);
            if (guesses_remaining == 1) {
                printf("THREAD %lu: sending reply: %s (%d guess left)\n", pthread_self(), result, guesses_remaining);
            } else {
                printf("THREAD %lu: sending reply: %s (%d guesses left)\n", pthread_self(), result, guesses_remaining);
            }
            to_uppercase(hidden_word);
            printf("THREAD %lu: game over; word was %s!\n", pthread_self(), hidden_word);
            write(sock, response, 8);
            free(result);
            break;
        } else {
            //If guess is incorrect
            guesses_remaining--;
            pthread_mutex_lock(&lock);
            if (guesses_remaining == 0) {
                total_losses++;
            }
            pthread_mutex_unlock(&lock);
            *(response) = 'Y';
            *(short *)(response + 1) = htons(guesses_remaining);
            strncpy(response + 3, result, 5);
            if (guesses_remaining == 1) {
                printf("THREAD %lu: sending reply: %s (%d guess left)\n", pthread_self(), result, guesses_remaining);
            } else {
                printf("THREAD %lu: sending reply: %s (%d guesses left)\n", pthread_self(), result, guesses_remaining);
            }
            write(sock, response, 8);
            free(result);
        }
    }
    //Game over
    to_uppercase(hidden_word);
    if(guesses_remaining == 0) { 
        printf("THREAD %lu: game over; word was %s!\n", pthread_self(), hidden_word);
    }
    write(sock, response, 8);
    free(buffer);
    free(response);
    pthread_mutex_lock(&active_games_mutex);
    active_games--;
    pthread_mutex_unlock(&active_games_mutex);
    close(sock);
    pthread_exit(NULL);
}

//Signal handler
void handle_signal(int signal) {
    if (signal == SIGUSR1) {
        free_everything();
        exit(EXIT_SUCCESS);
    }
}

int wordle_server(int argc, char **argv) {
    if (argc != 5) {
        fprintf(stderr, "ERROR: Invalid argument(s)\nUSAGE: hw3.out <listener-port> <seed> <dictionary-filename> <num-words>\n");
        return EXIT_FAILURE;
    }

    int port = atoi(*(argv+1));
    int seed = atoi(*(argv+2));
    const char *dictionary = *(argv+3);
    int num_words = atoi(*(argv+4));

    if (port <= 0 || seed < 0 || num_words <= 0) {
        fprintf(stderr, "ERROR: Invalid argument(s)\nUSAGE: hw3.out <listener-port> <seed> <dictionary-filename> <num-words>\n");
        return EXIT_FAILURE;
    }

    FILE *file = fopen(dictionary, "r");
    if (!file) {
        perror("fopen");
        return EXIT_FAILURE;
    }

    srand(seed);
    words = (char **)calloc(num_words + 1, sizeof(char *));
    if (!words) {
        perror("calloc");
        fclose(file);
        return EXIT_FAILURE;
    }

    printf("MAIN: opened %s (%d words)\n", dictionary, num_words);

    char *buffer = (char *)calloc(word_length + 2, sizeof(char));
    //Dictionary Parsing
    int index = 0;
    while (fgets(buffer, word_length + 2, file)) {
        char *newline_pos = buffer + strcspn(buffer, "\n");
        *newline_pos = '\0';
        *(words + index) = strdup(buffer);
        if (*(words + index) == NULL) {
            perror("ERROR: Memory allocation failed");
            free(buffer);
            fclose(file);
            return EXIT_FAILURE;
        }
        index++;
    }
    free(buffer);
    *(words+num_words) = NULL;
    fclose(file);

    printf("MAIN: seeded pseudo-random number generator with %d\n", seed);

    //Server set up
    int listener_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (listener_socket == -1) {
        perror("socket");
        return EXIT_FAILURE;
    }

    struct sockaddr_in server_addr = {0};
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);

    if (bind(listener_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
        perror("bind");
        close(listener_socket);
        return EXIT_FAILURE;
    }

    if (listen(listener_socket, 10) == -1) {
        perror("listen");
        close(listener_socket);
        return EXIT_FAILURE;
    }

    signal(SIGUSR1, handle_signal);
    printf("MAIN: Wordle server listening on port {%d}\n", port);

    //Server is up until noted otherwise
    while (server_active) {
        int client_socket = accept(listener_socket, NULL, NULL);
        if (client_socket == -1) {
            if (errno == EINTR) continue;
            perror("accept");
            continue;
        }

        printf("MAIN: rcvd incoming connection request\n");

        pthread_t thread;
        int word_index = rand() % num_words;
        char *hidden_word = strdup(*(words+word_index));
        insert_word(hidden_word);
        if (!hidden_word) {
            perror("strdup");
            close(client_socket);
            continue;
        }

        //Create argument array
        size_t arg_size = sizeof(int) + sizeof(char *);
        void *arg = calloc(1, arg_size);
        if (!arg) {
            perror("calloc");
            close(client_socket);
            free(hidden_word);
            continue;
        }
        *((int*)arg) = client_socket;
        *((char**)(arg + sizeof(int))) = hidden_word;

        if (pthread_create(&thread, NULL, handle_client, arg) != 0) {
            perror("pthread_create");
            close(client_socket);
            free(arg);
            free(hidden_word);
            continue;
        }
        pthread_detach(thread);
    }
    close(listener_socket);
    free_everything();
    printf("hello");
    return EXIT_SUCCESS;
}
