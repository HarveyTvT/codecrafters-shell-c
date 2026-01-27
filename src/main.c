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

// return NULL if not exist
int get_cmd_path(char* name, char* result) {
  char* paths = getenv("PATH");

  char* delim = ":";
  char* path = strtok(paths, delim);

  while (path != NULL) {
    if (search_dir(path, name, result) == 1) {
      return 1;
    }
    path = strtok(NULL, delim);
  }

  return 0;
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
        if (get_cmd_path(&command[i], cmd_path) == 1) {
          printf("%s is %s\n", &command[i], cmd_path);
          continue;
        }

        if (strlen(command) > 0) {
          printf("%s: command not found\n", &command[i]);
        }
      }
    }
  }

  return 0;
}
