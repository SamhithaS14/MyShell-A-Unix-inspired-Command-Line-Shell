#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h> // Include <string.h> for strdup
#include "arraylist.h"

#ifndef DEBUG
#define DEBUG 0
#endif

char *my_strdup(const char *src) {
    if (src == NULL) {
        return NULL; // Return NULL if src is NULL
    }

    size_t len = strlen(src) + 1; // +1 for null terminator
    char *dst = malloc(len);
    if (dst == NULL) {
        // Handle memory allocation failure
        perror("Memory allocation failed");
        exit(EXIT_FAILURE);
    }
    return strcpy(dst, src);
}

void al_init(arraylist_t *L, unsigned size)
{
    L->data = malloc(size * sizeof(char*)); // Change the data type to char*
    if (L->data == NULL) {
        perror("Memory allocation failed");
        exit(EXIT_FAILURE);
    }
    L->length = 0;
    L->capacity = size;
}


void al_destroy(arraylist_t *L)
{
    // Free each stored string before freeing the array
    for (unsigned i = 0; i < L->length; i++) {
        free(L->data[i]);
    }
    free(L->data);
}

unsigned al_length(arraylist_t *L)
{
    return L->length;
}

void al_push(arraylist_t *L, char *item) // Change the parameter type to char*
{
    if (L->length == L->capacity) {
        L->capacity *= 2;  // Update the capacity correctly
        char **temp = realloc(L->data, L->capacity * sizeof(char*)); // Change the data type to char**
        if (!temp) { 
            // Handle memory allocation failure
            fprintf(stderr, "Out of memory!\n");
            exit(EXIT_FAILURE);
        }

        L->data = temp;
        if (DEBUG) printf("Resized array to %u\n", L->capacity);
    }

    // Duplicate the string and store its pointer
    L->data[L->length] = my_strdup(item);
    L->length++;
}

// returns 1 on success and writes popped item to dest
// returns 0 on failure (list empty)

int al_pop(arraylist_t *L, char **dest) // Change the parameter type to char
{
    if (L->length == 0) return 0;

    L->length--;
    *dest = L->data[L->length];

    return 1;
}

//Added function for removing based on specific index
void al_remove(arraylist_t *L, unsigned index) {
    if (index >= L->length) {
        fprintf(stderr, "Index out of bounds\n");
        exit(EXIT_FAILURE);
    }

    free(L->data[index]);

    for (unsigned i = index; i < L->length - 1; i++) {
        L->data[i] = L->data[i + 1];
    }

    L->length--;
}