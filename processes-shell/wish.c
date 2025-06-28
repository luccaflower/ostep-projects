#include <errno.h>
#include <signal.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#define MAXARGS (128)
#define PATHC (2)
#define MAXBUF (512)

extern char **environ;
static const char *path[] = {"/bin/", "/usr/bin/"};

void unix_error(char *message, int err) {
  fprintf(stderr, "%s: %s\n", message, strerror(err));
  exit(1);
}

void cmd_error(void) { fputs("An error has occurred\n", stderr); }

void Execve(char *cmd, char **argv) {
  if (execve(cmd, argv, environ)) {
    unix_error(cmd, errno);
  }
}

pid_t Fork() {
  pid_t pid;
  if ((pid = fork()) < 0) {
    unix_error("fork", errno);
  }
  return pid;
}

FILE *Fopen(char *path, char *mode) {
  FILE *fd;
  if (!(fd = fopen(path, mode))) {
    unix_error("fopen", errno);
  }
  return fd;
}

bool builtin_cmd(int argc, char **argv) {
  char *cmd = argv[0];
  if (!strcmp(cmd, "exit")) {
    if (argc > 1) {
      cmd_error();
    } else {
      exit(0);
    }
    return true;
  } else if (!strcmp(cmd, "cd")) {
    if (argc != 2) {
      cmd_error();
    } else {
      int err;
      if ((err = chdir(argv[1]))) {
        cmd_error();
      }
    }
    return true;
  }
  return false;
}

void run_exec(char **argv) {
  if (Fork()) {
    int status;
    wait(&status);
  } else {
    char cmd[MAXBUF];
    for (size_t i = 0; i < PATHC; i++) {
      strcpy(cmd, path[i]);
      size_t prefix_len = strlen(cmd);
      strncat(cmd, argv[0], MAXBUF - prefix_len);
      if (!access(cmd, X_OK)) {
        Execve(cmd, argv);
      }
    }
    cmd_error();
  }
}

void eval(char *line) {
  int argc;
  char *token;
  char *argv[MAXARGS];
  for (argc = 0; (token = strsep(&line, " ")); argc++) {
    argv[argc] = token;
  }
  if (argv[0] == NULL) {
    return;
  }
  argv[argc] = NULL;
  if (!builtin_cmd(argc, argv)) {
    run_exec(argv);
  }
}

int main(int argc, char *argv[]) {
  char *line = NULL;
  size_t buf_size;
  ssize_t n_read;
  bool interactive;

  FILE *input;
  if (argc >= 2) {
    interactive = false;
    input = Fopen(argv[1], "r");
  } else {
    interactive = true;
    input = stdin;
  }

  while (1) {
    if (interactive) {
      printf("wish> ");
    }
    n_read = getline(&line, &buf_size, input);
    if (n_read == -1) {
      if (!feof(input)) {
        unix_error("getline", errno);
      } else {
        exit(0);
      }
    }
    line[n_read - 1] = '\0'; // trim the newline
    eval(line);
  }
  // unreachable
  return EXIT_SUCCESS;
}
