make
truncate -s 64K disk_image
./mkfs.a1fs -i 16 disk_image
./a1fs disk_image /tmp/chaohao1
cd /tmp/chaohao1
mkdir dir1
ls
cd dir1
touch file1
echo "random content" >> file1
ls
echo "another file" > file2
ls
mkdir another_directory
ls
cat file1
cat file2
unlink file2
ls
echo "add more content" >> file1
cat file1
ls
rmdir another_directory
ls
mkdir a_new_dir
ls
echo "a new file2" > file2
ls
fusermount -u /tmp/chaohao1
./a1fs disk_image /tmp/chaohao1
cd /tmp/chaohao1
ls
cd dir1
ls
cat file1
cat file2