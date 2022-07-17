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
void kfreehelper(void *pa, int icpu);

extern char end[]; // first address after kernel.
                   // defined by kernel.ld.

struct run {
  struct run *next;
};

struct {
  struct spinlock lock;
  struct run *freelist;
} kmem[NCPU];

int lk_cpuid() {
  int id;
  push_off();
  id = cpuid();
  pop_off();
  return id;
}

void
kinit()
{
  
  char v[] = "kem0";
  for(char i = 0; i < NCPU; ++i) {
    v[3] = i;
    initlock(&kmem[(int)i].lock, v);
  }
  freerange(end, (void*)PHYSTOP);
}

void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  uint64 cpuid = 0;
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE)
    kfreehelper(p, (cpuid++) % NCPU);
}

void
kfree(void *pa) {
  kfreehelper(pa, lk_cpuid());
}

// Free the page of physical memory pointed at by v,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfreehelper(void *pa, int icpu)
{
  struct run *r;

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  r = (struct run*)pa;

  acquire(&kmem[icpu].lock);
  r->next = kmem[icpu].freelist;
  kmem[icpu].freelist = r;
  release(&kmem[icpu].lock);
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  struct run *r;

  int id = lk_cpuid();
  acquire(&kmem[id].lock);
  r = kmem[id].freelist;
  if(r)
    kmem[id].freelist = r->next;
  else {
    // steal
  }
  release(&kmem[id].lock);
  if(!r) {
    for(int i = (id + 1) % NCPU; i != id; i = (i + 1) % NCPU) {
      char c = 0;
      acquire(&kmem[i].lock);
      r = kmem[i].freelist;
      if (r) {
        kmem[i].freelist = r->next;
        c = 1;
      }
      release(&kmem[i].lock);
      if (c) break;
    }
  }

  if(r)
    memset((char*)r, 5, PGSIZE); // fill with junk 
  return (void*)r;
}
