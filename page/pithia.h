
#define INPUT_EVENT_RESIZE 1
#define INPUT_EVENT_LEFT_MOUSE_DOWN 2
#define INPUT_EVENT_LEFT_MOUSE_UP 3
#define INPUT_EVENT_KEYDOWN 4

typedef struct input_event{
  uint32_t type;
  uint32_t param;
} input_event;


typedef struct glyph {
  int ch;
  int fg;
  int bg;
} glyph;

typedef struct screen {
  int width;
  int height;
  glyph gs[1];
} screen;

#define SCREEN_U32S_FOR(n) (2 + 3*(n))

static input_event get_input() {
  input_event evt;
  __asm volatile("addiu $2, $0, 2\n\t" // Prepare for syscall 2
        "not $3, $0\n\t" // set maximum timeout
        "syscall\n\t"
        "addu %0, $2, $0\n\t"
        "addu %1, $3, $0\n\t"
        : "=r"(evt.type), "=r"(evt.param) : : "$2", "$3");
  return evt; 
}

static input_event get_input_with_timeout(uint32_t timeout) {
  input_event evt;
  __asm volatile("addiu $2, $0, 2\n\t" // Prepare for syscall 2
        "move $3, %2\n\t" // set maximum timeout
        "syscall\n\t"
        "addu %0, $2, $0\n\t"
        "addu %1, $3, $0\n\t"
        : "=r"(evt.type), "=r"(evt.param) : "r"(timeout) : "$2", "$3");
  return evt; 
}

static void display_screen(screen *x) {
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

static void get_screen_size(uint32_t *width, uint32_t *height) {
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

static void navigate_to(const uint32_t *url, uint32_t glyph_count) {
  __asm volatile("addiu $2, $0, 7\n\t" // Prepare for syscall 6
        "move $3, %0\n\t" // set maximum timeout
        "move $4, %1\n\t" // set maximum timeout
        "syscall\n\t"
        : : "r"(url), "r"(glyph_count) : "$2", "$3", "$4");
}

static unsigned int current_milliseconds() {
  unsigned int res;
  __asm volatile("addiu $2, $0, 8\n\t" // Prepare for syscall 5
        "syscall\n\t"
        "addu %0, $2, $0\n\t"
        : "=r"(res) : : "$2", "$3");
  return res;
}


