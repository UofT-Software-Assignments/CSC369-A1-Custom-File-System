# CSC369 A1 Custom File System

A custom implementation of many common linux file system commands using FUSE file system

## Disclaimer

This repository is not solely my own code, this was an assignment completed for the University of Toronto's course CSC369 - Operating Systems, and is formed by my contributions to the starter code of the assignment

The following commands are implemented:
- mkdir r
- rmdir 
- touch 
- echo 
- cat 
- unlink 
- rm 
- cp 
- ls 
- truncate 


The following are my contributions to this repository:

in mkfs.c:

  - alfs_is_present
  - mkfs
 
I implemented the code in these functions to check the presence of a formatted file system and to format a new file system on an image

I implemented every function in a1fs.c eith the exception of get_fs(). The functions in this file are the repective implementations of each linux command listed above

The rest of the code with the exception of my above contributions is credited to the assignment creators.
  
