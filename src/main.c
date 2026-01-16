#include <stdio.h>
#include <stdlib.h>
#include <string.h>

const char* CMD_EXIT = "exit";
const char* CMD_ECHO = "echo";

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

      // exit
      if (strncmp(CMD_EXIT, command, strlen(CMD_EXIT)) == 0) {
        return 0;
      } else if (strncmp(CMD_ECHO, command, strlen(CMD_ECHO)) == 0) {
        int i = strlen(CMD_ECHO);
        while (command[i] == ' ' && i < strlen(command)) {
          i++;
        }
        printf("%s\n", &command[i]);
      } else if (strlen(command) > 0) {
        printf("%s: command not found\n", command);
      }
    }
  }

  return 0;
}
