/*
 * This code is provided solely for the personal and private use of students
 * taking the CSC369H course at the University of Toronto. Copying for purposes
 * other than this use is expressly prohibited. All forms of distribution of
 * this code, including but not limited to public repositories on GitHub,
 * GitLab, Bitbucket, or any other online platform, whether as given or with
 * any changes, are expressly prohibited.
 *
 * Authors: Alexey Khrabrov, Karen Reid
 *
 * All of the files in this directory and all subdirectories are:
 * Copyright (c) 2020 Karen Reid
 */

/**
 * CSC369 Assignment 1 - a1fs driver implementation.
 */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <time.h>
#include <libgen.h>

// Using 2.9.x FUSE API
#define FUSE_USE_VERSION 29
#include <fuse.h>

#include "a1fs.h"
#include "fs_ctx.h"
#include "options.h"
#include "map.h"

//NOTE: All path arguments are absolute paths within the a1fs file system and
// start with a '/' that corresponds to the a1fs root directory.
//
// For example, if a1fs is mounted at "~/my_csc369_repo/a1b/mnt/", the path to a
// file at "~/my_csc369_repo/a1b/mnt/dir/file" (as seen by the OS) will be
// passed to FUSE callbacks as "/dir/file".
//
// Paths to directories (except for the root directory - "/") do not end in a
// trailing '/'. For example, "~/my_csc369_repo/a1b/mnt/dir/" will be passed to
// FUSE callbacks as "/dir".


/**
 * Initialize the file system.
 *
 * Called when the file system is mounted. NOTE: we are not using the FUSE
 * init() callback since it doesn't support returning errors. This function must
 * be called explicitly before fuse_main().
 *
 * @param fs    file system context to initialize.
 * @param opts  command line options.
 * @return      true on success; false on failure.
 */
static bool a1fs_init(fs_ctx *fs, a1fs_opts *opts)
{
	// Nothing to initialize if only printing help
	if (opts->help) return true;

	size_t size;
	void *image = map_file(opts->img_path, A1FS_BLOCK_SIZE, &size);
	if (!image) return false;

	return fs_ctx_init(fs, image, size);
}

/**
 * Cleanup the file system.
 *
 * Called when the file system is unmounted. Must cleanup all the resources
 * created in a1fs_init().
 */
static void a1fs_destroy(void *ctx)
{
	fs_ctx *fs = (fs_ctx*)ctx;
	if (fs->image) {
		munmap(fs->image, fs->size);
		fs_ctx_destroy(fs);
	}
}

/** Get file system context. */
static fs_ctx *get_fs(void)
{
	return (fs_ctx*)fuse_get_context()->private_data;
}


/**
 * Get file system statistics.
 *
 * Implements the statvfs() system call. See "man 2 statvfs" for details.
 * The f_bfree and f_bavail fields should be set to the same value.
 * The f_ffree and f_favail fields should be set to the same value.
 * The following fields can be ignored: f_fsid, f_flag.
 * All remaining fields are required.
 *
 * Errors: none
 *
 * @param path  path to any file in the file system. Can be ignored.
 * @param st    pointer to the struct statvfs that receives the result.
 * @return      0 on success; -errno on error.
 */
static int a1fs_statfs(const char *path, struct statvfs *st)
{
	(void)path;// unused
	fs_ctx *fs = get_fs();

	//TODO: fill in the rest of required fields based on the information stored
	// in the superblock

	memset(st, 0, sizeof(*st));
	st->f_bsize   = A1FS_BLOCK_SIZE;  			/* Filesystem block size */
	st->f_frsize  = A1FS_BLOCK_SIZE;  			/* Fragment size */
	st->f_blocks = fs->size / A1FS_BLOCK_SIZE;  /* Size of fs in f_frsize units */
	st->f_bfree = fs->sb->free_blocks_count;    /* Number of free blocks */
	st->f_bavail = fs->sb->free_blocks_count;   /* Number of free blocks for
													unprivileged users */
	st->f_files = fs->sb->inodes_count;    		/* Number of inodes */
	st->f_ffree = fs->sb->free_inodes_count;    /* Number of free inodes */
	st->f_favail = fs->sb->free_inodes_count;   /* Number of free inodes for
													unprivileged users */
	/*st->f_fsid = ; */    /* Filesystem ID */
	/*st->f_flag = ; */    /* Mount flags */
	st->f_namemax = A1FS_NAME_MAX;  /* Maximum filename length */
	

	return 0;
}

/**
 * return array of directory entries contained in directory
 * NOTE: array pointer must be freed when using this function
 * 
**/
a1fs_dentry *get_entries(a1fs_inode *directory, fs_ctx *fs){

	a1fs_dentry *entries;
	if((entries = malloc(directory->size)) == NULL) return NULL;

	//used to keep track of where we are copying data to in memcpy
	void *dest = (void *)entries;

	//pointer to array of extents for directory
	a1fs_extent *extents = fs->image + (fs->sb->first_data_block + directory->extents) * A1FS_BLOCK_SIZE;

	//loop through extents
	for(int i = 0; i < directory->num_extents; i++){
		a1fs_extent extent = extents[i];
		
		//loop through data blocks in the current extent
		for(unsigned int j = extent.start; j < extent.start + extent.count; j++){
			//point to start of current data block
			const void *src = fs->image + (fs->sb->first_data_block + j) * A1FS_BLOCK_SIZE;
			
			//if last data block, only copy whats left in directory
			if(i == (directory->num_extents - 1) && j == (extent.start + extent.count - 1)){
				memcpy(dest, src, directory->size % A1FS_BLOCK_SIZE);
			} // else copy whole block
			else{
				memcpy(dest, src, A1FS_BLOCK_SIZE);
			}
			//move dest so we dont overwrite whats already copied into entries 
			dest += A1FS_BLOCK_SIZE;

		}
	}
	return entries;
	
}

/**
 * populate ino with the inode number associated with entry_name in directory
 * return 0 on success, else return -errno
**/
int find_entry_inode(a1fs_inode *directory, char *entry_name, int *ino, fs_ctx *fs){
    
	a1fs_dentry *entries;
	if((entries = get_entries(directory, fs)) == NULL) return -ENOMEM;

	int num_entries = directory->size / sizeof(a1fs_dentry);

	for(int i = 0; i < num_entries; i++){
		a1fs_dentry entry = entries[i];

		if(strcmp(entry.name, entry_name) == 0){
			free(entries);
			*ino = entry.ino;
			return 0;
		}
	}
	free(entries);
	return -ENOENT;
	
}

/**
 * populate the a1fs_inode result
 * return 0 on success, errno on error
**/
int path_lookup(const char *path, a1fs_inode **result, fs_ctx *fs){
	if(path[0] != '/') {
        fprintf(stderr, "Not an absolute path\n");
        return -1;
    }
	//copy path to pathstring bc path is const;
	char pathstring[A1FS_PATH_MAX];
    strncpy(pathstring, path, A1FS_PATH_MAX - 1);
    pathstring[A1FS_PATH_MAX - 1] = '\0'; // null terminate
	int inode_number = 0; //start at root inode
	int error;

	//array of inodes in inode table
	a1fs_inode *itable = (a1fs_inode *)(fs->image + (fs->sb->inode_table) * A1FS_BLOCK_SIZE);

	char *component = strtok(pathstring, "/");
	while(component != NULL){
        a1fs_inode *directory = &(itable[inode_number]);
		if(directory->mode != S_IFDIR) return -ENOTDIR;
        if((error = find_entry_inode(directory, component, &inode_number, fs)) != 0) return error;
        component = strtok(NULL, "/");
    }
	*result = &itable[inode_number];
	return 0;
}

unsigned int round_up_divide(unsigned int x, unsigned int y){
	return x / y + ((x % y) != 0);
}

/**
 * Get file or directory attributes.
 *
 * Implements the lstat() system call. See "man 2 lstat" for details.
 * The following fields can be ignored: st_dev, st_ino, st_uid, st_gid, st_rdev,
 *                                      st_blksize, st_atim, st_ctim.
 * All remaining fields are required.
 *
 * NOTE: the st_blocks field is measured in 512-byte units (disk sectors).
 *
 * Errors:
 *   ENAMETOOLONG  the path or one of its components is too long.
 *   ENOENT        a component of the path does not exist.
 *   ENOTDIR       a component of the path prefix is not a directory.
 *
 * @param path  path to a file or directory.
 * @param st    pointer to the struct stat that receives the result.
 * @return      0 on success; -errno on error;
 */
static int a1fs_getattr(const char *path, struct stat *st)
{
	if (strlen(path) >= A1FS_PATH_MAX) return -ENAMETOOLONG;
	fs_ctx *fs = get_fs();

	memset(st, 0, sizeof(*st));

	//TODO: lookup the inode for given path and, if it exists, fill in the
	// required fields based on the information stored in the inode

	a1fs_inode *inode;
	int error = path_lookup(path, &inode, fs);
	if(error != 0) return error;
	
	//NOTE: all the fields set below are required and must be set according
	// to the information stored in the corresponding inode
	st->st_ino = inode->inode_number;
	st->st_mode = inode->mode;
	st->st_nlink = inode->links;
	st->st_size = inode->size;
	st->st_blocks = round_up_divide(inode->size, A1FS_BLOCK_SIZE) * (A1FS_BLOCK_SIZE / 512);
	st->st_mtim = inode->mtime;
	
	return 0;
}

/**
 * Read a directory.
 *
 * Implements the readdir() system call. Should call filler(buf, name, NULL, 0)
 * for each directory entry. See fuse.h in libfuse source code for details.
 *
 * Assumptions (already verified by FUSE using getattr() calls):
 *   "path" exists and is a directory.
 *
 * Errors:
 *   ENOMEM  not enough memory (e.g. a filler() call failed).
 *
 * @param path    path to the directory.
 * @param buf     buffer that receives the result.
 * @param filler  function that needs to be called for each directory entry.
 *                Pass 0 as offset (4th argument). 3rd argument can be NULL.
 * @param offset  unused.
 * @param fi      unused.
 * @return        0 on success; -errno on error.
 */
static int a1fs_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                        off_t offset, struct fuse_file_info *fi)
{
	(void)offset;// unused
	(void)fi;// unused
	fs_ctx *fs = get_fs();

	//TODO: lookup the directory inode for given path and iterate through its
	// directory entries
	
	a1fs_inode *directory;
	path_lookup(path, &directory, fs);
	a1fs_dentry *entries;
	if((entries = get_entries(directory, fs)) == NULL) return -ENOMEM;

	int num_entries = directory->size / sizeof(a1fs_dentry);

	if(filler(buf, "." , NULL, 0) + filler(buf, "..", NULL, 0) > 0) return -ENOMEM;
	

	for(int i = 0; i < num_entries; i++){
		a1fs_dentry entry = entries[i];
		if(filler(buf, entry.name, NULL, 0) == 1) return -ENOMEM;
	}
	free(entries);
		
	return 0;

}

/**
 * search the given bitmap for an extent of length length
 * 
 * populate the extent struct given with the first extent found of length length, or,
 * if none exist, the extent of longest length
 * 
 * @param bitmap    pointer to the start of the bitmap
 * @param num_bits  the number of bits belonging ot the bitmap
 * @param length    the length of the extent we are searching for
 * @param extent    extent struct to populate 
 * @return          0 on success, -1 on error e.g no space
 */
int search_bitmap(unsigned char *bitmap, int num_bits, unsigned int length, a1fs_extent *extent){
	int num_bytes = num_bits / 8;
	int bits_iterated= 0;
	unsigned int start = 0;
	unsigned int count = 0;
	int i = 0;
	int remaining_bits = 0;
	extent->count = 0;

	while (i < num_bytes + 1){
		// the 8 bits that we are iterating
		unsigned char current_byte = bitmap[i];

		// number of bits of the current byte that needs to be counted
		if ((num_bits - bits_iterated) >= 8){
			remaining_bits = 8;
		} else {
			remaining_bits = num_bits - bits_iterated;
		}
		
		// iterate through each bit to see if an inode is free
		for (int n = 0; n < remaining_bits; n++){
			if (!(current_byte & (1 << (7 - n)))){
				if(count == 0){
					start = bits_iterated + n;
				}
				count++;

				if(count == length){
					extent->start = start;
					extent->count = count;
					return 0;
				}
			}else{
				if(count > extent->count){
					extent->start = start;
					extent->count = count;
				} 
				count = 0;
			}
		}
		// increase the counter for number of bits iterated
		bits_iterated += remaining_bits;
		i++;
	}
	if(extent->count == 0) return -1;
	return 0;
}

/**
 * switch bit bit_number to a 1 in bitmap
**/
void allocate_bit(unsigned char *bitmap, int bit_number){
	int byte_number = bit_number / 8;
	int bit_number_in_byte = bit_number % 8;
	bitmap[byte_number] = bitmap[byte_number] | (1 << (7 - bit_number_in_byte));
}

/**
 * switch all bits from extent start to extent start + extent count to 1
**/
void allocate_extent(unsigned char *bitmap, a1fs_extent *extent){
	for(unsigned int i = extent->start; i < extent->start + extent->count; i++){
		allocate_bit(bitmap, i);
	}
}

/**
 * Traverses the inode_bitmap and allocate the first available inode
 * return 0 on success, return -1 on error
 * 
 * @param fs 			file system context
 * @param inode_number 	index of free inode found, -1 if not found
 * @return int 0 on success, -1 on error
 */
int allocate_inode(fs_ctx *fs, int *inode_number){
    unsigned char *inode_bitmap = fs->image + (fs->sb->inode_bitmap) * A1FS_BLOCK_SIZE;
	a1fs_extent extent;

	if(search_bitmap(inode_bitmap, fs->sb->inodes_count, 1, &extent) == -1) return -1;

	*inode_number = extent.start;

	allocate_bit(inode_bitmap, *inode_number);

	return 0;

}

/**
 * return array of extents belonging to inode
**/
a1fs_extent *get_extents(a1fs_inode *inode, fs_ctx *fs){
	a1fs_extent *extents = fs->image + (fs->sb->first_data_block + inode->extents) * A1FS_BLOCK_SIZE;
	return extents;
}

/**
 * Allocate num_blocks data blocks to inode pointed to by inode
 * 
 * @param inode      pointer to inode to allocate space for
 * @param num_bytes  number of bytes to allocate
 * @param fs         file system context
 * @return           0 on success, -ENOSPC if not enough space available
**/
int allocate_blocks(a1fs_inode *inode, int num_blocks, fs_ctx *fs){

	if(num_blocks > (int)fs->sb->free_blocks_count){
		return -ENOSPC;
	}
	int num_bits_dmap = fs->sb->blocks_count - fs->sb->resv_blocks_count;
	unsigned char *data_bitmap = fs->image + fs->sb->data_bitmap * A1FS_BLOCK_SIZE;

	a1fs_extent extent;
	

	if(inode->num_extents == 0){
		search_bitmap(data_bitmap, num_bits_dmap, 1, &extent);
		allocate_bit(data_bitmap, extent.start);
		inode->extents = extent.start;
	}
	
	a1fs_extent *extents = get_extents(inode, fs);

	while(num_blocks > 0){
		search_bitmap(data_bitmap, num_bits_dmap, num_blocks, &extent);
		allocate_extent(data_bitmap, &extent);
		extents[inode->num_extents] = extent;
		inode->num_extents++;
		num_blocks -= extent.count;
	}

	return 0;
}

/**
 * return the block number of the last data block owned by the file represented by inode
 * 
 * @param inode  pointer to inode struct of the file
 * @param fs     file system context
**/
int get_last_block(a1fs_inode *inode, fs_ctx *fs){
	a1fs_extent *extents = fs->image + (fs->sb->first_data_block + inode->extents) * A1FS_BLOCK_SIZE;
	a1fs_extent last_extent = extents[inode->num_extents - 1];
	int last_block = last_extent.start + last_extent.count;
	return last_block;
}

/**
 * return pointer to the very front of the file represented by inode.
 * Front in this case points to the start of the first byte which is not part of the file
 * but that exists in the last data block owned by this file
 * 
 * @param inode  pointer to inode struct to find the front of
 * @param fs     file system context
**/
void *get_front(a1fs_inode *inode, fs_ctx *fs){
	int last_block = get_last_block(inode, fs);
	void *front = fs->image + (fs->sb->first_data_block + last_block) * A1FS_BLOCK_SIZE 
						+ inode->size % A1FS_BLOCK_SIZE;
	return front;
}

/**
 * add new directory entry to the directory represented by directory
 * with name set to filename and ino set to inode_number
 * 
 * @param directory     pointer to inode representing the directory
 * @param filename      name of the entry
 * @param inode_number  inode number of the inode pointed to by the new entry
 * @param fs            file system context
**/
int add_dentry(a1fs_inode *directory, char *filename, int inode_number, fs_ctx *fs){
	//if directory is full, allocate new block for entry

	int bytes_remainder = directory->size % A1FS_BLOCK_SIZE;
	if(bytes_remainder == 0){
		allocate_blocks(directory, 1, fs);
	}
	//otherwise add entry to last data block

	a1fs_dentry *new_entry = (a1fs_dentry *)(get_front(directory, fs));
	new_entry->ino = inode_number;
	strcpy(new_entry->name, filename);
	directory->size += sizeof(a1fs_dentry);
	return 0;

}

/**
 * Create a directory.
 *
 * Implements the mkdir() system call.
 *
 * Assumptions (already verified by FUSE using getattr() calls):
 *   "path" doesn't exist.
 *   The parent directory of "path" exists and is a directory.
 *   "path" and its components are not too long.
 *
 * Errors:
 *   ENOMEM  not enough memory (e.g. a malloc() call failed).
 *   ENOSPC  not enough free space in the file system.
 *
 * @param path  path to the directory to create.
 * @param mode  file mode bits.
 * @return      0 on success; -errno on error.
 */
static int a1fs_mkdir(const char *path, mode_t mode)
{
	mode = mode | S_IFDIR;
	fs_ctx *fs = get_fs();

	//TODO: create a directory at given path with given mode
	int inode_number;
	if ((allocate_inode(fs, &inode_number)) != 0){
		return -ENOSPC;
	}
	
	// Create the directory only if there exists an available slot
	a1fs_inode *directory = fs->image + (fs->sb->inode_table + inode_number) * sizeof(a1fs_inode);
	directory->inode_number = inode_number;
	directory->mode = mode;
	directory->links = 2;	// ".." and "."
	directory->size = 0;
	clock_gettime(CLOCK_REALTIME, &(directory->mtime));
	directory->num_extents = 0;

	char pathstring[A1FS_PATH_MAX];
	strcpy(pathstring, path);

	char filename[A1FS_NAME_MAX]; //have to copy like this to avoid bugs
	strcpy(filename, basename(pathstring));

	char parent_path[A1FS_PATH_MAX];
	strcpy(parent_path, dirname(pathstring)); 


	a1fs_inode *parent_dir;
	path_lookup((const char *)(parent_path), &parent_dir, fs);

	
	add_dentry(parent_dir, filename, inode_number, fs);
	
	

	//allocate inode in bitmap, get inode number from this
	//no data blocks allocated since directory is empty

	//int inode_number = allocate_inode(fs);

	return 0;
}

/**
 * switch bit bit_number to a 0 in bitmap
**/
void deallocate_bit(unsigned char *bitmap, int bit_number){
	int byte_number = bit_number / 8;
	int bit_number_in_byte = bit_number % 8;
	unsigned char bitmask = ~(1 << (7 - bit_number_in_byte));
	bitmap[byte_number] = bitmap[byte_number] & bitmask;
}

/**
 * deallocate all data blocks pointed to by the inodes extents
 * change inode bitmap at index of the inode's number to 0
 * 
 * @param inode  the inode to deallocate
 * @param fs     file system context
**/
void deallocate_inode(a1fs_inode *inode, fs_ctx *fs){
	a1fs_extent *extents = get_extents(inode, fs);
	unsigned char *data_bitmap = fs->image + fs->sb->data_bitmap * A1FS_BLOCK_SIZE;
	unsigned char *inode_bitmap = fs->image + fs->sb->inode_bitmap * A1FS_BLOCK_SIZE;
	//loop through all data blocks in all extents and deallocate the block
	for(int i = 0; i < inode->num_extents; i++){
		a1fs_extent extent = extents[i];
		for(unsigned int j = extent.start; j < extent.start + extent.count; j++){
				deallocate_bit(data_bitmap, j);
		}
	}

	deallocate_bit(inode_bitmap, inode->inode_number);

}

/**
 * Remove a directory.
 *
 * Implements the rmdir() system call.
 *
 * Assumptions (already verified by FUSE using getattr() calls):
 *   "path" exists and is a directory.
 *
 * Errors:
 *   ENOTEMPTY  the directory is not empty.
 *
 * @param path  path to the directory to remove.
 * @return      0 on success; -errno on error.
 */
static int a1fs_rmdir(const char *path)
{
	fs_ctx *fs = get_fs();

	//TODO: remove the directory at given path (only if it's empty)
	a1fs_inode *directory;
	path_lookup(path, &directory, fs);

	if(directory->size > 0) return -ENOTEMPTY;

	deallocate_inode(directory, fs);
	
	return 0;
}

/**
 * Create a file.
 *
 * Implements the open()/creat() system call.
 *
 * Assumptions (already verified by FUSE using getattr() calls):
 *   "path" doesn't exist.
 *   The parent directory of "path" exists and is a directory.
 *   "path" and its components are not too long.
 *
 * Errors:
 *   ENOMEM  not enough memory (e.g. a malloc() call failed).
 *   ENOSPC  not enough free space in the file system.
 *
 * @param path  path to the file to create.
 * @param mode  file mode bits.
 * @param fi    unused.
 * @return      0 on success; -errno on error.
 */
static int a1fs_create(const char *path, mode_t mode, struct fuse_file_info *fi)
{
	(void)fi;// unused
	assert(S_ISREG(mode));
	fs_ctx *fs = get_fs();

	//TODO: create a file at given path with given mode

	
	int error, inode_number;
	if((error = allocate_inode(fs, &inode_number)) == -1) return -ENOSPC;

	a1fs_inode *inode = fs->image + fs->sb->inode_table * A1FS_BLOCK_SIZE 
						+ inode_number * sizeof(a1fs_inode);
	
	inode->mode = mode;
	inode->links = 1;
	inode->size = 0;
	clock_gettime(CLOCK_REALTIME, &(inode->mtime));
	inode->inode_number = inode_number;
	inode->num_extents = 0;

	//split path string into parent directory and filename
	char pathstring[A1FS_PATH_MAX];
	strcpy(pathstring, path);

	char *filename = basename(pathstring);
	char *parent_path = dirname(pathstring);

	a1fs_inode *parent_dir;
	path_lookup((const char *)(parent_path), &parent_dir, fs);

	//append file to parent directory
	//note that the only info given to the parent is relative to the inode
	//hence the process of appending file is the same as that of a directory
	add_dentry(parent_dir, filename, inode_number, fs);
	

	return 0;
}

/**
 * Remove a file.
 *
 * Implements the unlink() system call.
 *
 * Assumptions (already verified by FUSE using getattr() calls):
 *   "path" exists and is a file.
 *
 * Errors: none
 *
 * @param path  path to the file to remove.
 * @return      0 on success; -errno on error.
 */
static int a1fs_unlink(const char *path)
{
	fs_ctx *fs = get_fs();

	//TODO: remove the file at given path
	(void)fs;

	char pathstring[A1FS_PATH_MAX];
	strcpy(pathstring, path);

	char *filename = basename(pathstring);
	char *parent_path = dirname(pathstring);

	a1fs_inode *inode;
	path_lookup(path, &inode, fs);

	// remove link to the inode in its parent directory
	a1fs_inode *parent_inode;
	path_lookup((const char *)parent_path, &parent_inode, fs);
	int num_entries = parent_inode->size / sizeof(a1fs_dentry);
	a1fs_extent *extents = get_extents(parent_inode, fs);
	int i = 0;
	int found = 0;
	// Loop through the extents to look for the entry
	while (i < parent_inode->num_extents && found == 0){
		a1fs_extent extent = extents[i];
		
		// Loop through the datablocks within the current extent
		unsigned int j = extent.start;
		while (j < extent.start + extent.count && found == 0){
			a1fs_dentry *entry = fs->image+(fs->sb->first_data_block + j) * A1FS_BLOCK_SIZE;
			if (entry->ino == inode->inode_number){
				found = 1;
			} else {
				j++;
			}
		}
		if (found == 0){
			i++;
		}
	}
	if (found == 0){
		//Something's messed up
		printf("Can't find file to unlink")
	}
	// j is the datablock number that contains the entry we want to overwrite
	// Now we copy the last entry to overwrite this entry.
	unsigned int last_block_num = extents[parent_inode->num_extents-1].start + extents[parent_inode->num_extents-1].count - 1
	a1fs_dentry *target_entry = fs->image+(fs->sb->first_data_block + j) * A1FS_BLOCK_SIZE;
	a1fs_dentry *last_entry = fs->image+(fs->sb->first_data_block + last_block_num) * A1FS_BLOCK_SIZE;
	memcpy(target_entry, last_entry, sizeof(a1fs_dentry));

	// Change metadata in parent inode
	extents[parent_inode->num_extents-1].count -= 1;
	parent_inode->size -= sizeof(a1fs_dentry);

	//TODO: remove the datablocks of the file from datablock bitmap

	//TODO: remove the inode from inode bitmap.

	deallocate_inode(inode, fs);

	return 0;
}


/**
 * Change the modification time of a file or directory.
 *
 * Implements the utimensat() system call. See "man 2 utimensat" for details.
 *
 * NOTE: You only need to implement the setting of modification time (mtime).
 *
 * Assumptions (already verified by FUSE using getattr() calls):
 *   "path" exists.
 *
 * Errors: none
 *
 * @param path   path to the file or directory.
 * @param times  timestamps array. See "man 2 utimensat" for details.
 * @return       0 on success; -errno on failure.
 */
static int a1fs_utimens(const char *path, const struct timespec times[2])
{
	fs_ctx *fs = get_fs();

	//TODO: update the modification timestamp (mtime) in the inode for given
	// path with either the time passed as argument or the current time,
	// according to the utimensat man page
	a1fs_inode *inode;
	path_lookup(path, &inode, fs);
	if(times[1].tv_nsec == UTIME_NOW){
		clock_gettime(CLOCK_REALTIME, &(inode->mtime));
	}
	else{
		inode->mtime = times[1]; //times[1] is last modification time
	}
	
	return 0;
}

/**
 * Change the size of a file.
 *
 * Implements the truncate() system call. Supports both extending and shrinking.
 * If the file is extended, the new uninitialized range at the end must be
 * filled with zeros.
 *
 * Assumptions (already verified by FUSE using getattr() calls):
 *   "path" exists and is a file.
 *
 * Errors:
 *   ENOMEM  not enough memory (e.g. a malloc() call failed).
 *   ENOSPC  not enough free space in the file system.
 *
 * @param path  path to the file to set the size.
 * @param size  new file size in bytes.
 * @return      0 on success; -errno on error.
 */
static int a1fs_truncate(const char *path, off_t size)
{
	fs_ctx *fs = get_fs();

	//TODO: set new file size, possibly "zeroing out" the uninitialized range
	(void)path;
	(void)size;
	(void)fs;
	return -ENOSYS;
}


/**
 * Read data from a file.
 *
 * Implements the pread() system call. Must return exactly the number of bytes
 * requested except on EOF (end of file). Reads from file ranges that have not
 * been written to must return ranges filled with zeros. You can assume that the
 * byte range from offset to offset + size is contained within a single block.
 *
 * Assumptions (already verified by FUSE using getattr() calls):
 *   "path" exists and is a file.
 *
 * Errors: none
 *
 * @param path    path to the file to read from.
 * @param buf     pointer to the buffer that receives the data.
 * @param size    buffer size (number of bytes requested).
 * @param offset  offset from the beginning of the file to read from.
 * @param fi      unused.
 * @return        number of bytes read on success; 0 if offset is beyond EOF;
 *                -errno on error.
 */
static int a1fs_read(const char *path, char *buf, size_t size, off_t offset,
                     struct fuse_file_info *fi)
{
	(void)fi;// unused
	fs_ctx *fs = get_fs();

	//TODO: read data from the file at given offset into the buffer
	(void)path;
	(void)buf;
	(void)size;
	(void)offset;
	(void)fs;
	return -ENOSYS;
}

/**
 * Write data to a file.
 *
 * Implements the pwrite() system call. Must return exactly the number of bytes
 * requested except on error. If the offset is beyond EOF (end of file), the
 * file must be extended. If the write creates a "hole" of uninitialized data,
 * the new uninitialized range must filled with zeros. You can assume that the
 * byte range from offset to offset + size is contained within a single block.
 *
 * Assumptions (already verified by FUSE using getattr() calls):
 *   "path" exists and is a file.
 *
 * Errors:
 *   ENOMEM  not enough memory (e.g. a malloc() call failed).
 *   ENOSPC  not enough free space in the file system.
 *
 * @param path    path to the file to write to.
 * @param buf     pointer to the buffer containing the data.
 * @param size    buffer size (number of bytes requested).
 * @param offset  offset from the beginning of the file to write to.
 * @param fi      unused.
 * @return        number of bytes written on success; -errno on error.
 */
static int a1fs_write(const char *path, const char *buf, size_t size,
                      off_t offset, struct fuse_file_info *fi)
{
	(void)fi;// unused
	fs_ctx *fs = get_fs();

	//TODO: write data from the buffer into the file at given offset, possibly
	// "zeroing out" the uninitialized range
	(void)path;
	(void)buf;
	(void)size;
	(void)offset;
	(void)fs;
	return -ENOSYS;
}


static struct fuse_operations a1fs_ops = {
	.destroy  = a1fs_destroy,
	.statfs   = a1fs_statfs,
	.getattr  = a1fs_getattr,
	.readdir  = a1fs_readdir,
	.mkdir    = a1fs_mkdir,
	.rmdir    = a1fs_rmdir,
	.create   = a1fs_create,
	.unlink   = a1fs_unlink,
	.utimens  = a1fs_utimens,
	.truncate = a1fs_truncate,
	.read     = a1fs_read,
	.write    = a1fs_write,
};

int main(int argc, char *argv[])
{
	a1fs_opts opts = {0};// defaults are all 0
	struct fuse_args args = FUSE_ARGS_INIT(argc, argv);
	if (!a1fs_opt_parse(&args, &opts)) return 1;

	fs_ctx fs = {0};
	if (!a1fs_init(&fs, &opts)) {
		fprintf(stderr, "Failed to mount the file system\n");
		return 1;
	}

	return fuse_main(args.argc, args.argv, &a1fs_ops, &fs);
}
