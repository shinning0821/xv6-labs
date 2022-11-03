// Buffer cache.
//
// The buffer cache is a linked list of buf structures holding
// cached copies of disk block contents.  Caching disk blocks
// in memory reduces the number of disk reads and also provides
// a synchronization point for disk blocks used by multiple processes.
//
// Interface:
// * To get a buffer for a particular disk block, call bread.
// * After changing buffer data, call bwrite to write it to disk.
// * When done with the buffer, call brelse.
// * Do not use the buffer after calling brelse.
// * Only one process at a time can use a buffer,
//     so do not keep them longer than necessary.


#include "types.h"
#include "param.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "riscv.h"
#include "defs.h"
#include "fs.h"
#include "buf.h"

#define NBUCKET 13  // 哈希桶的数目
extern uint ticks; 


struct {
  struct spinlock lock;
  struct buf buf[NBUF];

  // Linked list of all buffers, through prev/next.
  // Sorted by how recently the buffer was used.
  // head.next is most recent, head.prev is least.
  struct buf head;

  // 添加对哈希桶的支持
  struct buf buckets[NBUCKET];
  struct spinlock bucketslock[NBUCKET];
  struct spinlock hashlock;
  int size;
} bcache;

void
binit(void)
{
  struct buf *b;
  bcache.size = 0;
  initlock(&bcache.lock, "bcache");
  initlock(&bcache.hashlock, "bcache_hash");
  // Create linked list of buffers
  bcache.head.prev = &bcache.head;
  bcache.head.next = &bcache.head;
  // 初始化哈希桶相关
  for(int i = 0; i < NBUCKET; i++) {
    initlock(&bcache.bucketslock[i], "bcache_bucket");
  }
  
  for(b = bcache.buf; b < bcache.buf+NBUF; b++){
    // b->next = bcache.head.next;
    // b->prev = &bcache.head;
    initsleeplock(&b->lock, "buffer");
    // bcache.head.next->prev = b;
    // bcache.head.next = b;
  }
}

uint 
hash(uint blockno)
{
  return blockno % NBUCKET;
}
// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf*
bget(uint dev, uint blockno)
{
  struct buf *b;
  // lab8-2
  int id = hash(blockno);
  struct buf *pre, *minb, *minpre;
  uint mintimestamp;
  
  // loop up the buf in the buckets[id]
  acquire(&bcache.bucketslock[id]);  // lab8-2
  for(b = bcache.buckets[id].next; b; b = b->next){
    if(b->dev == dev && b->blockno == blockno){
      b->refcnt++;
      release(&bcache.bucketslock[id]);  // lab8-2
      acquiresleep(&b->lock);
      return b;
    }
  }

  // Not cached.
  // check if there is a buf not used -lab8-2
  acquire(&bcache.lock);
  if(bcache.size < NBUF) {
    b = &bcache.buf[bcache.size++];
    release(&bcache.lock);
    b->dev = dev;
    b->blockno = blockno;
    b->valid = 0;
    b->refcnt = 1;
    b->next = bcache.buckets[id].next;
    bcache.buckets[id].next = b;
    release(&bcache.bucketslock[id]);
    acquiresleep(&b->lock);
    return b;
  }
  release(&bcache.lock);
  // release(&bcache.bucketslock[id]);

  // select the last-recently used block int the bucket
  //based on the timestamp - lab8-2
  acquire(&bcache.hashlock);
  for(int current_id = id; current_id < NBUCKET; current_id++) {
      mintimestamp = 99999;
      acquire(&bcache.bucketslock[current_id]);
      for(pre = &bcache.buckets[current_id], b = pre->next; b; pre = b, b = b->next) {
          // search the block again
          if(current_id == id && b->dev == dev && b->blockno == blockno){
              b->refcnt++;
              release(&bcache.bucketslock[current_id]);
              release(&bcache.hashlock);
              acquiresleep(&b->lock);
              return b;
          }
          if(!b->refcnt && b->timestamp < mintimestamp) {
              minb = b;
              minpre = pre;
              mintimestamp = b->timestamp;
          }
      }
      // find an unused block
      if(minb) {
          minb->dev = dev;
          minb->blockno = blockno;
          minb->valid = 0;
          minb->refcnt = 1;
          // if block in another bucket, we should move it to correct bucket
          if(current_id != id) {
              minpre->next = minb->next;    // remove block
              release(&bcache.bucketslock[current_id]);
              acquire(&bcache.bucketslock[id]);
              minb->next = bcache.buckets[id].next;    // move block to correct bucket
              bcache.buckets[id].next = minb;
          }
          release(&bcache.bucketslock[id]);
          release(&bcache.hashlock);
          acquiresleep(&minb->lock);
          return minb;
      }
      release(&bcache.bucketslock[current_id]);
      if(current_id == NBUCKET-1) {
          current_id = 0;
      }
  }
// lab8-2
//  // Recycle the least recently used (LRU) unused buffer.
//  for(b = bcache.head.prev; b != &bcache.head; b = b->prev){
//    if(b->refcnt == 0) {
//      b->dev = dev;
//      b->blockno = blockno;
//      b->valid = 0;
//      b->refcnt = 1;
//      release(&bcache.lock);
//      acquiresleep(&b->lock);
//      return b;
//    }
//  }
  panic("bget: no buffers");
}


// Return a locked buf with the contents of the indicated block.
struct buf*
bread(uint dev, uint blockno)
{
  struct buf *b;

  b = bget(dev, blockno);
  if(!b->valid) {
    virtio_disk_rw(b, 0);
    b->valid = 1;
  }
  return b;
}

// Write b's contents to disk.  Must be locked.
void
bwrite(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("bwrite");
  virtio_disk_rw(b, 1);
}

// Release a locked buffer.
// Move to the head of the most-recently-used list.
void
brelse(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("brelse");

  releasesleep(&b->lock);
  
  int id = hash(b->blockno);
  acquire(&bcache.bucketslock[id]);
  b->refcnt--;

  if (b->refcnt == 0) {
    // no one is waiting for it.
    // b->next->prev = b->prev;
    // b->prev->next = b->next;
    // b->next = bcache.head.next;
    // b->prev = &bcache.head;
    // bcache.head.next->prev = b;
    // bcache.head.next = b;
    b->timestamp = ticks;
  }
  
  release(&bcache.bucketslock[id]);
}

// 改为获取对应的哈希桶的锁
void
bpin(struct buf *b) {
  int id = hash(b->blockno);

  acquire(&bcache.bucketslock[id]);
  b->refcnt++;
  release(&bcache.bucketslock[id]);
}

void
bunpin(struct buf *b) {
  int id = hash(b->blockno);

  acquire(&bcache.bucketslock[id]);
  b->refcnt--;
  release(&bcache.bucketslock[id]);
}


