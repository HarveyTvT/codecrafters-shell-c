#include <alloca.h>
#include <dirent.h>
#include <linux/limits.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static const char *builtin[] = {"exit", "echo", "type", "pwd", "cd"};

int search_dir(const char *path, const char *command, char *full_path) {
    DIR *dir = opendir(path);
    if (!dir) {
        return 0;
    }

    struct dirent *entry;
    struct stat st;
    int found = 0;

    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, command) == 0) {
            snprintf(full_path, PATH_MAX, "%s/%s", path, entry->d_name);

            if (stat(full_path, &st) == -1) {
                continue;
            }

            if (st.st_mode & S_IXUSR) {
                found = 1;
                break;
            }
        }
    }

    closedir(dir);
    return found;
}

int search_path(const char *command, char *full_path) {
    char *path = getenv("PATH");
    char *delim = ":";

    char *path_cpy = strdup(path);

    int found = 0;

    char *save;
    for (char *dir = strtok_r(path_cpy, delim, &save); dir;
         dir = strtok_r(NULL, delim, &save)) {
        if (search_dir(dir, command, full_path) == 1) {
            found = 1;
            break;
        }
    }

    free(path_cpy);

    return found;
}

// count_token, respect " and '
int count_token(const char *src) {
    if (src == NULL) {
        return 0;
    }

    int result = 0;
    char *src_cpy = strdup(src);
    char *save;
    char *delim = " ";
    char *token = strtok_r(src_cpy, delim, &save);
    while (token) {
        result++;
        token = strtok_r(NULL, delim, &save);
    }

    free(src_cpy);

    return result;
}

char **parse_command(const char *src, size_t *size) {
    *size = count_token(src);
    char **results = malloc(sizeof(char *) * (*size));
    size_t idx = 0;

    char *src_cpy = strdup(src);
    char *save;
    char *delim = " ";
    char *token = strtok_r(src_cpy, delim, &save);
    while (token) {
        results[idx++] = token;
        token = strtok_r(NULL, delim, &save);
    }

    return results;
}

// exec_command
int exec_command(const char *command) {
    int cmd_l = strcspn(command, " ");
    char cmd_name[cmd_l + 1];
    memset(cmd_name, 0, cmd_l + 1);
    strncpy(cmd_name, command, cmd_l);

    char full_path[PATH_MAX];
    int found = 0;
    memset(full_path, 0, PATH_MAX);
    if (search_path(cmd_name, full_path) == 0) {
        printf("%s: command not found\n", cmd_name);
        return 0;
    }

    FILE *fp = popen(command, "r");
    char buffer[128];
    memset(buffer, 0, 128);
    while (fgets(buffer, sizeof(buffer), fp) != NULL) {
        printf("%s", buffer);
    }
    pclose(fp);

    return 0;
}

int builtin_echo(const char *command) {
    int cmd_l = strcspn(command, " ");
    if (cmd_l + 1 < strlen(command)) {
        printf("%s\n", &command[cmd_l + 1]);
    } else {
        printf("\n");
    }
    return 0;
};

int builtin_type(const char *command) {
    int cmd_l = strcspn(command, " ");

    for (size_t i = 0; i < sizeof(builtin) / sizeof(char *); i++) {
        if (strcmp(&command[cmd_l + 1], builtin[i]) == 0) {
            printf("%s is a shell builtin\n", builtin[i]);
            return 1;
        }
    }

    char full_path[PATH_MAX];
    memset(full_path, 0, PATH_MAX);
    if (search_path(&command[cmd_l + 1], full_path) == 1) {
        printf("%s is %s\n", &command[cmd_l + 1], full_path);
        return 1;
    }

    printf("%s: not found\n", &command[cmd_l + 1]);
    return 0;
}

int builtin_pwd(const char *command) {
    char path[PATH_MAX];
    getcwd(path, PATH_MAX);
    printf("%s\n", path);
    return 0;
}

int builtin_cd(const char *command) {
    int cmd_l = strcspn(command, " ");
    const char *path = &command[cmd_l + 1];

    if (strcmp(path, "~") == 0) {
        path = getenv("HOME");
    }

    struct stat s;
    if (stat(path, &s) == -1) {
        printf("cd: %s: No such file or directory\n", &command[cmd_l + 1]);
        return 0;
    }

    return chdir(path);
};

// exit if return -1
int repit(const char *command) {
    int cmd_l = strcspn(command, " ");

    // built-in shell
    if (strncmp("exit", command, cmd_l) == 0) {
        return -1;
    }

    if (strncmp("echo", command, cmd_l) == 0) {
        return builtin_echo(command);
    }

    if (strncmp("type", command, cmd_l) == 0) {
        return builtin_type(command);
    }

    if (strncmp("pwd", command, cmd_l) == 0) {
        return builtin_pwd(command);
    }

    if (strncmp("cd", command, cmd_l) == 0) {
        return builtin_cd(command);
    }

    // run a program
    return exec_command(command);
}

int main(int argc, char *argv[]) {
    // Flush after every printf
    setbuf(stdout, NULL);
    char command[256];

    while (true) {
        printf("$ ");
        if (fgets(command, sizeof(command), stdin) != NULL) {
            // remove new line
            command[strcspn(command, "\n")] = '\0';

            if (repit(command) == -1) {
                return 0;
            }
        }
    }

    return 0;
}
