#include <stdio.h>
#include <stdlib.h>

int main(int argc, char* argv[]) {
  // Flush after every printf
  setbuf(stdout, NULL);

  // TODO: Uncomment the code below to pass the first stage
  printf("$ ");
  if (argc > 1) {
    printf("%s: command not found\n", argv[1]);
  }

  return 0;
}
