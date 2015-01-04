#include <stdio.h>

static int get_input() {
  int x;
  __asm volatile("addiu $2, $0, 2\n\t" // Prepare for syscall 2
        "not $3, $0\n\t" // set maximum timeout
        "syscall\n\t"
        "addu %0, $2, $0\n\t"
        : "=r"(x) : : "$2", "$3");
  return x; 
}

static int get_input_with_timeout(unsigned int timeout) {
  int x;
  __asm volatile("addiu $2, $0, 2\n\t" // Prepare for syscall 2
        "move $3, %1\n\t" // set maximum timeout
        "syscall\n\t"
        "addu %0, $2, $0\n\t"
        : "=r"(x) : "r"(timeout) : "$2", "$3");
  return x; 
}

static void display_screen(int *x) {
  __asm volatile("addiu $2, $0, 1\n\t" // Prepare for syscall 1
        "addu $3, %0, $0\n\t"
        "syscall\n\t"
        : : "r"(x) : "$2", "$3", "memory");
}

static void get_cursor_coords(int *x, int *y) {
  int xval, yval;
  __asm volatile("addiu $2, $0, 3\n\t" // Prepare for syscall 3
        "syscall\n\t"
        "addu %0, $2, $0\n\t"
        "addu %1, $3, $0\n\t"
        : "=r"(xval), "=r"(yval): : "$2", "$3");
  *x = xval;
  *y = yval;
}

static void get_screen_size(int *width, int *height) {
  int widthval, heightval;
  __asm volatile("addiu $2, $0, 4\n\t" // Prepare for syscall 4
        "syscall\n\t"
        "addu %0, $2, $0\n\t"
        "addu %1, $3, $0\n\t"
        : "=r"(widthval), "=r"(heightval): : "$2", "$3");
  *width = widthval;
  *height = heightval;
}

static unsigned int rand_u32() {
  unsigned int res;
  __asm volatile("addiu $2, $0, 5\n\t" // Prepare for syscall 5
        "syscall\n\t"
        "addu %0, $2, $0\n\t"
        : "=r"(res) : : "$2", "$3");
  return res;
}

typedef struct glyph {
  int ch;
  int fg;
  int bg;
} glyph;

#define SCREEN_WIDTH 20
#define SCREEN_HEIGHT 10
#define SCREEN_CELLS (SCREEN_WIDTH*SCREEN_HEIGHT)

typedef struct screen {
  int width;
  int height;
  glyph gs[SCREEN_CELLS];
} screen;


int __start() {
  screen s;
  int i;
  s.width = SCREEN_WIDTH;
  s.height = SCREEN_HEIGHT;
  for ( i=0; i<SCREEN_CELLS; i++ ){
    s.gs[i].ch = 10+(i%5);
    s.gs[i].fg = 0x00ff00;
    s.gs[i].bg = 0;
  }
  
  while(1) {
    display_screen(&s.width);
    int event = get_input_with_timeout(200);
    
    s.gs[0].ch = 0x1000 + (event<<4); 
    s.gs[1].ch = 10 + (rand_u32()%10);
    if (event==2) {
      int x, y;
      get_cursor_coords(&x, &y);
      if (x>=0 && x<SCREEN_WIDTH&& y>=0 && y<SCREEN_HEIGHT) {
        s.gs[x + y*SCREEN_WIDTH].bg = (s.gs[x + y*SCREEN_WIDTH].bg+0x40) & 0xff;
      }
    }
  }
  return 0;
}

