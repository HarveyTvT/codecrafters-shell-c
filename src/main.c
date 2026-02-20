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
#include <sys/ucontext.h>
#include <sys/wait.h>
#include <termios.h>
#include <unistd.h>

static const char *builtins[]  = {"exit", "echo", "type", "pwd", "cd", "history"};
static const char *completes[] = {"echo", "exit", "history"};
char              *histories[1024];
int                history_count = 0;

int                search_and_print_prefix_dir(const char *path, const char *command, char **results, size_t *size) {
    DIR *dir = opendir(path);
    if (!dir) {
        return 0;
    }

    struct dirent *entry;
    struct stat    st;
    int            found = 0;

    while ((entry = readdir(dir)) != NULL) {
        if (strncmp(entry->d_name, command, strlen(command)) == 0) {
            int is_dup = 0;
            for (size_t i = 0; i < (*size); i++) {
                if (strcmp(entry->d_name, results[i]) == 0) {
                    is_dup = 1;
                    break;
                }
            }

            if (is_dup == 0) {
                found          = 1;
                results[*size] = strdup(entry->d_name);
                (*size)++;
            }
        }
    }

    closedir(dir);
    return found;
}

int search_and_print_prefix_path(const char *command, char **results, size_t *size) {
    char *path     = getenv("PATH");
    char *delim    = ":";

    char *path_cpy = strdup(path);

    int   found    = 0;

    char *save;
    for (char *dir = strtok_r(path_cpy, delim, &save); dir;
         dir       = strtok_r(NULL, delim, &save)) {
        if (search_and_print_prefix_dir(dir, command, results, size) == 1) {
            found = 1;
        }
    }

    free(path_cpy);

    return found;
}

int search_dir(const char *path, const char *command, char *full_path) {
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
int exec_command(char **args, size_t arg_l, const char *command) {
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

char **split_pipe(char **args, size_t arg_l, size_t *sub_command_size) {
    int total = 0;
    for (int i = 0; i < arg_l; i++) {
        if (strcmp(args[i], "|") == 0) {
            total++;
        }
    }
    total += 1;
    *sub_command_size = total;

    char **results    = malloc(total * sizeof(char *));
    memset(results, 0, total);

    char buf[PATH_MAX];
    memset(buf, 0, PATH_MAX);

    int sub_command_idx = 0;
    for (int i = 0; i < arg_l; i++) {
        if (strcmp(args[i], "|") == 0) {
            results[sub_command_idx++] = strdup(buf);
            memset(buf, 0, PATH_MAX);
        } else {
            if (strlen(buf) > 0) {
                strcat(buf, " ");
            }
            strcat(buf, args[i]);
        }
    }

    if (strlen(buf) > 0) {
        results[sub_command_idx++] = strdup(buf);
    }

    return results;
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

    for (size_t i = 0; i < sizeof(builtins) / sizeof(char *); i++) {
        if (strcmp(args[1], builtins[i]) == 0) {
            printf("%s is a shell builtin\n", builtins[i]);
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

int builtin_history(char **args, const size_t arg_l) {
    int   n = history_count;
    char *endptr;

    if (arg_l == 2) {
        n = strtol(args[1], &endptr, 10);
    }

    if (n > history_count) {
        n = history_count;
    }

    for (int i = history_count - n; i < history_count; i++) {
        printf("%s", histories[i]);
    }

    return 0;
}

int repit_pipes(char **args, const size_t arg_l);

// exit if return -1
int repit(const char *command) {
    size_t total_l = 0;
    char **args    = parse_command(command, &total_l);
    size_t arg_l   = total_l;

    if (total_l == 0) {
        return -1;
    }

    // 管道走特殊逻辑
    for (size_t i = 0; i < total_l; i++) {
        if (strcmp(args[i], "|") == 0) {
            return repit_pipes(args, arg_l);
        }
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
    } else if (strcmp("history", args[0]) == 0) {
        ret = builtin_history(args, arg_l);
    } else {
        ret = exec_command(args, arg_l, command);
    }

    for (size_t i = 0; i < total_l; i++) {
        free(args[i]);
    }

    free(args);

    // restore stdout
    if (redirect_stdout == 1 || append_stdout == 1) {
        if (dup2(o_stdout, STDOUT_FILENO) == -1) {
            perror("dup2 restore failed");
            return -1;
        }
    }

    if (redirect_stderr == 1 || append_stderr == 1) {
        if (dup2(o_stderr, STDERR_FILENO) == -1) {
            perror("dup2 restore failed");
            return -1;
        }
    }

    return ret;
}

int compare_str(const void *foo, const void *bar) {
    return strcmp(*(const char **)foo, *(const char **)bar);
}

// longest comman prefix
int lcp(char **results, size_t l) {
    int r = 0;
    while (1) {
        for (int i = 0; i < l; i++) {
            if (r >= strlen(results[i]) || results[i][r] != results[0][r]) {
                return r;
            }
        }
        r++;
    }
}

int autocomplete(char *command, size_t *i) {
    char *candicates[128];
    memset(candicates, 0, 128);
    size_t size = 0;

    // builtin
    for (size_t j = 0; j < sizeof(completes) / sizeof(char *); j++) {
        if (strncmp(command, completes[j], *i) == 0) {
            candicates[size++] = strdup(completes[j]);
        }
    }
    // custom
    search_and_print_prefix_path(command, candicates, &size);

    if (size == 0) {
        // no match
        putchar('\x07');
    } else if (size == 1) {
        // single match
        char *target = candicates[0];

        while (*i < strlen(target)) {
            command[(*i)] = target[*i];
            putchar(command[*i]);
            (*i)++;
        }

        command[(*i)] = ' ';
        (*i)++;
        putchar(' ');
    } else if (size > 1) {
        // multi match
        int   lcp_r  = lcp(candicates, size);
        char *target = candicates[0];
        if (lcp_r > *i) {
            // partial completion
            while (*i < lcp_r) {
                command[(*i)] = target[*i];
                putchar(command[*i]);
                (*i)++;
            }
        } else {
            putchar('\x07');
            char ch = getchar();
            if (ch == '\t') {
                printf("\n");

                qsort(candicates, size, sizeof(char *), compare_str);
                for (size_t i = 0; i < size; i++) {
                    printf("%s  ", candicates[i]);
                }

                printf("\n");
                printf("$ %s", command);
            } else {
                putchar(ch);
                command[*i] = ch;
                (*i)++;
            }
        }
    }

    for (size_t n = 0; n < size; n++) {
        free(candicates[n]);
    }

    return size;
}

int repit_pipes(char **args, const size_t arg_l) {
    size_t n            = 0;
    char **sub_commands = split_pipe(args, arg_l, &n);

    int    pipes[n - 1][2];
    for (int i = 0; i < n - 1; i++) {
        if (pipe(pipes[i]) == -1) {
            perror("pipe");
            exit(-1);
        }
    }

    for (int i = 0; i < n; i++) {
        pid_t pid = fork();
        if (pid < 0) {
            perror("fork");
            exit(1);
        }

        if (pid == 0) {
            if (i > 0) {
                dup2(pipes[i - 1][0], STDIN_FILENO);
            }

            if (i < n - 1) {
                dup2(pipes[i][1], STDOUT_FILENO);
            }

            for (int j = 0; j < n - 1; j++) {
                close(pipes[j][0]);
                close(pipes[j][1]);
            }

            repit(sub_commands[i]);
            exit(0);
        }
    }

    for (int i = 0; i < n - 1; i++) {
        close(pipes[i][0]);
        close(pipes[i][1]);
    }

    for (int i = 0; i < n; i++) {
        wait(NULL);
        free(sub_commands[i]);
    }

    free(sub_commands);

    return 0;
}

int main(int argc, char *argv[]) {
    // Flush after every printf
    setbuf(stdout, NULL);
    char           command[256];

    struct termios oldt, newt;
    tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt;
    newt.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);

    char history_buf[1024];
    memset(histories, 0, 1024);
    while (true) {
        memset(command, 0, 256);
        printf("$ ");

        size_t i         = 0;
        int    flag_exit = 0;

        char   ch;

        while ((ch = getchar()) != '\n') {
            if (ch == '\t') {
                autocomplete(command, &i);
                continue;
            }
            if (ch == 127 || ch == 8) {
                if (i > 0) {
                    printf("\b \b");

                    command[i - 1] = '\0';
                    i--;
                }

                continue;
            }

            command[i++] = ch;
            putchar(ch);
        }
        putchar('\n');

        // record history
        snprintf(history_buf, 1024, "    %d %s\n", history_count + 1, command);
        histories[history_count++] = strdup(history_buf);
        memset(history_buf, 0, 1024);

        if (repit(command) == -1) {
            break;
        }
    }

    for (int i = 0; i < history_count; i++) {
        free(histories[i]);
    }

    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
    return 0;
}
