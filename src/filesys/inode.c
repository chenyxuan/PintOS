#include "filesys/inode.h"
#include <list.h>
#include <debug.h>
#include <round.h>
#include <string.h>
#include "filesys/filesys.h"
#include "filesys/free-map.h"
#include "filesys/cache.h"
#include "threads/malloc.h"
#include "threads/synch.h"

/* Identifies an inode. */
#define INODE_MAGIC 0x494e4f44
#define TABLE_SIZE 128

static char zeros[BLOCK_SECTOR_SIZE];
static char empty[BLOCK_SECTOR_SIZE];

/* On-disk inode.
   Must be exactly BLOCK_SECTOR_SIZE bytes long. */
struct inode_disk
  {
    block_sector_t table;               /* Table data sector. */
    off_t length;                       /* File size in bytes. */
    unsigned magic;                     /* Magic number. */
    bool is_dir;
    uint8_t unused[499];               /* Not used. */
  };

/* Returns the number of sectors to allocate for an inode SIZE
   bytes long. */
static inline size_t
bytes_to_sectors (off_t size)
{
  return DIV_ROUND_UP (size, BLOCK_SECTOR_SIZE);
}

/* In-memory inode. */
struct inode 
  {
    struct list_elem elem;              /* Element in inode list. */
    block_sector_t sector;              /* Sector number of disk location. */
    int open_cnt;                       /* Number of openers. */
    bool removed;                       /* True if deleted, false otherwise. */
    int deny_write_cnt;                 /* 0: writes ok, >0: deny writes. */
    struct inode_disk data;             /* Inode content. */
  };

/* Map the pos into tables
*/

static off_t
byte_to_t1(off_t pos)
{
  return (pos >> 16) & (TABLE_SIZE - 1);
}

static off_t
byte_to_t2(off_t pos)
{
  return (pos >> 9) & (TABLE_SIZE - 1);
}

/* Returns the block device sector that contains byte offset POS
   within INODE.
   Returns -1 if INODE does not contain data for a byte at offset
   POS. */

static block_sector_t
byte_to_sector (struct inode *inode, off_t pos, bool create) 
{
  ASSERT (inode != NULL);
  
  block_sector_t *t1 = calloc(TABLE_SIZE, sizeof *t1);
  block_sector_t *t2 = calloc(TABLE_SIZE, sizeof *t2);

  if (!(pos < inode->data.length))
    {
      if (!create)
        {
          free(t1);
          free(t2);
          return -1;
        }
      else
        {
          off_t i, j;
          off_t t1_s = byte_to_t1(inode->data.length);
          off_t t2_s = byte_to_t2(inode->data.length);
          off_t t1_t = byte_to_t1(pos);
          off_t t2_t = byte_to_t2(pos);
          
          cache_read (inode->data.table, t1);
          for (i = t1_s; i <= t1_t; i++)
            {
              off_t l = (i == t1_s ? t2_s : 0);
              off_t r = (i == t1_t ? t2_t : TABLE_SIZE - 1);
              
              if (t1[i] == -1)
                {
                  if (!free_map_allocate (1, &t1[i]))
                  {
                    free(t1);
                    free(t2);
                    return -1;
                  }
                  cache_write (t1[i], empty);
                }
                
              cache_read (t1[i], t2);
              for(j = l; j <= r; j++)
                {
                  if (t2[j] == -1)
                    {
                      if (!free_map_allocate (1, &t2[j])) {
                        free(t1);
                        free(t2);
                        return -1;
                      }
                      cache_write (t2[j], zeros);
                    }
                }
              cache_write (t1[i], t2);
            }
          cache_write (inode->data.table, t1);
          
          inode->data.length = pos + 1;
          cache_write (inode->sector, &inode->data);
        }
    }
  
  cache_read (inode->data.table, t1);
  cache_read (t1[byte_to_t1 (pos)], t2);
  block_sector_t result = t2[byte_to_t2 (pos)];
  
  free(t1);
  free(t2);
  return result;
}

/* List of open inodes, so that opening a single inode twice
   returns the same `struct inode'. */
static struct list open_inodes;

/* Initializes the inode module. */
void
inode_init (void) 
{
  list_init (&open_inodes);
  memset(empty, -1, sizeof empty);
}

/* Initializes an inode with LENGTH bytes of data and
   writes the new inode to sector SECTOR on the file system
   device.
   Returns true if successful.
   Returns false if memory or disk allocation fails. */
bool
inode_create (block_sector_t sector, off_t length)
{
  struct inode_disk *disk_inode = NULL;
  bool success = false;

  ASSERT (length >= 0);

  /* If this assertion fails, the inode structure is not exactly
     one sector in size, and you should fix that. */
  ASSERT (sizeof *disk_inode == BLOCK_SECTOR_SIZE);

  disk_inode = calloc (1, sizeof *disk_inode);
  if (disk_inode != NULL)
    {
      disk_inode->length = length;
      disk_inode->magic = INODE_MAGIC;
      disk_inode->is_dir = false;
      if (free_map_allocate (1, &disk_inode->table)) 
        {
          cache_write (sector, disk_inode);
          cache_write (disk_inode->table, empty);
          
          if(length > 0)
            {
              block_sector_t *t1 = calloc(TABLE_SIZE, sizeof *t1);
              block_sector_t *t2 = calloc(TABLE_SIZE, sizeof *t2);
              
              int i, j;
              off_t t1_t = byte_to_t1(length - 1);
              off_t t2_t = byte_to_t2(length - 1);
          
              cache_read (disk_inode->table, t1);
              for(i = 0; i <= t1_t; i++)
                {
                  off_t r = (i == t1_t ? t2_t : TABLE_SIZE - 1);
              
                  if (!free_map_allocate (1, &t1[i]))
                  {
                    free(t1);
                    free(t2);
                    free (disk_inode);
                    return false;
                  }
                  cache_write(t1[i], empty);
                 
                  cache_read (t1[i], t2);
                  for(j = 0; j <= r; j++)
                    {
                      if (!free_map_allocate (1, &t2[j]))
                      {
                        free(t1);
                        free(t2);
                        free (disk_inode);
                        return false;
                      }
                      cache_write(t2[j], zeros);
                    }
                  cache_write (t1[i], t2);
                }
              cache_write (disk_inode->table, t1);
             
              free(t1);
              free(t2);
            }      
          success = true; 
        } 
      free (disk_inode);
    }
  return success;
}

/* Reads an inode from SECTOR
   and returns a `struct inode' that contains it.
   Returns a null pointer if memory allocation fails. */
struct inode *
inode_open (block_sector_t sector)
{
  struct list_elem *e;
  struct inode *inode;

  /* Check whether this inode is already open. */
  for (e = list_begin (&open_inodes); e != list_end (&open_inodes);
       e = list_next (e)) 
    {
      inode = list_entry (e, struct inode, elem);
      if (inode->sector == sector) 
        {
          inode_reopen (inode);
          return inode; 
        }
    }

  /* Allocate memory. */
  inode = malloc (sizeof *inode);
  if (inode == NULL)
    return NULL;

  /* Initialize. */
  list_push_front (&open_inodes, &inode->elem);
  inode->sector = sector;
  inode->open_cnt = 1;
  inode->deny_write_cnt = 0;
  inode->removed = false;
  cache_read (inode->sector, &inode->data);
  return inode;
}

/* Reopens and returns INODE. */
struct inode *
inode_reopen (struct inode *inode)
{
  if (inode != NULL)
    inode->open_cnt++;
  return inode;
}

/* Returns INODE's inode number. */
block_sector_t
inode_get_inumber (const struct inode *inode)
{
  return inode->sector;
}

/* Closes INODE and writes it to disk.
   If this was the last reference to INODE, frees its memory.
   If INODE was also a removed inode, frees its blocks. */
void
inode_close (struct inode *inode) 
{
  /* Ignore null pointer. */
  if (inode == NULL)
    return;
  /* Release resources if this was the last opener. */
  if (--inode->open_cnt == 0)
    {
      /* Remove from inode list and release lock. */
      list_remove (&inode->elem);
 
      /* Deallocate blocks if removed. */
      if (inode->removed) 
        {
          off_t length = inode->data.length;

          if(length > 0)
            {
              block_sector_t *t1 = calloc(TABLE_SIZE, sizeof *t1);
              block_sector_t *t2 = calloc(TABLE_SIZE, sizeof *t2);
              
              int i, j;
              off_t t1_t = byte_to_t1(length - 1);
              off_t t2_t = byte_to_t2(length - 1);
          
              cache_read (inode->data.table, t1);
              for(i = 0; i <= t1_t; i++)
                {
                  off_t r = (i == t1_t ? t2_t : TABLE_SIZE - 1);
              
                  cache_read (t1[i], t2);
                  for(j = 0; j <= r; j++)
                    {
                      free_map_release(t2[j], 1);
                    }
                  free_map_release(t1[i], 1);
                }
             
              free(t1);
              free(t2);
            }

          free_map_release (inode->sector, 1);
          free_map_release (inode->data.table, 1);
        }

      free (inode); 
    }
}

/* Marks INODE to be deleted when it is closed by the last caller who
   has it open. */
void
inode_remove (struct inode *inode) 
{
  ASSERT (inode != NULL);
  inode->removed = true;
}

/* Reads SIZE bytes from INODE into BUFFER, starting at position OFFSET.
   Returns the number of bytes actually read, which may be less
   than SIZE if an error occurs or end of file is reached. */
off_t
inode_read_at (struct inode *inode, void *buffer_, off_t size, off_t offset) 
{
  uint8_t *buffer = buffer_;
  off_t bytes_read = 0;
  uint8_t *bounce = NULL;

  while (size > 0) 
    {
      /* Disk sector to read, starting byte offset within sector. */
      block_sector_t sector_idx = byte_to_sector (inode, offset, false);
      int sector_ofs = offset % BLOCK_SECTOR_SIZE;

      /* Bytes left in inode, bytes left in sector, lesser of the two. */
      off_t inode_left = inode_length (inode) - offset;
      int sector_left = BLOCK_SECTOR_SIZE - sector_ofs;
      int min_left = inode_left < sector_left ? inode_left : sector_left;

      /* Number of bytes to actually copy out of this sector. */
      int chunk_size = size < min_left ? size : min_left;
      if (chunk_size <= 0)
        break;

      if (sector_ofs == 0 && chunk_size == BLOCK_SECTOR_SIZE)
        {
          /* Read full sector directly into caller's buffer. */
          cache_read (sector_idx, buffer + bytes_read);
        }
      else 
        {
          /* Read sector into bounce buffer, then partially copy
             into caller's buffer. */
          if (bounce == NULL) 
            {
              bounce = malloc (BLOCK_SECTOR_SIZE);
              if (bounce == NULL)
                break;
            }
          cache_read (sector_idx, bounce);
          memcpy (buffer + bytes_read, bounce + sector_ofs, chunk_size);
        }
      
      /* Advance. */
      size -= chunk_size;
      offset += chunk_size;
      bytes_read += chunk_size;
    }
  free (bounce);

  return bytes_read;
}

/* Writes SIZE bytes from BUFFER into INODE, starting at OFFSET.
   Returns the number of bytes actually written, which may be
   less than SIZE if end of file is reached or an error occurs.
   (Normally a write at end of file would extend the inode, but
   growth is not yet implemented.) */
off_t
inode_write_at (struct inode *inode, const void *buffer_, off_t size,
                off_t offset) 
{
  const uint8_t *buffer = buffer_;
  off_t bytes_written = 0;
  uint8_t *bounce = NULL;

  if (inode->deny_write_cnt)
    return 0;

  while (size > 0) 
    {
      /* Sector to write, starting byte offset within sector. */
      block_sector_t sector_idx = byte_to_sector (inode, offset, true);
      int sector_ofs = offset % BLOCK_SECTOR_SIZE;

      /* Bytes left in inode, bytes left in sector, lesser of the two. */
      off_t inode_left = inode_length (inode) - offset;
      int sector_left = BLOCK_SECTOR_SIZE - sector_ofs;
      int min_left = inode_left < sector_left ? inode_left : sector_left;

      /* Number of bytes to actually write into this sector. */
      int chunk_size = size < min_left ? size : min_left;
      if (chunk_size <= 0)
        break;

      if (sector_ofs == 0 && chunk_size == BLOCK_SECTOR_SIZE)
        {
          /* Write full sector directly to disk. */
          cache_write (sector_idx, buffer + bytes_written);
        }
      else 
        {
          /* We need a bounce buffer. */
          if (bounce == NULL) 
            {
              bounce = malloc (BLOCK_SECTOR_SIZE);
              if (bounce == NULL)
                break;
            }

          /* If the sector contains data before or after the chunk
             we're writing, then we need to read in the sector
             first.  Otherwise we start with a sector of all zeros. */
          if (sector_ofs > 0 || chunk_size < sector_left) 
            cache_read (sector_idx, bounce);
          else
            memset (bounce, 0, BLOCK_SECTOR_SIZE);
          memcpy (bounce + sector_ofs, buffer + bytes_written, chunk_size);
          cache_write (sector_idx, bounce);
        }

      /* Advance. */
      size -= chunk_size;
      offset += chunk_size;
      bytes_written += chunk_size;
    }
  free (bounce);

  return bytes_written;
}

/* Disables writes to INODE.
   May be called at most once per inode opener. */
void
inode_deny_write (struct inode *inode) 
{
  inode->deny_write_cnt++;
  ASSERT (inode->deny_write_cnt <= inode->open_cnt);
}

/* Re-enables writes to INODE.
   Must be called once by each inode opener who has called
   inode_deny_write() on the inode, before closing the inode. */
void
inode_allow_write (struct inode *inode) 
{
  ASSERT (inode->deny_write_cnt > 0);
  ASSERT (inode->deny_write_cnt <= inode->open_cnt);
  inode->deny_write_cnt--;
}

/* Returns the length, in bytes, of INODE's data. */
off_t
inode_length (const struct inode *inode)
{
  return inode->data.length;
}

/* Returns if this inode is directory */
bool 
inode_isdir (const struct inode *inode)
{
  return inode->data.is_dir;
}

/* Set inode to be a directory. God bless it runs OK */
void
inode_set_dir (struct inode *inode)
{
  inode->data.is_dir = true;
  cache_write (inode->sector, &inode->data);
}

/* Implementation by ymt Started */
int inode_get_opencnt(struct inode *inode)
{
  return inode->open_cnt;
}
/* Implementation by ymt Ended */
