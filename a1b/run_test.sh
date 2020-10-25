make clean
make
fusermount -u /tmp/myrskogr
truncate -s 64K image
./mkfs.a1fs -f -i 4 image
gdb --args ./a1fs image /tmp/myrskogr -d