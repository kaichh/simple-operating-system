#include "assert.H"
#include "exceptions.H"
#include "console.H"
#include "paging_low.H"
#include "page_table.H"

PageTable * PageTable::current_page_table = NULL;
unsigned int PageTable::paging_enabled = 0;
ContFramePool * PageTable::kernel_mem_pool = NULL;
ContFramePool * PageTable::process_mem_pool = NULL;
unsigned long PageTable::shared_size = 0;
VMPool * PageTable::vm_pool_head = NULL;


void PageTable::init_paging(ContFramePool * _kernel_mem_pool,
                            ContFramePool * _process_mem_pool,
                            const unsigned long _shared_size)
{
    kernel_mem_pool = _kernel_mem_pool;
    process_mem_pool = _process_mem_pool;
    shared_size = _shared_size;
}

PageTable::PageTable()
{
    // Require 1 frame for the page directory
    page_directory = (unsigned long *) (process_mem_pool->get_frames(1) * PAGE_SIZE);
    // Require 1 frame for the first page table page
    unsigned long * page_table = (unsigned long *) (process_mem_pool->get_frames(1) * PAGE_SIZE);

    // Init the first page table page
    unsigned long address = 0;
    for (int i = 0; i < 1024; i++)
    {
        // Set as 1 to 1 mapping to physical memory (0 - 4MB)
        page_table[i] = address | 3; // Set supervisor, read/write, present mode. This means the last 3 bits is 011
        address += PAGE_SIZE;
    }

    // Init the page directory entry for the first page table page
    page_directory[0] = (unsigned long) page_table;
    page_directory[0] |= 3; 

    // Init the remaining 1023 entries in page_directory
    for (int i = 1; i < 1024; i++)
    {
        if(i == 1023) {
            // Set the last entry to point to the page directory itself
            page_directory[i] = (unsigned long) page_directory | 3;
        }
        else {
            page_directory[i] = 0 | 2; // Set supervisor, read/write, not present mode. This means the last 3 bits is 010
        }
    }
}


void PageTable::load()
{
    current_page_table = this;
    // Store the address of the page_directory to CR3
    write_cr3((unsigned long) current_page_table->page_directory);
}

void PageTable::enable_paging()
{
    write_cr0(read_cr0() | 0x80000000); // Set the paging bit in CR0 to 1
    paging_enabled = 1;
}

void PageTable::handle_fault(REGS * _r)
{
    unsigned long fault_address = read_cr2();
    unsigned long page_directory_index = fault_address >> 22;
    unsigned long page_table_index = (fault_address >> 12) & 0x3FF;
    unsigned long * PD_recursive_addr = (unsigned long *) 0xFFFFF000; // 1023 | 1023 | 0 * 12
    unsigned long * PT_recursive_addr = (unsigned long *) (0xFFC00000 | page_directory_index << 12); // 1023 | page_directory_index | 0 * 12
    
    // Find which pool the fault address belongs to
    VMPool * curr_pool = vm_pool_head;
    bool found = false;
    while (curr_pool != NULL)
    {
        if (curr_pool->is_legitimate(fault_address)) {
            found = true;
            break;
        }
        curr_pool = curr_pool->next;
    }

    if(!found && curr_pool != NULL) {
        Console::puts("Can't find the address in any VM pool\n");
        assert(false);
    }

    if ((PD_recursive_addr[page_directory_index] & 1) == 0) {
        // The page directory entry is not present
        // Allocate a frame for the page table, and get its physical address as new_page_table
        unsigned long * new_page_table = (unsigned long *) (process_mem_pool->get_frames(1) * PAGE_SIZE);
        
        // Set the page directory entry to the new page table
        PD_recursive_addr[page_directory_index] = (unsigned long) new_page_table | 3;
        
        // Init the new page table
        for (int i = 0; i < 1024; i++)
        {
            PT_recursive_addr[i] = 0 | 2; // Set supervisor, read/write, not present mode. This means the last 3 bits is 010
        }
    } 
    else {
        // The page table page entry is not present
        PT_recursive_addr[page_table_index] = (process_mem_pool->get_frames(1) * PAGE_SIZE) | 3; 
    }
}

void PageTable::register_pool(VMPool * _vm_pool)
{
    if(vm_pool_head == NULL) {
        vm_pool_head = _vm_pool;
    }
    else {
        // Move to the end of the linked list
        VMPool * curr_pool = vm_pool_head;
        while (curr_pool->next != NULL)
        {
            curr_pool = curr_pool->next;
        }
        curr_pool->next = _vm_pool;
    }
    Console::puts("registered VM pool\n");
}

void PageTable::free_page(unsigned long _page_no) {
    unsigned long pd_index = _page_no >> 22;
    unsigned long pt_index = (_page_no >> 12) & 0x3FF;
    unsigned long * PT_recursive_addr = (unsigned long *) (0xFFC00000 | pd_index << 12); // 1023 | pd_index | 0 * 12
    if((PT_recursive_addr[pt_index] & 1) == 1) {
        // The page is valid
        // Release the frame
        process_mem_pool->release_frames(PT_recursive_addr[pt_index] / PAGE_SIZE);
        // Mark the page as invalid
        PT_recursive_addr[pt_index] = 2; // Set supervisor, read/write, not present mode. This means the last 3 bits is 010
        load();
    }
    Console::puts("freed page\n");
}
