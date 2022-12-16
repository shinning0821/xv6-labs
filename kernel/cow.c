// COW reference count struct and functions - lab6

#include "types.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"

// COW reference count
struct {
  uint8 ref_cnt;
  struct spinlock lock;
} cows[PHYSTOP/PGSIZE];

// increase the reference count
void increfcnt(uint64 pa) {
  pa = pa / PGSIZE;
  acquire(&cows[pa].lock);
  cows[pa].ref_cnt += 1;
  release(&cows[pa].lock);
}

// decrease the reference count
uint8 decrefcnt(uint64 pa) {
  uint8 ret;
  pa = pa / PGSIZE;
  acquire(&cows[pa].lock);
  ret = --cows[pa].ref_cnt;
  release(&cows[pa].lock);
  return ret;
}
