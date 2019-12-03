#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <math.h>

#include "disk.h"

// define DEBUG for messages about all written blocks
// #define DEBUG

#define DISK_MAGIC 0xf0f03410

#define FREE_BLOCK -1

static FILE *diskfile;
static int nblocks = 0;
static int nreads = 0;
static int nwrites = 0;

// Data structures for the cache
typedef struct __cache_memory {
	char data[DISK_BLOCK_SIZE];
} cache_memory;

cache_memory* cache_data;	// cache data space

#define FREE_BLOCK -1
typedef struct __cache_entry {
	int disk_block_number;  // identifies the block number in disk
	int dirty_bit;   // this value is 1 if the block has been written, 0 otherwise
	cache_memory* datab;    // a pointer to a disk data block cached in memory
} cache_entry;

cache_entry* cache;	// cache metadata

static int cache_nblocks = 0;
static int cachehits = 0;
static int cachemisses = 0;

int disk_init( const char *filename, int n ) {
    diskfile = fopen( filename, "r+" );
    if ( diskfile != NULL && n == -1 ) {
        fseek( diskfile, 0L, SEEK_END );
        n = ftell( diskfile );
        fprintf( stderr, "filesize=%d, %d\n", n, n / DISK_BLOCK_SIZE );
        n = n / DISK_BLOCK_SIZE;
    }
    if ( !diskfile )
        diskfile = fopen( filename, "w+" );
    if ( !diskfile )
        return 0;

    ftruncate( fileno( diskfile ), n * DISK_BLOCK_SIZE );

    nblocks = n;
    nreads = 0;
    nwrites = 0;

	cache_nblocks = (int)ceil((float)nblocks * 0.2);
  	cache = (cache_entry*)malloc(sizeof(cache_entry) * cache_nblocks);
  	cache_data = (cache_memory*)malloc(sizeof(cache_memory) * cache_nblocks);

	for(int i = 0; i < cache_nblocks; i++) {
		cache[i].disk_block_number = FREE_BLOCK;
		cache[i].datab = &cache_data[i];
	}

#ifdef DEBUG
    printf( "Cache blocks %d\n", cache_nblocks );
#endif


    srand( 0 );	// to generate always the same sequence of blocks to evict

    return 1;
}

int disk_size( ) {
    return nblocks;
}

static void sanity_check( int blocknum, const void *data ) {
    if (blocknum < 0) {
        printf( "ERROR: blocknum (%d) is negative!\n", blocknum );
        abort();
    }

    if (blocknum >= nblocks) {
        printf( "ERROR: blocknum (%d) is too big!\n", blocknum );
        abort();
    }

    if (!data) {
        printf( "ERROR: null data pointer!\n" );
        abort();
    }
}

/* Searches the cache for a block; returns its position in the cache or -1 otherwise

Returns the index of a cache_entry in cache with a matching blocknum,
-1 if there's no such entry in the cache
(If blockNum == -1 then this function will search for a free entry in cache).*/
int search_cache(int data_block_num)
{
	int entry_num = 0;
	while (entry_num < cache_nblocks) {
		if (cache[entry_num++].disk_block_number == data_block_num) {
			return --entry_num;
		}
	}
	return -1;
}

/*Writes data from the cache at cacheIndex in the given buffer.*/
void writeFromCacheToBuffer(int cacheIndex, char* buffer) {
	for (size_t i = 0; i < DISK_BLOCK_SIZE; i++) {
		buffer[i] = cache[cacheIndex].datab->data[i];
	}
}

/*Writes data from the given buffer in the cache at cacheIndex.*/
void writeFromBufferToCache(int cacheIndex, const char* buffer) {
	cache[cacheIndex].dirty_bit = 1;
	for (size_t i = 0; i < DISK_BLOCK_SIZE; i++) {
		cache[cacheIndex].datab->data[i] = buffer[i];
	}
}

/*Sets a new entry in cache at cacheIndex for the block at blocknum in disk.*/
void setNewCacheEntry(int cacheIndex, int blocknum) {
	cache[cacheIndex].dirty_bit = 0;
	cache[cacheIndex].disk_block_number = blocknum;
}
int entry_selection();

/*Flushes the contents of a cache block at cacheIndex into disk*/
void disk_flush_block(int cacheIndex) {
	disk_write(cache[cacheIndex].disk_block_number, cache[cacheIndex].datab->data);
	cache[cacheIndex].dirty_bit = 0;
}

/*Sets a new entry in cache for the block at blocknum in disk.
Returns the cacheIndex in which the new entry was stored.*/
int setNewEntryForBlock(int blocknum) {
	int cacheIndex = entry_selection();
	setNewCacheEntry(cacheIndex, blocknum);
	return cacheIndex;
}

void disk_read( int blocknum, char *data ) {
    sanity_check( blocknum, data );

    if ( fseek( diskfile, blocknum * DISK_BLOCK_SIZE, SEEK_SET )<0 )
        perror("disk_read seek");

    if (fread( data, DISK_BLOCK_SIZE, 1, diskfile ) == 1) {
        nreads++;
    } else {
        printf( "ERROR: couldn't access simulated disk: %s\n",
                strerror( errno ) );
        abort();
    }
}

void disk_write( int blocknum, const char *data ) {
#ifdef DEBUG
    printf( "Writing block %d\n", blocknum );
#endif
    sanity_check( blocknum, data );

    if ( fseek( diskfile, blocknum * DISK_BLOCK_SIZE, SEEK_SET )<0 )
        perror("disk_write seek");

    if ( fwrite( data, DISK_BLOCK_SIZE, 1, diskfile ) == 1) {
        nwrites++;
    } else {
        printf( "ERROR: couldn't access simulated disk: %s\n",
                strerror( errno ) );
        abort();
    }
}

// allocates a cache_entry where to place the new block
int entry_selection()
{
	// TODO
	// note: the function rand() generates a random number
	int entry_num = search_cache(FREE_BLOCK);
	if (entry_num == -1) {
		entry_num = rand() % cache_nblocks;
		if (cache[entry_num].dirty_bit == 1) {
			disk_flush_block(entry_num);
		}
		cache[entry_num].disk_block_number = -1;
	}
	return entry_num;
}

// Cache aware read
void disk_read_data( int blocknum, char *data ) {
 	sanity_check( blocknum, data );
	int cacheIndex;
#ifdef DEBUG
    printf( "disk_read_data for block %d \n", blocknum );
#endif

	cacheIndex = search_cache(blocknum);
	if (cacheIndex == -1) {
		cachemisses++;
		cacheIndex = setNewEntryForBlock(blocknum);
		disk_read(blocknum, cache[cacheIndex].datab->data);
	} else {
		cachehits++;
	}
	writeFromCacheToBuffer(cacheIndex, data);
}

// Cache aware write
void disk_write_data(int blocknum, const char* data) {
	sanity_check( blocknum, data );

#ifdef DEBUG
	printf( "disk_write_data for block %d \n", blocknum );
#endif
	int cacheIndex = search_cache(blocknum);
	if (cacheIndex == -1) {
		cachemisses++;
		cacheIndex = setNewEntryForBlock(blocknum);
	} else {
		cachehits++;
	}
	writeFromBufferToCache(cacheIndex, data);
}

// Writes the cache's metadata
void cache_debug() {
	for( int i = 0; i < cache_nblocks; i++ ) {
    	// TODO
		printf("Cache block: %d\n", i);
		printf("	disk_block_number: %d\n", cache[i].disk_block_number);
		printf("	dirty_bit: %d\n", cache[i].dirty_bit);
		//printf("	datab: %d\n\n", &cache->datab);
	}
}


// flushes the modified data blocks to disk
void disk_flush() {
	for (size_t cacheIndex = 0; cacheIndex < cache_nblocks; cacheIndex++) {
		if (cache[cacheIndex].dirty_bit == 1) {
			disk_flush_block(cacheIndex);
		}
	}
}


void disk_close( ) {
	if (diskfile)  {
		// TODO: flushes the cache and frees the allocated memory
		disk_flush();
		free(cache);
		free(cache_data);
		// Writes statistics
		printf( "%d disk block reads\n", nreads );
  		printf( "%d disk block writes\n", nwrites );
		printf( "%d cache hits, %d cache misses\n", cachehits, cachemisses),

		fclose( diskfile );
		diskfile = 0;

	}

}
