#![feature(std_misc, io, fs, rand, core)]


#[macro_use]
extern crate gridui;

extern crate "mips_cpu" as mips;

use std::old_io::{IoResult, MemWriter};
use std::fs::File;
use std::io::{Read, Write};

use std::iter::range_step;
use std::rand::{Rng, OsRng};
use std::net::{TcpListener, TcpStream};

use mips::cpu::{MipsCpu, FaultType};
use std::old_io::timer::Timer;
use std::time::Duration;
use std::sync::mpsc::{Select};

use std::io;

use gridui::gridui::{InputEvent, Glyph, WindowsGridUi, Screen, GridUiInterface};
use gridui::glyphcode;

fn place_code(cpu: &mut MipsCpu, code: &[u8], index: u32) {
    // Assumed code length is a multiple of 4
    for (offset, chunk) in code.chunks(4).enumerate() {
        let val = (chunk[0] as u32) 
            | (if chunk.len()>1 {((chunk[1] as u32) << 8)  } else { 0 } )
            | (if chunk.len()>2 {((chunk[2] as u32) << 16) } else { 0 } )
            | (if chunk.len()>3 {((chunk[3] as u32) << 24) } else { 0 } );
        cpu.set_mem(index + 4*(offset as u32), val);
    }
}

fn encode_u32_le(dest: &mut[u8], x: u32) {
    dest[0] = ((x>> 0) & 0xff) as u8;
    dest[1] = ((x>> 8) & 0xff) as u8;
    dest[2] = ((x>>16) & 0xff) as u8;
    dest[3] = ((x>>24) & 0xff) as u8;
}

// Things that the application can do wrong to get it crashed
#[derive(Debug)]
enum ApplicationException {
    InvalidSyscall,
    ScreenTooBig,
    UnalignedScreenMemory,
    ClosedByUi,
    OverlongURL,
}

struct UiRunner {
    ui: WindowsGridUi, // TODO: be generic over different UIs
    rng: OsRng,
    cpu: MipsCpu,
    timer: Timer,
    screen_width: u32,
    screen_height: u32,
    cursor_down: bool,
    cursor_x: u32,
    cursor_y: u32,
}

enum InputEventTranslation {
    Actual(u32, u32),
    NoOp,
    Close,
}


const MAXIMUM_SCREEN_MEMORY : u32 = 400*400;
impl UiRunner {
    fn new(initial_url: Vec<u32>) -> IoResult<UiRunner> {
        let ui = WindowsGridUi::new();
        
        let mut cpu = MipsCpu::new();
        
        //place_code(&mut cpu, &code[], 0);
        
        let mut runner = UiRunner {
            ui: ui,
            cpu: cpu,
            screen_width: 0,
            screen_height: 0,
            cursor_down: false,
            cursor_x: 0,
            cursor_y: 0,
            rng: try!(OsRng::new()),
            timer: try!(Timer::new()),
        };
        
        runner.load_url(&initial_url[]);
        
        Ok(runner)
    }
    
    fn run(&mut self) {
        // TODO: Flush message pump. This will help an initial run be indistinguishable from a later run...
        // otherwise, the initial resize event would be received only for an initial run.
        
        loop {
            if let Some(interrupt) = self.cpu.run(800000) {
                println!("Interrupt: {:?}", interrupt);
                match interrupt {
                    FaultType::Syscall => {
                        if let Some(error) = self.dispatch_syscall() {
                            println!("CPU halting executing with {:?}", error);
                            break;
                        }
                        self.cpu.clear_fault();
                    },
                    FaultType::InvalidInstruction => {
                        println!("Invalid instruction from CPU");
                        let pc = self.cpu.pc;
                        println!("  Address: 0x{:x}", pc);
                        println!("  Instruction: 0x{:x}", self.cpu.read_mem(pc));
                        break;
                    },
                    _ => {
                        break;
                    }
                }
            } else {
                // Signal that the CPU is busy
                println!("CPU hung");
                break;
            }
        }
    }
    
    fn read_hash_from_cpu_memory(&mut self, address_base: u32) -> Vec<u8> {
        let hash_bytes = 64u32;
        let mut w = MemWriter::with_capacity(hash_bytes as usize);
        for address in range_step(address_base, address_base+hash_bytes, 4) {
            w.write_le_u32(self.cpu.read_mem(address)).ok().expect("Failed to store hash from CPU memory");
        }
        return w.into_inner();
    }
    
    fn load_from_cache(&mut self, hash: Vec<u8>) -> Result<Vec<u8>, ()> {
        Err( () )
    }
    
    fn write_to_memory(&mut self, address: u32, bytes: &[u8]) {
        
    }
    
    fn try_load_url(&mut self, url: &[u32]) -> io::Result<()> {
    
        let mut stream = TcpStream::connect("127.0.0.1:5692").unwrap();
        let mut prefixed_url : Vec<u8> = Vec::new();
        prefixed_url.resize(2 + url.len()*4, 0u8);
        encode_u16_le(&mut prefixed_url[0..2], url.len() as u16);
        for (glyph, byte_chunk) in url.iter().zip(prefixed_url[2..].chunks_mut(4)) {
            encode_u32_le(&mut *byte_chunk, *glyph);
        }
        println!("Going to write {:?}", prefixed_url);
        stream.write_all(&prefixed_url[]);
        println!("Wrote URL");
        let payload_len = decode_u32_le(&try!(read_exactly(&mut stream, 4))[]);
        println!("Payload len = {}", payload_len);
        let payload = try!(read_exactly(&mut stream, payload_len as usize));
        
        self.cpu = MipsCpu::new();
        place_code(&mut self.cpu, &payload[], 0);
        
        Ok( () )
    }
    
    fn load_url(&mut self, url: &[u32]) {
        if let Err(_) = self.try_load_url(url) {
            println!("Error loading url! {:?}", url);
        }
    }
    
    fn read_memory_words(&mut self, offset: u32, count: u32) -> Vec<u32> {
        let mut words = Vec::new();
        for address in range_step(offset, offset+count*4, 4) {
            words.push(self.cpu.read_mem(address));
        }
        words
    }
    
    fn dispatch_syscall(&mut self) -> Option<ApplicationException> {
        println!("Issued syscall: {}", self.cpu.regs[2]);
        let syscall_number = self.cpu.regs[2];
        let syscall_arg = self.cpu.regs[3];
        let syscall_arg_2 = self.cpu.regs[4];
        
        println!("at 6720: {}", self.cpu.read_mem(6720));
        println!("at 16: {}", self.cpu.read_mem(16));
        match syscall_number {
            1 => { // Show screen
                let address_base = syscall_arg;
                if address_base%4 != 0 {
                    return Some(ApplicationException::UnalignedScreenMemory);
                }
                let width = self.cpu.read_mem(address_base);
                let height = self.cpu.read_mem(address_base+4);
                let size = (width as u64)*(height as u64);
                if size >= MAXIMUM_SCREEN_MEMORY as u64 {
                    return Some(ApplicationException::ScreenTooBig);
                }
                let glyph_address_base = address_base + 8;
                let glyphs = (0..size as usize).map(|idx| {
                    let glyph_start_address = glyph_address_base + (idx as u32)*12;
                    Glyph {
                        character: self.cpu.read_mem(glyph_start_address),
                        foreground: self.cpu.read_mem(glyph_start_address + 4),
                        background: self.cpu.read_mem(glyph_start_address + 8),
                    }
                }).collect();
                self.ui.send_screen(Screen{
                    width: width,
                    glyphs: glyphs
                });
                None
            }
            2 => { // Get input event
                match self.get_application_input_event(syscall_arg as i64) {
                    Some((input_event_type, param)) => {
                        self.cpu.regs[2] = input_event_type;
                        self.cpu.regs[3] = param;
                    }
                    None => { return Some(ApplicationException::ClosedByUi); }
                }
                None
            }
            3 => { // Get cursor coordinates
                self.cpu.regs[2] = self.cursor_x;
                self.cpu.regs[3] = self.cursor_y;
                None
            }
            4 => { // Get screen dimensions
                self.cpu.regs[2] = self.screen_width;
                self.cpu.regs[3] = self.screen_height;
                None
            }
            5 => { // Get random u32
                self.cpu.regs[2] = self.rng.next_u32();
                None
            }
            6 => { // Load from cache
                let hash_address = syscall_arg;
                let destination_address = syscall_arg_2;
                let hash = self.read_hash_from_cpu_memory(hash_address);
                match self.load_from_cache(hash) {
                    Err(_) => {
                        self.cpu.regs[2] = 0;
                    }
                    Ok(contents) => {
                        self.write_to_memory(destination_address, &contents[]);
                        self.cpu.regs[2] = 1;
                    }
                }
                None
            }
            7 => {
                println!("Following a URL");
                let url_memory_address = syscall_arg;
                let url_glyph_count = syscall_arg_2;
                
                if url_glyph_count > 0xffff {
                    Some(ApplicationException::OverlongURL)
                } else {
                    let url = self.read_memory_words(url_memory_address, url_glyph_count);
                    self.load_url(&url[]);
                    None
                }
            }
            _ => {
                // Maybe crash the application here?
                println!("Unrecognized syscall: {}", syscall_number);
                Some(ApplicationException::InvalidSyscall)
            }
        }
        
    }
    
    fn translate_input_event(&mut self, event: InputEvent) -> InputEventTranslation {
        
        match event {
            InputEvent::Close => { return InputEventTranslation::Close; }
            InputEvent::Size(cols, rows) => {
                println!("Resized to {} by {}", cols, rows);
                self.screen_width = cols;
                self.screen_height = rows;
                return InputEventTranslation::Actual(1, 0);
            }
            InputEvent::MouseDown(col, row) => {
                self.cursor_down = true;
                self.cursor_x = col;
                self.cursor_y = row;
                return InputEventTranslation::Actual(2, 0);
            }
            InputEvent::MouseUp(col, row) => {
                self.cursor_down = false;
                self.cursor_x = col;
                self.cursor_y = row;
                return InputEventTranslation::Actual(3, 0);
            }
            InputEvent::KeyDown(glyphcode) => {
                return InputEventTranslation::Actual(4, glyphcode);
            }
            InputEvent::KeyUp(glyphcode) => {
                return InputEventTranslation::Actual(5, glyphcode);
            }
        }
    }
    
    fn get_application_input_event(&mut self, timeout_millis: i64) -> Option<(u32, u32)> {
        let timeout = self.timer.oneshot(Duration::milliseconds(timeout_millis));
        
        loop {
            let (event_ready, timer_ready) = {
                let sel = Select::new();
                let mut rx1 = sel.handle(&self.ui.input_event_source);
                let mut rx2 = sel.handle(&timeout);
                unsafe {
                    // add() is unsafe because it requires that the `Handle` is not moved
                    // while it is added to the `Select` set.
                    rx1.add();
                    rx2.add();
                }
                let ret = sel.wait();
                (ret == rx1.id(), ret==rx2.id())
            };
            if event_ready {
                let event = self.ui.input_event_source.recv().ok().expect("Failed to get input event from UI");
                
                match self.translate_input_event(event) {
                    InputEventTranslation::Actual(a, b) => { return Some((a, b));}
                    InputEventTranslation::Close => { return None; }
                    InputEventTranslation::NoOp => {}
                }
            }
            if timer_ready {
                let _ = timeout.recv();
                return Some((0,0));
            }
        }
    }
}

fn run_window(initial_url: Vec<u32>) {
    let mut uirunner = UiRunner::new(initial_url).unwrap_or_else(|e| {
        panic!("Failed to initialize UI runner: {}", e);
    });
    uirunner.run();
}

fn read_exactly<R: Read>(source: &mut R, len: usize) -> io::Result<Vec<u8>> {
    println!("Attempting to read {} bytes", len);
    let mut dest : Vec<u8> = Vec::new();
    dest.resize(len, 0u8);
    let mut read_count: usize = 0;
    while read_count < len {
        let this_read_count = try!(source.read(&mut dest[read_count..len]));
        if this_read_count==0 {
            return Err(io::Error::new(io::ErrorKind::Other, "Not enough bytes to read", None));
        }
        read_count += this_read_count;
    }
    println!("Reading complete!");
    Ok( dest )
}

fn encode_u16_le(dest: &mut[u8], x: u16) {
    dest[0] = ((x>>0) & 0xff) as u8;
    dest[1] = ((x>>8) & 0xff) as u8;
}

fn decode_u32_le(dest: &[u8]) -> u32 {
    return (dest[0] as u32) 
        | ((dest[1] as u32) << 8)
        | ((dest[2] as u32) << 16)
        | ((dest[3] as u32) << 24);
}
/*
fn download_code_crudely() -> io::Result<Vec<u8>> {
    let mut stream = TcpStream::connect("127.0.0.1:5692").unwrap();
    let mut prefixed_url = b"\0\0pia1:abcdefg@127.0.0.1/decl.txt".to_vec();
    let url_len = prefixed_url.len() - 2;
    encode_u16_le(&mut prefixed_url[0..2], url_len as u32);
    println!("Going to write {:?}", prefixed_url);
    stream.write_all(&prefixed_url[]);
    println!("Wrote URL");
    let payload_len = decode_u32_le(&try!(read_exactly(&mut stream, 4))[]);
    Ok( try!(read_exactly(&mut stream, payload_len as usize)) )
}
*/

fn main() {
    let mut code_file = File::open("page.bin").ok().expect("Failed to open page.bin");
    let mut contents = Vec::new();//download_code_crudely().ok().expect("Failed to download");
    //println!("{:?}", contents);
    
    let url_glyphs = glyphcode::from_str("pia1:abcdefg@127.0.0.1/menu.txt".as_slice()).expect("Failed to convert initial url to glyphcode");
    
    //println!("{}", contents[6720]);
    code_file.read_to_end(&mut contents).unwrap_or_else(|_| { panic!("Could not read MIPS code"); } );
    run_window(url_glyphs);
}