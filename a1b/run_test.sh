make clean
make
fusermount -u /tmp/myrskogr
./mkfs.a1fs -f -i 10 image
gdb --args ./a1fs image /tmp/myrskogr -d