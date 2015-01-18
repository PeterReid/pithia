#include <stdio.h>
#include <stdint.h>

#define INPUT_EVENT_RESIZE 1
#define INPUT_EVENT_KEYDOWN 4
typedef struct input_event{
  uint32_t type;
  uint32_t param;
} input_event;

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

static input_event get_input_with_timeout(unsigned int timeout) {
  input_event evt;
  __asm volatile("addiu $2, $0, 2\n\t" // Prepare for syscall 2
        "move $3, %1\n\t" // set maximum timeout
        "syscall\n\t"
        "addu %0, $2, $0\n\t"
        : "=r"(evt.type), "=r"(evt.param) : "r"(timeout) : "$2", "$3");
  return evt; 
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

typedef struct glyph {
  int ch;
  int fg;
  int bg;
} glyph;

#define SCREEN_WIDTH 180
#define SCREEN_HEIGHT 80
#define SCREEN_CELLS (SCREEN_WIDTH*SCREEN_HEIGHT)

typedef struct screen {
  int width;
  int height;
  glyph gs[SCREEN_CELLS];
} screen;

uint8_t file_contents[] = "\0" // leading 0 stops lookback from overrunning
  "Four score and seven years ago our fathers brought forth on this continent,"
  " a new nation, conceived in Liberty, and dedicated to the proposition that "
  "all men are created equal.\n"
  "\n"
  "Now we are engaged in a great civil war, testing whether that nation, or an"
  "y nation so conceived and so dedicated, can long endure. We are met on a gr"
  "eat battle-field of that war. We have come to dedicate a portion of that fi"
  "eld, as a final resting place for those who here gave their lives that that"
  " nation might live. It is altogether fitting and proper that we should do t"
  "his.\n"
  "\n"
  "But, in a larger sense, we can not dedicate -- we can not consecrate -- we "
  "can not hallow -- this ground. The brave men, living and dead, who struggle"
  "d here, have consecrated it, far above our poor power to add or detract. Th"
  "e world will little note, nor long remember what we say here, but it can ne"
  "ver forget what they did here. It is for us the living, rather, to be dedic"
  "ated here to the unfinished work which they who fought here have thus far s"
  "o nobly advanced. It is rather for us to be here dedicated to the great tas"
  "k remaining before us -- that from these honored dead we take increased dev"
  "otion to that cause for which they gave the last full measure of devotion -"
  "- that we here highly resolve that these dead shall not have died in vain -"
  "- that this nation, under God, shall have a new birth of freedom -- and tha"
  "t government of the people, by the people, for the people, shall not perish"
  " from the earth.\n"
  "\n"
  "Abraham Lincoln\n"
  "November 19, 1863"
  ;

uint32_t unicode_to_glyphcode(uint32_t unicode) {
  if( unicode>='0' && unicode<='9' ){
    return unicode - (uint32_t)'0' + 10;
  }else if( unicode>='a' && unicode<='z' ){
    return 0x1000 + ((unicode-'a')<<4);
  }else if( unicode>='A' && unicode<='Z' ){
    return 0x3000 + ((unicode-'A')<<4);
  }
  switch( unicode ){
    case ' ': return 0;
    case '_': return 1;
    case '-': return 2;
    case '.': return 3;
    case ',': return 4;
    case '/': return 5;
    case'\\': return 6;
    case ':': return 7;
    case ';': return 8;
    case '~': return 9;
  }
  return 999;
}

const uint8_t *first_utf8(const uint8_t *text, uint32_t *dest) {
  // TODO: Handle multibyte characters
  *dest = text[0];
  return text + 1;
}

void maybe_put_glyphcode(screen *dest, uint32_t x, uint32_t y, uint32_t glyphcode){
  if( dest && x>=0 && y>=0 && x<dest->width && y<dest->height ){
    dest->gs[x + y*dest->width].ch = glyphcode;
  }
}
void maybe_put_glyph(screen *dest, uint32_t x, uint32_t y, glyph g){
  if( dest && x>=0 && y>=0 && x<dest->width && y<dest->height ){
    dest->gs[x + y*dest->width] = g;
  }
}
const uint8_t *draw_text_wrappingly(
  screen *dest, 
  const uint8_t *text,
  uint32_t left, uint32_t top, uint32_t width, uint32_t height
){
  uint32_t x, y;
  uint32_t right, bottom; // exclusive
  uint32_t unicode;
  const uint8_t *last_after_whitespace;
  uint32_t last_after_whitespace_x = 0;
  uint32_t glyphcode;
  
  if( *text==0 ) return text; // end of document
  
  right = left + width;
  bottom = top + height;
  for( y=top; y<bottom; y++ ){
    if( text[-1]!='\r' && text[-1]!='\n' ){ // if we're on a word-wrapped line
      // trim a single leading space, since the line break looks (kind of) like one.
      const uint8_t *maybe_next = first_utf8(text, &unicode);
      if( unicode==' ' ){
        text = maybe_next;
      }
    }
    last_after_whitespace = 0;
    for( x=left; x<right; x++ ){
      text = first_utf8(text, &unicode);
      if( unicode==0 ){
        // blank out the remaining area
        for( ; x<right; x++ ){
          maybe_put_glyphcode(dest, x, y, 0);
        }
        y++;
        for(; y<bottom; y++ ){
          for( x=left; x<right; x++ ){
            maybe_put_glyphcode(dest, x, y, 0);
          }
        }
        return text;
      }
      if( unicode==13 ){ // carriage return
        const uint8_t *after_crlf = first_utf8(text, &unicode);
        if( unicode==10 ){ // crlf detected
          text = after_crlf;
        }else{
          unicode = 10; // treat a lone carriage return like a line feed
        }
      }
      if( unicode==10 ){ //line feed
        while( x<right ){
          maybe_put_glyphcode(dest, x, y, 0);
          x++;
        }
        last_after_whitespace = 0;
        break;
      }
      glyphcode = unicode_to_glyphcode(unicode);
      if( glyphcode==0 ){
        last_after_whitespace = text;
        last_after_whitespace_x = x+1;
      }
      maybe_put_glyphcode(dest, x, y, glyphcode);
    }
    
    if( last_after_whitespace ) {// If we had whitespace somewhere in this line, we can wrap.
      first_utf8(text, &unicode);
      if( unicode_to_glyphcode(unicode) ){ // If we're not on whitepace now, we want to wrap.
        for( x=last_after_whitespace_x; x<right; x++ ){
          maybe_put_glyphcode(dest, x, y, 0);
        }
        text = last_after_whitespace;
      }
    }
  }
  
  return text;
}

void fill_rect( screen *dest, uint32_t left, uint32_t top, uint32_t w, uint32_t h, uint32_t bg, uint32_t fg ){
  uint32_t x, y;
  glyph g;
  g.ch = 0;
  g.bg = bg;
  g.fg = fg;
  for( y=top; y<top+h; y++) {
    for( x=left; x<left+w; x++ ){
      maybe_put_glyph(dest, x, y, g);
    }
  }
}

const uint8_t *previous_utf8(const uint8_t *text){
  //do{
    text--;
  //}while( (*text & 0x80)==0x80 );
  //return text;
  return text;
}

const uint8_t *next_line(const uint8_t *text, uint32_t line_width){
  const uint8_t *maybe_next = draw_text_wrappingly(0, text, 0,0, line_width, 1);
  if( *maybe_next ) return maybe_next;
  return text;
}

const uint8_t *previous_line(const uint8_t *text, uint32_t line_width){
  const uint8_t *first_of_line;
  uint32_t unicode;
  const uint8_t *line_begin;
  
  first_of_line = previous_utf8(text);
  if( !*first_of_line ) return text; // beginning of document
  while( 1 ){
    first_of_line = previous_utf8(first_of_line);
    unicode = *first_of_line;//first_utf8(previous_char, &unicode);
    if( unicode=='\0' || unicode=='\r' || unicode=='\n' ){
      first_of_line = first_utf8(first_of_line, &unicode);
      break;
    }
  }
  
  // Step forward until we get to text (at least)
  line_begin = first_of_line;
  while( 1 ){
    const uint8_t *next_line_begin = next_line(line_begin, line_width);
    if( next_line_begin>=text || next_line_begin<=line_begin ) break;
    line_begin = next_line_begin;
  }
  
  return line_begin;
}


int __start() {
  screen s;

  s.width = SCREEN_WIDTH;
  s.height = SCREEN_HEIGHT;
  /*for ( i=0; i<SCREEN_CELLS; i++ ){
    s.gs[i].ch = 10+(i%2);
    s.gs[i].fg = 0x00ff00;
    s.gs[i].bg = 0;
  }*/
  
  uint32_t line_width = 16;
  
  const uint8_t *resizing_around = 0;
  const uint8_t *text_cursor = file_contents + 1;
  while(1) {
    draw_text_wrappingly(&s, text_cursor, 1,1, line_width,s.height-2);
    display_screen(&s.width);
    input_event evt = get_input();
    if( evt.type==INPUT_EVENT_KEYDOWN ){
      if( evt.param==(0x1000|(('s'-'a')<<4)) ){
        const uint8_t *next = next_line(text_cursor, line_width);
        if( *next ) text_cursor = next;
        resizing_around = 0;
      }else if( evt.param==(0x1000|(('w'-'a')<<4))){
        text_cursor = previous_line(text_cursor, line_width);
        resizing_around = 0;
      }
    }else if( evt.type==INPUT_EVENT_RESIZE ){
      uint32_t screen_width, screen_height;
      get_screen_size(&screen_width, &screen_height);
      if( screen_height>SCREEN_HEIGHT ) screen_height = SCREEN_HEIGHT;
      if( screen_width>SCREEN_WIDTH ) screen_width = SCREEN_WIDTH;
      s.width = screen_width>SCREEN_WIDTH ? SCREEN_WIDTH : screen_width;
      s.height = screen_height>SCREEN_HEIGHT ? SCREEN_HEIGHT : screen_height;
      line_width = s.width-4;
      fill_rect(&s, 0,0, s.width,s.height, 0, 0xffffff);
      
      if( resizing_around==0 ){
        resizing_around = text_cursor;
      }
      
      // We want to keep what they currently had in the top left in view, but still be 
      // be on a natural line break at this width.
      const uint8_t *rounded_back = previous_line(resizing_around, line_width);
      const uint8_t *rounded_up = next_line(rounded_back, line_width);
      text_cursor = rounded_up <= text_cursor ? rounded_up : rounded_back;
    }
    
  }
  
  while(1) {
    display_screen(&s.width);
    input_event evt = get_input_with_timeout(200);
    
    s.gs[0].ch = 0x1000 + (evt.type<<4); 
    s.gs[1].ch = 10 + (rand_u32()%10);
    if (evt.type==2) {
      int x, y;
      get_cursor_coords(&x, &y);
      if (x>=0 && x<SCREEN_WIDTH&& y>=0 && y<SCREEN_HEIGHT) {
        s.gs[x + y*SCREEN_WIDTH].bg = (s.gs[x + y*SCREEN_WIDTH].bg+0x40) & 0xff;
      }
    }
  }
  return 0;
}

