
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
const uint8_t *draw_text_wrappingly_ex(
  screen *dest, 
  const uint8_t *text,
  uint32_t left, uint32_t top, uint32_t width, uint32_t height,
  uint32_t hit_test_x, uint32_t hit_test_y, const uint8_t **hit_test_result
){
  uint32_t x, y;
  uint32_t right, bottom; // exclusive
  uint32_t unicode;
  const uint8_t *last_after_whitespace;
  uint32_t last_after_whitespace_x = 0;
  uint32_t glyphcode;
  
  if( *text==0 || width==0 ) return text; // end of document
  
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
      const uint8_t *text_was = text;
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
      if( hit_test_result && x==hit_test_x && y==hit_test_y ){
        *hit_test_result = text_was;
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

const uint8_t *draw_text_wrappingly(
  screen *dest, 
  const uint8_t *text,
  uint32_t left, uint32_t top, uint32_t width, uint32_t height
){
  return draw_text_wrappingly_ex(dest, text, left, top, width, height, 0,0,0);
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

void follow_utf8_url(const uint8_t *utf8) {
  uint32_t url_glyphs[256];
  uint32_t glyph_count = 0;
  
  // Step backwards until we get off a url-ish character
  while( 1 ){
    const uint8_t *prev = previous_utf8(utf8);
    uint32_t codepoint;
    first_utf8(prev, &codepoint);
    uint32_t glyphcode = unicode_to_glyphcode(codepoint);
    if( glyphcode==INVALID_GLYPHCODE || glyphcode==0 ) break;
    utf8 = prev;
  }
  
  while( glyph_count<255 ){
    uint32_t codepoint;
    utf8 = first_utf8(utf8, &codepoint);
    
    uint32_t glyphcode = unicode_to_glyphcode(codepoint);
    if( glyphcode==INVALID_GLYPHCODE || glyphcode==0 ) break;
    
    url_glyphs[glyph_count++] = glyphcode;
  }
  url_glyphs[glyph_count] = 0;
  
  navigate_to(url_glyphs, glyph_count);
}

#define SCREEN_WIDTH 180
#define SCREEN_HEIGHT 80
#define SCREEN_CELLS (SCREEN_WIDTH*SCREEN_HEIGHT)

int __start() {
  uint32_t screen_buffer[SCREEN_U32S_FOR(SCREEN_WIDTH*SCREEN_HEIGHT)];
  
  screen *s = (screen *)&screen_buffer;

  s->width = SCREEN_WIDTH;
  s->height = SCREEN_HEIGHT;
  
  uint32_t line_width = 16;
  
  const uint8_t *resizing_around = 0;
  const uint8_t *text_cursor = *(const uint8_t **)(12); // The address of the text content begin, where we want to put our text cursor, will be written in byte 12 of the bootloader.
  int first_round = 1;
  
  while(1) {
    input_event evt;
    if( first_round ){
      first_round = 0;
      evt.type = INPUT_EVENT_RESIZE;
      evt.param = 0;
    } else {
      evt = get_input();
    }
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
      s->width = screen_width>SCREEN_WIDTH ? SCREEN_WIDTH : screen_width < 5 ? 5 : screen_width;
      s->height = screen_height>SCREEN_HEIGHT ? SCREEN_HEIGHT : screen_height==0 ? 1 : screen_height;
      line_width = s->width-4;
      fill_rect(s, 0,0, s->width,s->height, 0, 0xffffff);
      
      if( resizing_around==0 ){
        resizing_around = text_cursor;
      }
      
      // We want to keep what they currently had in the top left in view, but still be 
      // be on a natural line break at this width.
      const uint8_t *rounded_back = previous_line(resizing_around, line_width);
      const uint8_t *rounded_up = next_line(rounded_back, line_width);
      text_cursor = rounded_up <= text_cursor ? rounded_up : rounded_back;
    }else if( evt.type==INPUT_EVENT_LEFT_MOUSE_DOWN ){
      int32_t x, y;
      get_cursor_coords(&x, &y);
      const uint8_t *hit = 0;
      draw_text_wrappingly_ex(s, text_cursor, 1,1, line_width,s->height-2, x, y, &hit);
      if(hit) {
        follow_utf8_url(hit);
      }
    }
    
    draw_text_wrappingly(s, text_cursor, 1,1, line_width,s->height-2);
    display_screen(s);
  }
  
  while(1) {
    display_screen(s);
    input_event evt = get_input_with_timeout(200);
    
    s->gs[0].ch = 0x1000 + (evt.type<<4); 
    s->gs[1].ch = 10 + (rand_u32()%10);
    if (evt.type==2) {
      int x, y;
      get_cursor_coords(&x, &y);
      if (x>=0 && x<SCREEN_WIDTH&& y>=0 && y<SCREEN_HEIGHT) {
        s->gs[x + y*SCREEN_WIDTH].bg = (s->gs[x + y*SCREEN_WIDTH].bg+0x40) & 0xff;
      }
    }
  }
  return 0;
}

