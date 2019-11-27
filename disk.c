
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>

#include "disk.h"

#define DISK_MAGIC 0xf0f03410

#define FREE_BLOCK -1

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

/*Returns the index of a cache_entry in cache with a matching blocknum,
-1 if there's no such entry in the cache
(If blockNum == -1 then this function will search for a free entry in cache).*/
int getCacheEntry(int blocknum) {
	int i = 0;
	do {
		if (cache[i++].disk_block_number == blocknum) {
			return --i;
		}
	} while (i < cache_nblocks);
	return -1;
}

/*Writes data from the cache at cacheIndex in the given buffer.*/
void writeFromCacheToBuffer(int cacheIndex, char* buffer) {
	for (size_t i = 0; i < DISK_BLOCK_SIZE; i++) {
		buffer[i] = cache[cacheIndex].datab->data[i];
	}
}

/*Writes data from the given buffer in the cache at cacheIndex.*/
void writeFromCacheToBuffer(int cacheIndex, char* buffer) {
	cache[cacheIndex].dirty_bit = 1;
	for (size_t i = 0; i < DISK_BLOCK_SIZE; i++) {
		cache[cacheIndex].datab->data[i] = buffer[i];
	}
}

/*Sets a new entry in cache for the block at blocknum in disk.
Returns the cacheIndex in which the new entry was stored.*/
int setNewEntryForBlock(int blocknum) {
	int cacheIndex = getCacheEntry(FREE_BLOCK);
	if (cacheIndex == -1) {
		cacheIndex = rand() % cache_nblocks;
		disk_flush_block(cacheIndex);
	}
	setNewCacheEntry(cacheIndex, blocknum);
	return cacheIndex;
}

/*Sets a new entry in cache at cacheIndex for the block at blocknum in disk.*/
void setNewCacheEntry(int cacheIndex, int blocknum) {
	cache[cacheIndex].dirty_bit = 0;
	cache[cacheIndex].disk_block_number = blocknum;
	cache[cacheIndex].datab = &cache_data[cacheIndex];
}

/*Flushes the contents of a cache block at cacheIndex into disk*/
void disk_flush_block(int cacheIndex) {
	disk_write(cache[cacheIndex].disk_block_number, cache[cacheIndex].datab->data);
	cache[cacheIndex].dirty_bit = 0;
}

/*Flushes the contents of a cache block at cacheIndex into disk*/
void disk_update_block(int blocknum) {

	int cacheIndex = getCacheEntry(blocknum);
	if (cacheIndex != -1) {
		disk_flush_block(cacheIndex);
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

void disk_read_data(int blocknum, char* data) {
	sanity_check(blocknum, data);
	int cacheIndex = getCacheEntry(blocknum);
	if (cacheIndex != -1) {
		writeFromCacheToBuffer(cacheIndex, data);
	} else {
		cacheIndex = setNewEntryForBlock(blocknum);
		disk_read(blocknum, cache[cacheIndex].datab->data);
		writeFromCacheToBuffer(cacheIndex, data);
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

void disk_write_data(int blocknum, char* data) {
	sanity_check(blocknum, data);
	int cacheIndex = getCacheEntry(blocknum);
	if (cacheIndex != -1) {
		writeFromCacheToBuffer(cacheIndex, data);
	} else {
		cacheIndex = setNewEntryForBlock(blocknum);
		writeFromBufferToCache(cacheIndex, data);
	}
}

void disk_flush() {
	for (size_t cacheIndex = 0; cacheIndex < cache_nblocks; cacheIndex++) {
		if (cache->dirty_bit == 1) {
			disk_flush_block(cacheIndex);
		}
	}
}

void disk_close()
{
	if(diskfile) {
		disk_flush();
		printf("%d disk block reads\n",nreads);
		printf("%d disk block writes\n",nwrites);
		fclose(diskfile);
		diskfile = NULL;
	}
}
