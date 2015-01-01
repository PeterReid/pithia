# This is meant to be repositionable, so don't use J -- just do BEQ so it uses relative addresses

top:
li $a0, 8 # load base address of the ELF
lw $a1, -4($a0)


# Load entry point into $t5
lw $t5, 0x18($a0)
or $t5, $t5, $zero



# Load section header offset into $t0
lw $t0, 0x20($a0)
add $t0, $t0, $a0 # add ELF base address
or $t0, $t0, $zero


lhu $a2, 0x2E($a0) # Load section header size
lhu $a3, 0x30($a0) # Load section header count
or $a2, $a2, $v0
or $a3, $a3, $v0


section_processing_top:

  lw $t1, 8($t0) # load flags
  li $v0, 0x02 # prepare the flag we want to check
  and $t1, $t1, $v0 # mask off all the other flags
  beq $t1, $zero, skip_section # skip this section if the flag is not set
  nop
  # If this is a section that we need to copy to memory
     # Load destination address into $t2
     lw $t2, 12($t0)
     
     # Load the source address into $t3
     lw $t3, 16($t0)
     add $t3, $t3, $a0 # add ELF base address
     
     # Load the length into $t4
     lw $t4, 20($t0)
     
     # length has better not be 0!
     #beq $t4, $zero, copy_bottom
     copy_top:
     lw $v0,($t3)
     sw $v0,($t2)
     
     addi $t3, $t3, 4
     addi $t2, $t2, 4
     subi $t4, $t4, 4
     bne $t4, $zero, copy_top
     nop
     
     
     xor $t2, $zero, $t2
  
  or $v0, $zero $a3 #temp
  
  skip_section:
  
  
  add $t0, $t0, $a2 # move to the next section header
  sub $a3, $a3, 1 # decrement the number of sections left
bne $zero, $a3, section_processing_top
nop

li $sp, 0x300000

jr $t5
nop



message_pump:
li $v0, 2 # get message
syscall
beq $zero, $zero, message_pump
nop
