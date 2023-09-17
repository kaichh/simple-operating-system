/*
 File: ContFramePool.C
 
 Author:
 Date  : 
 
 */

/*--------------------------------------------------------------------------*/
/* 
 POSSIBLE IMPLEMENTATION
 -----------------------

 The class SimpleFramePool in file "simple_frame_pool.H/C" describes an
 incomplete vanilla implementation of a frame pool that allocates 
 *single* frames at a time. Because it does allocate one frame at a time, 
 it does not guarantee that a sequence of frames is allocated contiguously.
 This can cause problems.
 
 The class ContFramePool has the ability to allocate either single frames,
 or sequences of contiguous frames. This affects how we manage the
 free frames. In SimpleFramePool it is sufficient to maintain the free 
 frames.
 In ContFramePool we need to maintain free *sequences* of frames.
 
 This can be done in many ways, ranging from extensions to bitmaps to 
 free-lists of frames etc.
 
 IMPLEMENTATION:
 
 One simple way to manage sequences of free frames is to add a minor
 extension to the bitmap idea of SimpleFramePool: Instead of maintaining
 whether a frame is FREE or ALLOCATED, which requires one bit per frame, 
 we maintain whether the frame is FREE, or ALLOCATED, or HEAD-OF-SEQUENCE.
 The meaning of FREE is the same as in SimpleFramePool. 
 If a frame is marked as HEAD-OF-SEQUENCE, this means that it is allocated
 and that it is the first such frame in a sequence of frames. Allocated
 frames that are not first in a sequence are marked as ALLOCATED.
 
 NOTE: If we use this scheme to allocate only single frames, then all 
 frames are marked as either FREE or HEAD-OF-SEQUENCE.
 
 NOTE: In SimpleFramePool we needed only one bit to store the state of 
 each frame. Now we need two bits. In a first implementation you can choose
 to use one char per frame. This will allow you to check for a given status
 without having to do bit manipulations. Once you get this to work, 
 revisit the implementation and change it to using two bits. You will get 
 an efficiency penalty if you use one char (i.e., 8 bits) per frame when
 two bits do the trick.
 
 DETAILED IMPLEMENTATION:
 
 How can we use the HEAD-OF-SEQUENCE state to implement a contiguous
 allocator? Let's look a the individual functions:
 
 Constructor: Initialize all frames to FREE, except for any frames that you 
 need for the management of the frame pool, if any.
 
 get_frames(_n_frames): Traverse the "bitmap" of states and look for a 
 sequence of at least _n_frames entries that are FREE. If you find one, 
 mark the first one as HEAD-OF-SEQUENCE and the remaining _n_frames-1 as
 ALLOCATED.

 release_frames(_first_frame_no): Check whether the first frame is marked as
 HEAD-OF-SEQUENCE. If not, something went wrong. If it is, mark it as FREE.
 Traverse the subsequent frames until you reach one that is FREE or 
 HEAD-OF-SEQUENCE. Until then, mark the frames that you traverse as FREE.
 
 mark_inaccessible(_base_frame_no, _n_frames): This is no different than
 get_frames, without having to search for the free sequence. You tell the
 allocator exactly which frame to mark as HEAD-OF-SEQUENCE and how many
 frames after that to mark as ALLOCATED.
 
 needed_info_frames(_n_frames): This depends on how many bits you need 
 to store the state of each frame. If you use a char to represent the state
 of a frame, then you need one info frame for each FRAME_SIZE frames.
 
 A WORD ABOUT RELEASE_FRAMES():
 
 When we releae a frame, we only know its frame number. At the time
 of a frame's release, we don't know necessarily which pool it came
 from. Therefore, the function "release_frame" is static, i.e., 
 not associated with a particular frame pool.
 
 This problem is related to the lack of a so-called "placement delete" in
 C++. For a discussion of this see Stroustrup's FAQ:
 http://www.stroustrup.com/bs_faq2.html#placement-delete
 
 */
/*--------------------------------------------------------------------------*/


/*--------------------------------------------------------------------------*/
/* DEFINES */
/*--------------------------------------------------------------------------*/

/* -- (none) -- */

/*--------------------------------------------------------------------------*/
/* INCLUDES */
/*--------------------------------------------------------------------------*/

#include "cont_frame_pool.H"
#include "console.H"
#include "utils.H"
#include "assert.H"

/*--------------------------------------------------------------------------*/
/* DATA STRUCTURES */
/*--------------------------------------------------------------------------*/

ContFramePool * ContFramePool::pool_head = NULL;
int ContFramePool::pool_num = 0;

/*--------------------------------------------------------------------------*/
/* CONSTANTS */
/*--------------------------------------------------------------------------*/

/* -- (none) -- */

/*--------------------------------------------------------------------------*/
/* FORWARDS */
/*--------------------------------------------------------------------------*/

/* -- (none) -- */

/*--------------------------------------------------------------------------*/
/* METHODS FOR CLASS   C o n t F r a m e P o o l */
/*--------------------------------------------------------------------------*/
ContFramePool::FrameState ContFramePool::get_state(unsigned long _frame_no) {
    unsigned int bitmap_index = _frame_no / 4; // 2 bits per frame,  1 byte can represent 4 frames
    unsigned char mask = 0x3; 
    unsigned char target = bitmap[bitmap_index] >> ((_frame_no % 4) * 2);

    switch (target & mask) {
        case 0x0:
            return FrameState::Free;
        case 0x1:
            return FrameState::Used;
        case 0x2:
            return FrameState::HoS;
        default:
            assert(false);
    }    
}

void ContFramePool::set_state(unsigned long _frame_no, FrameState _state) {
    unsigned int bitmap_index = _frame_no / 4;
    unsigned char mask_init = 0x3 << ((_frame_no % 4) * 2); 
    unsigned char mask;
    bitmap[bitmap_index] &= ~mask_init; //set the bits to 00
    

    switch(_state) {
        case FrameState::Free:
            mask = 0x0 << ((_frame_no % 4) * 2);
            break;
        case FrameState::Used:
            mask = 0x1 << ((_frame_no % 4) * 2);
            break;
        case FrameState::HoS:
            mask = 0x2 << ((_frame_no % 4) * 2);
            break;
    }
    bitmap[bitmap_index] ^= mask; 

}
    

ContFramePool::ContFramePool(unsigned long _base_frame_no,
                             unsigned long _n_frames,
                             unsigned long _info_frame_no)
{
    base_frame_no = _base_frame_no;
    nframes = _n_frames;
    nFreeFrames = _n_frames;
    info_frame_no = _info_frame_no;

    // If _info_frame_no is zero then we keep management info in the first
    //frame, else we use the provided frame to keep management info
    if(info_frame_no == 0) {
        bitmap = (unsigned char *) (base_frame_no * FRAME_SIZE);
    } else {
        bitmap = (unsigned char *) (info_frame_no * FRAME_SIZE);
    }

    // Proceed to mark all frame as free.
    for(int fno = 0; fno < nframes; fno++) {
        set_state(fno, FrameState::Free);
    }

    // Mark the first frame as being used if it is being used
    if(_info_frame_no == 0) {
        set_state(0, FrameState::HoS);
        nFreeFrames--;
    }

    // Manage the linked list of frame pools
    if(pool_head == NULL){
        pool_head = this;
        pool_next = NULL;
    } else {
        ContFramePool * cur = pool_head;
        for(int i = 1; i < pool_num; i++){
            cur = cur->pool_next;
        }
        cur->pool_next = this;
    }
    pool_num++;
}

unsigned long ContFramePool::get_frames(unsigned int _n_frames)
{
    if(_n_frames > nFreeFrames){
        return 0;
    }

    unsigned long start_frame = 0;
    unsigned long cur_frame = 0;
    unsigned int count = 0;
    bool found = false;
    
    while(start_frame <= nframes) {
        count = 0;
        cur_frame = start_frame;

        while(cur_frame < nframes && get_state(cur_frame) == FrameState::Free){
            count++;
            if (count == _n_frames){
                found = true;
                break;
            }
            cur_frame++;
        }

        if(found){
            break;
        } else {
            start_frame = cur_frame + 1;
        }
    }

    // If we found an available sequence of frames, mark them as used, then return the first frame
    if(found) {
        set_state(start_frame, FrameState::HoS);
        nFreeFrames--;
        for(int i = 1; i < _n_frames; i++){
            set_state(start_frame + i, FrameState::Used);
            nFreeFrames--;
        }
        return base_frame_no + start_frame;
    } else {
        return 0;
    }

}

void ContFramePool::mark_inaccessible(unsigned long _base_frame_no,
                                      unsigned long _n_frames)
{
    // Mark all frames in the range as being used.
    set_state(_base_frame_no - base_frame_no, FrameState::HoS);
    nFreeFrames--;
    for(int i = 1; i < _n_frames; i++){
        set_state(_base_frame_no - base_frame_no + i, FrameState::Used);
        nFreeFrames--;
    }
}

void ContFramePool::release_frames(unsigned long _first_frame_no)
{
    ContFramePool * cur_pool = pool_head;
    while(_first_frame_no < cur_pool->base_frame_no || _first_frame_no > (cur_pool->base_frame_no + cur_pool->nframes)){
        cur_pool = cur_pool->pool_next;
    }

    cur_pool->release_frame_within_pool(_first_frame_no);
}

void ContFramePool::release_frame_within_pool(unsigned long _first_frame_no)
{
    unsigned long cur_frame = _first_frame_no - base_frame_no;
    
    // If the frame is not the head of a sequence, then something went wrong
    assert(get_state(cur_frame) == FrameState::HoS);

    set_state(cur_frame, FrameState::Free);
    nFreeFrames++;
    cur_frame++;
    while(cur_frame < nframes && get_state(cur_frame) == FrameState::Used){
        set_state(cur_frame, FrameState::Free);
        nFreeFrames++;
        cur_frame++;
    }
}

unsigned long ContFramePool::needed_info_frames(unsigned long _n_frames)
{
    unsigned long frames_per_bitmap = 4096 * 8 / 2; // How many frames a 1-frame bitmap can manage? 2 bits per frame, 1 byte can represent 4 frames
    return _n_frames / frames_per_bitmap + (_n_frames % frames_per_bitmap > 0 ? 1 : 0);
}
