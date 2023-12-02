/*
     File        : file.C

     Author      : Riccardo Bettati
     Modified    : 2021/11/28

     Description : Implementation of simple File class, with support for
                   sequential read/write operations.
*/

/*--------------------------------------------------------------------------*/
/* DEFINES */
/*--------------------------------------------------------------------------*/

/* -- (none) -- */

/*--------------------------------------------------------------------------*/
/* INCLUDES */
/*--------------------------------------------------------------------------*/

#include "assert.H"
#include "console.H"
#include "file.H"

/*--------------------------------------------------------------------------*/
/* CONSTRUCTOR/DESTRUCTOR */
/*--------------------------------------------------------------------------*/

File::File(FileSystem *_fs, int _id) {
    Console::puts("Opening file.\n");
    curr_pos = 0;
    fs = _fs;
    id = _id;
    inode = fs->LookupFile(id);
    fs->disk->read(inode->block_id, block_ids);
}

File::~File() {
    Console::puts("Closing file.\n");
    /* Make sure that you write any cached data to disk. */
    /* Also make sure that the inode in the inode list is updated. */

    delete block_cache;
    delete block_ids;
}

/*--------------------------------------------------------------------------*/
/* FILE FUNCTIONS */
/*--------------------------------------------------------------------------*/

int File::Read(unsigned int _n, char *_buf) {
    Console::puts("reading from file\n");
    for(int i = 0; i < _n; i++) {
        if(EoF()) {
            Console::puts("reached end of file\n");
            Reset();
            return i;
        }
        
        if(curr_pos % SimpleDisk::BLOCK_SIZE == 0) {
            // Read the next block from disk to block_cache
            fs->disk->read(block_ids[(curr_pos / SimpleDisk::BLOCK_SIZE)], block_cache);
        }

        _buf[i] = block_cache[curr_pos % SimpleDisk::BLOCK_SIZE];
        curr_pos++;
    }
    Reset();
    return _n;
}

int File::Write(unsigned int _n, const char *_buf) {
    Console::puts("writing to file\n");
    for(int i = 0; i < _n; i++) {      
        if(curr_pos == MAX_FILE_SIZE) {
            Console::puts("file exceeds limit: 64KB\n");
            return i;
        }
        
        if(curr_pos % SimpleDisk::BLOCK_SIZE == 0) { // The current block is full or 0
            if (curr_pos != 0) {
                // Write the current fulled block_cache to disk
                fs->disk->write(block_ids[(curr_pos / SimpleDisk::BLOCK_SIZE) - 1], block_cache);
            }
            // We need to prepare a new block
            block_ids[curr_pos / SimpleDisk::BLOCK_SIZE] = fs->GetFreeBlock(); // Here we ignore the possiblily that there is no free block
            // Clear the block_cache
            for(int j = 0; j < SimpleDisk::BLOCK_SIZE; j++) {
                block_cache[j] = 0;
            }
        }

        block_cache[curr_pos % SimpleDisk::BLOCK_SIZE] = _buf[i];
        curr_pos++;
    }
    // Write the last block_cache to disk
    fs->disk->write(block_ids[(curr_pos / SimpleDisk::BLOCK_SIZE)], block_cache);
    
    // Record the size of the file
    fs->disk->write(inode->block_id, block_ids);
    inode->size = curr_pos;
    Reset();
    return _n;
}

void File::Reset() {
    // Console::puts("resetting file\n");
    // Clear the block_cache
    for(int i = 0; i < SimpleDisk::BLOCK_SIZE; i++) {
        block_cache[i] = 0;
    }
    curr_pos = 0;
}

bool File::EoF() {
    // Console::puts("checking for EoF\n");
    return curr_pos >= inode->size;
}
