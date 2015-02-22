
#include <stdint.h>
#include "pithia.h"

#define INVALID_GLYPHCODE 999
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
    case '@': return 9;
  }
  return INVALID_GLYPHCODE;
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


#define SCREEN_WIDTH 30
#define SCREEN_HEIGHT 20
#define SCREEN_CELLS (SCREEN_WIDTH*SCREEN_HEIGHT)

typedef struct block {
  uint32_t color[5][5];
} block;

#define BLOCK_TYPES 7
block BLOCKS[BLOCK_TYPES] = {
  #define _ 0
  
  #define C 0x0000a8
  { { { _,_,_,_,_ },
      { _,_,_,_,_ },
      { _,C,C,C,C },
      { _,_,_,_,_ },
      { _,_,_,_,_ } } },
  #undef C
  
  #define C 0xa8a8a8
  { { { _,_,_,_,_ },
      { _,C,_,_,_ },
      { _,C,C,C,_ },
      { _,_,_,_,_ },
      { _,_,_,_,_ } } },
  #undef C
  
  #define C 0xa800a8
  { { { _,_,_,_,_ },
      { _,_,_,C,_ },
      { _,C,C,C,_ },
      { _,_,_,_,_ },
      { _,_,_,_,_ } } },
  #undef C
  
  #define C 0xa80000
  { { { _,_,_,_,_ },
      { _,_,C,C,_ },
      { _,_,C,C,_ },
      { _,_,_,_,_ },
      { _,_,_,_,_ } } },
  #undef C
      
  #define C 0x00a800
  { { { _,_,_,_,_ },
      { _,_,C,C,_ },
      { _,C,C,_,_ },
      { _,_,_,_,_ },
      { _,_,_,_,_ } } },
  #undef C
  
  #define C 0x0054a8
  { { { _,_,_,_,_ },
      { _,_,C,_,_ },
      { _,C,C,C,_ },
      { _,_,_,_,_ },
      { _,_,_,_,_ } } },
  #undef C
  
  #define C 0xa8a800
  { { { _,_,_,_,_ },
      { _,C,C,_,_ },
      { _,_,C,C,_ },
      { _,_,_,_,_ },
      { _,_,_,_,_ } } },
  #undef C
  
  #undef _
};

#define ARENA_WIDTH 15
#define ARENA_HEIGHT 20
uint32_t arena[ARENA_WIDTH][ARENA_HEIGHT];

uint32_t faller_x;
uint32_t faller_y;

block faller;


int faller_collides() {
  uint32_t x, y;
  for( x=0; x<5; x++ ){
    for( y=0; y<5; y++ ){
      if (faller.color[x][y] && (faller_x+x>=ARENA_WIDTH || faller_x+x<0 || faller_y+y >= ARENA_HEIGHT || arena[faller_x+x][faller_y+y])) {
        return 1;
      }
    }
  }
  return 0;
}

void compact(){
  int32_t x, read_y;
  int32_t write_y;
  for( read_y=ARENA_HEIGHT-1, write_y = ARENA_HEIGHT-1; read_y>=0; read_y-- ){
    int full = 1;
    for( x=0; x<ARENA_WIDTH; x++ ){
      if( !arena[x][read_y] ) {
        full = 0;
        break;
      }
    }
    
    if (!full) {
      for( x=0; x<ARENA_WIDTH; x++ ){
        arena[x][write_y] = arena[x][read_y];
      }
      write_y--;
    }
  }
  
  while( write_y>=0 ){
    for( x=0; x<ARENA_WIDTH; x++ ){
      arena[x][write_y] = 0;
    }
    write_y--;
  }
  
  
}

void freeze_faller() {
  uint32_t x, y;
  for( x=0; x<5; x++ ){
    for( y=0; y<5; y++ ){
      if( faller.color[x][y] ){
        arena[faller_x+x][faller_y+y] = faller.color[x][y];
      }
    }
  }
  
  compact();
}

void rotate_left(block *b) {
  uint32_t x, y, min_x=5, min_y=5, max_x=0, max_y=0;
  for( x=0; x<5; x++ ){
    for( y=0; y<5; y++ ){
      if( b->color[x][y] ){
        if( min_x>x ) min_x = x;
        if( max_x<x ) max_x = x;
        if( min_y>y ) min_y = y;
        if( max_y<y ) max_y = y;
      }
    }
  }
  
  min_x=0;min_y=0;max_x=4;max_y=4;
  uint32_t width = max_x - min_x + 1;
  uint32_t height = max_y - min_y + 1;
  uint32_t size = width>height ? width : height;
  
  block temp;
  for( x=0; x<5; x++ ){
    for( y=0; y<5; y++ ){
      temp.color[x][y] = b->color[x][y];
      b->color[x][y] = 0;
    }
  }
  
  uint32_t dx, dy;
  for( dx=0; dx<size; dx++) {
    for( dy=0; dy<size; dy++ ){
      b->color[min_x + size-1 - dy][min_y + dx] = temp.color[min_x + dx][min_y + dy];
    }
  }
}
void rotate_right(block *b){
  rotate_left(b);
  rotate_left(b);
  rotate_left(b);
}

void new_faller() {
  uint32_t faller_type = rand_u32() % BLOCK_TYPES;
  faller = BLOCKS[faller_type];
  faller_x = 4;
  faller_y = 0;
}

int __start() {
  uint32_t screen_buffer[SCREEN_U32S_FOR(SCREEN_WIDTH*SCREEN_HEIGHT)];
  
  screen *s = (screen *)&screen_buffer;

  s->width = SCREEN_WIDTH;
  s->height = SCREEN_HEIGHT;
  
  uint32_t x, y;
  
  for( x=0; x<ARENA_WIDTH; x++ ){
    for( y=0; y<ARENA_HEIGHT; y++ ){
      arena[x][y] = 0;
    }
  }
  
  new_faller();
  
  uint32_t previous_tick_millis = current_milliseconds();
  
  while(1) {
    uint32_t now = current_milliseconds();
    uint32_t since_last_tick = now - previous_tick_millis;
    uint32_t millis_per_step = 500;
    uint32_t timeout = since_last_tick > millis_per_step ? 0 : millis_per_step - since_last_tick;
    
    input_event evt = get_input_with_timeout(timeout);
    
    
    if( evt.type==0 ) {
      faller_y++;
      if( faller_collides() ){
        faller_y--;
        freeze_faller();
        new_faller();
      }
      previous_tick_millis = current_milliseconds();
    }else if( evt.type==INPUT_EVENT_KEYDOWN ){
      if( evt.param==(0x1000|(('a'-'a')<<4)) ){
        faller_x--;
        if( faller_collides() ){
          faller_x++;
        }
      }else if( evt.param==(0x1000|(('d'-'a')<<4))){
        faller_x++;
        if( faller_collides() ){
          faller_x--;
        }
      }else if( evt.param==(0x1000|(('w'-'a')<<4))){
        rotate_left(&faller);
        if( faller_collides() ) rotate_right(&faller);
      }else if( evt.param==(0x1000|(('s'-'a')<<4))){
        rotate_right(&faller);
        if( faller_collides() ) rotate_left(&faller);
      }else if( evt.param==0 ){
        while( !faller_collides() ){
          faller_y++;
        }
        faller_y--;
        freeze_faller();
        new_faller();
        previous_tick_millis = current_milliseconds();
      }
    }
    
    for( x=0; x<ARENA_WIDTH; x++ ){
      for( y=0; y<ARENA_HEIGHT; y++ ){
        glyph g;
        g.ch = 0;
        g.fg = arena[x][y];
        g.bg = arena[x][y];
        maybe_put_glyph(s, x*2, y, g);
        maybe_put_glyph(s, x*2+1, y, g);
      }
    }
    for( x=0; x<5; x++ ){
      for( y=0; y<5; y++ ){
        if (faller.color[x][y]) {
          glyph g;
          g.ch = 0;
          g.fg = faller.color[x][y];
          g.bg = faller.color[x][y];
          maybe_put_glyph(s, (faller_x+x)*2, faller_y+y, g);
          maybe_put_glyph(s, (faller_x+x)*2+1, faller_y+y, g);
        }
      }
    }
    
    display_screen(s);
  }
  
  
  return 0;
}

