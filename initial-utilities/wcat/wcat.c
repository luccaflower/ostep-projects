#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BUF_LEN (512)

int main(int argc, char *argv[]) {
  if (argc < 2) {
    return EXIT_SUCCESS;
  }
  for (size_t i = 1; i < argc; i++) {
    FILE *file = fopen(argv[i], "r");
    if (!file) {
      puts("wcat: cannot open file");
      return EXIT_FAILURE;
    }

    char buf[BUF_LEN];
    while (fgets(buf, BUF_LEN, file)) {
      printf("%s", buf);
    }
  }

  return EXIT_SUCCESS;
}
