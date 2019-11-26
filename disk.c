
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>

#include "disk.h"

#define DISK_MAGIC 0xf0f03410

static FILE *diskfile;
static int nblocks;
static int nreads;
static int nwrites;

typedef struct __cache_entry {
	int disk_block_number;	// identifies the block number in disk or -1 for a free entry
	unsigned int dirty_bit; // this value is 1 if the block has been written, 0 otherwise
	cache_block* datab;			// a pointer to a disk data block cached in memory
} cache_entry;

cache_entry* cache;

typedef struct cache_block {
	char data[DISK_BLOCK_SIZE];
} cache_block;

cache_block* cache_data;

int cache_nblocks;


int disk_init( const char *filename, int n )
{
	diskfile = fopen(filename,"r+");
	if(diskfile==NULL)
		diskfile = fopen(filename,"w+");
	if(diskfile==NULL) 
		return 0;

	ftruncate(fileno(diskfile),n*DISK_BLOCK_SIZE);

	nblocks = n;
	nreads = 0;
	nwrites = 0;

	cache_nblocks = (int)ceil((float)nblocks * 0.2);
	cache = (cache_entry*)malloc(sizeof(cache_entry) * nblocks);
	cache_data = (cache_block*)malloc(sizeof(cache_block) * nblocks);

	return 1;
}

int disk_size()
{
	return nblocks;
}

static void sanity_check( int blocknum, const void *data )
{
	if(blocknum<0) {
		printf("ERROR: blocknum (%d) is negative!\n",blocknum);
		abort();
	}

	if(blocknum>=nblocks) {
		printf("ERROR: blocknum (%d) is too big!\n",blocknum);
		abort();
	}

	if(data==NULL) {
		printf("ERROR: null data pointer!\n");
		abort();
	}
}

void disk_read( int blocknum, char *data )
{
	sanity_check(blocknum,data);

	fseek(diskfile,blocknum*DISK_BLOCK_SIZE,SEEK_SET);

	if(fread(data,DISK_BLOCK_SIZE,1,diskfile)==1) {
		nreads++;
	} else {
		printf("ERROR: couldn't access simulated disk: %s\n",strerror(errno));
		abort();
	}
}

void disk_write( int blocknum, const char *data )
{
	sanity_check(blocknum,data);

	fseek(diskfile,blocknum*DISK_BLOCK_SIZE,SEEK_SET);

	if(fwrite(data,DISK_BLOCK_SIZE,1,diskfile)==1) {
		nwrites++;
	} else {
		printf("ERROR: couldn't access simulated disk: %s\n",strerror(errno));
		abort();
	}
}

void disk_close()
{
	if(diskfile) {
		printf("%d disk block reads\n",nreads);
		printf("%d disk block writes\n",nwrites);
		fclose(diskfile);
		diskfile = NULL;
	}
}

