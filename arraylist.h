typedef struct {
    char **data; // Change the data type to char
    unsigned length;
    unsigned capacity;
} arraylist_t;

char *my_strdup(const char *str);

void al_init(arraylist_t *, unsigned);
void al_destroy(arraylist_t *);

unsigned al_length(arraylist_t *);

void al_push(arraylist_t *, char *); // Change the parameter type to char
int al_pop(arraylist_t *, char **); // Change the parameter type to char
void al_remove(arraylist_t *L, unsigned index);