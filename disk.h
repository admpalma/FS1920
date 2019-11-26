#ifndef DISK_H
#define DISK_H

#define DISK_BLOCK_SIZE 4096

/*This function must be invoked before calling other API functions.	
It is only possible to have one active disk at some point in time.*/
int  disk_init( const char *filename, int nblocks );

/*Returns an integer with the total number of the blocks in the disk.*/
int  disk_size();

/*Reads the contents of the block disk numbered blocknum (4096 bytes) to a memory buffer that starts at address data.*/
void disk_read( int blocknum, char *data );

/*Function that uses the cache whenever a data block has to be read from disk.*/
void disk_read_data(int blocknum, char* data);

/*Writes, in the block blocknum of the disk, a total of 4096 bytes starting at memory address data.*/
void disk_write( int blocknum, const char *data );

/*Function that uses the cache whenever a data block has to be written on disk.*/
void disk_write_data(int blocknum, char* data);

/*Function that flushes all the dirty data blocks in the cache onto disk*/
void disk_flush();

/*Function to be called at the end of the program.*/
void disk_close();


#endif
