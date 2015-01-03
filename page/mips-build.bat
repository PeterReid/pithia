scp page.c peterreid@%BUILDMACHINE%:~/mipsbuild/in.c
scp mips-build.sh peterreid@%BUILDMACHINE%:~/mipsbuild/mips-build.sh
ssh peterreid@%BUILDMACHINE% '~/mipsbuild/mips-build.sh'
scp peterreid@%BUILDMACHINE%:~/mipsbuild/out page.elf
cp page.elf withmain
elf-to-bin
cp withmain-binned.bin ../page.bin
