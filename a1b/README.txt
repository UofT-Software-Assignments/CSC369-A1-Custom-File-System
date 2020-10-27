Functionality that we've tested that works:
- mkdir dir
- rmdir dir
- touch file
- echo text > file
- echo test >> file
- cat file
- unlink file
- rm -r dir
- cp file dir
- truncate -s size new_file
- ls (-s, -lh, -la, -a) dir
- truncate then write (python snippet of small_file autotest)
- truncate -s +size existing_file (increasing file size)
- truncate -s -size existing_file (decreasing file size)

Funtionality that we've found to be bugged:
...

