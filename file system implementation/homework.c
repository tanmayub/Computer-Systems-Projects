/*
 * file:        homework.c
 * description: skeleton file for CS 5600/7600 file system
 *
 * CS 5600, Computer Systems, Northeastern CCIS
 * Peter Desnoyers, November 2016
 */

#define FUSE_USE_VERSION 27

#include <stdlib.h>
#include <stddef.h>
#include <unistd.h>
#include <fuse.h>
#include <fcntl.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <time.h>

#include "fsx600.h"
#include "blkdev.h"

#define DIRENTS_PER_BLOCK (FS_BLOCK_SIZE / sizeof(struct fs_dirent))
#define DIRECT_BLOCKS 6
#define INDIRECT_BLOCKS 256
#define SUPER_BLK_SZ 1

#define MIN(a, b) (a < b ? a : b) /* Macro defined to find the minimum */

extern int homework_part;       /* set by '-part n' command-line option */

/* 
 * disk access - the global variable 'disk' points to a blkdev
 * structure which has been initialized to access the image file.
 *
 * NOTE - blkdev access is in terms of 1024-byte blocks
 */
extern struct blkdev *disk;

/* by defining bitmaps as 'fd_set' pointers, you can use existing
 * macros to handle them. 
 *   FD_ISSET(##, inode_map);
 *   FD_CLR(##, block_map);
 *   FD_SET(##, block_map);
 */
fd_set *inode_map;              /* = malloc(sb.inode_map_size * FS_BLOCK_SIZE); */
fd_set *block_map;
struct fs_inode *fs_inodes;
struct fs_super super_blk;
char *globalBuf;
int blockForIndir1;

static int removeDirOrFile(int, int, struct fs_dirent*, int, int);
static int getInodeNum(char*, struct fs_dirent*, int*);
static int handleDirentsBuffer(struct fs_dirent*, mode_t, char*, int, int);
static void getBaseNameAndParent(const char*, char**, char**);


/* init - this is called once by the FUSE framework at startup. Ignore
 * the 'conn' argument.
 * recommended actions:
 *   - read superblock
 *   - allocate memory, read bitmaps and inodes
 */
void* fs_init(struct fuse_conn_info *conn)
{
	int blk_size;

    disk->ops->read(disk, 0, 1, &super_blk);

	blk_size = super_blk.num_blocks;

	inode_map = (fd_set *)malloc(super_blk.inode_map_sz * blk_size);
	disk->ops->read(disk, SUPER_BLK_SZ, super_blk.inode_map_sz, inode_map);

	block_map = (fd_set *)malloc(super_blk.block_map_sz * blk_size);
	disk->ops->read(disk, SUPER_BLK_SZ + super_blk.inode_map_sz, super_blk.block_map_sz, block_map);

	fs_inodes = (struct fs_inode *)malloc(super_blk.inode_region_sz * blk_size);
	disk->ops->read(disk, SUPER_BLK_SZ + super_blk.inode_map_sz + super_blk.block_map_sz, super_blk.inode_region_sz, fs_inodes);

	//init globalBuf for write functionality
	globalBuf = malloc(FS_BLOCK_SIZE);

    return 0;
}

/* Note on path translation errors:
 * In addition to the method-specific errors listed below, almost
 * every method can return one of the following errors if it fails to
 * locate a file or directory corresponding to a specified path.
 *
 * ENOENT - a component of the path is not present.
 * ENOTDIR - an intermediate component of the path (e.g. 'b' in
 *           /a/b/c) is not a directory
 */

/* note on splitting the 'path' variable:
 * the value passed in by the FUSE framework is declared as 'const',
 * which means you can't modify it. The standard mechanisms for
 * splitting strings in C (strtok, strsep) modify the string in place,
 * so you have to copy the string and then free the copy when you're
 * done. One way of doing this:
 *
 *    char *_path = strdup(path);
 *    int inum = translate(_path);
 *    free(_path);
 */

static int isValidToken(char *token, int inum) 
{
	int inum_ret = 0;
	int i = 0;
	struct fs_dirent *fs_dirents;
	if(S_ISDIR(fs_inodes[inum].mode)) {	
		// allocate memory for fs_dirents
		fs_dirents = malloc(DIRENTS_PER_BLOCK * sizeof(struct fs_dirent));
		// Load the dirents block for the given inode number i.e. read from disk
		if (disk->ops->read(disk, fs_inodes[inum].direct[0], 1, fs_dirents) >= 0)
		{
			// Iterate through the dirents buffer to get the relevant directory/file
			for(i = 0; i < DIRENTS_PER_BLOCK; i++) {
				if(fs_dirents[i].valid && strcmp(token, fs_dirents[i].name) == 0) {
					// Return the corresponding inode number of the matched file/directory
					return fs_dirents[i].inode;					
				}
			}
		}
	}
	// If no match is found then return ENOENT (No such file or directory)
	return -ENOENT;
}

static int translate(char *path) 
{
	char *token;
	char *duppath = strdup(path);
	const char delim[2] = "/";
	token = strtok(path, delim);
	int ct = 1;
	while(token != NULL) {
		ct = isValidToken(token, ct);
		token = strtok(NULL, delim);
		if(ct < 0) { //this will take care of dir/file not found
			return ct;
		}
		if(!S_ISDIR(fs_inodes[ct].mode)) { //this is for 'file/' or 'file/dir'
			if(token != NULL || duppath[strlen(duppath) - 1] == '/') {
				return -ENOTDIR;
			}
		}
	}
	return ct;
}

static void fillStatStruc(struct stat *sb, int inum) {
	(*sb).st_ino = inum;
	(*sb).st_mode = fs_inodes[inum].mode;
	(*sb).st_uid = fs_inodes[inum].uid;
	(*sb).st_gid = fs_inodes[inum].gid;
	(*sb).st_size = fs_inodes[inum].size;
	(*sb).st_blksize = FS_BLOCK_SIZE;
	(*sb).st_blocks = S_ISDIR(fs_inodes[inum].mode) ? 1 : (fs_inodes[inum].size/FS_BLOCK_SIZE) + 1;
	(*sb).st_atime = fs_inodes[inum].mtime;
	(*sb).st_mtime = fs_inodes[inum].mtime;
	(*sb).st_ctime = fs_inodes[inum].ctime;
}

/* getattr - get file or directory attributes. For a description of
 *  the fields in 'struct stat', see 'man lstat'.
 *
 * Note - fields not provided in fsx600 are:
 *    st_nlink - always set to 1
 *    st_atime, st_ctime - set to same value as st_mtime
 *
 * errors - path translation, ENOENT
 */
static int fs_getattr(const char *path, struct stat *sb)
{	
	char *_path = strdup(path);
	int inum = translate(_path);
	if(inum >= 0) {
		fillStatStruc(sb, inum);
        	return 0;
	}
	free(_path);
	return inum;
}

/* readdir - get directory contents.
 *
 * for each entry in the directory, invoke the 'filler' function,
 * which is passed as a function pointer, as follows:
 *     filler(buf, <name>, <statbuf>, 0)
 * where <statbuf> is a struct stat, just like in getattr.
 *
 * Errors - path resolution, ENOTDIR, ENOENT
 */
static int fs_readdir(const char *path, void *ptr, fuse_fill_dir_t filler,
		       off_t offset, struct fuse_file_info *fi)
{
	struct fs_dirent *fs_dirents;
	struct stat sb;
	int retval = fs_getattr(path, &sb);
	if(retval < 0)
		return retval;
	if(S_ISDIR(fs_inodes[sb.st_ino].mode)) {

		fs_dirents = malloc(DIRENTS_PER_BLOCK * sizeof(struct fs_dirent));
		disk->ops->read(disk, fs_inodes[sb.st_ino].direct[0], 1, fs_dirents);

		int i = 0;
		for(i = 0; i < DIRENTS_PER_BLOCK; i++) {
			if(fs_dirents[i].valid) {
				fillStatStruc(&sb, fs_dirents[i].inode);
				filler(ptr, fs_dirents[i].name, &sb, offset);
			}
		}
	}
	else {
		filler(ptr, path, &sb, offset);
	}
    return 0;
}

/* see description of Part 2. In particular, you can save information 
 * in fi->fh. If you allocate memory, free it in fs_releasedir.
 */
/*
static int fs_opendir(const char *path, struct fuse_file_info *fi)
{
    return 0;
}

static int fs_releasedir(const char *path, struct fuse_file_info *fi)
{
    return 0;
}*/

/* mknod - create a new file with permissions (mode & 01777)
 *
 * Errors - path resolution, EEXIST
 *          in particular, for mknod("/a/b/c") to succeed,
 *          "/a/b" must exist, and "/a/b/c" must not.
 *
 * If a file or directory of this name already exists, return -EEXIST.
 * If this would result in >32 entries in a directory, return -ENOSPC
 * if !S_ISREG(mode) return -EINVAL [i.e. 'mode' specifies a device special
 * file or other non-file object]
 */
static int fs_mknod(const char *path, mode_t mode, dev_t dev)
{
	if(!S_ISREG(mode))
		return -EINVAL;
	struct fs_dirent *fs_dirents;
	struct stat sb;
	char *parentName;
	char *baseName;

	getBaseNameAndParent(path, &baseName, &parentName);

	//printf("\n%s : %s\n", baseName, parentName);

	int retVal = fs_getattr(parentName, &sb);
	if(retVal < 0)
		return retVal;

	// Load the dirents buffer for the inode num
	fs_dirents = malloc(DIRENTS_PER_BLOCK * sizeof(struct fs_dirent));
	disk->ops->read(disk, fs_inodes[sb.st_ino].direct[0], 1, fs_dirents);

	/*
	 * Call handleDirentsBuffer which will 
	 * - get the first invalid entry in dirents buffer
	 * - load the invalid entry with the new directory 
	     to be created i.e. populate the names
	 */
	int isFile = 1;
	retVal = handleDirentsBuffer(fs_dirents, mode, baseName, sb.st_ino, isFile);
	return retVal;
	
}

static int getNewInodeNum() {
	int i = 0;
	for(i = 0; i < (INODES_PER_BLK * super_blk.inode_region_sz); i++) {
		if(!FD_ISSET(i,inode_map)){
			/* 
	 	 	 * Set the new inode number and block number returned in 
	 	 	 * the inode map and block map respectively
	 	 	 */
			FD_SET(i, inode_map);
			return i;
		}
	}
	return -ENOSPC;
}

static int getNewBlockNum() {
	int i = 0;
	for(i = 0; i < super_blk.num_blocks; i++) {
		if(!FD_ISSET(i,block_map)){
			/* 
	 	 	* Set the new inode number and block number returned in 
	 	 	* the inode map and block map respectively
	 	 	*/
			FD_SET(i, block_map);
			return i;
		}
	}
	return -ENOSPC;
}

static void populateInodeForNewDir(mode_t mode, int nodeNum, int blockNum, int isFile){
	//int super_blk_sz = 1;
	//Create a new inode entry in fs_inodes buffer
	struct fuse_context *ctx = fuse_get_context();
	fs_inodes[nodeNum].uid = ctx->uid;
	fs_inodes[nodeNum].gid = ctx->gid;
	if(!isFile) {
		fs_inodes[nodeNum].mode = mode | S_IFDIR;
		fs_inodes[nodeNum].direct[0] = blockNum;
		disk->ops->write(disk, (SUPER_BLK_SZ + super_blk.inode_map_sz), super_blk.block_map_sz, block_map); //writing to block_map
	}
	else {
		fs_inodes[nodeNum].mode = mode | S_IFREG;
		fs_inodes[nodeNum].direct[0] = 0;
	}
	fs_inodes[nodeNum].ctime = (unsigned)time(NULL);
	fs_inodes[nodeNum].mtime = (unsigned)time(NULL);
	fs_inodes[nodeNum].size = isFile ? 0 : FS_BLOCK_SIZE;
	// Assign a new block number for direct[0]
	fs_inodes[nodeNum].indir_1 = 0;
	fs_inodes[nodeNum].indir_2 = 0;
	//marking all other directs = 0
	int i = 0;
	for(i = 1; i < DIRECT_BLOCKS; i++)
		fs_inodes[nodeNum].direct[i] = 0;

	// Write back to the image file
	disk->ops->write(disk, (SUPER_BLK_SZ + super_blk.inode_map_sz + super_blk.block_map_sz), super_blk.inode_region_sz, fs_inodes); //writing to fs_inodes
	disk->ops->write(disk, SUPER_BLK_SZ, super_blk.inode_map_sz, inode_map); //writing to inode_map
}

static void populateDirentForNewDir(int index, int inodeNum, int parentNodeNum, char *dirName, struct fs_dirent *dirent_buff, int isFile){
	dirent_buff[index].valid = 1;
	dirent_buff[index].isDir = !isFile;
	dirent_buff[index].inode = inodeNum;
	strcpy(dirent_buff[index].name, dirName);
	// Write back to image file
	disk->ops->write(disk, fs_inodes[parentNodeNum].direct[0], 1, dirent_buff);
}

static int getFirstInvalidEntryInDirents(struct fs_dirent *temp, char *baseName){
	int invalidIndex = -1;
	int i = 0;
	for(i = 0; i < DIRENTS_PER_BLOCK; i++) {
		if(temp[i].valid) {
			if(!strcmp(temp[i].name, baseName)){
				// Return entry already exists
				return -EEXIST;
			}
		}
		else 
			invalidIndex = i;
	}
	
	return (invalidIndex >= 0 ? invalidIndex : -ENOSPC);
}

static int handleDirentsBuffer(struct fs_dirent *dirent_buff, mode_t mode, char *newname, int parentDirNodeNum, int isFile) {
	int nodeNum = 0;
	int blockNum = 0;
	/*
	 * Get the first invalid entry in the dirents buffer 
	 * for the corresponding inode number 
	 */
	int direntIndex = getFirstInvalidEntryInDirents(dirent_buff, newname);

	if(direntIndex >= 0){

		nodeNum = getNewInodeNum();
		if(!isFile)
			blockNum = getNewBlockNum();
		if(nodeNum < 0 || blockNum < 0)
			return -ENOSPC;

		populateInodeForNewDir(mode, nodeNum, blockNum, isFile);
		populateDirentForNewDir(direntIndex, nodeNum, parentDirNodeNum, newname, dirent_buff, isFile);
		
		return 0;
	}
	
	return direntIndex;
}

/* mkdir - create a directory with the given mode.
 * Errors - path resolution, EEXIST
 * Conditions for EEXIST are the same as for create. 
 * If this would result in >32 entries in a directory, return -ENOSPC
 *
 * Note that you may want to combine the logic of fs_mknod and
 * fs_mkdir. 
 */ 

static void getBaseNameAndParent(const char *path, char **baseName, char **parentName){
	char *slash = strrchr(path, '/');
	int lstSlashIndx = (int)(slash - path);
	lstSlashIndx += 1;

	*parentName = malloc(lstSlashIndx + 1);
	*baseName = malloc((strlen(path) - lstSlashIndx) + 1);

	strncpy(*parentName, path, lstSlashIndx);
	(*parentName)[lstSlashIndx] = '\0';

	strncpy(*baseName, path + lstSlashIndx, strlen(path) - lstSlashIndx);
	(*baseName)[(strlen(path) - lstSlashIndx)] = '\0';
}

static int fs_mkdir(const char *path, mode_t mode)
{
	struct fs_dirent *fs_dirents;
	struct stat sb;
	char *parentName;
	char *baseName;

	getBaseNameAndParent(path, &baseName, &parentName);

	//printf("\n%s : %s\n", baseName, parentName);

	int retVal = fs_getattr(parentName, &sb);
	if(retVal < 0)
		return retVal;

	// Load the dirents buffer for the inode num
	fs_dirents = malloc(DIRENTS_PER_BLOCK * sizeof(struct fs_dirent));
	disk->ops->read(disk, fs_inodes[sb.st_ino].direct[0], 1, fs_dirents);

	/*
	 * Call handleDirentsBuffer which will 
	 * - get the first invalid entry in dirents buffer
	 * - load the invalid entry with the new directory 
	     to be created i.e. populate the names
	 */
	int isFile = 0;
	retVal = handleDirentsBuffer(fs_dirents, mode, baseName, sb.st_ino, isFile);
	return retVal;
	
}

static void truncFile(int inodenum, int len) {
	//check direct
	int i = 0;
	char nullBlock[FS_BLOCK_SIZE];
	memset(nullBlock, '\0', FS_BLOCK_SIZE);

	for(i = 0; i < DIRECT_BLOCKS; i++) {
		if(fs_inodes[inodenum].direct[i] != 0) {
			//free blocknum and write null to block
			FD_CLR(fs_inodes[inodenum].direct[i], block_map);
			disk->ops->write(disk, fs_inodes[inodenum].direct[i], 1, nullBlock); //writing nullblock to freed block
			
			//mark direct[i] = 0
			fs_inodes[inodenum].direct[i] = 0;
		}
		else
			break;
	}
	if(fs_inodes[inodenum].indir_1 != 0) {
		//remove blocks from indir1
		//read block indir1 from disk
		int indirl1 [FS_BLOCK_SIZE/sizeof(int)];
		disk->ops->read(disk, fs_inodes[inodenum].indir_1, 1, &indirl1);
		for(i = 0; i < INDIRECT_BLOCKS; i++) {
			if(indirl1[i] != 0) {
				//free blocknum and write null to block
				FD_CLR(indirl1[i], block_map);
				disk->ops->write(disk, indirl1[i], 1, nullBlock); //writing nullblock to freed block
				
				//mark direct[i] = 0-- No need
				indirl1[i] = 0;
			}
			else
				break;
		}
		FD_CLR(fs_inodes[inodenum].indir_1, block_map);
		//clearing indir_1 block
		disk->ops->write(disk, fs_inodes[inodenum].indir_1, 1, nullBlock); //writing nullblock to freed block
		fs_inodes[inodenum].indir_1 = 0;
	}
	if(fs_inodes[inodenum].indir_2 != 0) {
		//remove blocks from indir1
		//read block indir2 from disk
		int j = 0;
		int indirl2 [FS_BLOCK_SIZE/sizeof(int)];
		int indirl1 [FS_BLOCK_SIZE/sizeof(int)];
		disk->ops->read(disk, fs_inodes[inodenum].indir_2, 1, &indirl1);
		for(i = 0; i < INDIRECT_BLOCKS; i++) {
			if(indirl1[i]) {
				disk->ops->read(disk, indirl1[i], 1, &indirl2);
				for(j = 0; j < INDIRECT_BLOCKS; j++) {
					if(indirl2[j] != 0) {
						//free blocknum and write null to block
						FD_CLR(indirl2[j], block_map);
						disk->ops->write(disk, indirl2[j], 1, nullBlock); //writing nullblock to freed block
						//mark direct[i] = 0
						//indirl2[j] = 0;
					}
					else
						break;
				}
				FD_CLR(indirl1[i], block_map);
				//clearing indir_1 block
				disk->ops->write(disk, indirl1[i], 1, nullBlock); //writing nullblock to freed block
				//indirl1[i] = 0;
			}
			else
				break;
		}
		FD_CLR(fs_inodes[inodenum].indir_2, block_map);
		//clearing indir_2 block
		disk->ops->write(disk, fs_inodes[inodenum].indir_2, 1, nullBlock); //writing nullblock to freed block
		fs_inodes[inodenum].indir_2 = 0;
	}
	//writing block_map back to disk
	disk->ops->write(disk, SUPER_BLK_SZ + super_blk.inode_map_sz, super_blk.block_map_sz, block_map); //writing block map	

	//writing back fs_inodes to disk
	fs_inodes[inodenum].size = len;
	fs_inodes[inodenum].mtime = time(NULL);
	disk->ops->write(disk, (SUPER_BLK_SZ + super_blk.inode_map_sz + super_blk.block_map_sz), super_blk.inode_region_sz, fs_inodes);
}

/* truncate - truncate file to exactly 'len' bytes
 * Errors - path resolution, ENOENT, EISDIR, EINVAL
 *    return EINVAL if len > 0.
 */
static int fs_truncate(const char *path, off_t len)
{
    /* you can cheat by only implementing this for the case of len==0,
     * and an error otherwise.
     */
        if (len != 0)
		return -EINVAL;		/* invalid argument */
	struct stat sb;
	/*
	 * Calling fs_getattr to validate the input path 
	 */
	int retval = fs_getattr(path, &sb);
	if(retval < 0)
		return retval; /* Return if path is not valid */

	if(S_ISDIR(sb.st_mode))
		return -EISDIR; /* Return error if it's a directory */

	truncFile(sb.st_ino, len); //len will always be 0-- we're cheating ..!!
	
	return 0;
}

/* unlink - delete a file
 *  Errors - path resolution, ENOENT, EISDIR
 * Note that you have to delete (i.e. truncate) all the data.
 */
static int fs_unlink(const char *path)
{
	int rmvIndx = -1;
	struct stat sb;
	char *parentName;
	char *baseName;
	struct fs_dirent *fs_dirents;

	getBaseNameAndParent(path, &baseName, &parentName);
	int retval = fs_getattr(parentName, &sb);
	if(retval < 0)
		return retval;
	
	// Load the dirents buffer from inode number
	fs_dirents = malloc(DIRENTS_PER_BLOCK * sizeof(struct fs_dirent));
	disk->ops->read(disk, fs_inodes[sb.st_ino].direct[0], 1, fs_dirents);

	// Get the inode num of dir to be removed
	int nodeNum = getInodeNum(baseName, fs_dirents, &rmvIndx);

	if(nodeNum >= 0){
		if(fs_dirents[rmvIndx].isDir)
			return -EISDIR;
		//truncate file

		truncFile(nodeNum, 0);
		return removeDirOrFile(nodeNum, rmvIndx, fs_dirents, sb.st_ino, fs_dirents[rmvIndx].isDir);
	}
	return nodeNum;
}

static int removeDirOrFile(int nodeNum, int rmvIndx, struct fs_dirent *dirent_buff, int parentNodeNum, int isDir) {

	if(isDir) {
		int i = 0;
		struct fs_dirent *child_dirent;
		// Load the dirents buffer from inode number
		child_dirent = malloc(DIRENTS_PER_BLOCK * sizeof(struct fs_dirent));
		disk->ops->read(disk, fs_inodes[nodeNum].direct[0], 1, child_dirent);

		for(i = 0; i < DIRENTS_PER_BLOCK; i++){
			if(child_dirent[i].valid){
				return -ENOTEMPTY;
			}
		}
		FD_CLR(fs_inodes[nodeNum].direct[0], block_map);
		disk->ops->write(disk, 2, 1, block_map); //writing to block_map
	}
	FD_CLR(nodeNum, inode_map);

	//fill dirent_buff
	dirent_buff[rmvIndx].valid = 0;
	dirent_buff[rmvIndx].isDir = 0;

	//write back to disk
	//disk->ops->write(disk, SUPER_BLK_SZ + super_blk.inode_map_sz + super_blk.block_map_sz, super_blk.inode_region_sz, fs_inodes); //writing to fs_inodes
	disk->ops->write(disk, SUPER_BLK_SZ, super_blk.inode_map_sz, inode_map); //writing to inode_map
	disk->ops->write(disk, fs_inodes[parentNodeNum].direct[0], 1, dirent_buff);
	return 0;
} 

static int getInodeNum(char *baseName, struct fs_dirent *dirent_buff, int *rmvIndx){
	int i = 0;
	for(i = 0; i < DIRENTS_PER_BLOCK; i++){
		if(dirent_buff[i].valid && !strcmp(dirent_buff[i].name, baseName)){
			*rmvIndx = i;
			return dirent_buff[i].inode;
		}
	}
	return -ENOENT;
}

/* rmdir - remove a directory
 *  Errors - path resolution, ENOENT, ENOTDIR, ENOTEMPTY
 */
static int fs_rmdir(const char *path)
{	
	int rmvIndx = -1;
	struct stat sb;
	char *parentName;
	char *baseName;
	struct fs_dirent *fs_dirents;

	getBaseNameAndParent(path, &baseName, &parentName);
	int retval = fs_getattr(parentName, &sb);
	if(retval < 0)
		return retval;
	
	// Load the dirents buffer from inode number
	fs_dirents = malloc(DIRENTS_PER_BLOCK * sizeof(struct fs_dirent));
	disk->ops->read(disk, fs_inodes[sb.st_ino].direct[0], 1, fs_dirents);

	// Get the inode num of dir to be removed
	int nodeNum = getInodeNum(baseName, fs_dirents, &rmvIndx);
	if(!fs_dirents[rmvIndx].isDir)
		return -ENOTDIR;
	if(nodeNum >= 0){
		return removeDirOrFile(nodeNum, rmvIndx, fs_dirents, sb.st_ino, fs_dirents[rmvIndx].isDir);
	}
	return nodeNum;
}

/* rename - rename a file or directory
 * Errors - path resolution, ENOENT, EINVAL, EEXIST
 *
 * ENOENT - source does not exist
 * EEXIST - destination already exists
 * EINVAL - source and destination are not in the same directory
 *
 * Note that this is a simplified version of the UNIX rename
 * functionality - see 'man 2 rename' for full semantics. In
 * particular, the full version can move across directories, replace a
 * destination file, and replace an empty directory with a full one.
 */
static int fs_rename(const char *src_path, const char *dst_path)
{
	char *_src_path = strdup(src_path);
	char *_dst_path = strdup(dst_path);
	int i = 0;
	char *src;
	char *dest;
	char *srcParentName;
	char *dstParentName;
	struct stat sb;
	struct fs_dirent *dirent_buff;
	
	getBaseNameAndParent(_src_path, &src, &srcParentName);
	getBaseNameAndParent(_dst_path, &dest, &dstParentName);
	
	if(!strcmp(srcParentName, dstParentName)){
		// Both parent directories match; hence can perform renaming of files
			int retval = fs_getattr(srcParentName, &sb);
			if(retval < 0)
				return retval;
	
		// Load the dirents buffer from inode number
			dirent_buff = malloc(DIRENTS_PER_BLOCK * sizeof(struct fs_dirent));
			disk->ops->read(disk, fs_inodes[sb.st_ino].direct[0], 1, dirent_buff);
				
			if(strcmp(src, dest)){	
				if(getFirstInvalidEntryInDirents(dirent_buff, dest) == -EEXIST)
					return -EEXIST;
			}
			// Loop into the dirents buff to find the src file
			for(i = 0; i < DIRENTS_PER_BLOCK; i++){
				if(dirent_buff[i].valid && !strcmp(dirent_buff[i].name, src)){
					strcpy(dirent_buff[i].name, dest);
					disk->ops->write(disk, fs_inodes[sb.st_ino].direct[0], 1, dirent_buff);
					return 0;
				}
			}
			
			return -ENOENT;
		}
		return -EINVAL;
}

/* chmod - change file permissions
 * utime - change access and modification times
 *         (for definition of 'struct utimebuf', see 'man utime')
 *
 * Errors - path resolution, ENOENT.
 */
static int fs_chmod(const char *path, mode_t mode)
{
	struct stat sb;

	int retval = fs_getattr(path, &sb);
	if(retval < 0)
		return retval;
	
	//set permissions for inodenum
	if(S_ISDIR(sb.st_mode))
		fs_inodes[sb.st_ino].mode = mode | S_IFDIR;
	else
		fs_inodes[sb.st_ino].mode = mode | S_IFREG;

	// Write back to the image file
	//writing to fs_inodes
	disk->ops->write(disk, SUPER_BLK_SZ + super_blk.inode_map_sz + super_blk.block_map_sz, super_blk.inode_region_sz, fs_inodes); 
	return 0;
}

int fs_utime(const char *path, struct utimbuf *ut)
{
	struct stat sb;

	int retval = fs_getattr(path, &sb);
	if(retval < 0)
		return retval;
	
	if(sb.st_ino >= 0) {
		fs_inodes[sb.st_ino].mtime = ut->modtime;

		// Write back to the image file
		disk->ops->write(disk, 3, 4, fs_inodes); //writing to fs_inodes
		return 0;
	}
	
        return -ENOENT;
}

static void getBlockAndIndex(int *blocknum, int *startindex, int offset) {	
	*blocknum = offset / FS_BLOCK_SIZE;
	*startindex = offset % FS_BLOCK_SIZE;
}

static int min(int a, int b, int c) {
	return MIN(MIN(a, b), c);
}

static int readFromDataBlocks(int offset, int blksize, int inodenum, char **buf) {
	int blocknum;
	int startindex;
	int read = 0;

	char *auxbuf = calloc(1,FS_BLOCK_SIZE);
	memset(*buf, '\0', blksize);
	while(strlen(*buf) < blksize && offset < fs_inodes[inodenum].size) {
		getBlockAndIndex(&blocknum, &startindex, offset);
		if(blocknum < DIRECT_BLOCKS){
			if(!fs_inodes[inodenum].direct[blocknum])
				break;
			disk->ops->read(disk, fs_inodes[inodenum].direct[blocknum], 1, auxbuf);
		}else if (blocknum < (INDIRECT_BLOCKS + DIRECT_BLOCKS)){
			int indirectPtr1Buf[INDIRECT_BLOCKS];
			disk->ops->read(disk, fs_inodes[inodenum].indir_1, 1, &indirectPtr1Buf);
			disk->ops->read(disk, indirectPtr1Buf[blocknum - DIRECT_BLOCKS], 1, auxbuf);
		}else {
			int indirectPtr2Buf1[INDIRECT_BLOCKS];
			int indirectPtr2Buf2[INDIRECT_BLOCKS];
			disk->ops->read(disk, fs_inodes[inodenum].indir_2, 1, &indirectPtr2Buf1);
			int lvl1Indx = (blocknum - DIRECT_BLOCKS - INDIRECT_BLOCKS);
			disk->ops->read(disk, indirectPtr2Buf1[lvl1Indx / INDIRECT_BLOCKS], 1, &indirectPtr2Buf2);
			
			disk->ops->read(disk, indirectPtr2Buf2[lvl1Indx % INDIRECT_BLOCKS], 1, auxbuf);
		}
		int minimum  = min((FS_BLOCK_SIZE - startindex), (blksize - read), (fs_inodes[inodenum].size - offset));
		strncpy(&(*buf)[read] , auxbuf + startindex, minimum);
		offset += minimum;
		read += minimum;
	}
	return read;
}

/* read - read data from an open file.
 * should return exactly the number of bytes requested, except:
 *   - if offset >= file len, return 0
 *   - if offset+len > file len, return bytes from offset to EOF
 *   - on error, return <0
 * Errors - path resolution, ENOENT, EISDIR
 */
static int fs_read(const char *path, char *buf, size_t len, off_t offset,
		    struct fuse_file_info *fi)
{
	struct stat sb;
	memset(buf, '\0', len + 1);
	/*
	 * Calling fs_getattr to validate the input path 
	 */
	int retval = fs_getattr(path, &sb);
	if(retval < 0)
		return retval; /* Return if path is not valid */
	if(S_ISDIR(sb.st_mode))
		return -EISDIR; /* Return error if it's a directory */
	
	return readFromDataBlocks(offset, len, sb.st_ino, &buf);
}

static int populateDirectBlocks(int fileNodeNum, int blocknum){
	//if block allocated previously do not call getNewBlockNum()
	if(!fs_inodes[fileNodeNum].direct[blocknum]) {
		int newBlockNum = getNewBlockNum();
		if(newBlockNum < 0)
			return -ENOSPC;
		fs_inodes[fileNodeNum].direct[blocknum] = newBlockNum;
		disk->ops->write(disk, fs_inodes[fileNodeNum].direct[blocknum], 1, globalBuf);
		disk->ops->write(disk, (SUPER_BLK_SZ + super_blk.inode_map_sz), super_blk.block_map_sz, block_map); //writing to block_map
	}
	//overwrite the previous block with globalBuf
	else {	
		disk->ops->write(disk, fs_inodes[fileNodeNum].direct[blocknum], 1, globalBuf);
	}
	return 0;
}

static int populateIndirectPtr1Blocks(int fileNodeNum, int blocknum){
	int indirectPtr1Buf[INDIRECT_BLOCKS];
	//if block allocated previously do not call getNewBlockNum()
	if(!fs_inodes[fileNodeNum].indir_1) {
		blockForIndir1 = getNewBlockNum();
		if(blockForIndir1 < 0)
			return -ENOSPC;
		fs_inodes[fileNodeNum].indir_1 = blockForIndir1;
		
		int newBlockNum = getNewBlockNum();
		if(newBlockNum < 0)
			return -ENOSPC;
		disk->ops->read(disk, blockForIndir1, 1, &indirectPtr1Buf);
		memset(indirectPtr1Buf, 0, INDIRECT_BLOCKS);
		
		indirectPtr1Buf[blocknum - DIRECT_BLOCKS] = newBlockNum;
		disk->ops->write(disk, blockForIndir1, 1, &indirectPtr1Buf);
		disk->ops->write(disk, indirectPtr1Buf[blocknum - DIRECT_BLOCKS], 1, globalBuf);
	}
	//overwrite the previous block with globalBuf
	else {	
		disk->ops->read(disk, blockForIndir1, 1, &indirectPtr1Buf);
		if(!indirectPtr1Buf[blocknum - DIRECT_BLOCKS]){
			int newBlockNum = getNewBlockNum();
			if(newBlockNum < 0)
				return -ENOSPC;
			indirectPtr1Buf[blocknum - DIRECT_BLOCKS] = newBlockNum;
			disk->ops->write(disk, blockForIndir1, 1, &indirectPtr1Buf);
			disk->ops->write(disk, indirectPtr1Buf[blocknum - DIRECT_BLOCKS], 1, globalBuf);
		}
		else{
			disk->ops->write(disk, indirectPtr1Buf[blocknum - DIRECT_BLOCKS], 1, globalBuf);
		}
	}
	disk->ops->write(disk, (SUPER_BLK_SZ + super_blk.inode_map_sz), super_blk.block_map_sz, block_map); //writing to block_map
	return 0;	
}

static int populateIndirectPtr2Blocks(int fileNodeNum, int blocknum){
	int indirectPtr2Buf1[INDIRECT_BLOCKS];
	int indirectPtr2Buf2[INDIRECT_BLOCKS];
	int lvlIndx = (blocknum - DIRECT_BLOCKS - INDIRECT_BLOCKS);
	//if block allocated previously do not call getNewBlockNum()
	if(!fs_inodes[fileNodeNum].indir_2) {
		/*
		 * Get level 1 block, populate it with level 2 block
		 * and write back to disk
		 */
		int blockForIndir2 = getNewBlockNum();
		if(blockForIndir2 < 0)
			return -ENOSPC;
		fs_inodes[fileNodeNum].indir_2 = blockForIndir2;
		disk->ops->read(disk, blockForIndir2, 1, &indirectPtr2Buf1);
		
		int blockForIndir2lvl1 = getNewBlockNum();
		if(blockForIndir2lvl1 < 0)
			return -ENOSPC;
		memset(indirectPtr2Buf1, 0, INDIRECT_BLOCKS);
		indirectPtr2Buf1[lvlIndx / INDIRECT_BLOCKS] = blockForIndir2lvl1;
		disk->ops->write(disk, blockForIndir2, 1, &indirectPtr2Buf1);
		
		disk->ops->read(disk, blockForIndir2lvl1, 1, &indirectPtr2Buf2);
		int blockForIndir2lvl2 = getNewBlockNum();
		if(blockForIndir2lvl2 < 0)
			return -ENOSPC;
		memset(indirectPtr2Buf2, 0, INDIRECT_BLOCKS);
		indirectPtr2Buf2[lvlIndx % INDIRECT_BLOCKS] = blockForIndir2lvl2;
		disk->ops->write(disk, blockForIndir2lvl1, 1, &indirectPtr2Buf2);
		
		disk->ops->write(disk, blockForIndir2lvl2, 1, globalBuf);
		
	}
	//overwrite the previous block with globalBuf
	else {	
		disk->ops->read(disk, fs_inodes[fileNodeNum].indir_2, 1, &indirectPtr2Buf1);
		if(!indirectPtr2Buf1[lvlIndx / INDIRECT_BLOCKS]){
			
			int blockForIndir2lvl1 = getNewBlockNum();
			if(blockForIndir2lvl1 < 0)
				return -ENOSPC;
			indirectPtr2Buf1[lvlIndx / INDIRECT_BLOCKS] = blockForIndir2lvl1;
			disk->ops->write(disk, fs_inodes[fileNodeNum].indir_2, 1, &indirectPtr2Buf1);
		
			disk->ops->read(disk, blockForIndir2lvl1, 1, &indirectPtr2Buf2);
			int blockForIndir2lvl2 = getNewBlockNum();
			if(blockForIndir2lvl2 < 0)
				return -ENOSPC;
			memset(indirectPtr2Buf2, 0, INDIRECT_BLOCKS);
			indirectPtr2Buf2[lvlIndx % INDIRECT_BLOCKS] = blockForIndir2lvl2;
			disk->ops->write(disk, blockForIndir2lvl1, 1, &indirectPtr2Buf2);
		
			disk->ops->write(disk, blockForIndir2lvl2, 1, globalBuf);
		}
		else{
			disk->ops->read(disk, indirectPtr2Buf1[lvlIndx / INDIRECT_BLOCKS], 1, &indirectPtr2Buf2);
			if(!indirectPtr2Buf2[lvlIndx % INDIRECT_BLOCKS]){
				int blockForIndir2lvl2 = getNewBlockNum();
				if(blockForIndir2lvl2 < 0)
					return -ENOSPC;
				indirectPtr2Buf2[lvlIndx % INDIRECT_BLOCKS] = blockForIndir2lvl2;
				disk->ops->write(disk, indirectPtr2Buf1[lvlIndx / INDIRECT_BLOCKS], 1, &indirectPtr2Buf2);
				disk->ops->write(disk, blockForIndir2lvl2, 1, globalBuf);
				
			}else{
				disk->ops->write(disk, indirectPtr2Buf2[lvlIndx % INDIRECT_BLOCKS], 1, globalBuf);
			}
		}
	}
	
	disk->ops->write(disk, (SUPER_BLK_SZ + super_blk.inode_map_sz), super_blk.block_map_sz, block_map); //writing to block_map
	return 0;	
}

static int writeBlock(int inodenum, int offset, int len, const char **buf) {
	int blocknum;
	int startindex;
	int read = 0;
	int retval = 0;

	while(read < len) {
		getBlockAndIndex(&blocknum, &startindex, offset);
		int minimum  = MIN((FS_BLOCK_SIZE - startindex), (len - read));
		strncpy(globalBuf + startindex, *buf + read, minimum);
		
		if(blocknum < DIRECT_BLOCKS){
			retval = populateDirectBlocks(inodenum, blocknum);
		}else if (blocknum < DIRECT_BLOCKS + INDIRECT_BLOCKS){
			retval = populateIndirectPtr1Blocks(inodenum, blocknum);
		}else{
			retval = populateIndirectPtr2Blocks(inodenum, blocknum);
		}

		offset += minimum;
		read += minimum;
		
		//Empty the globalBuf in case if it's full
		if(strlen(globalBuf) == FS_BLOCK_SIZE)
			memset(globalBuf, '\0', FS_BLOCK_SIZE);
		if(retval < 0)
			break;
	}
	fs_inodes[inodenum].mtime = (unsigned)time(NULL);
	fs_inodes[inodenum].size += len;
	//writing to fs_inodes
	disk->ops->write(disk, (SUPER_BLK_SZ + super_blk.inode_map_sz + super_blk.block_map_sz), super_blk.inode_region_sz, fs_inodes);
	return retval < 0 ? retval : len;
}

/* write - write data to a file
 * It should return exactly the number of bytes requested, except on
 * error.
 * Errors - path resolution, ENOENT, EISDIR
 *  return EINVAL if 'offset' is greater than current file length.
 *  (POSIX semantics support the creation of files with "holes" in them, 
 *   but we don't)
 */
static int fs_write(const char *path, const char *buf, size_t len,
		     off_t offset, struct fuse_file_info *fi)
{
	//getattr- inodenum of file
	struct stat sb;
	/*
	 * Calling fs_getattr to validate the input path 
	 */
	int retval = fs_getattr(path, &sb);
	if(retval < 0)
		return retval; /* Return if path is not valid */
	if(S_ISDIR(sb.st_mode))
		return -EISDIR; /* Return error if it's a directory */
		
	/*
	 * Initialize the global buffer in case of 
	 * first call to write function for a file
	 */
	if(!offset){
		memset(globalBuf, '\0', FS_BLOCK_SIZE);
	}
	return writeBlock(sb.st_ino, offset, len, &buf);
}
/*
static int fs_open(const char *path, struct fuse_file_info *fi)
{
    return 0;
}

static int fs_release(const char *path, struct fuse_file_info *fi)
{
    return 0;
}
* */
static int calculateUsedBlocks(){
	int i, count = 0;
	for(i = 0; i < super_blk.num_blocks; i++) {
		if(FD_ISSET(i,block_map)){
			/* 
	 	 	* Increment counter for used blocks
	 	 	*/
			count++;
		}
	}
	return count;
}
/* statfs - get file system statistics
 * see 'man 2 statfs' for description of 'struct statvfs'.
 * Errors - none. 
 */
static int fs_statfs(const char *path, struct statvfs *st)
{
    /* needs to return the following fields (set others to zero):
     *   f_bsize = BLOCK_SIZE
     *   f_blocks = total image - metadata
     *   f_bfree = f_blocks - blocks used
     *   f_bavail = f_bfree
     *   f_namelen = <whatever your max namelength is>
     *
     * this should work fine, but you may want to add code to
     * calculate the correct values later.
     */
    st->f_bsize = FS_BLOCK_SIZE;
    st->f_blocks = super_blk.num_blocks - (SUPER_BLK_SZ + super_blk.inode_map_sz + super_blk.block_map_sz + super_blk.root_inode + super_blk.inode_region_sz);           /* probably want to */
    st->f_bfree = super_blk.num_blocks - (calculateUsedBlocks());            /* change these */
    st->f_bavail = st->f_bfree;           /* values */
    st->f_namemax = 27;

    return 0;
}

/* operations vector. Please don't rename it, as the skeleton code in
 * misc.c assumes it is named 'fs_ops'.
 */
struct fuse_operations fs_ops = {
    .init = fs_init,
    .getattr = fs_getattr,
    //.opendir = fs_opendir,
    .readdir = fs_readdir,
    //.releasedir = fs_releasedir,
    .mknod = fs_mknod,
    .mkdir = fs_mkdir,
    .unlink = fs_unlink,
    .rmdir = fs_rmdir,
    .rename = fs_rename,
    .chmod = fs_chmod,
    .utime = fs_utime,
    .truncate = fs_truncate,
    //.open = fs_open,
    .read = fs_read,
    .write = fs_write,
    //.release = fs_release,
    .statfs = fs_statfs,
};

