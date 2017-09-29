#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BUF_SIZE 8192

struct vector {
    size_t size;
    size_t allocated;
    size_t type_size;
    void **data;
};

static struct vector *init_vector(size_t type_size) {
    struct vector *vector = malloc(sizeof(struct vector));
    vector->size = 0;
    vector->allocated = 0;
    vector->type_size = type_size;
    vector->data = NULL;
    return vector;
}

static void put(struct vector *vector, void *obj, size_t index) {
    size_t old_size = vector->size;
    vector->size = (index > vector->size)? index : vector->size;

    if (!vector->data || vector->size >= vector->allocated) {
        vector->data = realloc(vector->data, vector->type_size*(vector->size+1)); 
        vector->allocated = vector->size + 1;
    }
    for (int i = old_size-1; i > (int) index; --i) {
        vector->data[i + 1] = vector->data[i];
    }

    vector->size++;
    vector->data[index] = obj;
}

static void add(struct vector *vector, void *obj) {
    put(vector, obj, vector->size);
}

static inline void *get(struct vector *vector, size_t index) {
    return vector->data[index];
}

static inline void sort(struct vector *vector) {
    for (int i = 0; i < vector->size; i++) {
        char **min = (char **) &vector->data[i];
        for (int j = i; j < vector->size; j++) {
            if (strcmp(*min,vector->data[j]) > 0) {
                min = (char **) &vector->data[j];
            }
        }
        char *tmp = vector->data[i];
        vector->data[i] = *min;
        *min = tmp;
    }
}

int my_strcmp(const void *a, const void *b) {
    return strcmp((char *) a, (char *) b);
}

int main(int argc, char **argv) {
    if (argc < 3) {
        fprintf(stderr, "Usage: %s CONFIG FILES...\n", argv[0]);
        return 1;
    }
    struct vector *import_order = init_vector(sizeof(char *));
    FILE *config_file = fopen(argv[1], "r");
    char buf[BUF_SIZE];

    if (NULL == config_file) {
        fprintf(stderr, "Couldn't open config file %s\n", argv[1]);
        return 1;
    }

    while (fgets(buf, BUF_SIZE, config_file)) {
        if ('#' == *buf) {
            continue;
        }
        char *split = strchr(buf, '=');
        *split = '\0';
        int index = atoi(buf);
        char *package = split + 1;
        char *new_element = malloc(strlen(package) + 1);
        strcpy(new_element, package);
        new_element[strlen(new_element) - 1] = '\0';
        if (strlen(new_element) > 0 && new_element[strlen(new_element) - 1] == '*') {
            new_element[strlen(new_element) - 1] = '\0';
        }
        put(import_order, new_element, index);
    }
    fclose(config_file);

    for (int i = 2; i < argc; i++) {
        FILE *file = fopen(argv[i], "r");
        if (NULL == file) {
            fprintf(stderr, "Couldn't read file %s; skipping\n", argv[i]);
            continue;
        }
        struct vector **imports = malloc(sizeof(struct vector *) * import_order->size);
        for (size_t j = 0; j < import_order->size; j++) {
            imports[j] = init_vector(sizeof(char *));
        }

        while (fgets(buf, BUF_SIZE, file)) {
            if (!strncmp("import", buf, strlen("import"))) {
                char *import = buf + strlen("import") + 1;
                char *copy = malloc(strlen(import) + 1);
                size_t match_size = 0;
                size_t match;
                strcpy(copy, import);
                for (size_t j = 0; j < import_order->size; j++) {
                    char *import_matcher = (char *) get(import_order, j);
                    if (strlen(import_matcher) >= match_size && !strncmp(import_matcher, import, strlen(import_matcher))) {
                        match = j;
                        match_size = strlen(import_matcher);
                    }
                }
                add(imports[match], copy);
            }
            else if (!strncmp("public", buf, strlen("public"))) {
                break;
            }
        }
        fclose(file);

        for (size_t j = 0; j < import_order->size; ++j) {
            sort(imports[j]);
        }

        char *tmpfile_name = tmpnam(NULL);
        FILE *tmpfile = fopen(tmpfile_name, "w");
        file = fopen(argv[i], "r");
        int state = 0;
        while (fgets(buf, BUF_SIZE, file)) {
            switch (state) {
                case 0:
                    if (!strncmp("import",buf,strlen("import"))) {
                        for (size_t j = 0; j < import_order->size; j++) {
                            for (size_t k = 0; k < imports[j]->size; k++) {
                                 fputs("import ", tmpfile);
                                 fputs(get(imports[j], k),tmpfile);
                            }
                            if (imports[j]->size > 0) {
                                fputs("\n", tmpfile);
                            }
                        }
                        state = 1;
                    }
                    else {
                        fputs(buf, tmpfile);
                    }
                    break;

                case 1:
                    if (*buf == '@' || *buf == '/' || !strncmp("public",buf,strlen("public"))) {
                       fputs(buf, tmpfile);
                       state = 2;
                    }
                    break;

                case 2:
                    fputs(buf, tmpfile);
                    break;
            }
        }
        fclose(tmpfile);
        fclose(file);
        char command[BUF_SIZE];
        strcpy(command, "/usr/bin/mv ");
        strcat(command, tmpfile_name);
        strcat(command, " '");
        strcat(command, argv[i]);
        strcat(command, "'");
        system(command);
    }
    return 0;
}
