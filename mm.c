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

/*********************************************************
 * NOTE TO STUDENTS: Before you do anything else, please
 * provide your team information in the following struct.
 ********************************************************/
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
#define CHUNKSIZE (1 << 12)
/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8

#define MAX(x, y) ((x) > (y) ? (x) : (y))

#define GET(p) (*(unsigned int*) (p))
#define PUT(p, val) (*(unsigned int*) (p) = (val))

// Put size and allocated bit into a single word
#define PACK(size, alloc) ((size) | (alloc))

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0x7)
// Returns size of block after masking out the allocated bit
#define GET_SIZE(p) (GET(p) & -2)
#define GET_ALLOC(p) (GET(p) & 1)
// Get address of header and fooder from a block pointer
#define HDRP(bp) ((char*)(bp) - WSIZE)
#define FTRP(bp) ((char*)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)
// Get next and previous blocks from a block ptr
#define NEXT_BLKP(bp)  ((char*)(bp) + GET_SIZE(((char*)(bp) - WSIZE)))
#define PREV_BLKP(bp)  ((char*)(bp) - GET_SIZE(((char*)(bp) - DSIZE)))

#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

void* heap_listp = NULL;

static void *extend_heap(size_t);
static void *coalesce(void*);
static void place(void*, size_t);
static void *find_fit(size_t);

// cleared
/* 
 * mm_init - initialize the malloc package.
 */
int mm_init(void) {
    if((heap_listp = mem_sbrk(4 * WSIZE)) == (void*)-1) 
        return -1;
    PUT(heap_listp, 0);
    PUT(heap_listp + (1 * WSIZE), PACK(DSIZE, 1));
    PUT(heap_listp + (2 * WSIZE), PACK(DSIZE, 1));
    PUT(heap_listp + (3 * WSIZE), PACK(DSIZE, 1));
    heap_listp += (2 * WSIZE);

    if(extend_heap(CHUNKSIZE/WSIZE) == NULL) 
        return -1;
    return 0;
}

// cleared
/* 
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size) {
    size_t asize; // Adjustec block size
    size_t extendsize; // Amount to extend heap if there is no fit
    char *bp;

    if (size == 0) return NULL;

    if (size <= DSIZE)
        asize = 2 * DSIZE;
    else
        asize = DSIZE * ((size + (DSIZE) + (DSIZE - 1)) / DSIZE);
    
    // Search the free list for a fit
    if ((bp = find_fit(asize)) != NULL) {
        place(bp, asize);
        return bp;
    }

    // No fit found. Get more memory and place block
    extendsize = MAX(asize, CHUNKSIZE);
    if ((bp = extend_heap(extendsize / WSIZE)) == NULL)
        return NULL;
    place(bp, asize);
    return bp;
    // int newsize = ALIGN(size + SIZE_T_SIZE);
    // void *p = mem_sbrk(newsize);
    // if (p == (void *)-1) {
    //     return NULL;
    // } else {
    //     *(size_t *)p = size;
    //     return (void *)((char *)p + SIZE_T_SIZE);
    // }
}

// cleared
/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *ptr) {
    if (!ptr) {
        return;
    } else {
        size_t size = GET_SIZE(HDRP(ptr));
        if (!heap_listp){
            if (mm_init() == -1) { // error in mm_init
                return;
            }
        }
        PUT(HDRP(ptr), PACK(size, 0));
        PUT(FTRP(ptr), PACK(size, 0));
        coalesce(ptr);
    }
}

/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
void *mm_realloc(void *ptr, size_t size) {
    return (void*) -1;
    // if (size == 0) {
    //     mm_free(ptr);
    //     return 0;
    // }
    // if (!ptr) {
    //     return mm_malloc(size);
    // }
    // void *oldptr = ptr;
    // void *newptr;
    // size_t copySize;

    // newptr = mm_malloc(size);
    // if (newptr == NULL)
    //   return NULL;
    // copySize = *(size_t *)((char *)oldptr - SIZE_T_SIZE);
    // if (size < copySize)
    //   copySize = size;
    // memcpy(newptr, oldptr, copySize);
    // mm_free(oldptr);
    // return newptr;
}

// cleared
// extend_heap function from textbook
static void *extend_heap(size_t words) {
    char * bp;
    size_t size;

    size = (words % 2) ? (words + 1) * WSIZE : words * WSIZE;
    if ((long) (bp = mem_sbrk(size)) == -1) return NULL;

    PUT(HDRP(bp), PACK(size, 0));
    PUT(FTRP(bp), PACK(size, 0));
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1));

    return coalesce(bp);
}

// cleared
static void place(void *bp, size_t asize) {
    // Size of the current size
    size_t currentSize = GET_SIZE(HDRP(bp));

    // If size >= minimum block size
    if ((currentSize - asize) >= (2 * DSIZE)) {
        PUT(HDRP(bp), PACK(asize, 1));
        PUT(FTRP(bp), PACK(asize, 0));
        bp = NEXT_BLKP(bp);
        PUT(HDRP(bp), PACK(currentSize - asize, 0));
        PUT(FTRP(bp), PACK(currentSize - asize, 0));
    } else {
        PUT(HDRP(bp), PACK(currentSize, 1));
        PUT(FTRP(bp), PACK(currentSize, 1));
    }
}

// cleared
static void *find_fit(size_t asize) {
    if(!heap_listp) return NULL;
    void* p;
    for(p = heap_listp; GET_SIZE(p) > 0; p = NEXT_BLKP(p)) {
        if(!GET_ALLOC(p) && GET_SIZE(p) > asize) return p;
    }
    return p;
}

// cleared
static void *coalesce(void* bp) {
    // TODO: implement coalesce function from textbook

    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
    size_t size = GET_SIZE(HDRP(bp));

    if (prev_alloc && next_alloc) {
        return bp;
    } else if (prev_alloc && !next_alloc) {
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));
    } else if (!prev_alloc && next_alloc) {
        size += GET_SIZE(FTRP(PREV_BLKP(bp)));
        PUT(FTRP(bp), PACK(size, 0));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
    } else {
        size += GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(FTRP(NEXT_BLKP(bp)));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
    }
    return bp;
}
