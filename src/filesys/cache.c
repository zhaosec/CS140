#include "filesys/cache.h"

#define BUFFER_CACHE_SIZE 64

/* struct for cache entry */
struct cache_entry
{
  block_sector_t sector_id;                     /* sector id */
  bool accessed;                                        /* whether the entry is recently accessed */
  bool dirty;                                           /* whether this cache is dirty */
  /* TODO: need to check whether loading is necessary */
  bool loading;                                         /* whether this cache is being loaded */
  /* TODO: need to check whether flushing is necessary */
  bool flushing;                                        /* whether this cache is being flushed */
  uint32_t AW;                                          /* # of processes actively writing */
  uint32_t AR;                                          /* # of processes actively reading */
  uint32_t WW;                                          /* # of processes waiting to write */
  uint32_t WR;                                          /* # of processes waiting to read */
  struct condition can_read;            /* whether this cache can be read now */
  struct condition can_write;           /* whether this cache can be written now */
  struct lock lock;             /* fine grained lock for a single cache */
  uint8_t data[BLOCK_SECTOR_SIZE];      /* data for this sector */
};

typedef struct cache_entry cache_entry_t;

/* global buffer cache */
static cache_entry_t buffer_cache[BUFFER_CACHE_SIZE];

static uint32_t hand;
//static struct cache_entry * cache_eviction ();
//static struct cache_entry * cache_get_block (block_sector_t sector_id);
//static int is_in_cache (block_sector_t sector_id);

/* Initialize cache */
void
cache_init (void)
{
  hand = 0;
  int i = 0;
  for (i = 0; i < BUFFER_CACHE_SIZE; i++)
  {
        buffer_cache[i].sector_id = UINT32_MAX;
        buffer_cache[i].accessed = false;
        buffer_cache[i].dirty = false;
        buffer_cache[i].loading = false;
        buffer_cache[i].flushing = false;
        buffer_cache[i].AW = 0;
        buffer_cache[i].AR = 0;
        buffer_cache[i].WW = 0;
        buffer_cache[i].WR = 0;
        cond_init(&buffer_cache[i].can_read);
        cond_init(&buffer_cache[i].can_write);
        lock_init(&buffer_cache[i].lock);
        memset(buffer_cache[i].data, 0, BLOCK_SECTOR_SIZE*sizeof(uint8_t));
  }
}

/* See whether there is a hit for sector. If yes, return cache id.
 * Else, return -1 */
static int
is_in_cache (block_sector_t sector)
{
  uint32_t i = 0;
  for (i = 0; i < BUFFER_CACHE_SIZE; i++)
  {
        /* TODO: do I need to acquire lock here?  */
        lock_acquire(&buffer_cache[i].lock);
        if(buffer_cache[i].sector_id == sector)
        {
          lock_release(&buffer_cache[i].lock);
          return i;
        }
        lock_release(&buffer_cache[i].lock);
  }
  return -1;
}

/* If the cache is full, find one cache to be evicted using clock algorithm
 * return the pointer of the cache to be evicted */
/* When the cache isn't full, get the very first unused cache entry */
static uint32_t
cache_evict_id (void)
{
  while (1)
  {
        if (buffer_cache[hand].accessed)
        {
          buffer_cache[hand].accessed = false;
          hand = (hand + 1) % BUFFER_CACHE_SIZE;
        }
        else
        {
          uint32_t result = hand;
          hand = (hand + 1) % BUFFER_CACHE_SIZE;
          return result;
        }
  }
  /* will never reach here */
  ASSERT(1==0);
  return 0;
}

static cache_entry_t *
cache_get_entry (block_sector_t sector_id)
{
  int cache_hit_id = is_in_cache(sector_id);
  if (cache_hit_id == -1)
  {
    uint32_t evict_id = cache_evict_id();
    if (buffer_cache[evict_id].dirty)
    {
          block_write(fs_device, buffer_cache[evict_id].sector_id,
                                             buffer_cache[evict_id].data);
    }
    buffer_cache[evict_id].dirty = false;
    buffer_cache[evict_id].accessed = false;
    buffer_cache[evict_id].sector_id = sector_id;
    return &buffer_cache[evict_id];
  }
  else
  {
        return &buffer_cache[cache_hit_id];
  }
}

/* Reads sector SECTOR from cache into BUFFER. */
void
cache_read ( block_sector_t sector, void * buffer)
{
  cache_read_partial(sector, buffer, 0, BLOCK_SECTOR_SIZE);
}

/* If there is a hit, copy from cache to buffer */
static void
cache_read_hit (void *buffer, off_t start, off_t length, uint32_t cache_id)
{
  struct cache_entry *cur_c;
  cur_c = &buffer_cache[cache_id];
  lock_acquire(&cur_c->lock);
  memcpy(buffer, cur_c->data + start, length);
  cur_c->accessed = true;
  lock_release(&cur_c->lock);
}

/* If it is a miss, load this sector from disk to cache, then copy to buffer */
static void
cache_read_miss (block_sector_t sector, void *buffer, off_t start, off_t length)
{
  struct cache_entry *cur_c;
  cur_c = cache_get_entry(sector);
  lock_acquire(&cur_c->lock);
  block_read (fs_device, sector, cur_c->data);
  memcpy(buffer, cur_c->data + start, length);
  cur_c->accessed = true;
  lock_release(&cur_c->lock);
}

/* Reads bytes [start, start + length) in sector SECTOR from cache into
 * BUFFER. */
void
cache_read_partial (block_sector_t sector, void *buffer,
                                      off_t start, off_t length)
{
  int cache_id_hit = is_in_cache(sector);
  if(cache_id_hit != -1)
  {
        cache_read_hit(buffer, start, length, cache_id_hit);
  }
  else
  {
        cache_read_miss(sector, buffer, start, length);
  }
}

/* If there is a hit, copy from buffer to cache */
static void
cache_write_hit (const void *buffer, off_t start,
                         off_t length, uint32_t cache_id)
{
  struct cache_entry *cur_c;
  cur_c = &buffer_cache[cache_id];
  lock_acquire(&cur_c->lock);
  memcpy(cur_c->data + start, buffer, length);
  cur_c->accessed = true;
  cur_c->dirty = true;
  lock_release(&cur_c->lock);
}

/* If it is a miss, load this sector from disk to cache, then copy buffer
 * to cache */
static void
cache_write_miss (block_sector_t sector, const void *buffer,
                                  off_t start, off_t length)
{
  struct cache_entry *cur_c;
  cur_c = cache_get_entry(sector);
  lock_acquire(&cur_c->lock);
  memcpy(cur_c->data + start, buffer, length);
  cur_c->accessed = true;
  cur_c->dirty = true;
  lock_release(&cur_c->lock);
}

/* Writes BUFFER to the cache entry corresponding to sector. */
void
cache_write ( block_sector_t sector, const void *buffer)
{
  cache_write_partial(sector, buffer, 0, BLOCK_SECTOR_SIZE);
}

/* Writes BUFFER to bytes [start, start + length) in the cache entry
 * corresponding to sector */
void
cache_write_partial (block_sector_t sector, const void *buffer,
                                             off_t start, off_t length)
{
  int cache_id_hit = is_in_cache (sector);
  if(cache_id_hit != -1)
  {
    cache_write_hit (buffer, start, length, cache_id_hit);
  }
  else
  {
    cache_write_miss (sector, buffer, start, length);
  }
}