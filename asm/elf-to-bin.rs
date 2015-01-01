
use std::io::{File, Open, Write};

fn main() {
    let elf_path = "withmain";
    let elf_contents = File::open(&Path::new(elf_path)).read_to_end().unwrap_or_else(|_| { panic!("Could not read ELF code"); } );
    
    let loader_path = "loader.bin";
    let mut loader_contents = File::open(&Path::new(loader_path)).read_to_end().unwrap_or_else(|_| { panic!("Could not read ELF code"); } );
    
    let elf_offset = loader_contents.len() + 4;
    if elf_offset > 1000 {
        panic!("Why is the loader so long?");
    }
    if elf_contents.len() >= 0x0400000 {
        panic!("This ELF is too long! The loader needs to do something fancy to not write over itself");
    }
    loader_contents[0] = (elf_offset & 0xff) as u8;
    loader_contents[1] = ((elf_offset>>8) & 0xff) as u8;
    
    let output_path = Path::new("withmain-binned.bin");

    let mut output = File::open_mode(&output_path, Open, Write)
        .unwrap_or_else(|e| { panic!("Could not open output file: {}", e); });
    output.write(loader_contents.as_slice()).unwrap_or_else(|_| { panic!("Failed to write loader"); } );
    output.write_le_u32(elf_contents.len() as u32).unwrap_or_else(|_| { panic!("Failed to write ELF length"); } );
    output.write(elf_contents.as_slice()).unwrap_or_else(|_| { panic!("Failed to write ELF"); } );
}