#include <stdio.h>

static int get_input() {
  int x;
  __asm volatile("addiu $2, $0, 2\n\t" // Prepare for syscall 2
        "syscall\n\t"
        "addu %0, $2, $0\n\t"
        : "=r"(x) : : "$2");
  return x; 
}

static void display_screen(int *x) {
  __asm volatile("addiu $2, $0, 1\n\t" // Prepare for syscall 1
        "addu $3, %0, $0\n\t"
        "syscall\n\t"
        : : "r"(x) : "$2", "$3", "memory");
}

int __start() {
  int screen[5];
  screen[0] = 1; // cols
  screen[1] = 1; // height;
  screen[2] = 0x1000;
  screen[3] = 0x00ff0000;
  screen[4] = 0;
  while(1) {
    screen[2] = 0x1000 + (get_input()<<4); 
    display_screen(screen);
  }
  return 0;
}

