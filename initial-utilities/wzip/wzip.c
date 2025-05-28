#include <stdio.h>
#include <stdlib.h>

#define BUF_LEN (512)
int main(int argc, char *argv[]) {
  if (argc < 2) {
    puts("wzip: file1 [file2 ...]");
    return EXIT_FAILURE;
  }
  int count = 0;
  char current = '\0';
  for (size_t i = 1; i < argc; i++) {
    FILE *file = fopen(argv[i], "r");
    if (!file) {
      puts("wzip: cannot open file");
      return EXIT_FAILURE;
    }
    char buf[BUF_LEN];
    while (fgets(buf, BUF_LEN, file)) {
      for (size_t i = 0; buf[i]; i++) {
        char c = buf[i];
        if (c != current) {
          if (count > 0) {
            fwrite(&count, 4, 1, stdout);
            fwrite(&current, 1, 1, stdout);
          }
          current = c;
          count = 1;
        } else {
          count++;
        }
      }
    }
    fclose(file);
  }
  if (count > 0) {
    fwrite(&count, 4, 1, stdout);
    fwrite(&current, 1, 1, stdout);
  }
  return EXIT_SUCCESS;
}
