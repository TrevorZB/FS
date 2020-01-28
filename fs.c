#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>

#define stat xv6_stat
#include "types.h"
#include "stat.h"
#include "fs.h"
#include "param.h"
#undef stat


int second_multi_check(struct dinode *inode, uint inodeCnt, char* imgStart) {
  int i, j, m, k, good;
  struct dirent *dir;

  struct dinode *currInode = inode;
  currInode += 2;
  struct dinode *temp = inode;

  // loops through inodes
  for (i = 2; i < inodeCnt; i++, currInode++) {
    // valid inode
    if (currInode->type != 0) {
      good = 0;
      // loop through inodes
      for (m = 0; m < inodeCnt; m++, temp++) {
        // find dir inodes
        if (temp->type == T_DIR) {
          // iterate through data blocks
          for (j = 0; j < NDIRECT; j++) {
            if (temp->addrs[j] != 0) {
              dir = (struct dirent *) (imgStart + (temp->addrs[j] * BSIZE));
              // iterate through dir entries
              for (k = 0; k < 32; k++, dir++) {
                if (dir) {
                  if (dir->inum == i) {
                    good = 1;
                    break;
                  }
                }
              }
            }
            if (good == 1) {
              break;
            }
          }
        }
        if (good == 1) {
          break;
        }
      }
      if (good == 0) {
        return -1;
      }
    }
  }
  return 0;
}

int first_multi_check(struct dinode *inode, uint inodeCnt, char* imgStart) {
  int i, j, k;
  struct dirent *dir;
  struct dinode *temp;

  struct dinode *currInode = inode;

  // iterate through inodes
  for (i = 0; i < inodeCnt; i++, currInode++) {
    // find dir inodes
    if (currInode->type == T_DIR) {
      // iterate through data blocks
      for (j = 0; j < NDIRECT; j++) {
        if (currInode->addrs[j] != 0) {
          dir = (struct dirent *) (imgStart + (currInode->addrs[j] * BSIZE));
          // iterate through dir entries
          for (k = 0; k < 32; k++, dir++) {
            if (dir) {
              // find the inumber of the entry
              int inumber = dir->inum;
              // weird case
              if (inumber == 0) {
                continue;
              }
              temp = inode;
              // move temp pointer to correct inode
              temp += inumber;
              // inode not valid
              if (temp->type == 0) {
                return -1;
              }
            }
          }
        }
      }
    }
  }
  return 0;
}

int second_bit_map_check(struct dinode *inode, uint inodeCnt,
                         char* imgStart, char *bit_map_start) {
  int i, j, m, k, result;
  char *bits;

  struct dinode *currInode = inode;
  int inode_blocks[1000];

  for (i = 0; i < 1000; i++) {
    inode_blocks[i] = 0;
  }

  for (i = 0; i < 59; i++) {
    inode_blocks[i] = 1;
  }

  // loops through inodes
  for (i = 0; i < inodeCnt; i++, currInode++) {
    if (currInode->type != 0) {
      // loops through data in inodes
      for (j = 0; j < NDIRECT; j++) {
        if (currInode->addrs[j] != 0) {
          // save inode block into correct index
          inode_blocks[currInode->addrs[j]] = 1;
        }
      }
      // have to go through indirect block
      if (currInode->addrs[NDIRECT] != 0) {
        // save indirect block as well
        inode_blocks[currInode->addrs[NDIRECT]] = 1;
        uint *indir = (uint *) (imgStart + (currInode->addrs[NDIRECT] * BSIZE));
        for (k = 0; k < NINDIRECT; k++, indir++) {
          if (*indir != 0) {
          // save inode block into correct index
          inode_blocks[*indir] = 1;
          }
        }
      }
    }
  }

  bits = bit_map_start;
  m = 0;
  // iterate through bitmap, recording bits
  for (i = 0; i < 512; i++, bits++) {
    for (j = 0; j < 8; j++) {
      if (m >= 1000) {
        break;
      }
      // gets bits lowest to highest
      result = ((*bits) >> j) & 1;
      if (result == 1) {
        if (inode_blocks[m] != 1) {
          return -1;
        }
      }
      m++;
    }
    if (m >= 1000) {
        break;
    }
  }
  return 0;
}

int first_bit_map_check(struct dinode *inode, uint inodeCnt,
                       char* imgStart, char *bit_map_start) {
  int i, j, m, k;
  char *bits;
  uint bit_blocks[4096];

  struct dinode *currInode = inode;
  bits = bit_map_start;

  m = 0;
  // iterate through bitmap, recording bits
  for (i = 0; i < 512; i++, bits++) {
    for (j = 0; j < 8; j++) {
      // gets bits lowest to highest
      bit_blocks[m++] = ((*bits) >> j) & 1;
    }
  }
  // loops through inodes
  for (i = 0; i < inodeCnt; i++, currInode++) {
    if (currInode->type != 0) {
      // loops through data in inodes
      for (j = 0; j < NDIRECT; j++) {
        if (currInode->addrs[j] != 0) {
          // bit entry is 0, return -1
          if (bit_blocks[currInode->addrs[j]] == 0) {
            return -1;
          }
        }
      }
      // have to go through indirect block
      if (currInode->addrs[NDIRECT] != 0) {
        // check indirect block itself
        if (bit_blocks[currInode->addrs[NDIRECT]] == 0) {
          return -1;
        }
        uint *indir = (uint *) (imgStart + (currInode->addrs[NDIRECT] * BSIZE));
        for (k = 0; k < NINDIRECT; k++, indir++) {
          if (*indir != 0) {
            // bit entry is 0, return -1
            if (bit_blocks[*indir] == 0) {
              return -1;
            }
          }
        }
      }
    }
  }
  return 0;
}

int check_dot_entry(struct dinode *inode, uint inodeCnt, char* imgStart) {
  int i, j, k;
  struct dirent *dir;

  struct dinode *currInode = inode;
  char *dot = ".";

  // iterate through inodes
  for (i = 0; i < inodeCnt; i++, currInode++) {
    // find dir inodes
    if (currInode->type == T_DIR) {
      // iterate through data blocks
      for (j = 0; j < NDIRECT; j++) {
        if (currInode->addrs[j] != 0) {
          dir = (struct dirent *) (imgStart + (currInode->addrs[j] * BSIZE));
          // iterate through dir entries
          for (k = 0; k < 32; k++, dir++) {
            if (dir) {
              // "." entry
              if (strcmp(dir->name, dot) == 0) {
                // make sure it refers to correct number
                if (dir->inum == i) {
                  continue;
                } else {
                  return -1;
                }
              }
            }
          }
        }
      }
    }
  }
  return 0;
}

int check_root_directory_exists(struct dinode *inode, char* imgStart) {
  int i;

  struct dinode *currInode = inode;

  // go to root inode
  currInode++;
  // make sure root is a directory
  if (currInode->type != T_DIR) {
    return -1;
  }
  // get root node's dirents
  struct dirent *dir = (struct dirent *) (imgStart +
                       (currInode->addrs[0] * BSIZE));
  char *dotdot = "..";
  // iterate 32 times since thats the number of dirents in a block
  for (i = 0; i < 32; i++, dir++) {
    if (dir) {
      // ".." entry
      if (strcmp(dir->name, dotdot) == 0) {
        // inum of .. entry is root inum, this is valid root
        if (dir->inum == 1) {
          return 0;
        }
      }
    }
  }
  // not valid root
  return -1;
}

int check_inode_size(struct dinode *inode, uint inodeCnt, char* imgStart) {
  int numBlocks, i, j, k, total_block_size;

  numBlocks = i = j = k = 0;
  struct dinode *currInode = inode;

  // iterate through inodes
  for (i = 0; i < inodeCnt; i++, currInode++) {
    // if the inode is valid
    if (currInode->type != 0) {
      numBlocks = 0;
      // count how many blocks are in use
      for (j = 0; j < NDIRECT; j++) {
        if (currInode->addrs[j] != 0) {
          numBlocks++;
        }
      }
      // indirect block allocated
      if (currInode->addrs[NDIRECT] != 0) {
        uint *indir = (uint *) (imgStart + (currInode->addrs[NDIRECT] * BSIZE));
        for (k = 0; k < NINDIRECT; k++, indir++) {
          if (*indir != 0) {
            numBlocks++;
          }
        }
      }
      total_block_size = numBlocks * BSIZE;
      // block size is less than the file's size
      if (total_block_size < currInode->size) {
        return -1;
      }
      // difference is larger than a block
      if (total_block_size - currInode->size > 512) {
        return -1;
      }
    }
  }
  return 0;
}

int check_inode_type(struct dinode *inode, uint inodeCnt) {
  int i;

  struct dinode *currInode = inode;

  for (i = 0; i < inodeCnt; i++, currInode++) {
    if (currInode->type != 0 &&
        currInode->type != T_FILE
         && currInode->type != T_DIR
         && currInode->type != T_DEV) {
        return -1;
    }
  }
  return 0;
}

int main(int argc, char **argv) {
  // incorrect useage
  if (argc < 2) {
    exit(1);
  }
  int fd;
  // open file system img
  fd = open(argv[1], O_RDONLY);
  struct stat sb;
  // error when getting fstat of fd
  if (fstat(fd, &sb) < 0) {
    exit(1);
  }

// [ boot block | super block | log | inode blocks |
//                                          free bit map | data blocks]

  // start of boot block
  char *imgStart = mmap(NULL, sb.st_size, PROT_READ, MAP_PRIVATE, fd, 0);

  // add a block to get from boot block to the superblock
  struct superblock *superblockStart = (struct superblock *)(imgStart + BSIZE);

  // first inode
  struct dinode *firstInode = (struct dinode *)(imgStart +
                               (superblockStart->inodestart * BSIZE));

  // start of bit map
  char *bit_map_start = (char *)(imgStart +
                         (superblockStart->bmapstart * BSIZE));

  // inode type check
  if (check_inode_type(firstInode, superblockStart->ninodes) == -1) {
    fprintf(stderr, "%s\n", "ERROR: bad inode.");
    exit(1);
  }

  // inode size check
  if (check_inode_size(firstInode, superblockStart->ninodes, imgStart) == -1) {
    fprintf(stderr, "%s\n", "ERROR: bad size in inode.");
    exit(1);
  }

  // root directory check
  if (check_root_directory_exists(firstInode, imgStart) == -1) {
    fprintf(stderr, "%s\n", "ERROR: root directory does not exist.");
    exit(1);
  }

  // dot entry check
  if (check_dot_entry(firstInode, superblockStart->ninodes, imgStart) == -1) {
    fprintf(stderr, "%s\n", "ERROR: current directory mismatch.");
    exit(1);
  }

  // first bit map check
  if (first_bit_map_check(firstInode, superblockStart->ninodes,
                           imgStart, bit_map_start) == -1) {
    fprintf(stderr, "%s\n",
     "ERROR: bitmap marks data free but data block used by inode.");
    exit(1);
  }

  // second bit map check
  if (second_bit_map_check(firstInode, superblockStart->ninodes,
                            imgStart, bit_map_start) == -1) {
    fprintf(stderr, "%s\n",
     "ERROR: bitmap marks data block in use but not used.");
    exit(1);
  }

  // first multi check
  if (first_multi_check(firstInode, superblockStart->ninodes, imgStart)) {
    fprintf(stderr, "%s\n",
     "ERROR: inode marked free but referred to in directory.");
    exit(1);
  }

  // second multi check
  if (second_multi_check(firstInode, superblockStart->ninodes, imgStart)) {
    fprintf(stderr, "%s\n",
     "ERROR: inode marked in use but not found in a directory.");
    exit(1);
  }

  // no errors
  exit(0);
}
