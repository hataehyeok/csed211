//20210273
//hth021002
//TaeHyeok Ha

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

/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT - 1)) & ~0x7)

#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

// Setting Mecro
#define WSIZE 4             // word and header footer
#define DSIZE 8             // double word size
#define CHUNKSIZE (1 << 12) // Extend heap 4KB

#define MAX(x, y) ((x) > (y) ? (x) : (y))

#define PACK(size, alloc) ((size) | (alloc))

#define GET(p) (*(unsigned int *)(p))
#define PUT(p, val) (*(unsigned int *)(p) = (int)(val))

#define GET_SIZE(p) (GET(p) & ~0x7)
#define GET_ALLOC(p) (GET(p) & 0x1)

#define HDRP(bp) ((char *)(bp)-WSIZE)
#define FTRP(bp) ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)

#define NEXT_BLKP(bp) ((char *)(bp) + GET_SIZE(((char *)(bp)-WSIZE)))
#define PREV_BLKP(bp) ((char *)(bp)-GET_SIZE(((char *)(bp)-DSIZE)))

// Setting Mecro for segregation free list
#define PUT_ADD(p, ptr) (*(unsigned int *)(p) = (unsigned int)(ptr))

#define NEXT_PTR(ptr) ((char *)(ptr))
#define PREV_PTR(ptr) ((char *)(ptr) + WSIZE)

#define NEXT_SEG(ptr) (*(char **)(ptr))
#define PREV_SEG(ptr) (*(char **)(PREV_PTR(ptr)))

static char *heap_listp;
static void *seg_list[20];//seg size 20 get the best score

// Function Prototype
static void *extend_heap(unsigned words);
static void *coalesce(void *bp);
static void *find_fit(unsigned asize);
static void place(void *bp, unsigned asize);
static void linkToSeg(void *ptr, unsigned size_class);
static void deleteInSeg(void *ptr);

/*
 * mm_init - initialize the malloc package.
 */
int mm_init(void)
{
    int i;
    for (i = 0; i < 20; i++)//Initialize
        seg_list[i] = NULL;

    if ((heap_listp = mem_sbrk(4 * WSIZE)) == (void *)-1)//increase size of mrm brk
        return -1;

    PUT(heap_listp, 0);
    PUT(heap_listp + (1 * WSIZE), PACK(DSIZE, 1));//prologue header
    PUT(heap_listp + (2 * WSIZE), PACK(DSIZE, 1));//prologue footer
    PUT(heap_listp + (3 * WSIZE), PACK(0, 1));//epilogue block header
    heap_listp = heap_listp +(2 * WSIZE);//moving pointer location into middle of header and footer

    if (extend_heap(CHUNKSIZE / WSIZE) == NULL)
        return -1;

    return 0;
}

/*
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(unsigned size)
{
    int newsize = ALIGN(size + SIZE_T_SIZE);
    // void *p = mem_sbrk(newsize);
    // if (p == (void *)-1)
    //     return NULL;
    // else {
    //     *(size_t *)p = size;
    //     return (void *)((char *)p + SIZE_T_SIZE);
    // }

    unsigned extend;//size when heap is extended
    char *bp;

    if (size == 0)
        return NULL;

    if ((bp = find_fit(newsize)) != NULL)
    {
        place(bp, newsize);
        return bp;
    }

    extend = MAX(newsize, CHUNKSIZE);//need to memory -> get more memory space
    if ((bp = extend_heap(extend / WSIZE)) == NULL)
        return NULL;

    place(bp, newsize);
    return bp;
}

/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *ptr)
{
    unsigned s = GET_SIZE(HDRP(ptr));
    PUT(HDRP(ptr), PACK(s, 0));
    PUT(FTRP(ptr), PACK(s, 0));

    linkToSeg(ptr, s);//block goto segregation table
    coalesce(ptr);
}

/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
void *mm_realloc(void *ptr, unsigned size)
{
    // void *oldptr = ptr;
    // void *newptr;
    // size_t copySize;

    // newptr = mm_malloc(size);
    // if (newptr == NULL)
    //     return NULL;
    // copySize = *(size_t *)((char *)oldptr - SIZE_T_SIZE);
    // if (size < copySize)
    //     copySize = size;
    // memcpy(newptr, oldptr, copySize);
    // mm_free(oldptr);
    // return newptr;

    if (size <= 0)//exceptional sieze
    {
        mm_free(ptr);
        return NULL;
    }
    //set new point and setting oid size one and new size
    void *new_ptr;
    unsigned s1 = ALIGN(size);//new size
    unsigned s2 = GET_SIZE(HDRP(ptr));//old size
    unsigned s3 = s1 + DSIZE;
    int bs;
    int extend;

    if (s2 == s3)//not change block size
        return ptr;
    else if (s2 > s3) // old size is bigger than new size
    {
        PUT(HDRP(ptr), PACK(s3, 1));
        PUT(FTRP(ptr), PACK(s3, 1));

        new_ptr = ptr;
        ptr = NEXT_BLKP(ptr);
        
        PUT((HDRP(ptr)), PACK(s2 - s3, 0));
        PUT((FTRP(ptr)), PACK(s2 - s3, 0));
        linkToSeg(ptr, GET_SIZE(HDRP(ptr)));
        coalesce(ptr);
        return new_ptr;
    }
    else //new size is bigger than old size -> need extend
    {
        if ((NEXT_BLKP(ptr) != NULL) && (GET_ALLOC(HDRP(NEXT_BLKP(ptr))) == 0))
        {
            bs = s2 + GET_SIZE(HDRP(NEXT_BLKP(ptr))) - s1;  //need block size
            if (bs < DSIZE)//needs extend size
            {
                extend = MAX(-bs, CHUNKSIZE + DSIZE);
                if (extend_heap(extend) == NULL)
                    return NULL;
                bs = bs + extend;
            }

            deleteInSeg(NEXT_BLKP(ptr));

            if (bs <= 2 * DSIZE)
            {
                PUT(HDRP(ptr), PACK(s1 + bs, 1));
                PUT(FTRP(ptr), PACK(s1 + bs, 1));
                return ptr;
            }
            else
            {
                PUT(HDRP(ptr), PACK(s1 + DSIZE, 1));
                PUT(FTRP(ptr), PACK(s1 + DSIZE, 1));
                
                new_ptr = ptr;
                ptr = NEXT_BLKP(new_ptr);

                PUT(HDRP(ptr), PACK(bs - DSIZE, 0));
                PUT(FTRP(ptr), PACK(bs - DSIZE, 0));
                linkToSeg(ptr, GET_SIZE(HDRP(ptr)));
                coalesce(ptr);
            }
        }
        else
        {
            new_ptr = mm_malloc(size);
            memcpy(new_ptr, ptr, s2);
            mm_free(ptr);
        }
    }
    return new_ptr;
}



//========================================================================


// Other function
static void *extend_heap(size_t words)
{
    char *bp;
    unsigned size = (words % 2) ? (words + 1) * WSIZE : words * WSIZE;
    //Override the size to increase on the heap.
    if ((long)(bp = mem_sbrk(size)) == -1)
        return NULL;

    PUT(HDRP(bp), PACK(size, 0));//free block header
    PUT(FTRP(bp), PACK(size, 0));//free block footer
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1));//modify epilogue header location
    linkToSeg(bp, size);

    return coalesce(bp);
}

static void *coalesce(void *bp)
{
    // check prev, next block free state
    unsigned prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));
    unsigned next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
    unsigned size = GET_SIZE(HDRP(bp));

    if (prev_alloc && next_alloc)
        return bp;
    else if (prev_alloc && !next_alloc)
    {
        //delete block in segregation table
        deleteInSeg(bp);
        deleteInSeg(NEXT_BLKP(bp));

        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));
    }
    else if (!prev_alloc && next_alloc)
    {
        //delete block in segregation table
        deleteInSeg(bp);
        deleteInSeg(PREV_BLKP(bp));

        size += GET_SIZE(HDRP(PREV_BLKP(bp)));
        PUT(FTRP(bp), PACK(size, 0));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
    }
    else
    {
        //delete block in segregation table
        deleteInSeg(bp);
        deleteInSeg(PREV_BLKP(bp));
        deleteInSeg(NEXT_BLKP(bp));

        size += GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(FTRP(NEXT_BLKP(bp)));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
    }

    linkToSeg(bp, size);//insert block in segregation table

    return bp;
}

static void *find_fit(unsigned asize)
{
    // use the segregation and First fit search algorithm.
    // This algorithm show the best score in this case
    void *bp;
    unsigned seg_idx = asize;
    int i;

    for (i = 0; i < 20; i++)
    {
        if ((i == 19) || ((seg_idx <= 1) && (seg_list[i] != NULL)))
        {
            bp = seg_list[i];

            while ((bp != NULL) && ((GET_SIZE(HDRP(bp))) < asize))//use first fit
                bp = NEXT_SEG(bp);

            if (bp != NULL)
                return bp;
        }
        seg_idx >>= 1;
    }

    // for (bp= heap_listp; GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp))
    // {
    //     if (!GET_ALLOC(HDRP(bp)) && (asize<=GET_SIZE(HDRP(bp))))
    //     {
    //         return bp;
    //     }
    // }
    return NULL;
}

static void place(void *bp, unsigned asize)
{
    unsigned csize = GET_SIZE(HDRP(bp));

    deleteInSeg(bp);//delete block

    if ((csize - asize) >= (2 * DSIZE))//if left over, can put data
    {
        PUT(HDRP(bp), PACK(asize, 1));
        PUT(FTRP(bp), PACK(asize, 1));
        bp = NEXT_BLKP(bp);
        
        PUT(HDRP(bp), PACK(csize - asize, 0));
        PUT(FTRP(bp), PACK(csize - asize, 0));
        linkToSeg(bp, (csize - asize));
    }
    else
    {
        PUT(HDRP(bp), PACK(csize, 1));
        PUT(FTRP(bp), PACK(csize, 1));
    }
}

static void linkToSeg(void *ptr, unsigned size_class)
{
    void *next_ptr = NULL;
    void *idxptr = NULL;

    //calculate index of segregation list
    int i;
    for(i = 0; i < 19; i++)
    {
        if(size_class <= 1)
            break;
        size_class >>= 1;
    }

    next_ptr = seg_list[i];

    while ((next_ptr != NULL) && (size_class > GET_SIZE(HDRP(next_ptr))))
    {
        idxptr = next_ptr;
        next_ptr = NEXT_SEG(ptr);
    }
    //get the ptr of index, ptr of next index
    if (next_ptr == NULL)
    {
        if (idxptr != NULL)
        {
            PUT_ADD(NEXT_PTR(ptr), NULL);
            PUT_ADD(PREV_PTR(ptr), idxptr);
            PUT_ADD(NEXT_PTR(idxptr), ptr);
        }
        else
        {
            PUT_ADD(NEXT_PTR(ptr), NULL);
            PUT_ADD(PREV_PTR(ptr), NULL);
            seg_list[i] = ptr;
        }
    }
    else
    {
        if (idxptr != NULL)
        {
            PUT_ADD(NEXT_PTR(ptr), next_ptr);
            PUT_ADD(PREV_PTR(next_ptr), ptr);
            PUT_ADD(PREV_PTR(ptr), idxptr);
            PUT_ADD(NEXT_PTR(idxptr), ptr);
        }
        else
        {
            PUT_ADD(NEXT_PTR(ptr), next_ptr);
            PUT_ADD(PREV_PTR(next_ptr), ptr);
            PUT_ADD(PREV_PTR(ptr), NULL);
            seg_list[i] = ptr;
        }
    }
}

static void deleteInSeg(void *ptr)
{
    unsigned size = GET_SIZE(HDRP(ptr));

    int i = 0;//find the index
    for(i = 0; i < 19; i++)
    {
        if(size <= 1)
            break;
        size >>= 1;
    }

    //seperate the each case, and put right address to memory
    if (NEXT_SEG(ptr) == NULL)
    {
        if (PREV_SEG(ptr) != NULL)
            PUT_ADD(NEXT_PTR(PREV_SEG(ptr)), NULL);
        else
            seg_list[i] = NULL;
    }
    else
    {
        if (PREV_SEG(ptr) != NULL)
        {
            PUT_ADD(PREV_PTR(NEXT_SEG(ptr)), PREV_SEG(ptr));
            PUT_ADD(NEXT_PTR(PREV_SEG(ptr)), NEXT_SEG(ptr));
        }
        else
        {
            PUT_ADD(PREV_PTR(NEXT_SEG(ptr)), NULL);
            seg_list[i] = NEXT_SEG(ptr);
        }
    }

}
