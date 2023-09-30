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
   page_directory = (unsigned long *) (kernel_mem_pool->get_frames(1) * PAGE_SIZE);
   // Require 1 frame for the first page table page
   unsigned long * page_table = (unsigned long *) (kernel_mem_pool->get_frames(1) * PAGE_SIZE);
   
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
      page_directory[i] = 0 | 2; // Set supervisor, read/write, not present mode. This means the last 3 bits is 010
   }
}


void PageTable::load()
{
   current_page_table = this;
   // Store the address of the page_directory to CR3
   write_cr3((unsigned long) this->page_directory);
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
   unsigned long * page_table = (unsigned long *) (current_page_table->page_directory[page_directory_index] & 0xFFFFF000);
   
   if ((current_page_table->page_directory[page_directory_index] & 1) == 0)
   {
      // The page directory entry is not present
      // Allocate a frame for the page table
      unsigned long * new_page_table = (unsigned long *) (kernel_mem_pool->get_frames(1) * PAGE_SIZE);
      // Set the page directory entry to the new page table
      current_page_table->page_directory[page_directory_index] = (unsigned long) new_page_table;
      current_page_table->page_directory[page_directory_index] |= 3;  // Set supervisor, read/write, present mode. This means the last 3 bits is 011
      // Init the new page table
      for (int i = 0; i < 1024; i++)
      {
         new_page_table[i] = 0 | 2; // Set supervisor, read/write, not present mode. This means the last 3 bits is 010
      }
   } 
   else {
      // The page table page entry is not present
      // Allocate a frame for the page
      unsigned long new_frame = (unsigned long) (process_mem_pool->get_frames(1) * PAGE_SIZE);
      // Set the page table entry to the new frame
      page_table[page_table_index] = new_frame | 3; // Set supervisor, read/write, present mode. This means the last 3 bits is 011
   }
   
}

