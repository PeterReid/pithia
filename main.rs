
#![feature(globs, phase, macro_rules)]

#[phase(plugin, link)]
extern crate gridui;

extern crate "mips_cpu" as mips;

use std::io::File;

use mips::cpu::{MipsCpu, FaultType};

use gridui::gridui::{InputEvent, Glyph, WindowsGridUi, Screen, GridUiInterface};

fn place_code(cpu: &mut MipsCpu, code: &[u8], index: u32) {
    // Assumed code length is a multiple of 4
    for (offset, chunk) in code.chunks(4).enumerate() {
        let val = (chunk[0] as u32) | (chunk[1] as u32 << 8) | (chunk[2] as u32 << 16) | (chunk[3] as u32 << 24);
        cpu.set_mem(index + 4*(offset as u32), val);
    }
}

// Things that the application can do wrong to get it crashed
#[deriving(Show)]
enum ApplicationException {
    InvalidSyscall,
    ScreenTooBig,
    UnalignedScreenMemory,
    ClosedByUi,
}

struct UiRunner {
    ui: WindowsGridUi, // TODO: be generic over different UIs
    cpu: MipsCpu,
    screen_width: u32,
    screen_height: u32,
    cursor_down: bool,
    cursor_x: u32,
    cursor_y: u32,
}

const MAXIMUM_SCREEN_MEMORY : u32 = 400*400;
impl UiRunner {
    fn new(code: Vec<u8>) -> UiRunner {
        let ui = WindowsGridUi::new();
        
        let mut cpu = MipsCpu::new();
        
        place_code(&mut cpu, code.as_slice(), 0);
        
        UiRunner {
            ui: ui,
            cpu: cpu,
            screen_width: 0,
            screen_height: 0,
            cursor_down: false,
            cursor_x: 0,
            cursor_y: 0,
        }
    }
    
    fn run(&mut self) {
        loop {
            if let Some(interrupt) = self.cpu.run(800000) {
                println!("Interrupt: {}", interrupt);
                match interrupt {
                    FaultType::Syscall => {
                        if let Some(error) = self.dispatch_syscall() {
                            println!("CPU halting executing with {}", error);
                            break;
                        }
                        self.cpu.clear_fault();
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
    
    fn dispatch_syscall(&mut self) -> Option<ApplicationException> {
        println!("Issued syscall: {}", self.cpu.regs[2]);
        let syscall_number = self.cpu.regs[2];
        let syscall_arg = self.cpu.regs[3];
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
                let glyphs = Vec::from_fn(size as uint, |idx| {
                    let glyph_start_address = glyph_address_base + (idx as u32)*12;
                    Glyph {
                        character: self.cpu.read_mem(glyph_start_address),
                        foreground: self.cpu.read_mem(glyph_start_address + 4),
                        background: self.cpu.read_mem(glyph_start_address + 8),
                    }
                });
                self.ui.send_screen(Screen{
                    width: width as uint,
                    glyphs: glyphs
                });
                None
            }
            2 => { // Get input event
                match self.get_application_input_event() {
                    Some(input_event_type) => { self.cpu.regs[2] = input_event_type; }
                    None => { return Some(ApplicationException::ClosedByUi); }
                }
                None
            }
            3 => { // Get cursor coordinates
                self.cpu.regs[2] = self.cursor_x;
                self.cpu.regs[3] = self.cursor_y;
                None
            }
            _ => {
                // Maybe crash the application here?
                println!("Unrecognized syscall: {}", syscall_number);
                Some(ApplicationException::InvalidSyscall)
            }
        }
        
    }
    
    fn get_application_input_event(&mut self) -> Option<u32> {
        loop {
            match self.ui.get_input_event() {
                InputEvent::Close => { return None; }
                InputEvent::Size(cols, rows) => {
                    println!("Resized to {} by {}", cols, rows);
                    self.screen_width = cols;
                    self.screen_height = rows;
                    return Some(1);
                }
                InputEvent::MouseDown(col, row) => {
                    self.cursor_down = true;
                    self.cursor_x = col;
                    self.cursor_y = row;
                    return Some(2);
                }
                InputEvent::MouseUp(col, row) => {
                    self.cursor_down = false;
                    self.cursor_x = col;
                    self.cursor_y = row;
                    return Some(3);
                }
                x => {
                    println!("{}", x);
                }
            }
        }
    }
}

fn run_window(code: Vec<u8>) {
    UiRunner::new(code).run();
}

fn main() {
    let contents = File::open(&Path::new("page.bin")).read_to_end().unwrap_or_else(|_| { panic!("Could not read MIPS code"); } );
    run_window(contents);
}