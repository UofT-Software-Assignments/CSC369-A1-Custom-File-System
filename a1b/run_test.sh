make clean
make
./mkfs.a1fs -f -i 10 image
gdb ./a1fs image /tmp/chaohao1