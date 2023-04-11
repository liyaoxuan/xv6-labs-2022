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

#define NBUCKET 13
#define hash(x) (x % NBUCKET)
struct bucket{
	struct spinlock lock;
	struct buf head;
	int hasEmpty;
};

struct {
	struct spinlock lock;
  struct buf buf[NBUF];
	struct bucket bucket[NBUCKET];
} bcache;

void
binit(void)
{
  struct buf *b;
	struct bucket *bk;

	initlock(&bcache.lock, "bcache_bucket");
  // Create linked list of buffers
	for (bk = bcache.bucket; bk < bcache.bucket+NBUCKET; bk++) {
		initlock(&bk->lock, "bcache_bucket");
		bk->head.prev = &bk->head;
		bk->head.next = &bk->head;
		bk->hasEmpty = 0;
	}
  for(b = bcache.buf; b < bcache.buf+NBUF; b++){
    b->next = bcache.bucket[0].head.next;
    b->prev = &bcache.bucket[0].head;
    initsleeplock(&b->lock, "bcache_buffer");
    bcache.bucket[0].head.next->prev = b;
    bcache.bucket[0].head.next = b;
  }
	bcache.bucket[0].hasEmpty = NBUF;
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf*
bget(uint dev, uint blockno)
{
  struct buf *b;
	int bid;
	int victim;

	bid = hash(blockno);
  acquire(&bcache.bucket[bid].lock);
  // Is the block already cached?
  for(b = bcache.bucket[bid].head.next; b != &bcache.bucket[bid].head; b = b->next){
    if(b->dev == dev && b->blockno == blockno){
      b->refcnt++;
      release(&bcache.bucket[bid].lock);
      acquiresleep(&b->lock);
      return b;
    }
  }

  // Not cached.
	// try to find in current bucket
  // Recycle a bucket with refcount == 0
	if (bcache.bucket[bid].hasEmpty > 0) {
		for(b = bcache.bucket[bid].head.next; b != &bcache.bucket[bid].head; b = b->next){
			if(b->refcnt == 0){
				b->dev = dev;
				b->blockno = blockno;
				b->valid = 0;
				b->refcnt = 1;
				release(&bcache.bucket[bid].lock);
				acquiresleep(&b->lock);
				return b;
			}
		}
	}

	// try other bucket
  // Recycle a bucket with refcount == 0
	for (victim = (bid+1)%NBUCKET; victim != bid; victim = (victim+1)%NBUCKET) {
		acquire(&bcache.bucket[victim].lock);
		if (bcache.bucket[victim].hasEmpty == 0) {
			release(&bcache.bucket[victim].lock);
			continue;
		}
		for (b = bcache.bucket[victim].head.next; b != &bcache.bucket[victim].head; b = b->next) {
			if (b->refcnt == 0) {
				b->dev = dev;
				b->blockno = blockno;
				b->valid = 0;
				b->refcnt = 1;
				// move from victim to bid
				b->prev->next = b->next;
				b->next->prev = b->prev;
				b->next = bcache.bucket[bid].head.next;
				b->prev = &bcache.bucket[bid].head;
				b->prev->next = b;
				b->next->prev = b;
				// update hasEmpty
				bcache.bucket[victim].hasEmpty -= 1;
				release(&bcache.bucket[victim].lock);
				release(&bcache.bucket[bid].lock);
				acquiresleep(&b->lock);
				return b;
			}
		}
		release(&bcache.bucket[victim].lock);
	}
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
	int bid;
  if(!holdingsleep(&b->lock))
    panic("brelse");
	bid = hash(b->blockno);
  releasesleep(&b->lock);

  acquire(&bcache.bucket[bid].lock);
  b->refcnt--;
  if (b->refcnt == 0) {
    // no one is waiting for it.
		bcache.bucket[bid].hasEmpty += 1;
  }
  
  release(&bcache.bucket[bid].lock);
}

void
bpin(struct buf *b) {
	int bid;
  bid = hash(b->blockno);
  acquire(&bcache.bucket[bid].lock);
  b->refcnt++;
  release(&bcache.bucket[bid].lock);
}

void
bunpin(struct buf *b) {
	int bid;
	bid = hash(b->blockno);
  acquire(&bcache.bucket[bid].lock);
  b->refcnt--;
	release(&bcache.bucket[bid].lock);
}


