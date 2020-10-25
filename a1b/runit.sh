truncate -s 64K disk_image
./mkfs.a1fs -i 16 disk_image
./a1fs disk_image /tmp/chaohao1
mkdir /tmp/chaohao1/dir1
ls /tmp/chaohao1
touch /tmp/chaohao1/dir1/file1
echo "random content" >> /tmp/chaohao1/dir1/file1
ls /tmp/chaohao1/dir1
echo "another file" > /tmp/chaohao1/dir1/file2
ls /tmp/chaohao1/dir1
mkdir /tmp/chaohao1/dir1/another_directory
ls /tmp/chaohao1/dir1
cat /tmp/chaohao1/dir1/file1
cat /tmp/chaohao1/dir1/file2
unlink /tmp/chaohao1/dir1/file2
ls /tmp/chaohao1/dir1
echo "add more content" >> /tmp/chaohao1/dir1/file1
cat /tmp/chaohao1/dir1/file1
ls /tmp/chaohao1/dir1
rmdir /tmp/chaohao1/dir1/another_directory
ls /tmp/chaohao1/dir1
mkdir /tmp/chaohao1/dir1/a_new_dir
ls /tmp/chaohao1/dir1
echo "a new file2" > /tmp/chaohao1/dir1/file2
ls /tmp/chaohao1/dir1
fusermount -u /tmp/chaohao1
./a1fs disk_image /tmp/chaohao1
ls /tmp/chaohao1
ls /tmp/chaohao1/dir1
cat /tmp/chaohao1/dir1/file1
cat /tmp/chaohao1/dir1/file2