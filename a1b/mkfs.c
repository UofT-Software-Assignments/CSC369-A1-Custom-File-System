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
 * CSC369 Assignment 1 - a1fs formatting tool.
 */

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
#include <time.h>

#include "a1fs.h"
#include "map.h"


/** Command line options. */
typedef struct mkfs_opts {
	/** File system image file path. */
	const char *img_path;
	/** Number of inodes. */
	size_t n_inodes;

	/** Print help and exit. */
	bool help;
	/** Overwrite existing file system. */
	bool force;
	/** Zero out image contents. */
	bool zero;

} mkfs_opts;

static const char *help_str = "\
Usage: %s options image\n\
\n\
Format the image file into a1fs file system. The file must exist and\n\
its size must be a multiple of a1fs block size - %zu bytes.\n\
\n\
Options:\n\
    -i num  number of inodes; required argument\n\
    -h      print help and exit\n\
    -f      force format - overwrite existing a1fs file system\n\
    -z      zero out image contents\n\
";

static void print_help(FILE *f, const char *progname)
{
	fprintf(f, help_str, progname, A1FS_BLOCK_SIZE);
}


static bool parse_args(int argc, char *argv[], mkfs_opts *opts)
{
	char o;
	while ((o = getopt(argc, argv, "i:hfvz")) != -1) {
		switch (o) {
			case 'i': opts->n_inodes = strtoul(optarg, NULL, 10); break;

			case 'h': opts->help  = true; return true;// skip other arguments
			case 'f': opts->force = true; break;
			case 'z': opts->zero  = true; break;

			case '?': return false;
			default : assert(false);
		}
	}

	if (optind >= argc) {
		fprintf(stderr, "Missing image path\n");
		return false;
	}
	opts->img_path = argv[optind];

	if (opts->n_inodes == 0) {
		fprintf(stderr, "Missing or invalid number of inodes\n");
		return false;
	}
	return true;
}


/** Determine if the image has already been formatted into a1fs. */
static bool a1fs_is_present(void *image)
{
	//TODO: check if the image already contains a valid a1fs superblock
	const struct a1fs_superblock *sb = (const struct a1fs_superblock *)(image);
	if (sb->magic != A1FS_MAGIC){
		return false;
	}
	return true;
}

unsigned int round_up_divide(unsigned int x, unsigned int y){
	return x / y + ((x % y) != 0);
}
/**
 * Format the image into a1fs.
 *
 * NOTE: Must update mtime of the root directory.
 *
 * @param image  pointer to the start of the image.
 * @param size   image size in bytes.
 * @param opts   command line options.
 * @return       true on success;
 *               false on error, e.g. options are invalid for given image size.
 */
static bool mkfs(void *image, size_t size, mkfs_opts *opts)
{
	//TODO: initialize the superblock and create an empty root directory
	//NOTE: the mode of the root directory inode should be set to S_IFDIR | 0777
	
	
	unsigned int inodes_count = opts->n_inodes;
	unsigned int blocks_count = size / A1FS_BLOCK_SIZE;
	unsigned int inodes_per_block = A1FS_BLOCK_SIZE / sizeof(a1fs_inode);
	
	//find number of blocks for inode table
	unsigned int num_blocks_itable = round_up_divide(inodes_count, inodes_per_block);

	//find number of blocks needed for the inode bitmap
	unsigned int num_blocks_imap = round_up_divide(inodes_count, (unsigned int)(A1FS_BLOCK_SIZE));

	//count number blocks left after allocating for superblock, inode table, inode bitmap
	unsigned int num_blocks_left = blocks_count - 1 - num_blocks_itable - num_blocks_imap;

	if(num_blocks_left < 2){
		return false; // options were invalid to leave less than 2 blocks for data bitmap + data blocks
	}

	//find number blocks needed for databitmap
	unsigned int  num_blocks_dmap = round_up_divide(num_blocks_left, A1FS_BLOCK_SIZE);
	//subtract number of 4096 bits that dont need representing
	num_blocks_dmap -= num_blocks_dmap / A1FS_BLOCK_SIZE;

	//find total number data blocks reserved
	unsigned int resv_blocks_count = 1 + num_blocks_imap + num_blocks_itable + num_blocks_dmap;

	//initialize free_inodes_count to inodes_count - 1 for root directory
	unsigned int free_inodes_count = inodes_count - 1;

	//initialize free_blocks_count to blocks_count - reserved blocks
	unsigned int free_blocks_count = blocks_count - resv_blocks_count;

	//initialize pointers

	a1fs_blk_t data_bitmap = 1;
	a1fs_ino_t inode_bitmap = data_bitmap + num_blocks_dmap;
	a1fs_ino_t inode_table = inode_bitmap + num_blocks_imap;
	a1fs_blk_t first_data_block = inode_table + num_blocks_itable;

	a1fs_superblock *sb = (a1fs_superblock *)image;

	//write superblock to image (taken care of by mmap, image acts like pointer into actual file)
	
	sb->magic = A1FS_MAGIC;
	sb->size = size;
	sb->inodes_count = inodes_count;
	sb->blocks_count = blocks_count;
	sb->resv_blocks_count = resv_blocks_count;
	sb->free_inodes_count = free_inodes_count;
	sb->free_blocks_count = free_blocks_count;
	sb->data_bitmap = data_bitmap;
	sb->inode_bitmap = inode_bitmap;
	sb->inode_table = inode_table;
	sb->first_data_block = first_data_block;

	//TODO 
	//initialize root directory !

	//cast data bitmap into array of unsigned char/ array of bytes
	unsigned char *data_bitmap_as_array = image + sb->data_bitmap * A1FS_BLOCK_SIZE;
	unsigned char *inode_bitmap_as_array = image + sb->inode_bitmap * A1FS_BLOCK_SIZE;
	memset(data_bitmap_as_array, 0, num_blocks_dmap * A1FS_BLOCK_SIZE);
	memset(inode_bitmap_as_array, 0, num_blocks_imap * A1FS_BLOCK_SIZE);
	
	inode_bitmap_as_array[0] = 1 << 7; // = 1000 0000
	
	a1fs_inode *root_inode = image + sb->inode_table * A1FS_BLOCK_SIZE;

	//populate root inode metadata
	root_inode->mode = S_IFDIR;
	root_inode->size = 0;
	root_inode->links = 2;
	clock_gettime(CLOCK_REALTIME, &(root_inode->mtime));
	root_inode->inode_number = 0;
	root_inode->num_extents = 0;
	root_inode->extents = -1; //initialize to -1 when file is empty
	

	return true;
}


int main(int argc, char *argv[])
{
	mkfs_opts opts = {0};// defaults are all 0
	if (!parse_args(argc, argv, &opts)) {
		// Invalid arguments, print help to stderr
		print_help(stderr, argv[0]);
		return 1;
	}
	if (opts.help) {
		// Help requested, print it to stdout
		print_help(stdout, argv[0]);
		return 0;
	}

	// Map image file into memory
	size_t size;
	void *image = map_file(opts.img_path, A1FS_BLOCK_SIZE, &size);
	if (image == NULL) return 1;

	// Check if overwriting existing file system
	int ret = 1;
	if (!opts.force && a1fs_is_present(image)) {
		fprintf(stderr, "Image already contains a1fs; use -f to overwrite\n");
		goto end;
	}

	if (opts.zero) memset(image, 0, size);
	if (!mkfs(image, size, &opts)) {
		fprintf(stderr, "Failed to format the image\n");
		goto end;
	}

	ret = 0;
end:
	munmap(image, size);
	return ret;
}
