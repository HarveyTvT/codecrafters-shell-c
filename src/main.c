#include <alloca.h>
#include <assert.h>
#include <dirent.h>
#include <fcntl.h>
#include <linux/limits.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static const char *builtin[] = {"exit", "echo", "type", "pwd", "cd"};

int                search_dir(const char *path, const char *command, char *full_path) {
    DIR *dir = opendir(path);
    if (!dir) {
        return 0;
    }

    struct dirent *entry;
    struct stat    st;
    int            found = 0;

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
    char *path     = getenv("PATH");
    char *delim    = ":";

    char *path_cpy = strdup(path);

    int   found    = 0;

    char *save;
    for (char *dir = strtok_r(path_cpy, delim, &save); dir;
         dir       = strtok_r(NULL, delim, &save)) {
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

    int cnt = 0;

    int i   = 0;
    while (i < strlen(src)) {
        char ch = src[i];
        switch (ch) {
        case '\"':
            while (++i < strlen(src)) {
                if (src[i] == '\\') {
                    i += 2;
                }
                if (i < strlen(src) && src[i] == '\"') {
                    break;
                }
            }
            i++;
            break;
        case '\'':
            while (++i < strlen(src)) {
                if (src[i] == '\'') {
                    break;
                }
            }
            i++;
            break;
        case '\\':
            i += 2;
            break;
        case ' ':
            if (i > 0) {
                cnt++;
            }
            while (++i < strlen(src)) {
                if (src[i] != ' ') {
                    break;
                }
            }
            break;
        default:
            i++;
        }
        if (i == strlen(src)) {
            cnt++;
        }
    }

    return cnt;
}

char **
parse_command(const char *src, size_t *size) {
    *size          = count_token(src);

    char **results = malloc(sizeof(char *) * (*size));
    size_t idx     = 0;

    char   buf[1024];
    memset(buf, 0, 1024);
    int l = 0;

    int i = 0;
    while (i < strlen(src)) {
        char ch = src[i];
        switch (ch) {
        case '\"':
            while (++i < strlen(src)) {
                if (src[i] == '\\') {
                    buf[l++] = src[i + 1];
                    i += 2;
                }
                if (i < strlen(src) && src[i] != '\"') {
                    buf[l++] = src[i];
                } else {
                    break;
                }
            }
            i++;
            break;
        case '\'':
            while (++i < strlen(src)) {
                if (src[i] != '\'') {
                    buf[l++] = src[i];
                } else {
                    break;
                }
            }
            i++;
            break;
        case '\\':
            if (i + 1 < strlen(src)) {
                if (src[i + 1] == '\n') {
                    buf[l++] = '\\';
                    buf[l++] = 'n';
                } else {
                    buf[l++] = src[i + 1];
                }
                i += 2;
            }
            break;
        case ' ':
            if (i > 0) {
                results[idx++] = strndup(buf, l);
                l              = 0;
            }
            while (++i < strlen(src)) {
                if (src[i] != ' ') {
                    break;
                }
            }
            break;
        default:
            buf[l++] = src[i];
            i++;
        }
        if (i == strlen(src)) {
            results[idx++] = strndup(buf, l);
            l              = 0;
        }
    }

    return results;
}

// exec_command
int exec_command(char **args, size_t art_l, const char *command) {
    char full_path[PATH_MAX];
    int  found = 0;
    memset(full_path, 0, PATH_MAX);
    if (search_path(args[0], full_path) == 0) {
        printf("%s: command not found\n", args[0]);
        return 0;
    }

    FILE *fp = popen(command, "r");
    char  buffer[128];
    memset(buffer, 0, 128);
    while (fgets(buffer, sizeof(buffer), fp) != NULL) {
        printf("%s", buffer);
    }
    pclose(fp);

    return 0;
}

int builtin_echo(char **args, size_t arg_l) {
    for (size_t i = 1; i < arg_l; i++) {
        if (i > 1) {
            printf(" ");
        }
        printf("%s", args[i]);
    }
    printf("\n");

    return 0;
};

int builtin_type(char **args, const size_t arg_l) {
    if (arg_l < 2) {
        return 0;
    }

    for (size_t i = 0; i < sizeof(builtin) / sizeof(char *); i++) {
        if (strcmp(args[1], builtin[i]) == 0) {
            printf("%s is a shell builtin\n", builtin[i]);
            return 1;
        }
    }

    char full_path[PATH_MAX];
    memset(full_path, 0, PATH_MAX);
    if (search_path(args[1], full_path) == 1) {
        printf("%s is %s\n", args[1], full_path);
        return 1;
    }

    printf("%s: not found\n", args[1]);
    return 0;
}

int builtin_pwd() {
    char path[PATH_MAX];
    getcwd(path, PATH_MAX);
    printf("%s\n", path);
    return 0;
}

int builtin_cd(char **args, const size_t arg_l) {
    if (arg_l < 2) {
        return 0;
    }

    const char *path = args[1];

    if (strcmp(path, "~") == 0) {
        path = getenv("HOME");
    }

    struct stat s;
    if (stat(path, &s) == -1) {
        printf("cd: %s: No such file or directory\n", args[1]);
        return 0;
    }

    return chdir(path);
};

// exit if return -1
int repit(const char *command) {
    size_t total_l = 0;
    char **args    = parse_command(command, &total_l);
    size_t arg_l   = total_l;

    if (total_l == 0) {
        return -1;
    }

    // 重定向
    int   redirect_stdout = 0;
    int   redirect_stderr = 0;
    int   append_stdout   = 0;
    int   append_stderr   = 0;
    char *redirect_target;
    for (size_t i = 0; i < total_l; i++) {
        if ((strcmp(args[i], ">") == 0 || strcmp(args[i], "1>") == 0) && i + 1 < total_l) {
            redirect_stdout = 1;
            redirect_target = args[i + 1];
            arg_l           = i;
            break;
        }

        if ((strcmp(args[i], "2>") == 0) && i + 1 < total_l) {
            redirect_stderr = 1;
            redirect_target = args[i + 1];
            arg_l           = i;
            break;
        }

        if ((strcmp(args[i], ">>") == 0 || (strcmp(args[i], "1>>")) == 0) && i + 1 < total_l) {
            append_stdout   = 1;
            redirect_target = args[i + 1];
            arg_l           = i;
            break;
        }

        if ((strcmp(args[i], "2>>") == 0) && i + 1 < total_l) {
            append_stderr   = 1;
            redirect_target = args[i + 1];
            arg_l           = i;
            break;
        }
    }

    int o_stdout;
    int o_stderr;
    int fd;
    if (redirect_stdout == 1 || append_stdout == 1) {
        if ((o_stdout = dup(STDOUT_FILENO)) == -1) {
            perror("dup failed");
            return -1;
        }

        int flag = O_WRONLY | O_CREAT;
        if (redirect_stdout == 1) {
            flag |= O_TRUNC;
        }
        if (append_stdout == 1) {
            flag |= O_APPEND;
        }

        fd = open(redirect_target, flag, 0644);
        if (fd == -1) {
            perror("open failed");
            return -1;
        }

        if (dup2(fd, STDOUT_FILENO) == -1) {
            perror("dup2 failed");
            return -1;
        }

        close(fd);
    }

    if (redirect_stderr == 1 || append_stderr == 1) {
        if ((o_stderr = dup(STDERR_FILENO)) == -1) {
            perror("dup failed");
            return -1;
        }

        int flag = O_WRONLY | O_CREAT;
        if (redirect_stderr == 1) {
            flag |= O_TRUNC;
        }
        if (append_stderr == 1) {
            flag |= O_APPEND;
        }

        fd = open(redirect_target, flag, 0644);
        if (fd == -1) {
            perror("open failed");
            return -1;
        }

        if (dup2(fd, STDERR_FILENO) == -1) {
            perror("dup2 failed");
            return -1;
        }

        close(fd);
    }

    int ret = 0;

    // built-in shell
    if (strcmp("exit", args[0]) == 0) {
        ret = -1;
    } else if (strcmp("echo", args[0]) == 0) {
        ret = builtin_echo(args, arg_l);
    } else if (strcmp("type", args[0]) == 0) {
        ret = builtin_type(args, arg_l);
    } else if (strcmp("pwd", args[0]) == 0) {
        ret = builtin_pwd();
    } else if (strcmp("cd", args[0]) == 0) {
        ret = builtin_cd(args, arg_l);
    } else {
        ret = exec_command(args, arg_l, command);
    }

    for (size_t i = 0; i < total_l; i++) {
        free(args[i]);
    }

    free(args);

    // restore stdout
    if (redirect_stdout == 1 || append_stdout == 1) {
        fflush(stdout);
        if (dup2(o_stdout, STDOUT_FILENO) == -1) {
            perror("dup2 restore failed");
            return -1;
        }
    }

    if (redirect_stderr == 1 || append_stderr == 1) {
        fflush(stderr);
        if (dup2(o_stderr, STDERR_FILENO) == -1) {
            perror("dup2 restore failed");
            return -1;
        }
    }

    return ret;
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
