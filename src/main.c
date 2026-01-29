#include <alloca.h>
#include <dirent.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

static const char CMD_EXIT[] = "exit";
static const char CMD_ECHO[] = "echo";
static const char CMD_TYPE[] = "type";
static const char* const CMDS[] = {CMD_EXIT, CMD_ECHO, CMD_TYPE};

int search_dir(char* path, char* command, char* result) {
  DIR* dir = opendir(path);
  if (!dir) {
    return 0;
  }

  struct dirent* entry;
  struct stat st;
  int found;

  while ((entry = readdir(dir)) != NULL) {
    if (strcmp(entry->d_name, command) == 0) {
      snprintf(result, 1024, "%s/%s", path, entry->d_name);

      if (stat(result, &st) == -1) {
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

int search_path(char* command, char* result) {
  char* path = getenv("PATH");
  char* delim = ":";

  char* path_cpy = malloc(strlen(path) + 1);
  strcpy(path_cpy, path);

  int found;

  char* save;
  for (char* dir = strtok_r(path_cpy, delim, &save); dir;
       dir = strtok_r(NULL, delim, &save)) {
    if (search_dir(dir, command, result) == 1) {
      found = 1;
      break;
    }
  }

  free(path_cpy);

  return found;
}

// count_token, respect " and '
int count_token(char* line) {
  if (line == NULL) {
    return 0;
  }

  int cnt = 0;
  int prev_single_quot = 0;
  int prev_double_quot = 0;  // is string
  int prev_tk = 0;           // preceding token

  int i = 0;  // index
  while (i < strlen(line)) {
    char ch = line[i++];
    if (ch == ' ') {
      if (prev_tk == 0) {
        continue;
      }

      if (prev_double_quot == 1 || prev_single_quot == 1) {
        continue;
      }

      prev_tk = 0;
      cnt++;
    } else if (ch == '"') {
      prev_double_quot ^= 1;
    } else if (ch == '\'') {
      prev_single_quot ^= 1;
    } else {
      if (prev_tk == 0) {
        prev_tk = 1;
      }
    }
  }

  if (prev_tk == 1) {
    cnt++;
  }

  if (prev_double_quot == 1) {
    printf("unfinished \"");
    return -1;
  }

  if (prev_single_quot == 1) {
    printf("unfinished \'");
    return -1;
  }

  return cnt;
}

int main(int argc, char* argv[]) {
  // Flush after every printf
  setbuf(stdout, NULL);
  char command[256];

  while (true) {
    printf("$ ");
    if (fgets(command, sizeof(command), stdin) != NULL) {
      // remove new line
      command[strcspn(command, "\n")] = '\0';

      // built-in shell
      if (strncmp(CMD_EXIT, command, strlen(CMD_EXIT)) == 0) {
        return 0;
      }

      if (strncmp(CMD_ECHO, command, strlen(CMD_ECHO)) == 0) {
        int i = strlen(CMD_ECHO);
        while (command[i] == ' ' && i < strlen(command)) {
          i++;
        }
        printf("%s\n", &command[i]);
        continue;
      }

      if (strncmp(CMD_TYPE, command, strlen(CMD_TYPE)) == 0) {
        int i = strlen(CMD_TYPE);
        while (command[i] == ' ' && i < strlen(command)) {
          i++;
        }

        // check if is builtin
        int is_builtin = 0;
        for (size_t j = 0; j < sizeof(CMDS) / sizeof(CMDS[0]); j++) {
          if (strncmp(CMDS[j], &command[i], strlen(CMDS[j])) == 0) {
            printf("%s is a shell builtin\n", &command[i]);
            is_builtin = 1;
            break;
          }
        }

        if (is_builtin == 1) {
          continue;
        }

        // check is is in PATH
        char cmd_path[1024];
        if (search_path(&command[i], cmd_path) == 1) {
          printf("%s is %s\n", &command[i], cmd_path);
          continue;
        }

        if (strlen(command) > 0) {
          printf("%s: not found\n", &command[i]);
          continue;
        }
      }

      printf("%s: command not found\n", command);
    }
  }

  return 0;
}
