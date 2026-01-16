#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char CMD_EXIT[] = "exit";
static const char CMD_ECHO[] = "echo";
static const char CMD_TYPE[] = "type";
static const char* const CMDS[] = {CMD_EXIT, CMD_ECHO, CMD_TYPE};

int echo(const char* command) { return 0; }

int main(int argc, char* argv[]) {
  // Flush after every printf
  setbuf(stdout, NULL);
  char command[256];

  while (true) {
    printf("$ ");
    if (fgets(command, sizeof(command), stdin) != NULL) {
      // remove new line
      command[strcspn(command, "\n")] = '\0';

      if (strncmp(CMD_EXIT, command, strlen(CMD_EXIT)) == 0) {
        return 0;
      } else if (strncmp(CMD_ECHO, command, strlen(CMD_ECHO)) == 0) {
        int i = strlen(CMD_ECHO);
        while (command[i] == ' ' && i < strlen(command)) {
          i++;
        }
        printf("%s\n", &command[i]);
      } else if (strncmp(CMD_TYPE, command, strlen(CMD_TYPE)) == 0) {
        int i = strlen(CMD_TYPE);
        while (command[i] == ' ' && i < strlen(command)) {
          i++;
        }

        int found = 0;
        for (size_t j = 0; j < sizeof(CMDS) / sizeof(CMDS[0]); j++) {
          if (strncmp(CMDS[j], &command[i], strlen(CMDS[j])) == 0) {
            printf("%s is a shell builtin\n", &command[i]);
            found = 1;
            break;
          }
        }

        if (found == 0) {
          printf("%s: not found\n", &command[i]);
        }
      } else if (strlen(command) > 0) {
        printf("%s: command not found\n", command);
      }
    }
  }

  return 0;
}
