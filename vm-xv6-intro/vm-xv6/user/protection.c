#include "types.h"
#include "user.h"

void print_pointer(char *p) {
  printf(1, "The first four bytes at the supplied address are:\n");
  printf(1, "(ignore leading FFFFFF)\n");
  printf(1, "%x ", (uint)*p);
  printf(1, "%x ", (uint) * (p + 1));
  printf(1, "%x ", (uint) * (p + 2));
  printf(1, "%x\n", (uint) * (p + 3));
}
int main(int argc, char *argv[])
{
  char *p = (char *)main;
  print_pointer(p);
  
  printf(1, "Attempt to modify main function\n");
  uint *ip = (uint *)p;
  *ip = 0x1000;
  print_pointer(p);
  printf(1, "Protecting page with main fucntion\n");
  mprotect(p, 1);
  if (fork() == 0) {
    printf(1, "Attempt to modify main function from child process\n");
    *ip = 0x2000;
    print_pointer(p);
    printf(1, "ERROR: allowed to modify protected page\n");
    exit();
  } else {
    printf(1, "Unprotecting page with main function\n");
    munprotect(p, 1);
    printf(1, "Attempt to modify main function\n");
    *ip = 0x2000;
    print_pointer(p);
    wait();
    exit();
  }
}

