/*
 File: vm_pool.C
 
 Author:
 Date  :
 
 */

/*--------------------------------------------------------------------------*/
/* DEFINES */
/*--------------------------------------------------------------------------*/

/* -- (none) -- */

/*--------------------------------------------------------------------------*/
/* INCLUDES */
/*--------------------------------------------------------------------------*/

#include "vm_pool.H"
#include "console.H"
#include "utils.H"
#include "assert.H"
#include "simple_keyboard.H"

/*--------------------------------------------------------------------------*/
/* DATA STRUCTURES */
/*--------------------------------------------------------------------------*/

/* -- (none) -- */

/*--------------------------------------------------------------------------*/
/* CONSTANTS */
/*--------------------------------------------------------------------------*/

/* -- (none) -- */

/*--------------------------------------------------------------------------*/
/* FORWARDS */
/*--------------------------------------------------------------------------*/

/* -- (none) -- */

/*--------------------------------------------------------------------------*/
/* METHODS FOR CLASS   V M P o o l */
/*--------------------------------------------------------------------------*/

VMPool::VMPool(unsigned long  _base_address,
               unsigned long  _size,
               ContFramePool *_frame_pool,
               PageTable     *_page_table) {
    base_address = _base_address;
    size = _size;
    frame_pool = _frame_pool;
    page_table = _page_table;

    // Register this pool to given page table
    page_table->register_pool(this);

    // Set the allocated region array to the first page in the pool. 
    allocated_region_array = (AllocatedRegion *) base_address;

    // Originally there are no allocated regions.
    total_regions = 0;

    Console::puts("Constructed VMPool object.\n");
}

unsigned long VMPool::allocate(unsigned long _size) {
    // Calculate the number of frames needed to allocate the requested size. Equivilent to ceil(_size / PAGE_SIZE).
    int allocating_frames = (_size - 1) / Machine::PAGE_SIZE + 1;
    unsigned long allocating_size = (unsigned long) allocating_frames * Machine::PAGE_SIZE;

    // Check if there are enough frames to allocate the requested size.
    // First we walk through all the regions and check if there is enough space between them to allocate the requested size.
    // If there is enough space, we insert the new region in between. If not, we add the new region to the end of the array.

    if(total_regions == 0) {
        // If there are no regions, we can allocate the requested size at the second frame in the pool, 
        // since the first frame is used for the allocated region array.
        allocated_region_array[0].base_address = base_address + Machine::PAGE_SIZE;
        allocated_region_array[0].size = allocating_size;
    }
    else {
        bool found = false;
        // If there are regions, we walk through them and check if there is enough space between them to allocate the requested size.
        for(int i = 1; i < total_regions; i++) {
            // If there is enough space between the current region and the next region, we insert the new region in between.
            if(allocated_region_array[i].base_address - (allocated_region_array[i-1].base_address + allocated_region_array[i-1].size) >= allocating_size) {
                found = true;
                // Shift all the regions after the current region by one to the right.
                for(int j = total_regions; j > i; j--) {
                    allocated_region_array[j] = allocated_region_array[j-1];
                }
                
                // Insert the new region
                allocated_region_array[i].base_address = allocated_region_array[i-1].base_address + allocated_region_array[i-1].size;
                allocated_region_array[i].size = allocating_size;
                break;
            }
        }

        if(!found) {
            // If there is not enough space between the regions, we add the new region to the end of the array.
            // But first check if the adding new region will out of bound of the pool.
            if(base_address + size - (allocated_region_array[total_regions-1].base_address + allocated_region_array[total_regions-1].size) < allocating_size) {
                return 0;
            }
            
            allocated_region_array[total_regions].base_address = allocated_region_array[total_regions-1].base_address + allocated_region_array[total_regions-1].size;
            allocated_region_array[total_regions].size = allocating_size;
        }
        
    }
    total_regions++;
    Console::puts("Allocated region of memory.\n");
        
    return allocated_region_array[total_regions-1].base_address;   
}

void VMPool::release(unsigned long _start_address) {
    // Walk through all the regions and find the region that contains the given address.
    region_to_delete = -1;
    for(int i = 0; i < total_regions; i++) {
        if(allocated_region_array[i].base_address == _start_address) {
            region_to_delete = i;
            break;
        }
    }

    // If the given address is not in the allocated region, return.
    if(region_to_delete == -1) {
        return;
    }

    // Free the pages in the region.
    for(int i = 0; i < allocated_region_array[region_to_delete].size / Machine::PAGE_SIZE; i++) {
        page_table->free_page(allocated_region_array[region_to_delete].base_address + i * Machine::PAGE_SIZE);
    }

    // Shift all the regions after the region to delete by one to the left.
    for(int i = region_to_delete; i < total_regions - 1; i++) {
        allocated_region_array[i] = allocated_region_array[i+1];
    }
    total_regions--;

    Console::puts("Released region of memory.\n");
}

bool VMPool::is_legitimate(unsigned long _address) {
    if(_address < base_address || _address >= base_address + size) {
        return false;
    }
    return true;
}

