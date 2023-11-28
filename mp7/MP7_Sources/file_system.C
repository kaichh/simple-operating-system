/*
     File        : file_system.C

     Author      : Riccardo Bettati
     Modified    : 2021/11/28

     Description : Implementation of simple File System class.
                   Has support for numerical file identifiers.
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
#include "file_system.H"

/*--------------------------------------------------------------------------*/
/* CLASS Inode */
/*--------------------------------------------------------------------------*/

/* You may need to add a few functions, for example to help read and store 
   inodes from and to disk. */

/*--------------------------------------------------------------------------*/
/* CLASS FileSystem */
/*--------------------------------------------------------------------------*/

/*--------------------------------------------------------------------------*/
/* CONSTRUCTOR */
/*--------------------------------------------------------------------------*/

FileSystem::FileSystem() {
    Console::puts("In file system constructor.\n");
    disk = NULL;
    size = 0;
    inodes = NULL;
    // inodes = new Inode[MAX_INODES];
    free_blocks = NULL;
}

FileSystem::~FileSystem() {
    Console::puts("unmounting file system\n");
    /* Make sure that the inode list and the free list are saved. */
    
    // Write the inode list to block 0
    disk->write(0, (unsigned char *) inodes);
    // Write the free list to block 1
    disk->write(1, free_blocks);
    
    delete inodes;
    delete free_blocks;
}


/*--------------------------------------------------------------------------*/
/* FILE SYSTEM FUNCTIONS */
/*--------------------------------------------------------------------------*/


bool FileSystem::Mount(SimpleDisk * _disk) {
    Console::puts("mounting file system from disk\n");
    /* Here you read the inode list and the free list into memory */

    disk = _disk;
    size = _disk->size();
    
    unsigned char * temp_inode_block  = new unsigned char[SimpleDisk::BLOCK_SIZE];
    disk->read(0, temp_inode_block);
    inodes = (Inode *) temp_inode_block;

    free_blocks = new unsigned char[SimpleDisk::BLOCK_SIZE];
    disk->read(1, free_blocks);

    delete temp_inode_block;
    return true;
}

bool FileSystem::Format(SimpleDisk * _disk, unsigned int _size) { // static!
    Console::puts("formatting disk\n");
    /* Here you populate the disk with an initialized (probably empty) inode list
       and a free list. Make sure that blocks used for the inodes and for the free list
       are marked as used, otherwise they may get overwritten. */
    
    // Set the inodes list with size MAX_INODES to 0 and write to block 0
    Inode * empty_inode_block = new Inode[MAX_INODES];
    memset(empty_inode_block, 0, MAX_INODES);
    _disk->write(0, (unsigned char *) empty_inode_block);

    // Mark the 1st and 2nd position as used and write to the disk at block 1
    unsigned char empty_free_block[SimpleDisk::BLOCK_SIZE];
    memset(empty_free_block, 0, SimpleDisk::BLOCK_SIZE);
    empty_free_block[0] = 1;
    empty_free_block[1] = 1;
    _disk->write(1, empty_free_block);

    return true;
}

Inode * FileSystem::LookupFile(int _file_id) {
    Console::puts("looking up file with id = "); Console::puti(_file_id); Console::puts("\n");
    /* Here you go through the inode list to find the file. */
    
    for(int i = 0 ; i < MAX_INODES; i++) {
        if(inodes[i].id == _file_id) {
            return &inodes[i];
        }
    }
    // If the file is not found, return NULL
    return NULL; 
}

bool FileSystem::CreateFile(int _file_id) {
    Console::puts("creating file with id:"); Console::puti(_file_id); Console::puts("\n");
    /* Here you check if the file exists already. If so, throw an error.
       Then get yourself a free inode and initialize all the data needed for the
       new file. After this function there will be a new file on disk. */
    int free_block_id = GetFreeBlock();

    if(LookupFile(_file_id) != NULL || free_block_id == -1) {
        return false;
    }
    // Find a free inode and initialize it
    for(int i = 0 ; i < MAX_INODES; i++) {
        if(inodes[i].id == 0) { 
            inodes[i].id = _file_id;
            inodes[i].block_id = free_block_id;
            inodes[i].fs = this;
            inodes[i].size = 0;
            
            // Mark the block as used in the free list
            free_blocks[inodes[i].block_id] = 1;
            return true;
        }
    }
    // If no free inode is found, return false
    return false;
}

bool FileSystem::DeleteFile(int _file_id) {
    Console::puts("deleting file with id:"); Console::puti(_file_id); Console::puts("\n");
    /* First, check if the file exists. If not, throw an error. 
       Then free all blocks that belong to the file and delete/invalidate 
       (depending on your implementation of the inode list) the inode. */
    
    Inode * file_inode = LookupFile(_file_id);
    if(file_inode == NULL) {
        return false;
    }
    
    // First we need to free all the blocks that belong to the file. All the blocks ids are stored in inode->block_id
    unsigned char block_ids[SimpleDisk::BLOCK_SIZE];
    disk->read(file_inode->block_id, block_ids);
    for(int i = 0 ; i < SimpleDisk::BLOCK_SIZE; i++) {
        if(block_ids[i] != 0) {
            free_blocks[block_ids[i]] = 0;
        }
    }
    // Then we can free the block_id of the inode itself
    free_blocks[file_inode->block_id] = 0;

    // Finally we can invalidate the inode
    file_inode->id = 0;
    file_inode->block_id = 0;
    file_inode->fs = NULL;
    file_inode->size = 0;

    return true;
}

int FileSystem::GetFreeBlock() {
    Console::puts("getting free block\n");
    /* Here you go through the free list to find a free block. */
    
    for(int i = 0 ; i < SimpleDisk::BLOCK_SIZE; i++) {
        if(free_blocks[i] == 0) {
            // Wipe the block
            unsigned char empty_block[SimpleDisk::BLOCK_SIZE];
            memset(empty_block, 0, SimpleDisk::BLOCK_SIZE);
            disk->write(i, empty_block);
            
            // Mark the block as used in the free list
            free_blocks[i] = 1;
            return i;
        }
    }
    // If no free block is found, return -1
    Console::puts("no free block found!!\n");
    return -1;
}

// unsigned char * FileSystem::GetBlockIds(int _block_id) {
//     Console::puts("getting actual block ids\n");
    
//     unsigned char * block_ids = new unsigned char[SimpleDisk::BLOCK_SIZE];
//     disk->read(_block_id, block_ids);
//     return block_ids;
// }
