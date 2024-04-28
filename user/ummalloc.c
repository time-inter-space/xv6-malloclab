#include "kernel/types.h"

//
#include "user/user.h"

//
#include "ummalloc.h"

/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT - 1)) & ~0x7)

#define uint_SIZE (ALIGN(sizeof(uint)))

char *heap_listp;

static void *coalesce(void *bp) {
  uint size = *(unsigned int *)((char *)bp - 4) & -8;
  uint prev_alloc = *(unsigned int *)((char *)bp - 8) & 1;
  uint next_alloc = *(unsigned int *)((char *)bp + size - 4) & 1;
  if (prev_alloc && next_alloc) return bp;
  if (!next_alloc) {
    size += *(unsigned int *)((char *)bp + size - 4) & -8;
  }
  if (!prev_alloc) {
    uint prev_size = *(unsigned int *)((char *)bp - 8) & -8;
    size += prev_size;
    bp -= prev_size;
  }
  *(unsigned int *)((char *)bp - 4) = size;
  *(unsigned int *)((char *)bp + size - 8) = size;
  return bp;
}

static void *extend_heap(uint words) {
  uint size = (words + (words & 1)) * 4;
  char *bp = sbrk(size);
  if ((long)bp == -1) return 0;
  *(unsigned int *)(bp - 4) = size;
  *(unsigned int *)(bp + size - 8) = size;
  *(unsigned int *)(bp + size - 4) = 1;
  return coalesce(bp);
}

static void *find_fit(uint size) {
  char *bp = heap_listp;
  uint c = *(unsigned int *)(bp - 4);
  char *res = 0;
  uint mn = -1;
  while ((c & -8) != 0) {
    if ((c & 7) == 0 && (c & -8) >= size && (c & -8) < mn) {
      res = bp;
      mn = c & -8;
    }
    bp += (c & -8);
    c = *(unsigned int *)(bp - 4);
  }
  return res;
}

static void place(void *bp, uint size) {
  uint s = *(unsigned int *)((char *)bp - 4) & -8;
  if (size + 8 < s) {
    *(unsigned int *)((char *)bp - 4) = size | 1;
    *(unsigned int *)((char *)bp + size - 8) = size | 1;
    *(unsigned int *)((char *)bp + size - 4) = s - size;
    *(unsigned int *)((char *)bp + s - 8) = s - size;
  }
  else {
    *(unsigned int *)((char *)bp - 4) = s | 1;
    *(unsigned int *)((char *)bp + s - 8) = s | 1;
  }
}

/*
 * mm_init - initialize the malloc package.
 */
int mm_init(void) {
  heap_listp = sbrk(16);
  if (heap_listp == (char *)-1) return -1;
  *(unsigned int *)heap_listp = 0;
  *(unsigned int *)(heap_listp + 4) = 9;
  *(unsigned int *)(heap_listp + 8) = 9;
  *(unsigned int *)(heap_listp + 12) = 1;
  heap_listp += 8;
  if (extend_heap(4096 / 4) == 0) return -1;
  return 0;
}

/*
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(uint size) {
  if (size == 0) return 0;
  uint asize = (size + 15) / 8 * 8;
  char *bp = find_fit(asize);
  if (bp != 0) {
    place(bp, asize);
    return bp;
  }
  uint extendsize = asize > 4096 ? asize : 4096;
  bp = extend_heap(extendsize / 4);
  if (bp == 0) return 0;
  place(bp, asize);
  return bp;
}

/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *bp) {
  uint size = *(unsigned int *)((char *)bp - 4) & -8;
  *(unsigned int *)((char *)(bp - 4)) = size;
  *(unsigned int *)((char *)(bp + size - 8)) = size;
  coalesce(bp);
}

/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
void *mm_realloc(void *ptr, uint size) {
  void *oldptr = ptr;
  void *newptr;
  uint copySize;

  if (size == 0) {
    mm_free(ptr);
    return 0;
  }
  
  newptr = mm_malloc(size);
  if (newptr == 0) return 0;
  copySize = *(uint *)((char *)oldptr - 4) - 8;
  if (size < copySize) copySize = size;
  memcpy(newptr, oldptr, copySize);
  mm_free(oldptr);
  return newptr;
}
