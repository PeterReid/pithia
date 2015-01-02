echo "building"
cd mipsbuild
mips-linux-gnu-gcc -march=mips1 -EL -s -Wall -nostartfiles -nostdlib -O2 -o out in.c
echo "Built"
exit
echo "Logouted"