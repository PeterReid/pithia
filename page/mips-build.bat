scp tetris.c peterreid@%BUILDMACHINE%:~/mipsbuild/in.c
scp pithia.h peterreid@%BUILDMACHINE%:~/mipsbuild/pithia.h
scp mips-build.sh peterreid@%BUILDMACHINE%:~/mipsbuild/mips-build.sh
ssh peterreid@%BUILDMACHINE% '~/mipsbuild/mips-build.sh'
scp peterreid@%BUILDMACHINE%:~/mipsbuild/out tetris.elf
cp tetris.elf withmain
..\asm\elf-to-bin
copy /y withmain-binned.bin ..\..\pithia_static_server\static\tetris
