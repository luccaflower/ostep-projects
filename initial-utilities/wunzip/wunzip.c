#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char *argv[]) {
  if (argc < 2) {
    puts("wunzip: file1 [file2 ...]");
    return EXIT_FAILURE;
  }
  for (size_t i = 1; i < argc; i++) {
    FILE *file = fopen(argv[i], "r");
    if (!file) {
      puts("unable to open file");
      return EXIT_FAILURE;
    }
    int count;
    while (fread(&count, 4, 1, file) > 0) {
      char c = fgetc(file);
      for (size_t i = 0; i < count; i++) {
        putc(c, stdout);
      }
    }
    fclose(file);
  }
  return EXIT_SUCCESS;
}
