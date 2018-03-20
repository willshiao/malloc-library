/*
 * mm-naive.c - The fastest, least memory-efficient malloc package.
 *
 * In this naive approach, a block is allocated by simply incrementing
 * the brk pointer.  A block is pure payload. There are no headers or
 * footers.  Blocks are never coalesced or reused. Realloc is
 * implemented directly using mm_malloc and mm_free.
 *
 * NOTE TO STUDENTS: Replace this header comment with your own header
 * comment that gives a high level description of your solution.
 */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>

#include "mm.h"
#include "memlib.h"

team_t team = {
    /* Team name */
    "Piped Piper",
    /* First member's full name */
    "William Shiao",
    /* First member's email address */
    "wshia002@ucr.edu",
    /* Second member's full name (leave blank if none) */
    "James Luo",
    /* Second member's email address (leave blank if none) */
    "jluo011@ucr.edu"
};

// Word size
#define WSIZE 4
// Double word size
#define DSIZE 8
#define BSIZE 24
#define CHUNKSIZE (1 << 12)
// double word alignment
#define ALIGNMENT 8

#define MAX(x, y) ((x) > (y) ? (x) : (y))

// Read a word at p
#define GET(p) (*(unsigned int*) (p))
// Write a word at p
#define PUT(p, val) (*(unsigned int*) (p) = (val))

// Put pointer to next item and allocated bit into a single word
#define PACK(ptr, alloc) ((ptr) | (alloc))

// rounds up to the nearest multiple of ALIGNMENT
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0x7)

// Returns size of block after masking out the allocated bit
#define GET_SIZE(p) (GET(p) & -2)
// Get allocation status of a block
#define GET_ALLOC(p) (GET(p) & 1)

// Get pointer of header and footer from a block pointer
// Required for GET_SIZE and GET_ALLOC
#define HDRP(bp) ((char*)(bp) - WSIZE)
#define FTRP(bp) ((char*)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)

// Get next and previous block pointers from a block ptr
#define NEXT_BLKP(bp)  ((char*)(bp) + GET_SIZE(HDRP(bp)))
#define PREV_BLKP(bp)  ((char*)(bp) - GET_SIZE((char *)(bp) - DSIZE))

// Get next free list item from address of block ptr
//   Note: + DSIZE is because the address of the previous item is 1 dword away
#define NEXT_FREEP(bp) (*(void**) (bp + WSIZE))
#define PREV_FREEP(bp) (*(void**) (bp))

#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

// Pointer to the beginning of the heap
static void* heap_listp = NULL;
// Pointer to the beginning of the explicit free list
static void* f_listp = NULL;

// static function headers
static void *extend_heap(size_t);
static void *coalesce(void*);
static void place(void*, size_t);
static void *find_fit(size_t);
static void delete(void *);

// Debugging (trace) build print macro
#ifdef TRACE
    #define dbg_printf(...) \
        do { printf(__VA_ARGS__); } while (0)
#else
    #define dbg_printf(...) do {} while (0)
#endif


/**
 * Initializes the malloc package
 *
 * @return  Returns a negative int on failure and a 0 on success.
 */
int mm_init(void) {
    if ((heap_listp = mem_sbrk(8 * WSIZE)) == (void*)-1) {
        return -1;
    }

    PUT(heap_listp, 0);
    PUT(heap_listp + (1 * WSIZE), PACK(DSIZE, 1));
    PUT(heap_listp + (2 * WSIZE), PACK(DSIZE, 1));
    PUT(heap_listp + (3 * WSIZE), PACK(0, 1));
    f_listp = heap_listp + DSIZE;

    if (extend_heap(CHUNKSIZE / WSIZE) == NULL) { // extend_heap failed
        return -1;
    }
    return 0;
}


/**
 * Allocates a block and increments the brk pointer.
 *   Blocks are allocated in sizes that are multiples of the alignment.
 *
 * @param   sizes  The size of the new block
 * @return         A pointer to the start of the new block.
 */
void *mm_malloc(size_t size) {
    size_t asize;  // Adjusted block size block size
    char *bp;

    if (size == 0) return NULL;

    // Check if the block size is less than the minimum block size
    if (size <= DSIZE) {
        asize = 2 * DSIZE;
    } else {
        asize = DSIZE * ((size + (DSIZE) + (DSIZE - 1)) / DSIZE);
    }

    // Search the free list for a fit
    if ((bp = find_fit(asize)) != NULL) {
        place(bp, asize);
        return bp;
    }

    // No fit found. Get more memory and place block
    if ((bp = extend_heap(MAX(asize, CHUNKSIZE) / WSIZE)) == NULL) {
        return NULL;
    }
    place(bp, asize);
    return bp;
}


/**
 * Frees a block of memory starting at ptr
 * @param   ptr   A pointer to the block of memory. Returned by the malloc call.
 */
void mm_free(void *ptr) {
    if (!ptr) return;

    // See if heap_list is initialized,
    //   and if not, call mm_init, and return if it fails.
    if (!heap_listp && mm_init() == -1) return;

    size_t size = GET_SIZE(HDRP(ptr));
    // Reset allocation status
    PUT(HDRP(ptr), PACK(size, 0));
    PUT(FTRP(ptr), PACK(size, 0));
    coalesce(ptr);
}


/**
 * Increases the size of an existing block.
 * @param  ptr  A pointer to the start of the block. Returned by malloc().
 * @param  size The size of the new block.
 * @return      A pointer to the location of the new block.
 */
void *mm_realloc(void *ptr, size_t size) {
    if (size <= 0) {  // if size == 0, just free and we're done
        mm_free(ptr);
        return ptr;
    }
    // If the current block pointer is invalid, just malloc a new block
    if (!ptr) return mm_malloc(size);

    // Get size of block to know how much to copy
    size_t copySize = GET_SIZE(HDRP(ptr));
    // Optimization: if the new size == current size, don't malloc a new block
    if (size + 2 * DSIZE <= copySize) {
      return ptr;
    }

    int currSize = GET_SIZE(HDRP(ptr));
    int nextAlloc = GET_ALLOC(HDRP(NEXT_BLKP(ptr)));
    int nextSize = GET_SIZE(HDRP(NEXT_BLKP(ptr)));

    // Optimization: if the next block if not allocated and
    //   there is enough space, expand the block by merging
    //   the current block and next one
    if (!nextAlloc && (nextSize + currSize > size + DSIZE)) {
        delete(NEXT_BLKP(ptr));
        PUT(HDRP(ptr), PACK(nextSize + currSize, 1));
        PUT(FTRP(ptr), PACK(nextSize + currSize, 1));
        return ptr;
    }

    // Malloc and return NULL if it fails
    void *newptr = mm_malloc(size);
    if (newptr == NULL) return NULL;

    // If shrinking the block, only copy part of the data
    if (size < copySize) {
        copySize = size;
    }
    memcpy(newptr, ptr, copySize);
    mm_free(ptr);
    return newptr;
}
/**
 * Function to extend the size of the heap.
 * @param  words  The number of words to expand the heap by.
 *                If odd, it will be rounded up to the next even number.
 * @return        Returns a ptr to the location of the new start of the heap.
 */
static void *extend_heap(size_t words) {
    char * bp;

    // Always extend the heap by an even number of words
    size_t size = (words % 2) ? (words + 1) * WSIZE : words * WSIZE;
    if ((long) (bp = mem_sbrk(size)) == -1) {
        return NULL;
    }

    PUT(HDRP(bp), PACK(size, 0));  // Set header for new block
    PUT(FTRP(bp), PACK(size, 0));  // Set footer for new block
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1));  // Create new ending dummy block

    dbg_printf("Extending heap by %d words\n", words);

    return coalesce(bp);
}


/**
 * Initializes a block of memory, given the pointer and size.
 * @param bp      A pointer to the target block of memory.
 * @param asize   The size of the target block.
 */
static void place(void *bp, size_t asize) {
    dbg_printf("~~ Place called!\n");
    // Size of the current size
    size_t currentSize = GET_SIZE(HDRP(bp));

    // If the leftover space is more than one block
    if ((currentSize - asize) >= BSIZE) {
        PUT(HDRP(bp), PACK(asize, 1));
        PUT(FTRP(bp), PACK(asize, 1));
        delete(bp);  // remove it from the free list
        bp = NEXT_BLKP(bp);
        PUT(HDRP(bp), PACK(currentSize - asize, 0));
        PUT(FTRP(bp), PACK(currentSize - asize, 0));
        coalesce(bp);
    } else {
        PUT(HDRP(bp), PACK(currentSize, 1));
        PUT(FTRP(bp), PACK(currentSize, 1));
        delete(bp);
    }
    dbg_printf("~~ Place done!\n");
}


/**
 * Finds a suitable location for a block of memory.
 *   Currently using the first-fit algorithm.
 *
 * @param  asize  The size of the memory block we are looking for.
 * @return        A pointer to the start of the target location. NULL if there is no location found.
 */
static void *find_fit(size_t asize) {
    // Shouldn't happen, but just in case mm_init wasn't called
    if (!f_listp && mm_init() == -1) return NULL;

    void* p;
    for (p = f_listp; GET_ALLOC(HDRP(p)) == 0; p = NEXT_FREEP(p)) {
        if (GET_SIZE(HDRP(p)) >= asize) return p;
    }
    return NULL;
}


/**
 * Performs coalescing on free memory blocks.
 *   Joins adjacent free blocks
 * @param  bp  A pointer to the block.
 * @return     A pointer to the coalesced block.
 */
static void *coalesce(void* bp) {
    dbg_printf(">> Coalesce called on %p\n", bp);
    size_t prevAlloc = GET_ALLOC(FTRP(PREV_BLKP(bp))) || PREV_BLKP(bp) == bp;
    dbg_printf("prevAlloc = %d\n", prevAlloc);
    // Make sure we're not at the beginning of the list
    // if (!prevAlloc) prevAlloc = PREV_BLKP(bp) == bp;

    dbg_printf("HDRP(NEXT(BP)) = %p\n", HDRP(NEXT_BLKP(bp)));
    size_t nextAlloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
    dbg_printf("nextAlloc = %d\n", nextAlloc);
    size_t size = GET_SIZE(HDRP(bp));

    if (!prevAlloc && !nextAlloc) {
        dbg_printf("=> Case 1\n");

        // Eat both adjacent blocks, if possible
        size += GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(FTRP(NEXT_BLKP(bp)));
        delete(PREV_BLKP(bp));
        delete(NEXT_BLKP(bp));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
    } else if (prevAlloc && !nextAlloc) {
        dbg_printf("=> Case 2\n");

        // Eat the next block, if possible
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
        delete(NEXT_BLKP(bp));
        dbg_printf(" => Returned from delete\n");
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));
    } else if (!prevAlloc && nextAlloc) {
        dbg_printf("=> Case 3\n");

        // Eat the previous block, if possible
        size += GET_SIZE(FTRP(PREV_BLKP(bp)));
        bp = PREV_BLKP(bp);
        delete(bp);
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));
    }

    // Insert block at head of linked list
    NEXT_FREEP(bp) = f_listp;
    PREV_FREEP(f_listp) = bp;
    PREV_FREEP(bp) = NULL;
    f_listp = bp;

    return bp;
}


/**
 * Delete the targeted block from the explicit free list.
 * @param ptr  A pointer to the block to be deleted.
 */
static void delete(void *ptr) {
    dbg_printf("Delete called w/ ptr = %p!\n", ptr);

    if (PREV_FREEP(ptr)) {  // If the current block is not the head
        dbg_printf("-> Case 1!\n");
        // ptr->prev->next = ptr->next
        NEXT_FREEP(PREV_FREEP(ptr)) = NEXT_FREEP(ptr);
    } else {  // Otherwise, change the head
        dbg_printf("-> Case 2!\n");
        f_listp = NEXT_FREEP(ptr);
    }

    dbg_printf("-> f_listp = %p\n", f_listp);

    // ptr->next->prev = ptr->prev
    PREV_FREEP(NEXT_FREEP(ptr)) = PREV_FREEP(ptr);
    dbg_printf("Delete done!\n");
}
