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

/* The number of direct blocks */
#define DIRECT_BLOCKS 100

/* The number of indirect blocks */
#define INDIRECT_BLOCKS 24

/* The number of sector pointers in an indirect block */
#define INDIRECT_POINTERS 128

/* Commonly returned when data not found */
#define NOT_FOUND -1


//Brock driving now.
/* On-disk inode.
   Must be exactly BLOCK_SECTOR_SIZE bytes long. */
struct inode_disk
  {
    int isdir;
    off_t length;                       /* File size in bytes. */
    unsigned magic;                     /* Magic number. */

    block_sector_t direct_blocks[DIRECT_BLOCKS]; /* The Direct Blocks */
    block_sector_t indirect[INDIRECT_BLOCKS]; /* The Indirect Blocks */
    block_sector_t double_indirect; /* The one Double Indirect Block */
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
  if (pos <= inode->data.length)
  {
    int block_idx = pos / BLOCK_SECTOR_SIZE; //index value of byte pos
    //index is a direct block
    if(block_idx < DIRECT_BLOCKS)
    {
      //return block_sector_t value if allocated, otherwise NOT_FOUND
      if(inode->data.direct_blocks[block_idx] != 0) {
        return inode->data.direct_blocks[block_idx];
      }
      else
        return NOT_FOUND;
    }
    //index is an indirect block
    else if (pos / BLOCK_SECTOR_SIZE < INDIRECT_BLOCKS * INDIRECT_POINTERS)
    {
      //get index of pos in the indirect array
      int indirect_index = (pos / BLOCK_SECTOR_SIZE - DIRECT_BLOCKS)
       / INDIRECT_POINTERS;
      //get index of pos within the block at indirect index
      int direct_index = (pos / BLOCK_SECTOR_SIZE - DIRECT_BLOCKS)
       % INDIRECT_POINTERS;
      
      //read in the block_sector_t values in indirect block
      block_sector_t buff[INDIRECT_POINTERS];
      block_read(fs_device, inode->data.indirect[indirect_index], buff);
      //return block_sector_t value if allocated, otherwise NOT_FOUND
      if(buff[direct_index] != 0) {
        return buff[direct_index];
      }
      else
        return NOT_FOUND;
    }
    //index is in double indirect block
    else
    {
      void* buff = malloc(BLOCK_SECTOR_SIZE);
      //get index of pos in the double indirect block
      int indirect_index = pos / BLOCK_SECTOR_SIZE / INDIRECT_POINTERS;
      //get index of pos within the block at indirect index
      int direct_index = pos / BLOCK_SECTOR_SIZE % INDIRECT_POINTERS;
      //read in the double indirect block
      block_read(fs_device, inode->data.double_indirect, buff);
      //set buff to indirect index
      buff += indirect_index * sizeof(block_sector_t);
      void* buff2 = malloc(BLOCK_SECTOR_SIZE);
      //read in the indirect block at buff
      block_read(fs_device, *(block_sector_t *)buff, buff2);
      //set buff2 to direct index
      buff2 += direct_index * sizeof(block_sector_t);
      //return block_sector_t of direct index
      return *(block_sector_t *)buff2;
    }
  }
  //pos is outside the size of the file
  else
  {
    return NOT_FOUND;
  }

  //End of Brock driving
  //Viren driving now
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
inode_create (block_sector_t sector, off_t length, int isdir)
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
      //allocate the first direct block and fill it with zeroes
      if(sectors == 0)
      {
        free_map_allocate(1, &disk_inode->direct_blocks[0]);
        block_write(fs_device, disk_inode->direct_blocks[0], zeros);
        success = true;
      }
      //initialize disk_inode members
      disk_inode->length = length;
      disk_inode->magic = INODE_MAGIC;
      disk_inode->isdir = isdir;
      off_t temp = length;

      //loop through direct blocks until all have been allocated
      //or sectors number of blocks have been allocated
      int i;
      for(i = 0; i < DIRECT_BLOCKS && sectors != 0; i++)
      {
        //allocate direct block
        if (free_map_allocate (1, &disk_inode->direct_blocks[i]))
        {
          //fill this block with zeroes
          block_write(fs_device, disk_inode->direct_blocks[i], zeros);
          sectors--;
          success = true;
          if(sectors <= 0)
          {
            break;
          }
        }
        else //allocation failed
        {
          success = false;
          sectors = 0; 
          break;  
        }
      }
      //End of Viren Driving
      //Jordan driving now.
      //loop through indirect blocks until all have been allocated or
      //sectors number of blocks are allocated
      int d = 0;
      while(sectors > 0 && d < INDIRECT_BLOCKS)
      {
        //allocate an indirect block
        if (free_map_allocate (1, &disk_inode->indirect[d]))
        {
          block_sector_t indirect_sectors[INDIRECT_POINTERS];
          //only some of indirect block must be allocated
          if(sectors <= INDIRECT_POINTERS)
          {
            //loop through indirect block allocating sectors pointers
            int j;
            for(j = 0; j < sectors; j++)
            {
              block_sector_t block;
              //allocate direct block
              if(free_map_allocate(1, &block)) 
              {
                indirect_sectors[j] = block;
                block_write(fs_device, block, zeros);
              }
              else //allocation failed
              {
                success = false;
                break;
              }
            }
            //fill the rest of the indirect block with NULL vals
            while(j < INDIRECT_POINTERS)
            {
              indirect_sectors[j] = NULL;
              j++;
            }
            sectors -= sectors;
            //rewrite to disk
            block_write(fs_device, disk_inode->indirect[d], indirect_sectors);
          }
          //End of Jordan driving,
          //Jasper driving now.
          //fill indirect block entirely
          else 
          {
            //loop through direct blocks in indirect block
            int k;
            for(k = 0; k < INDIRECT_POINTERS; k++)
            {
              block_sector_t block;
              //allocate direct block
              if(free_map_allocate(1, &block)) 
              {
                indirect_sectors[k] = block;
                block_write(fs_device, block, zeros);
              }
              else //allocation failed
              {
                success = false;
                break;
              }
            }
            sectors -= INDIRECT_POINTERS;
            //write back to disk
            block_write(fs_device, disk_inode->indirect[d], indirect_sectors);
          }
          success = true;
          d++;
        }
        else //allocation failed
        {
          success = false;
          sectors = 0;
        }
        //End of Jasper driving.
        //Brock driving now.
      }

      //must allocate double indirect block
      if (sectors > 0)
      {
        //allocate double indirect block
        if (free_map_allocate(1, &disk_inode->double_indirect))
        {
          block_sector_t indirect_sectors[INDIRECT_POINTERS];
          int c = 0;
          //loop through indirect blocks within double indirect block
          while (sectors > 0)
          {
            //allocate an indirect block
            block_sector_t block;
            if(free_map_allocate(1, &block)) 
            {
              indirect_sectors[c] = block;
              int l;
              block_sector_t double_indirect_sectors[INDIRECT_POINTERS];
              //loop through direct blocks within indirect block
              for(l = 0; l < INDIRECT_POINTERS; l++)
              {
                //allocate a direct block
                block_sector_t block2;
                if(free_map_allocate(1, &block2))
                {
                  double_indirect_sectors[l] = block2;
                  block_write(fs_device, block2, zeros);
                  sectors--;
                  if(sectors <= 0)
                  {
                    break;
                  }
                }
                else //allocation failed
                {
                  success = false;
                  break;
                }
              }
              //write back to disk
              block_write(fs_device, block, double_indirect_sectors);
              c++;
            }
            else //allocation failed
            {
              success = false;
              break;
            }
          }
          //write back to disk
          block_write(fs_device, disk_inode->double_indirect, indirect_sectors);
          success = true;
        }
        else //allocation failed
        {
          success = false;
        }
      }
         
    }
    if(success)
    {
      //write back to disk
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
//End of Brock driving
//Viren driving now.
/* Closes INODE and writes it to disk. (Does it?  Check code.)
   If this was the last reference to INODE, frees its memory.
   If INODE was also a removed inode, frees its blocks. */
void
inode_close (struct inode *inode) 
{
  /* Ignore null pointer. */
  if (inode == NULL)
    return;

  //write back to disk
  block_write(fs_device, inode->sector, &inode->data);
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
          int num_bytes = temp.length;
          int i;
          //loop through allocated sectors and release them
          for(i = 0; i < num_bytes; i += BLOCK_SECTOR_SIZE) 
          {
            free_map_release(byte_to_sector(inode, i), 1);
          } 
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

//End of Viren driving
//Jordan driving now.
/*Method that grows the file associated with inode to offset length
and returns the block_sector_t of the block at offset
*/
block_sector_t file_growth(struct inode *inode, off_t offset)
{
  //difference between offset and current EOF
  int temp = offset - inode->data.length;
  //num bytes left in current EOF's block
  int temp2 = inode->data.length % BLOCK_SECTOR_SIZE;
  static char zeros[BLOCK_SECTOR_SIZE];
  int num_written = 0;
  //no new allocation necessary
  if(temp < temp2)
  {
    //update length
    inode->data.length = offset;
    //write back to disk
    block_write(fs_device, inode->sector, &inode->data);
    int bts_return = byte_to_sector(inode, offset);
    return bts_return;
  }
  else
  {
    //number of sectors that must be allocated
    int num_sectors = (temp - temp2) / BLOCK_SECTOR_SIZE + 1;
    //block index of current EOF
    int i = inode->data.length / BLOCK_SECTOR_SIZE;
    //block index is in direct blocks array
    if(i < DIRECT_BLOCKS)
    {
      //loop through direct blocks until all have been allocated
      while(num_sectors > 0 && i < DIRECT_BLOCKS) 
      {
        //allocate direct block
        if (free_map_allocate(1, &inode->data.direct_blocks[i]))
        {
           block_write(fs_device, inode->data.direct_blocks[i], zeros);
           num_written += BLOCK_SECTOR_SIZE;
           num_sectors--;
        }
        else //allocation failed
        {
          return NULL;
        }
        i++;
      } 
      //all sectors have been allocated
      if(num_sectors == 0)
      {
        //update length
        inode->data.length = offset;
        //write back to disk
        block_write(fs_device, inode->sector, &inode->data);
        return inode->data.direct_blocks[i];
      }
      inode->data.length += num_written;
    }
    block_sector_t return_block;
    //Jordan done driving
    //Jasper driving now
    //block index is in indirect blocks array
    if(i < INDIRECT_BLOCKS * INDIRECT_POINTERS || num_sectors > 0)
    {
      //index in indirect blocks array
      int j = (inode->data.length / BLOCK_SECTOR_SIZE - DIRECT_BLOCKS)
       / INDIRECT_POINTERS;
       //direct index in indirect block
      int indirect_offset = (inode->data.length / BLOCK_SECTOR_SIZE 
       - DIRECT_BLOCKS) % INDIRECT_POINTERS;
      num_written = 0;
      //loop through indirect blocks until all sectors have been allocated
      while(num_sectors > 0 && j < INDIRECT_BLOCKS)
      {
        block_sector_t indirect_sectors[INDIRECT_POINTERS];
        //read in indirect block if some direct blocks have been allocated
        if(indirect_offset != 0) 
          block_read(fs_device, inode->data.indirect[j], indirect_sectors);
        else //fill indirect block with zeroes initially
        {
          int z;
          for(z = 0; z < INDIRECT_POINTERS; z++) 
            indirect_sectors[z] = 0;
        }
        //loop through direct blocks withing indirect block
        while(num_sectors > 0 && indirect_offset < INDIRECT_POINTERS) {
          block_sector_t block;
          //allocate new direct block
          if(free_map_allocate(1, &block)) 
          {
            indirect_sectors[indirect_offset] = block;
            block_write(fs_device, block, zeros);
            return_block = block;
            num_written += BLOCK_SECTOR_SIZE;
            num_sectors--;
            indirect_offset++;
          }
          else //allocation failed
            return NULL;
        }
        block_write(fs_device, inode->data.indirect[j], indirect_sectors);
        j++;
        //allocate new indirect block and reset direct index to zero
        if(num_sectors > 0) 
        {
          indirect_offset = 0;
          if(!free_map_allocate(1, inode->data.indirect[j]))
            return NULL;
        }
      }
      if(num_sectors == 0)
      {
        //update length
        inode->data.length = offset;
        //write back to disk
        block_write(fs_device, inode->sector, &inode->data);
        return return_block;
      }
      //update length for each new allocation
      inode->data.length += num_written;
    }
    //Jasper done driving
    //Brock driving now
    //double indirect allocation necessary

    //indirect index withing double indirect block
    int indirect_index = inode->data.length / BLOCK_SECTOR_SIZE
     / INDIRECT_POINTERS;
    //direct index within indirect block
    int direct_index = inode->data.length / BLOCK_SECTOR_SIZE
     % INDIRECT_POINTERS;
    //loop until all sectors have been allocated
    while(num_sectors > 0)
    {
      //loop through indirect block
      while(num_sectors > 0 && direct_index < INDIRECT_POINTERS) {
        block_sector_t block;
        //allocate new direct block
        if(free_map_allocate(1, &block))
        {
          void *buff = malloc(BLOCK_SECTOR_SIZE);
          block_read(fs_device, inode->data.double_indirect, buff);
          buff += indirect_index * sizeof(block_sector_t);
          void *buff2 = malloc(BLOCK_SECTOR_SIZE);
          block_read(fs_device, *(block_sector_t *)buff, buff2);
          buff2 += direct_index * sizeof(block_sector_t);
          block_write(fs_device, *(block_sector_t *)buff2, &block);
          block_write(fs_device, block, zeros);
          return_block = block;
          num_written += BLOCK_SECTOR_SIZE;
          num_sectors--;          
          direct_index++;
        }
        else //allocation failed
          return NULL;
      }
      indirect_index++;
      //allocate new indirect block
      if(num_sectors > 0) {
        block_sector_t block2;
        if(free_map_allocate(1, &block2)) {
          block_write(fs_device, inode->data.double_indirect + 
          indirect_index * sizeof(block_sector_t), &block2);
          direct_index = 0;
        }
        else
          return NULL;
      }
    }
    //update length
    inode->data.length = offset;
    //write back to disk
    block_write(fs_device, inode->sector, &inode->data);
    return return_block;
  }

}

//Brock done driving
//Viren driving now.
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
      block_sector_t sector_idx;
      sector_idx = byte_to_sector (inode, offset);
      //file growth required
      if(sector_idx == NOT_FOUND)
      {
        file_growth(inode, offset);
        sector_idx = byte_to_sector(inode, offset);
      }
      int sector_ofs = offset % BLOCK_SECTOR_SIZE;
      /* Bytes left in inode, bytes left in sector, lesser of the two. */
      int sector_left = BLOCK_SECTOR_SIZE - sector_ofs;
      /* Number of bytes to actually write into this sector. */
      int chunk_size = size < sector_left ? size : sector_left;
      if (chunk_size <= 0)
        break;

      if (sector_ofs == 0 && chunk_size == BLOCK_SECTOR_SIZE)
        {
          /* Write full sector directly to disk. */
          block_write (fs_device, sector_idx, buffer + bytes_written);
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
            block_read (fs_device, sector_idx, bounce);
          else
            memset (bounce, 0, BLOCK_SECTOR_SIZE);
          memcpy (bounce + sector_ofs, buffer + bytes_written, chunk_size);
          block_write (fs_device, sector_idx, bounce);
        }
      if(offset + chunk_size > inode->data.length) {
        inode->data.length += chunk_size;
        block_write(fs_device, inode->sector, &inode->data);
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

/* Returns whether the inode is a directory or not */
bool inodeisdir(const struct inode *inode)
{
  return inode->data.isdir;
}

/* Returns the sector number for inode */
int inumber(const struct inode *inode)
{
  return inode->sector;
}

// Viren done driving
