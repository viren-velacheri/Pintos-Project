#include "filesys/inode.h"
#include <list.h>
#include <debug.h>
#include <round.h>
#include <string.h>
#include "filesys/filesys.h"
#include "filesys/free-map.h"
#include "threads/malloc.h"

/* Identifies an inode. */
#define INODE_MAGIC 0x494e4f44

/* On-disk inode.
   Must be exactly BLOCK_SECTOR_SIZE bytes long. */
struct inode_disk
  {
    //block_sector_t start;               /* First data sector. */
    off_t length;                       /* File size in bytes. */
    unsigned magic;                     /* Magic number. */
    //uint32_t unused[125];               /* Not used. */
    block_sector_t direct_blocks[100];
    block_sector_t indirect[25];
    block_sector_t double_indirect;
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

/* Returns the block device sector that contains byte offset POS
   within INODE.
   Returns -1 if INODE does not contain data for a byte at offset
   POS. */
static block_sector_t
byte_to_sector (const struct inode *inode, off_t pos) 
{
  ASSERT (inode != NULL);
 // printf("pos: %d\n", pos);
  //printf("length: %d\n", inode->data.length);
  if (pos <= inode->data.length)
  {
    //printf("in if\n");
    if(pos / BLOCK_SECTOR_SIZE < 100)
    {
      return inode->data.direct_blocks[pos / BLOCK_SECTOR_SIZE];
    }
    else if (pos / BLOCK_SECTOR_SIZE < 3200)
    {
      int indirect_index = pos / BLOCK_SECTOR_SIZE / 128;
      int direct_index = pos / BLOCK_SECTOR_SIZE % 128;
      void* buff = malloc(512);
      block_read(fs_device, inode->data.indirect[indirect_index], buff);
      buff += direct_index * 4;
      return *(block_sector_t *)buff;
    }
    else
    {
      void* buff = malloc(512);
      int indirect_index = pos / BLOCK_SECTOR_SIZE / 128;
      int direct_index = pos / BLOCK_SECTOR_SIZE % 128;
      block_read(fs_device, inode->data.double_indirect, buff);
      buff += indirect_index * 4;
      void* buff2 = malloc(512);
      block_read(fs_device, *(block_sector_t *)buff, buff2);
      buff2 += direct_index * 4;
      return *(block_sector_t *)buff2;
    }
  }
  else
  {
    return -1;
  }
  // if (pos < inode->data.length)
  //   return inode->data.start + pos / BLOCK_SECTOR_SIZE;
  // else
  //   return -1;
}

/* List of open inodes, so that opening a single inode twice
   returns the same `struct inode'. */
static struct list open_inodes;

/* Initializes the inode module. */
void
inode_init (void) 
{
  list_init (&open_inodes);
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
      size_t sectors = bytes_to_sectors (length);
      static char zeros[BLOCK_SECTOR_SIZE];
      disk_inode->length = length;
      disk_inode->magic = INODE_MAGIC;
      off_t temp = length;
      int i;
      for(i = 0; i < 100; i++)
      {
        if (free_map_allocate (1, &disk_inode->direct_blocks[i]))
        {
          block_write(fs_device, disk_inode->direct_blocks[i], zeros);
          sectors--;
          success = true;
          if(sectors <= 0)
          {
            break;
          }
        }
      }
      int d = 0;
      while(sectors > 0 && d < 25)
      {
        if (free_map_allocate (1, &disk_inode->indirect[d]))
        {
          if(sectors <= 128)
          {
            int j;
            for(j = 0; j < sectors; j++)
            {
            block_sector_t block;
            free_map_allocate(1, &block);
            block_write(fs_device, disk_inode->indirect[d] + j * 4, &block);
            block_write(fs_device, block, zeros);
            }
            while(j < 128)
            {
              block_write(fs_device, disk_inode->indirect[d] + j * 4, 0);
              j++;
            }
            sectors -= sectors;
          }
          else 
          {
            int k;
            for(k = 0; k < 128; k++)
            {
            block_sector_t block;
            free_map_allocate(1, &block);
            block_write(fs_device, disk_inode->indirect[d] + k * 4, &block);
            block_write(fs_device, block, zeros);
            }
            sectors -= 128;
          }
          success = true;
          d++;
        }
        else 
        {
          success = false;
          sectors = 0;   
          // Might have to free the already allocated sectors but not sure.
        }
      }

      
      if (sectors > 0)
      {
        if (free_map_allocate(1, &disk_inode->double_indirect))
        {
          int c = 0;
          while (sectors > 0)
          {
            block_sector_t block;
            free_map_allocate(1, &block);
            block_write(fs_device, disk_inode->double_indirect + c * 4, &block);
            int l;
            for(l = 0; l < 128; l++)
            {

              block_sector_t block2;
              free_map_allocate(1, &block2);
              block_write(fs_device, block + l * 4, &block2);
              block_write(fs_device, block2, zeros);
              sectors--;
              if(sectors <= 0)
              {
                break;
              }
            }
            c++;
          }
        }
        else
        {
          success = false;
        }
      }
         
    }
    if(success)
    {
      block_write(fs_device, sector, disk_inode);
    }
    free (disk_inode);
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
  block_read (fs_device, inode->sector, &inode->data);
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

/* Closes INODE and writes it to disk. (Does it?  Check code.)
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
          struct inode_disk temp = inode->data;
          free_map_release (inode->sector, 1);
          int j;
          for(j = 0; j < 100; j++)
          {
            free_map_release(temp.direct_blocks[j], 1);
          }
          int i;
          for(i = 0; i < 25; i++)
          {
            block_sector_t block = temp.indirect[i];
            int j = 0;
            void *buffer = malloc(512);
            block_read(fs_device, block, buffer);
            while(j < 128)
            {
              block_sector_t temp2 = *(block_sector_t *)buffer;
              if(temp2 == NULL)
              {
                i = 25;
                break;
              }
              free_map_release(temp2, 1);
              buffer += 4;
              j++;
            }
            free_map_release(block, 1);
          }

          if(temp.double_indirect != NULL)
          {
            int k;
            for(k = 0; k < 128; k++)
            {
            block_sector_t block = temp.double_indirect + k * 4;
            int l = 0;
            void *buffer = malloc(512);
            block_read(fs_device, block, buffer);
            while(l < 128)
            {
              block_sector_t temp2 = *(block_sector_t *)buffer;
              if(temp2 == NULL)
              {
                k = 128;
                break;
              }
              free_map_release(temp2, 1);
              buffer += 4;
              l++;
            }
            free_map_release(block, 1);
            }
          }
          // free_map_release (inode->data.start,
          //                   bytes_to_sectors (inode->data.length)); 
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
      block_sector_t sector_idx = byte_to_sector (inode, offset);
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
          block_read (fs_device, sector_idx, buffer + bytes_read);
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
          block_read (fs_device, sector_idx, bounce);
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

block_sector_t file_growth(struct inode *inode, off_t offset)
{
  int temp = offset - inode->data.length;
  int temp2 = inode->data.length % 512;
  static char zeros[BLOCK_SECTOR_SIZE];
  int num_written;
  if(temp <= temp2)
  {
    inode->data.length = offset;
    return byte_to_sector(inode, offset);
  }
  else
  {
    int num_sectors = (temp - temp2) / 512 + 1;
    int i = inode->data.length / 512;
    if(i < 100)
    {
      while(num_sectors > 0 && i < 100) 
      {
        i++;
        if (free_map_allocate(1,inode->data.direct_blocks[i]))
        {
           block_write(fs_device, inode->data.direct_blocks[i], zeros);
           num_written += 512;
           num_sectors--;
        }
        else 
        {
          return NULL;
        }
      } 
      if(num_sectors == 0)
      {
        inode->data.length = offset;
        return inode->data.direct_blocks[i];
      }
      inode->data.length += num_written;
    }
    block_sector_t return_block;
    if(i < 3200 || num_sectors > 0)
    {
      int j = inode->data.length / BLOCK_SECTOR_SIZE / 128;
      int indirect_offset = inode->data.length / BLOCK_SECTOR_SIZE % 128;
      num_written = 0;
      while(num_sectors > 0 && j < 25)
      {
        while(num_sectors > 0 && indirect_offset < 128) {
          block_sector_t block;
          free_map_allocate(1, &block);
          block_write(fs_device, inode->data.indirect[j] + indirect_offset * 4, &block);
          block_write(fs_device, block, zeros);
          return_block = block;
          num_written += 512;
          num_sectors--;
          indirect_offset++;
        }
        j++;
        if(num_sectors > 0) 
        {
          indirect_offset = 0;
          free_map_allocate(1, inode->data.indirect[j]);
        }
      }
      if(num_sectors == 0)
      {
        inode->data.length = offset;
        return return_block;
      }
      inode->data.length += num_written;
    }
    int indirect_index = inode->data.length / BLOCK_SECTOR_SIZE / 128;
    int direct_index = inode->data.length / BLOCK_SECTOR_SIZE % 128;
    while(num_sectors > 0)
    {
      while(num_sectors > 0 && direct_index < 128) {
        block_sector_t block;
        free_map_allocate(1, &block);
        void *buff = malloc(512);
        block_read(fs_device, inode->data.double_indirect, buff);
        buff += indirect_index * 4;
        void *buff2 = malloc(512);
        block_read(fs_device, *(block_sector_t *)buff, buff2);
        buff2 += direct_index *4;
        block_write(fs_device, *(block_sector_t *)buff2, &block);
        block_write(fs_device, block, zeros);
        return_block = block;
        num_written += 512;
        num_sectors--;          
        direct_index++;
      }
      indirect_index++;
      if(num_sectors > 0) {
        block_sector_t block2;
        free_map_allocate(1, &block2);
        block_write(fs_device, inode->data.double_indirect + indirect_index *4, &block2);
        direct_index = 0;
      }
    }
    inode->data.length = offset;
    return return_block;
  }
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
      //printf("Size: %d", size);
      /* Sector to write, starting byte offset within sector. */
      block_sector_t sector_idx = byte_to_sector (inode, offset);
      //printf("Sector index: %d\n", sector_idx);
      if(sector_idx == -1)
      {
        sector_idx = file_growth(inode, offset);
      }
      //printf("Block sector: %d\n", sector_idx);
      int sector_ofs = offset % BLOCK_SECTOR_SIZE;
      //printf("Offset: %d\n", offset);
      /* Bytes left in inode, bytes left in sector, lesser of the two. */
      off_t inode_left = inode_length (inode) - offset;
     // printf("Inode Length: %d\n", inode_length (inode));
      //printf("inode left: %d\n", inode_left);
      int sector_left = BLOCK_SECTOR_SIZE - sector_ofs;
     // printf("Sector left: %d\n", sector_left);
      int min_left = inode_left < sector_left ? inode_left : sector_left;
     // printf("Min Left: %d\n", min_left);

      /* Number of bytes to actually write into this sector. */
      int chunk_size = size < min_left ? size : min_left;
     // printf("Chunk size: %d\n", chunk_size);
      if (chunk_size <= 0)
        break;

      if (sector_ofs == 0 && chunk_size == BLOCK_SECTOR_SIZE)
        {
         // printf("Writing to the full sector\n");
          /* Write full sector directly to disk. */
          block_write (fs_device, sector_idx, buffer + bytes_written);
        }
      else 
        {
          //printf("Bounce Buffer needed\n");
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
            block_read (fs_device, sector_idx, bounce);
          else
            memset (bounce, 0, BLOCK_SECTOR_SIZE);
          memcpy (bounce + sector_ofs, buffer + bytes_written, chunk_size);
          block_write (fs_device, sector_idx, bounce);
        }

      /* Advance. */
      size -= chunk_size;
      offset += chunk_size;
      bytes_written += chunk_size;
    }
  free (bounce);
 // printf("written: %d\n", bytes_written);
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
