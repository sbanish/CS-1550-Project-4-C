/*
	FUSE: Filesystem in Userspace
	Copyright (C) 2001-2007  Miklos Szeredi <miklos@szeredi.hu>

	This program can be distributed under the terms of the GNU GPL.
	See the file COPYING.
*/

#define	FUSE_USE_VERSION 26

#include <fuse.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <math.h>

//size of a disk block
#define	BLOCK_SIZE 512

//we'll use 8.3 filenames
#define	MAX_FILENAME 8
#define	MAX_EXTENSION 3

//How many files can there be in one directory?
#define MAX_FILES_IN_DIR (BLOCK_SIZE - sizeof(int)) / ((MAX_FILENAME + 1) + (MAX_EXTENSION + 1) + sizeof(size_t) + sizeof(long))

//The attribute packed means to not align these things
struct cs1550_directory_entry
{
	int nFiles;	//How many files are in this directory.
				//Needs to be less than MAX_FILES_IN_DIR

	struct cs1550_file_directory
	{
		char fname[MAX_FILENAME + 1];	//filename (plus space for nul)
		char fext[MAX_EXTENSION + 1];	//extension (plus space for nul)
		size_t fsize;					//file size
		long nStartBlock;				//where the first block is on disk
	} __attribute__((packed)) files[MAX_FILES_IN_DIR];	//There is an array of these

	//This is some space to get this to be exactly the size of the disk block.
	//Don't use it for anything.  
	char padding[BLOCK_SIZE - MAX_FILES_IN_DIR * sizeof(struct cs1550_file_directory) - sizeof(int)];
} ;

typedef struct cs1550_root_directory cs1550_root_directory;

#define MAX_DIRS_IN_ROOT (BLOCK_SIZE - sizeof(int)) / ((MAX_FILENAME + 1) + sizeof(long))

struct cs1550_root_directory
{
	int nDirectories;	//How many subdirectories are in the root
						//Needs to be less than MAX_DIRS_IN_ROOT
	struct cs1550_directory
	{
		char dname[MAX_FILENAME + 1];	//directory name (plus space for nul)
		long nStartBlock;				//where the directory block is on disk
	} __attribute__((packed)) directories[MAX_DIRS_IN_ROOT];	//There is an array of these

	//This is some space to get this to be exactly the size of the disk block.
	//Don't use it for anything.  
	char padding[BLOCK_SIZE - MAX_DIRS_IN_ROOT * sizeof(struct cs1550_directory) - sizeof(int)];
} ;


typedef struct cs1550_directory_entry cs1550_directory_entry;

//How much data can one block hold?
#define	MAX_DATA_IN_BLOCK (BLOCK_SIZE - sizeof(long))

struct cs1550_disk_block
{
	//The next disk block, if needed. This is the next pointer in the linked 
	//allocation list
	long nNextBlock;

	//And all the rest of the space in the block can be used for actual data
	//storage.
	char data[MAX_DATA_IN_BLOCK];
};

typedef struct cs1550_disk_block cs1550_disk_block;

/*
 * Called whenever the system wants to know the file attributes, including
 * simply whether the file exists or not. 
 *
 * man -s 2 stat will show the fields of a stat structure
 */
static int cs1550_getattr(const char *path, struct stat *stbuf)
{
	int res = 0;
	bool is_root_dir = false;

	printf("ENTERING GETATTR(), path = %s\n", path);

	char directory[MAX_FILENAME+1];
	char filename[MAX_FILENAME+1];
	char extension[MAX_EXTENSION+1];

	memset(directory, 0, sizeof(char)*(MAX_FILENAME+1));
	memset(filename, 0, sizeof(char)*(MAX_FILENAME+1));
	memset(extension, 0, sizeof(char)*(MAX_EXTENSION+1));

	int splice_result = sscanf(path, "/%[^/]/%[^.].%s", directory, filename, extension);

   
	//is path the root dir?
	if (path[0] == '/' && chrcnt(path, '/') == 1) {
		if(strlen(path)==1) is_root_dir = true;
		else {
			printf("GETATTR(): Removing slash from path %s\n", path);
			rmchr(path, '/');
		}

	//Check if name is subdirectory
	/* 
		//Might want to return a structure with these fields
		stbuf->st_mode = S_IFDIR | 0755;
		stbuf->st_nlink = 2;
		res = 0; //no error
	*/

	//Check if name is a regular file
	/*
		//regular file, probably want to be read and write
		stbuf->st_mode = S_IFREG | 0666; 
		stbuf->st_nlink = 1; //file links
		stbuf->st_size = 0; //file size - make sure you replace with real size!
		res = 0; // no error
	*/

		//Else return that path doesn't exist
		//res = -ENOENT;
	}
	memset(stbuf, 0, sizeof(struct stat));

	if (is_root_dir == true)
	{
		printf("GETATTR: %s is the root directory!\n", path);
		stbuf->st_mode = S_IFDIR | 0755;
		stbuf->st_nlink = 2;
	}
	else
	{
		if(is_a_directory(path)==true)
		{
			printf("GETATTR: %s is a directory!\n", path);
			stbuf->st_mode = S_IFDIR | 0755;
			stbuf->st_nlink = 2;
			res = 0;
		}
		else
		{
			size_t filesize = 0;
			printf("GETATTR(): is %s a file?\n", path);
			if(is_a_file(filename, directory, extension, &filesize)==true)
			{
				printf("GETATTR(): Yes, returned value in filesize is %d\n", (int)filesize);
				if(splice_result == 2){
					printf("%s is a file with no extension!", path);
					stbuf->st_mode = S_IFREG | 0666;
					stbuf->st_nlink = 1;
					stbuf->st_size = filesize;
					res = 0;
				}
				else
				{
					printf("%s is a file with extension!\n", path);
					stbuf->st_mode = S_IFREG | 0666;
					stbuf->st_nlink = 1;
					stbuf->st_size = filesize;
					res = 0;
				}
			}
			else
			{
				printf("%s RETURNING ERROR: NO ENTRY!\n", path);
				res = -ENOENT;
			}
		}
	}
}

/* 
 * Called whenever the contents of a directory are desired. Could be from an 'ls'
 * or could even be when a user hits TAB to do autocompletion
 */
static int cs1550_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
			 off_t offset, struct fuse_file_info *fi)
{
	//Since we're building with -Wall (all warnings reported) we need
	//to "use" every parameter, so let's just cast them to void to
	//satisfy the compiler
	(void) offset;
	(void) fi;

	bool dir_found = false;

	FILE *directories_file = fopen("directories", "a+");
	char struct_buffer[sizeof(cs1550_directory_entry)];
	mmset(struct_buffer, 0, sizeof(cs1550_directory_entry));

#if DEBUG_ENABLE
		printf("READDIR: Calling readdir() with path = %s\n", path);
#endif

if(strcmp(path, "/") == 0)
{
	#if DEBUG_ENABLE
		printf("listing of root directory request...\n");
	#endif

	while(fread(struct_buffer, sizeof(cs1550_directory_entry), 1, directories_file))
	{
		#if DEBUG_ENABLE
			printf("Itrating over .directories file...\n");
		#endif

		cs1550_directory_entry *current_dir = (cs1550_directory_entry*)struct_buffer;
		printf("%s\n", current_dir->dname);

		filler(buf, current_dir->dname, NULL, 0);

		dir_found = true;
	}

	if(feof(directories_file))
	{
		fclose(directories_file);
	}
	else
	{
		perror("ERROR: DIRECTORIES FILE READ INTERRUPTED!");
		exit(-1);
	}
}
else
{
	if (path[0] == '/')
	{
		rmchr(path, '/');
	}

	printf("READDIR: FILE LISTING REQUESTED!\n");
	while(fread(struct_buffer, sizeof(cs1550_directory_entry), 1, directories_file))
	{
		cs1550_directory_entry *currenty_dir = (cs1550_directory_entry*)struct buffer;
		int number_of_files = current_dir->nFiles;
		if(strcmp(path, current_dir->dname)==0)
		{
			printf("READDIR(): Target path %s found! Iterating over %d files...\n", current_dir->dname, current_dir->nFiles);
			int i;

			for(i = 0; i < number_of_files; i++)
			{
				printf("READDIR(): file %s detected.\n", current_dir->files[i].fname);
				if(strlen(current_dir->files[i].fext) == 0) filler(buf, current_dir->files[i].fname, NULL, 0);
				else filler(buf, strncat(strncat(current_dir->files[i].fname, ".", 1), current_dir->files[i].fext, MAX_EXTENSION), NULL, 0);
			}
			dir_found = true;
			break;
		}
	}
	fclose(directories_file);

	if(dir_found == false)
	{
		puts("RETURNING ERROR NO ENTRY!");
		return -ENOENT;
	}
	else
	{
		return 0;
	}
}

	//This line assumes we have no subdirectories, need to change
	// if (strcmp(path, "/") != 0)
	// return -ENOENT;

	//the filler function allows us to add entries to the listing
	//read the fuse.h file for a description (in the ../include dir)
	filler(buf, ".", NULL, 0);
	filler(buf, "..", NULL, 0);

	/*
	//add the user stuff (subdirs or files)
	//the +1 skips the leading '/' on the filenames
	filler(buf, newpath + 1, NULL, 0);
	*/
	return 0;
}

/* 
 * Creates a directory. We can ignore mode since we're not dealing with
 * permissions, as long as getattr returns appropriate ones for us.
 */
static int cs1550_mkdir(const char *path, mode_t mode)
{
	(void) path;
	(void) mode;

	if(chrcnt(path, '/')>1)
	{
		printf("MKDIR: %s is being created beyond the root directory! Returning EPERM!\n");
		return -EPERM;
	}

	printf("Entering cs1550_mkdir() with path = %s\n", path);
	if ((strlen(path)-1) > MAX_FILENAME)
	{
		printf("DIRECTORY NAME LENGTH (%d) IS TO LONG!\n", (int)strlen(path)-1);
		return -ENAMETOOLONG;
	}

	if (path[0] == '/'){
		rmchr(path, '/');
	}

	if (is_a_directory(path) == true)
	{
		printf("MKDIR: %s is a directory! Returning EEXISTS!\n", path);
		return -EEXIST;
	}

	cs1550_directory_entry new_entry;
	mmset(&new_entry, 0, sizeof(cs1550_directory_entry));

	strncpy(new_entry.dname, path, MAX_FILENAME+1);
	new_entry.nFiles = 0;

	File *directories_file = fopen(".directores", "a");
	fwrite(&new_entry, sizeof(cs1550_directory_entry), 1, directories_file);
	fclose(directories_file);

	return 0;
}

/* 
 * Removes a directory.
 */
static int cs1550_rmdir(const char *path)
{
	(void) path;
    return 0;
}

/* 
 * Does the actual creation of a file. Mode and dev can be ignored.
 *
 */
static int cs1550_mknod(const char *path, mode_t mode, dev_t dev)
{
	(void) mode;
	(void) dev;
	return 0;
}

/*
 * Deletes a file
 */
static int cs1550_unlink(const char *path)
{
    (void) path;

    return 0;
}

/* 
 * Read size bytes from file into buf starting from offset
 *
 */
static int cs1550_read(const char *path, char *buf, size_t size, off_t offset,
			  struct fuse_file_info *fi)
{
	(void) buf;
	(void) offset;
	(void) fi;
	(void) path;

	//check to make sure path exists
	//check that size is > 0
	//check that offset is <= to the file size
	//read in data
	//set size and return, or error

	size = 0;

	return size;
}

/* 
 * Write size bytes from buf into file starting from offset
 *
 */
static int cs1550_write(const char *path, const char *buf, size_t size, 
			  off_t offset, struct fuse_file_info *fi)
{
	(void) buf;
	(void) offset;
	(void) fi;
	(void) path;

	//check to make sure path exists
	//check that size is > 0
	//check that offset is <= to the file size
	//write data
	//set size (should be same as input) and return, or error

	return size;
}

/******************************************************************************
 *
 *  DO NOT MODIFY ANYTHING BELOW THIS LINE
 *
 *****************************************************************************/

/*
 * truncate is called when a new file is created (with a 0 size) or when an
 * existing file is made shorter. We're not handling deleting files or 
 * truncating existing ones, so all we need to do here is to initialize
 * the appropriate directory entry.
 *
 */
static int cs1550_truncate(const char *path, off_t size)
{
	(void) path;
	(void) size;

    return 0;
}


/* 
 * Called when we open a file
 *
 */
static int cs1550_open(const char *path, struct fuse_file_info *fi)
{
	(void) path;
	(void) fi;
    /*
        //if we can't find the desired file, return an error
        return -ENOENT;
    */

    //It's not really necessary for this project to anything in open

    /* We're not going to worry about permissions for this project, but 
	   if we were and we don't have them to the file we should return an error

        return -EACCES;
    */

    return 0; //success!
}

/*
 * Called when close is called on a file descriptor, but because it might
 * have been dup'ed, this isn't a guarantee we won't ever need the file 
 * again. For us, return success simply to avoid the unimplemented error
 * in the debug log.
 */
static int cs1550_flush (const char *path , struct fuse_file_info *fi)
{
	(void) path;
	(void) fi;

	return 0; //success!
}


//register our new functions as the implementations of the syscalls
static struct fuse_operations hello_oper = {
    .getattr	= cs1550_getattr,
    .readdir	= cs1550_readdir,
    .mkdir	= cs1550_mkdir,
	.rmdir = cs1550_rmdir,
    .read	= cs1550_read,
    .write	= cs1550_write,
	.mknod	= cs1550_mknod,
	.unlink = cs1550_unlink,
	.truncate = cs1550_truncate,
	.flush = cs1550_flush,
	.open	= cs1550_open,
};

//Don't change this.
int main(int argc, char *argv[])
{
	return fuse_main(argc, argv, &hello_oper, NULL);
}
