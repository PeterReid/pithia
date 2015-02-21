scp page.c peterreid@%BUILDMACHINE%:~/mipsbuild/in.c
scp pithia.h peterreid@%BUILDMACHINE%:~/mipsbuild/pithia.h
scp mips-build.sh peterreid@%BUILDMACHINE%:~/mipsbuild/mips-build.sh
ssh peterreid@%BUILDMACHINE% '~/mipsbuild/mips-build.sh'
scp peterreid@%BUILDMACHINE%:~/mipsbuild/out page.elf
cp page.elf withmain
..\asm\elf-to-bin
cp withmain-binned.bin ../page.bin
copy /y withmain-binned.bin ..\..\pithia_static_server\handler\txt
