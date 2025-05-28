#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void grep(FILE *file, char *term) {
  char *line;
  size_t size = 0;
  while (getline(&line, &size, file) >= 0) {
    if (strstr(line, term)) {
      printf("%s", line);
    }
  }
}
int main(int argc, char *argv[]) {
  if (argc < 2) {
    puts("wgrep: searchterm [file ...]");
    return EXIT_FAILURE;
  }
  FILE *file;
  if (argc < 3) {
    grep(stdin, argv[1]);
    return EXIT_SUCCESS;
  } else {
    for (size_t i = 2; i < argc; i++) {
      file = fopen(argv[i], "r");
      if (!file) {
        puts("wgrep: cannot open file");
        return EXIT_FAILURE;
      }
      grep(file, argv[1]);
    }
    return EXIT_SUCCESS;
  }
}
