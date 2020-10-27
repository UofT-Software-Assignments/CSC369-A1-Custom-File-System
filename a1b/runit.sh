# constants 
root="/tmp/myrskogr"
image="disk_image"
image_size=64K
inodes=16

# Assumptions
# we assume the  script is ran in the working repository
# We assume that root is an existing empty directory

#prepare root and image
fusermount -u ${root}
truncate -s ${image_size} ${image}

# format and mount fs image
make
./mkfs.a1fs -f -i ${inodes} ${image}
./a1fs ${image} ${root}

# creating a simple directory
mkdir ${root}/dir1
ls ${root}

# create a file and append some content to it
touch ${root}/dir1/file1
echo "file1 content" >> ${root}/dir1/file1
ls ${root}/dir1

# make another file with some content
echo "another file" > ${root}/dir1/file2
ls ${root}/dir1

# make another directory
mkdir ${root}/dir1/another_directory
ls ${root}/dir1

# printing contents of the files
cat ${root}/dir1/file1
cat ${root}/dir1/file2

# remove a file
unlink ${root}/dir1/file2
ls ${root}/dir1

# appending more content to a file
echo "file1 added content" >> ${root}/dir1/file1
cat ${root}/dir1/file1
ls ${root}/dir1

# removing a directory
rmdir ${root}/dir1/another_directory
ls ${root}/dir1

# making a new directory
mkdir ${root}/dir1/a_new_dir
ls ${root}/dir1

# making a new file
echo "file2 contents" > ${root}/dir1/file2
ls ${root}/dir1

# unmount the file system
fusermount -u ${root}

# remount the file system
./a1fs ${image} ${root}

# check the contents of the file system
ls ${root}
ls ${root}/dir1
cat ${root}/dir1/file1
cat ${root}/dir1/a_new_dir
cat ${root}/dir1/file2

# unmount the file system
fusermount -u ${root}