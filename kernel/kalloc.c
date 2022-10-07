// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"

void freerange(void *pa_start, void *pa_end);

extern char end[]; // first address after kernel.
                   // defined by kernel.ld.

struct run {
  struct run *next;
};

struct {
  struct spinlock lock;
  struct run *freelist;
} kmem;

struct {
  struct spinlock lock;
  int cnt[(PHYSTOP - KERNBASE) >> PGSHIFT];
} pgref;

void 
kinit()
{
  initlock(&kmem.lock, "kmem");
  freerange(end, (void*)PHYSTOP);
  initlock(&pgref.lock, "pgref");
  memset(pgref.cnt, 0, sizeof(int) * ((PHYSTOP - KERNBASE) >> PGSHIFT));
}

void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE)
    kfree(p);
}

// Free the page of physical memory pointed at by pa,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree(void *pa)
{
  struct run *r;
  uint64 idx;

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");
    
  idx = ((uint64)pa - KERNBASE) >> PGSHIFT;
  acquire(&pgref.lock);
  pgref.cnt[idx] -= 1;
  if(pgref.cnt[idx] <= 0){
    pgref.cnt[idx] = 0;
    release(&pgref.lock);
  // Fill with junk to catch dangling refs.
    memset(pa, 1, PGSIZE);
    
    r = (struct run*)pa;

    acquire(&kmem.lock);
    r->next = kmem.freelist;
    kmem.freelist = r;
    release(&kmem.lock);
  } else
    release(&pgref.lock);
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  struct run *r;

  acquire(&kmem.lock);
  r = kmem.freelist;
  if(r)
    kmem.freelist = r->next;
  release(&kmem.lock);

  if(r) {
    memset((char*)r, 5, PGSIZE); // fill with junk
    acquire(&pgref.lock);
    pgref.cnt[((uint64)r - KERNBASE) >> PGSHIFT] = 1;
    release(&pgref.lock);
  }
  return (void*)r;
}

inline void
kincref(void *pa)
{
  if ((uint64)pa % PGSIZE != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kincref");
  acquire(&pgref.lock);
  pgref.cnt[((uint64)pa - KERNBASE) >> PGSHIFT] += 1;
  release(&pgref.lock);
}

inline int
kgetref(void *pa)
{
  if ((uint64)pa % PGSIZE != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kgetref");
  int refcnt;
  acquire(&pgref.lock);
  refcnt = pgref.cnt[((uint64)pa - KERNBASE) >> PGSHIFT];
  release(&pgref.lock);
  return refcnt;
}