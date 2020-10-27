# We expect the script to be ran in the working repository
# make needs to be called before the script is called
# we also assumed /tmp/chaohao1 is an available directory that exists.
#
# Creating an image and mounting the file system
truncate -s 64K disk_image
./mkfs.a1fs -i 16 disk_image
./a1fs disk_image /tmp/chaohao1
# creating a simple directory
mkdir /tmp/chaohao1/dir1
ls /tmp/chaohao1
# create a file and append some content to it
touch /tmp/chaohao1/dir1/file1
echo "random content" >> /tmp/chaohao1/dir1/file1
ls /tmp/chaohao1/dir1
# make another file with some content
echo "another file" > /tmp/chaohao1/dir1/file2
ls /tmp/chaohao1/dir1
# make another directory
mkdir /tmp/chaohao1/dir1/another_directory
ls /tmp/chaohao1/dir1
# printing contents of the files
cat /tmp/chaohao1/dir1/file1
cat /tmp/chaohao1/dir1/file2
# remove a file
unlink /tmp/chaohao1/dir1/file2
ls /tmp/chaohao1/dir1
# appending more content to a file
echo "add more content" >> /tmp/chaohao1/dir1/file1
cat /tmp/chaohao1/dir1/file1
ls /tmp/chaohao1/dir1
# removing a directory
rmdir /tmp/chaohao1/dir1/another_directory
ls /tmp/chaohao1/dir1
# making a new directory
mkdir /tmp/chaohao1/dir1/a_new_dir
ls /tmp/chaohao1/dir1
# making a new file
echo "a new file2" > /tmp/chaohao1/dir1/file2
ls /tmp/chaohao1/dir1
# unmount the file system
fusermount -u /tmp/chaohao1
# remount the file system
./a1fs disk_image /tmp/chaohao1
# check the contents of the file system
ls /tmp/chaohao1
ls /tmp/chaohao1/dir1
cat /tmp/chaohao1/dir1/file1
cat /tmp/chaohao1/dir1/a_new_dir
cat /tmp/chaohao1/dir1/file2
# unmount the file system
fusermount -u /tmp/chaohao1