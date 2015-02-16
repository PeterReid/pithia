#![feature(io, fs)]

use std::fs::File;
use std::io::{Read, Write};

fn encode_u32_le(x: u32) -> Vec<u8> {
    [((x>> 0) & 0xff) as u8,
     ((x>> 8) & 0xff) as u8,
     ((x>>16) & 0xff) as u8,
     ((x>>24) & 0xff) as u8].to_vec()
}

fn main() {
    let elf_path = "withmain";
    let mut elf_contents = Vec::new();
    File::open(elf_path).ok().expect("Could not open ELF").read_to_end(&mut elf_contents).unwrap_or_else(|_| { panic!("Could not read ELF code"); } );
    
    let loader_path = "loader.bin";
    let mut loader_contents = Vec::new();
    File::open(loader_path).ok().expect("Could not open loader.bin").read_to_end(&mut loader_contents).unwrap_or_else(|_| { panic!("Could not read ELF code"); } );
    
    let elf_offset = loader_contents.len() + 4;
    if elf_offset > 1000 {
        panic!("Why is the loader so long?");
    }
    if elf_contents.len() >= 0x0400000 {
        panic!("This ELF is too long! The loader needs to do something fancy to not write over itself");
    }
    let mut found_elf_jumps = 0;
    for chunk in loader_contents[].chunks_mut(4) {
        println!("{:?}", chunk);
        if chunk[3] == 0x24 && chunk[2]==0x04 && chunk[1] == 0x12 && chunk[0] == 0x34 {
            
            found_elf_jumps += 1;
            
            chunk[0] = (elf_offset & 0xff) as u8;
            chunk[1] = ((elf_offset>>8) & 0xff) as u8;
        }
    }
    assert!(found_elf_jumps==1, "Expected exactly one special jump!");
    
    let mut output = File::create("withmain-binned.bin")
        .unwrap_or_else(|e| { panic!("Could not open output file: {}", e); });
    output.write_all(&loader_contents[]).unwrap_or_else(|_| { panic!("Failed to write loader"); } );
    output.write_all(&encode_u32_le(elf_contents.len() as u32)[]).unwrap_or_else(|_| { panic!("Failed to write ELF length"); } );
    output.write_all(&elf_contents[]).unwrap_or_else(|_| { panic!("Failed to write ELF"); } );
}