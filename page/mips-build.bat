scp page.c peterreid@192.168.59.195:~/mipsbuild/in.c
scp mips-build.sh peterreid@192.168.59.195:~/mipsbuild/mips-build.sh
ssh peterreid@192.168.59.195 '~/mipsbuild/mips-build.sh'
scp peterreid@192.168.59.195:~/mipsbuild/out page.elf
cp page.elf withmain
elf-to-bin
cp withmain-binned.bin ../page.bin
