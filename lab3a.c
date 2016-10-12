#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>

#define UNIT_BLOCK_SIZE 1024
#define UNIT_INODE_SIZE 128

int blockSize;
int blocksPerGroup;
int inodesPerGroup;
int firstBlock;


int removeLeading16(int val){
  /*Removes the leading 16 bits of an integer.
    Useful if a signed short value is converted to a signed integer value,
    causing the first 16 bits to be filled with 1s.*/
  return ((unsigned)(val << 16) >> 16);
}

int is_free(void* mapBuffer, uint32_t pos){
	//Returns 1 if block/inode at bit position pos is free, or 0 if not
	char* cmapBuffer = (char*)mapBuffer;
	uint32_t byte_pos = pos/8;
	int bit_offset = pos%8;
	char mask = 1 << bit_offset;
	int val = !((cmapBuffer[byte_pos])&mask);
	return val;
}

struct single_indir_ent{
  int blockNum;
  int entryNum;
  int blockVal;
};
typedef struct single_indir_ent* single_indir_ent_t;

struct double_indir_ent{
  int blockNum;
  int entryNum;
  int blockVal;
  single_indir_ent_t singleEnts;
  int numEnts;
};
typedef struct double_indir_ent* double_indir_ent_t;

struct triple_indir_ent{
  int blockNum;
  int entryNum;
  int blockVal;
  double_indir_ent_t doubleEnts;
  int numEnts;
};
typedef struct triple_indir_ent* triple_indir_ent_t;
  
struct dir_ent{
  int parentInode;
  int entryNum;
  int entryLength;
  int nameLength;
  int inodeNumber;
  char* name;

  //Not necessary, but keep track of anyway  
};
typedef struct dir_ent* dir_ent_t;
  
struct inode{
  int inodeNumber;
  char fileType;
  int mode;
  int owner;
  int group;
  int linkCount;
  int creationTime;
  int modificationTime;
  int accessTime;
  int fileSize;
  int nBlocks;
  int bPointers[15];
  //Not necessary, but keep track of corresponding directory entries
  int nDirEnts;
  dir_ent_t dirEntPtr;
  
  //Also not part of EXT2 structs, but keep track of indirect nodes within each
  //block
  int nSingleEnts;
  single_indir_ent_t singleEnts;
  int nDoubleEnts;
  double_indir_ent_t doubleEnts;
  int nTripleEnts;
  triple_indir_ent_t tripleEnts;

};
typedef struct inode* inode_t;

struct group_des{
  int nBlocks;
  int nFreeBlocks;
  int nFreeInodes;
  int nDirs;
  int inodeMapBlock;
  int blockMapBlock;
  int inodeTableBlock;
  //----Below are not part of EXT2 structs
  int nInodes;
  int inodeStart; //first inode for block group
  int allocated_space; //for inodePtr
  int nAllocated;
  inode_t inodePtr;

};
typedef struct group_des* group_des_t;

struct super_block{
  int magicNumber;
  int nInodes;
  int nBlocks;
  int blockSize;
  int fragSize;
  int blocksPerGroup;
  int inodesPerGroup;
  int fragsPerGroup;
  int firstBlock;
  //----------------
  int nGroups;
  group_des_t groupDesPtr;
  //Pointing to an array of descriptors
};
typedef struct super_block* super_block_t;
	
int load_super_block(super_block_t superBlock, int fd){
  void* blockBuffer = malloc(UNIT_BLOCK_SIZE);
  pread(fd, blockBuffer, UNIT_BLOCK_SIZE, 1024);
  uint32_t* ptr4 = blockBuffer;
  superBlock->nInodes = *(ptr4++);
  superBlock->nBlocks = *ptr4;
  ptr4 = blockBuffer + 20;
  superBlock->firstBlock = *(ptr4++);
  superBlock->blockSize = UNIT_BLOCK_SIZE << *(ptr4++);
  superBlock->fragSize = UNIT_BLOCK_SIZE << *(ptr4++);
  superBlock->blocksPerGroup = *(ptr4++);
  superBlock->fragsPerGroup = *(ptr4++);
  superBlock->inodesPerGroup = *(ptr4++);
  ptr4 = blockBuffer + 56;
  superBlock->magicNumber = *ptr4;
  superBlock->nGroups = superBlock->nInodes/superBlock->inodesPerGroup
    + !!(superBlock->nInodes%superBlock->inodesPerGroup);
  //superBlock->groupDesPtr = (superBlock->firstBlock + 1)*blockSize;
  free(blockBuffer);
  return 0;
}

void print_super_block(super_block_t superBlock, FILE* stream){
	fprintf(stream, "%x,%d,%d,%d,%d,%d,%d,%d,%d\n", 
	superBlock->magicNumber,superBlock->nInodes,
	superBlock->nBlocks, superBlock->blockSize, 
	superBlock->fragSize, superBlock->blocksPerGroup,
	superBlock->inodesPerGroup, superBlock->fragsPerGroup,
	superBlock->firstBlock);
}

void print_super_block_wrapper(super_block_t superBlock, int fd){
	FILE* stream = fdopen(fd, "w");
	print_super_block(superBlock, stream);
}

void load_group_des(group_des_t groupDes, int fd, int groupNum){
  void* blockBuffer = malloc(blockSize);
  pread(fd, blockBuffer, blockSize,
	blockSize*(firstBlock + 1) + groupNum*32);
  uint32_t* ptr4 = blockBuffer;
  groupDes->blockMapBlock = *(ptr4++);
  groupDes->inodeMapBlock = *(ptr4++);
  groupDes->inodeTableBlock = *(ptr4);
  short* ptr2 = blockBuffer + 12;
  groupDes->nFreeBlocks =  *(ptr2);
  ptr2 = blockBuffer + 14;
  groupDes->nFreeInodes = *(ptr2);
  ptr2 = blockBuffer + 16;
  groupDes->nDirs = *(ptr2); 
  groupDes->inodeStart = groupNum * inodesPerGroup + 1;
  // delay assigning values to nAllocated and allocated_space
  //groupDes->nAllocated = inodesPerGroup-groupDes->nFreeInodes;  
  //groupDes->allocated_space = groupDes->nAllocated * sizeof(struct inode);
  /*
  pread(fd, blockBuffer, blockSize, inodeTableBlock * blockSize);
  groupDes->inodePtr = blockBuffer;*/
}

void print_group_des(group_des_t groupDes, FILE* stream){
	fprintf(stream, "%d,%d,%d,%d,%x,%x,%x\n",
		groupDes->nBlocks, groupDes->nFreeBlocks,
		groupDes->nFreeInodes, groupDes->nDirs,
		groupDes->inodeMapBlock, groupDes->blockMapBlock,
		groupDes->inodeTableBlock);
}

void print_group_des_wrapper(group_des_t groupDes, int fd){
	FILE* stream = fdopen(fd, "w");
	print_group_des(groupDes, stream);
}

void* load_map(uint32_t offset, int fd){
  void* blockBuffer = malloc(blockSize);
  pread(fd, blockBuffer, blockSize,offset);
  return blockBuffer;
}

void print_block_map(void* mapBuffer, uint32_t start, uint32_t size, 
FILE* stream, int block){
  uint32_t i;
  for(i=0; i<size; i++){
    if(is_free(mapBuffer, i)){
      fprintf(stream, "%x,%d\n", block, start+i+1);
    }
  }
}

int* print_inode_map(void* mapBuffer, FILE* stream, group_des_t groupDes){
  //int* allocatedInodes = malloc(groupDes->nAllocated * sizeof(struct inode));
  int currInode = 0;
  uint32_t size = groupDes->nInodes;
  int start = groupDes->inodeStart;
  int block = groupDes->inodeMapBlock;
  uint32_t i;
  for(i = 0; i<size; i++){
    uint32_t nth = start+i;
    if(is_free(mapBuffer, i)){
      fprintf(stream, "%x,%d\n", block, nth);
    }
    else{
      currInode++;
    }
  }
  //**********************
  // assign nAllocated and allocated_space here
  groupDes->nAllocated = currInode;
  groupDes->allocated_space = currInode * sizeof(struct inode);
  int* allocatedInodes = malloc(currInode * sizeof(int));
  currInode = 0;
  for (i = 0; i<size; i++){
    uint32_t nth = start+i;
    if (!is_free(mapBuffer, i))
      allocatedInodes[currInode++] = nth;
  }
  return allocatedInodes;
}

void load_inode(inode_t inode, int fd, int offset){
  void* blockBuffer = malloc(UNIT_INODE_SIZE);
  pread(fd, blockBuffer, UNIT_INODE_SIZE, offset);
  short* ptr2 = blockBuffer;
  inode->mode = *(ptr2++);
  inode->mode = ((unsigned)(inode->mode << 16) >> 16);//Remove any extra bits in higher 16-bit portion
  inode->owner = *(ptr2++);
  uint32_t* ptr4 = blockBuffer + 4;
  inode->fileSize = *(ptr4++);
  inode->accessTime = *(ptr4++);
  inode->creationTime = *(ptr4++);
  inode->modificationTime = *(ptr4++);
  ptr2 = blockBuffer + 24;
  inode->group = *(ptr2++);
  inode->linkCount = *(ptr2++);
  ptr4 = blockBuffer+28;
  inode->nBlocks = (*(ptr4))*512/blockSize;
  ptr4 = blockBuffer + 40; 
  for (int k = 0; k < 15; k++)
    inode->bPointers[k] = *(ptr4++);

  if(inode->mode&0x8000){
    inode->fileType = 'f';
  }
  else if(inode->mode&0x4000){
    inode->fileType = 'd';
  }
  else if(inode->mode&0xA000){
    inode->fileType = 's';
  }
  else{
    inode->fileType = '?';
  }
  free(blockBuffer);
}

inode_t load_all_inodes(int* inodeNums, int fd,  group_des_t group, int groupIndex){
  int offset = (group->inodeTableBlock)*blockSize;
  inode_t inodes = malloc(group->allocated_space);
  group->inodePtr = inodes;
  for(int i = 0; i < group->nAllocated; i++){
    inodes[i].inodeNumber = inodeNums[i];
    int index = inodeNums[i] - groupIndex * inodesPerGroup;  
    load_inode(&inodes[i], fd, offset + UNIT_INODE_SIZE * (index-1));
  }
  return inodes;
}

void print_all_inodes(group_des_t group, FILE* stream){
  inode_t inodes = group->inodePtr;
  for(int i = 0; i < group->nAllocated; i++){
      fprintf(stream, "%d,%c,%o,%d,%d,%d,%x,%x,%x,%d,%d",
	      inodes[i].inodeNumber, inodes[i].fileType,
	      inodes[i].mode, inodes[i].owner,
	      inodes[i].group, inodes[i].linkCount,
	      inodes[i].creationTime, inodes[i].modificationTime,
	      inodes[i].accessTime, inodes[i].fileSize,
	      inodes[i].nBlocks);
      for(int k = 0; k < 15; k++){
	fprintf(stream, ",%x", inodes[i].bPointers[k]);
      }
      fprintf(stream, "\n");
    }
}  


//Loads single indirect block
//Returns the number of allocated indirect block entries
int load_single_indirect_block(single_indir_ent_t* singleIndirEnts, int blockNum, int fd){
  if(blockNum==18){
    printf("here");
  }
  void* blockBuffer = malloc(blockSize);
  pread(fd, blockBuffer, blockSize, blockNum*blockSize);
  uint32_t* ptr4 = blockBuffer;
  int nonZeroBlockPointers = 0;
  int currEntry = 0;
  while(currEntry++ < blockSize/4){
    if((*(ptr4++)))
      nonZeroBlockPointers++;
  }
  single_indir_ent_t indirEnts = malloc(nonZeroBlockPointers * sizeof(struct single_indir_ent));
  if(indirEnts == 0)
    perror("malloc error.\n");
  nonZeroBlockPointers = 0;
  currEntry = 0;
  ptr4 = blockBuffer;
  while(currEntry < blockSize/4){
    if((*ptr4)){
      indirEnts[nonZeroBlockPointers].blockNum = blockNum;
      indirEnts[nonZeroBlockPointers].entryNum = currEntry;
      indirEnts[nonZeroBlockPointers].blockVal = *ptr4;
      nonZeroBlockPointers++;
    }
    ptr4++;
    currEntry++;
  }
  *singleIndirEnts = indirEnts;
  return nonZeroBlockPointers;
}

int load_double_indirect_block(double_indir_ent_t* doubleIndirEnts, int blockNum, int fd){
  void* blockBuffer = malloc(blockSize);
  pread(fd, blockBuffer, blockSize, blockNum*blockSize);
  uint32_t* ptr4 = blockBuffer;
  int nonZeroBlockPointers = 0;
  int currEntry = 0;
  while(currEntry++ < blockSize/4){
    if((*(ptr4++)))
      nonZeroBlockPointers++;
  }
  double_indir_ent_t indirEnts = malloc(nonZeroBlockPointers * sizeof(struct double_indir_ent));
  if(indirEnts == 0)
    perror("malloc error.\n");

  nonZeroBlockPointers = 0;
  currEntry = 0;
  ptr4 = blockBuffer;
  while(currEntry < blockSize/4){
    if((*ptr4)){
      indirEnts[nonZeroBlockPointers].blockNum = blockNum;
      indirEnts[nonZeroBlockPointers].entryNum = currEntry;
      indirEnts[nonZeroBlockPointers].blockVal = *ptr4;
      indirEnts[nonZeroBlockPointers].numEnts =
	load_single_indirect_block(&(indirEnts[nonZeroBlockPointers].singleEnts),*ptr4, fd);
      
      nonZeroBlockPointers++;
    }
    ptr4++;
    currEntry++;
  }
  *doubleIndirEnts = indirEnts;
  return nonZeroBlockPointers;
}

int load_triple_indirect_block(triple_indir_ent_t* tripleIndirEnts, int blockNum, int fd){
  void* blockBuffer = malloc(blockSize);
  pread(fd, blockBuffer, blockSize, blockNum*blockSize);
  uint32_t* ptr4 = blockBuffer;
  int nonZeroBlockPointers = 0;
  int currEntry = 0;
  while(currEntry++ < blockSize/4){
    if((*(ptr4++)))
      nonZeroBlockPointers++;
  }
  triple_indir_ent_t indirEnts = malloc(nonZeroBlockPointers * sizeof(struct triple_indir_ent));
  if(indirEnts == 0)
    perror("malloc error.\n");
  nonZeroBlockPointers = 0;
  currEntry = 0;
  while(currEntry < blockSize/4){
    if((*ptr4)){
      indirEnts[nonZeroBlockPointers].blockNum = blockNum;
      indirEnts[nonZeroBlockPointers].entryNum = currEntry;
      indirEnts[nonZeroBlockPointers].blockVal = *ptr4;
      indirEnts[nonZeroBlockPointers].numEnts =
	load_double_indirect_block(&(indirEnts[nonZeroBlockPointers].doubleEnts),*ptr4, fd);
      
      nonZeroBlockPointers++;
    }
    ptr4++;
    currEntry++;
  }
  *tripleIndirEnts = indirEnts;
  return nonZeroBlockPointers;
}

void print_single_indirect_block(single_indir_ent_t indirEnts, int numEnts, FILE* stream){
  for(int i = 0; i < numEnts; i++){
    fprintf(stream, "%x,%d,%x\n", indirEnts[i].blockNum,
	    indirEnts[i].entryNum, indirEnts[i].blockVal);
  }
}

void print_double_indirect_block(double_indir_ent_t indirEnts, int numEnts, FILE* stream){
  for(int i = 0; i < numEnts; i++){
    fprintf(stream, "%x,%d,%x\n", indirEnts[i].blockNum,
	    indirEnts[i].entryNum, indirEnts[i].blockVal);
  }
  for(int i = 0; i < numEnts; i++){
    print_single_indirect_block(indirEnts[i].singleEnts, indirEnts[i].numEnts, stream);
  }
} 
void print_triple_indirect_block(triple_indir_ent_t indirEnts, int numEnts, FILE* stream){
  for(int i = 0; i < numEnts; i++){
    fprintf(stream, "%x,%d,%x\n", indirEnts[i].blockNum,
	    indirEnts[i].entryNum, indirEnts[i].blockVal);
  }
  for(int i = 0; i < numEnts; i++){
    print_double_indirect_block(indirEnts[i].doubleEnts, indirEnts[i].numEnts, stream);
  }
}
void load_indir_ents(group_des_t group, int fd){
  inode_t inodes = group->inodePtr;
  for(int i = 0; i < group->nAllocated; i++){
    //Load single indirect blocks
    if(inodes[i].bPointers[12]){
      inodes[i].nSingleEnts = load_single_indirect_block(&(inodes[i].singleEnts), inodes[i].bPointers[12], fd);
    }
    else{
      inodes[i].nSingleEnts = 0;
    }
    //Load double indirect blocks
    if(inodes[i].bPointers[13]){
      inodes[i].nDoubleEnts = load_double_indirect_block(&(inodes[i].doubleEnts), inodes[i].bPointers[13], fd);
    }
    else{
      inodes[i].nDoubleEnts = 0;
    }
    //Load triple indirect blocks
    if(inodes[i].bPointers[14]){
      inodes[i].nTripleEnts = load_triple_indirect_block(&(inodes[i].tripleEnts), inodes[i].bPointers[14], fd);
    }
    else{
      inodes[i].nTripleEnts = 0;
   } 
  }
}
void print_indir_ents(group_des_t group, FILE* stream){
  inode_t inodes = group->inodePtr;
  for(int i = 0; i < group->nAllocated; i++){
    print_single_indirect_block(inodes[i].singleEnts, inodes[i].nSingleEnts, stream);
    print_double_indirect_block(inodes[i].doubleEnts, inodes[i].nDoubleEnts, stream);
    print_triple_indirect_block(inodes[i].tripleEnts, inodes[i].nTripleEnts, stream);
  }
}

void load_all_dir_ents(inode_t inode, dir_ent_t* dirEnts, int fd, int offset){
  single_indir_ent_t singleEnts;
  double_indir_ent_t doubleEnts;
  triple_indir_ent_t tripleEnts;
  void* indirectBlockBuffer = malloc(blockSize);
  void* blockBuffer = malloc(inode->nBlocks * blockSize);
  pread(fd, blockBuffer, inode->nBlocks * blockSize, offset);
  //Run through all directory entries within the directory to find out how many
  //directory entries require allocation
  void* currDirEnt = blockBuffer;
  uint16_t* reclen;
  int numDirEnts = 0;
  int numValidEnts = 0;
  int blocks = inode->nBlocks;
  if(blocks > 12)
    blocks = 12;
  while((currDirEnt-blockBuffer) < blocks*blockSize){
    if(*((uint32_t*)currDirEnt)){
      numValidEnts++;
    }
    reclen = currDirEnt + 4;
    currDirEnt = currDirEnt + *(reclen);
    numDirEnts++;
  }
  if(inode->bPointers[12]){
    //Cycle through the blocks and determine validity of pointers
    int numEnts = load_single_indirect_block(&singleEnts, inode->bPointers[12], fd);
    for(int i = 0; i < numEnts; i++){
        pread(fd, indirectBlockBuffer, singleEnts[i].blockVal * blockSize, offset);
	void* currIndirEnt = indirectBlockBuffer;
	while((currIndirEnt-indirectBlockBuffer) < blockSize){
	  if(*((uint32_t*)currIndirEnt)){
	    numValidEnts++;
	  }
	  reclen = currDirEnt + 4;
	  currDirEnt = currDirEnt + *(reclen);
	  numDirEnts++;
	}
    }
  }
  if(inode->bPointers[13]){
    //Cycle through the blocks and determine validity of pointers
    int numEnts = load_double_indirect_block(&doubleEnts, inode->bPointers[13], fd);
    for(int i = 0; i < numEnts; i++){
      for(int j = 0; j < (doubleEnts[i].numEnts); j++){
        pread(fd, indirectBlockBuffer, (doubleEnts[i].singleEnts)[j].blockVal * blockSize, offset);
	void* currIndirEnt = indirectBlockBuffer;
	while((currIndirEnt-indirectBlockBuffer) < blockSize){
	  if(*((uint32_t*)currIndirEnt)){
	    numValidEnts++;
	  }
	  reclen = currDirEnt + 4;
	  currDirEnt = currDirEnt + *(reclen);
	  numDirEnts++;
	}
      }
    }
  }
  if(inode->bPointers[14]){
    //Cycle through the blocks and determine validity of pointers
      int numEnts = load_triple_indirect_block(&tripleEnts, inode->bPointers[13], fd);
    for(int i = 0; i < numEnts; i++){
      for(int j = 0; j < (tripleEnts[i].numEnts); j++){
	for(int k = 0; k < (tripleEnts[i].doubleEnts[i].numEnts); k++)
        pread(fd, indirectBlockBuffer, (tripleEnts[i].doubleEnts[j].singleEnts)[k].blockVal * blockSize, offset);
	void* currIndirEnt = indirectBlockBuffer;
	while((currIndirEnt-indirectBlockBuffer) < blockSize){
	  if(*((uint32_t*)currIndirEnt)){
	    numValidEnts++;
	  }
	  reclen = currDirEnt + 4;
	  currDirEnt = currDirEnt + *(reclen);
	  numDirEnts++;
	}
      }
    }
  }
  
  //Initialize dir_ent structures equal to the number of directory entries found
  dir_ent_t dir_ents = malloc(numValidEnts * sizeof(struct dir_ent));
  currDirEnt = blockBuffer;
  int currDirEntNum = 0;
  int currValidEntNum = 0;
  while((currDirEnt-blockBuffer) < blocks*blockSize){
    reclen = currDirEnt + 4;
    if(*((uint32_t*)currDirEnt)){
      	dir_ents[currValidEntNum].parentInode = inode->inodeNumber;
	dir_ents[currValidEntNum].inodeNumber = *(uint32_t*)currDirEnt;
        dir_ents[currValidEntNum].entryNum = currDirEntNum;
	dir_ents[currValidEntNum].entryLength = *(reclen);
	uint8_t* ptr1 = currDirEnt+6;
	dir_ents[currValidEntNum].nameLength = *(ptr1);
	dir_ents[currValidEntNum].name = malloc(sizeof(char)*
		      dir_ents[currValidEntNum].nameLength + 1);
	memcpy(dir_ents[currValidEntNum].name, currDirEnt + 8,
	       dir_ents[currValidEntNum].nameLength);
	dir_ents[currValidEntNum].name[dir_ents[currValidEntNum].nameLength] = '\0';
	currValidEntNum++;
      }
    
    currDirEnt = currDirEnt + *(reclen);
    currDirEntNum++;
  }
   if(inode->bPointers[12]){
    //Cycle through the blocks and determine validity of pointers
    int numEnts = load_single_indirect_block(&singleEnts, inode->bPointers[12], fd);
    for(int i = 0; i < numEnts; i++){
        pread(fd, indirectBlockBuffer, singleEnts[i].blockVal * blockSize, offset);
	void* currDirEnt = indirectBlockBuffer;
	while((currDirEnt-indirectBlockBuffer) < blockSize){
	  reclen = currDirEnt + 4;
	  if(*((uint32_t*)currDirEnt)){
	    dir_ents[currValidEntNum].parentInode = inode->inodeNumber;
	    dir_ents[currValidEntNum].inodeNumber = *(uint32_t*)currDirEnt;
	    dir_ents[currValidEntNum].entryNum = currDirEntNum;
	    dir_ents[currValidEntNum].entryLength = *(reclen);
	    uint8_t* ptr1 = currDirEnt+6;
	    dir_ents[currValidEntNum].nameLength = *(ptr1);
	    dir_ents[currValidEntNum].name = malloc(sizeof(char)*
						    dir_ents[currValidEntNum].nameLength + 1);
	    memcpy(dir_ents[currValidEntNum].name, currDirEnt + 8,
		   dir_ents[currValidEntNum].nameLength);
	    dir_ents[currValidEntNum].name[dir_ents[currValidEntNum].nameLength] = '\0';
	    currValidEntNum++;
	  }
	  
	  currDirEnt = currDirEnt + *(reclen);
	  currDirEntNum++;
	}
    }
  }
  if(inode->bPointers[13]){
    //Cycle through the blocks and determine validity of pointers
    int numEnts = load_double_indirect_block(&doubleEnts, inode->bPointers[13], fd);
    for(int i = 0; i < numEnts; i++){
      for(int j = 0; j < (doubleEnts[i].numEnts); j++){
        pread(fd, indirectBlockBuffer, (doubleEnts[i].singleEnts)[j].blockVal * blockSize, offset);
        void* currDirEnt = indirectBlockBuffer;
	while((currDirEnt-indirectBlockBuffer) < blockSize){
	  reclen = currDirEnt + 4;
	  if(*((uint32_t*)currDirEnt)){
	    dir_ents[currValidEntNum].parentInode = inode->inodeNumber;
	    dir_ents[currValidEntNum].inodeNumber = *(uint32_t*)currDirEnt;
	    dir_ents[currValidEntNum].entryNum = currDirEntNum;
	    dir_ents[currValidEntNum].entryLength = *(reclen);
	    uint8_t* ptr1 = currDirEnt+6;
	    dir_ents[currValidEntNum].nameLength = *(ptr1);
	    dir_ents[currValidEntNum].name = malloc(sizeof(char)*
						    dir_ents[currValidEntNum].nameLength + 1);
	    memcpy(dir_ents[currValidEntNum].name, currDirEnt + 8,
		   dir_ents[currValidEntNum].nameLength);
	    dir_ents[currValidEntNum].name[dir_ents[currValidEntNum].nameLength] = '\0';
	    currValidEntNum++;
	  }
	  
	  currDirEnt = currDirEnt + *(reclen);
	  currDirEntNum++;
	}
      }
    }
  }
  if(inode->bPointers[14]){
    //Cycle through the blocks and determine validity of pointers
      int numEnts = load_triple_indirect_block(&tripleEnts, inode->bPointers[13], fd);
    for(int i = 0; i < numEnts; i++){
      for(int j = 0; j < (tripleEnts[i].numEnts); j++){
	for(int k = 0; k < (tripleEnts[i].doubleEnts[j].numEnts); k++)
        pread(fd, indirectBlockBuffer, (tripleEnts[i].doubleEnts[j].singleEnts)[j].blockVal * blockSize, offset);
        void* currDirEnt = indirectBlockBuffer;
	while((currDirEnt-indirectBlockBuffer) < blockSize){
	  reclen = currDirEnt + 4;
	  if(*((uint32_t*)currDirEnt)){
	    dir_ents[currValidEntNum].parentInode = inode->inodeNumber;
	    dir_ents[currValidEntNum].inodeNumber = *(uint32_t*)currDirEnt;
	    dir_ents[currValidEntNum].entryNum = currDirEntNum;
	    dir_ents[currValidEntNum].entryLength = *(reclen);
	    uint8_t* ptr1 = currDirEnt+6;
	    dir_ents[currValidEntNum].nameLength = *(ptr1);
	    dir_ents[currValidEntNum].name = malloc(sizeof(char)*
						    dir_ents[currValidEntNum].nameLength + 1);
	    memcpy(dir_ents[currValidEntNum].name, currDirEnt + 8,
		   dir_ents[currValidEntNum].nameLength);
	    dir_ents[currValidEntNum].name[dir_ents[currValidEntNum].nameLength] = '\0';
	    currValidEntNum++;
	  }
	  
	  currDirEnt = currDirEnt + *(reclen);
	  currDirEntNum++;
	}
      }
    }
  }
 
  //Set the corresponding inode to the dirent
  inode->nDirEnts = numValidEnts;
  inode->dirEntPtr = dir_ents;
  free(indirectBlockBuffer);
  free(blockBuffer);
}
void load_group_dirs(group_des_t group, int fd){
  inode_t inodes = group->inodePtr;
  int dirCount = 0;
  //Search to find the number of allocated directories. 
  for(int i = 0; i < group->nAllocated; i++){
    if(inodes[i].fileType == 'd'){
      dirCount++;
    }
  }
  //Create an array with pointers to arrays of directory entries (to be
  //initialized individually)
  dir_ent_t* dirs = malloc(dirCount * sizeof(struct dir_ent));
  int currDir = 0;
  for(int i = 0; i < group->nAllocated; i++){
    if(inodes[i].fileType == 'd'){
      load_all_dir_ents(&inodes[i],&dirs[currDir], fd, (inodes[i].bPointers)[0]*blockSize);
      currDir++;
    }
  }

}

void print_group_dirs(group_des_t group, FILE* stream){
  inode_t inodes = group->inodePtr;
  for(int i = 0; i < group->nAllocated; i++){
    if(inodes[i].fileType == 'd'){
      dir_ent_t dirEnts = inodes[i].dirEntPtr;;
      for(int j = 0; j < inodes[i].nDirEnts; j++){
	fprintf(stream, "%d,%d,%d,%d,%d,\"%s\"\n",
		dirEnts[j].parentInode, dirEnts[j].entryNum,
		dirEnts[j].entryLength, dirEnts[j].nameLength,
		dirEnts[j].inodeNumber, dirEnts[j].name);
      }
    }
  }
}

int main(int argc, char **argv)
{
  if(argc<2){
    fprintf(stderr, "Requires single filename argument. Exiting.\n");
    exit(-1);
  }
  int fd = open(argv[1], O_RDONLY);
  if(fd < 0){
    perror("System image file open failed. Exiting.");
    exit(-1);
  }
  int fd_super = creat("super.csv", S_IRUSR | S_IWUSR);
  int fd_group = creat("group.csv", S_IRUSR | S_IWUSR);
  FILE* groupstream = fdopen(fd_group, "w");
  int fd_bitmap = creat("bitmap.csv", S_IRUSR | S_IWUSR);
  int fd_inode = creat("inode.csv", S_IRUSR | S_IWUSR);
  FILE* inodestream = fdopen(fd_inode, "w");
  int fd_directory = creat("directory.csv", S_IRUSR | S_IWUSR);
  FILE* directorystream = fdopen(fd_directory, "w");
  int fd_indirect = creat("indirect.csv", S_IRUSR | S_IWUSR);
  FILE* indirectstream = fdopen(fd_indirect, "w");
 
  
  //Print the super block
  struct super_block s;
  load_super_block(&s, fd);
  //printf("---- super block entry: super.csv ----\n");
  print_super_block_wrapper(&s, fd_super);

  blocksPerGroup = s.blocksPerGroup;
  inodesPerGroup = s.inodesPerGroup;
  firstBlock = s.firstBlock;
  blockSize = s.blockSize;
  //Print group blocks
  //printf("---- super block entry: group.csv ----\n");
  group_des_t g = malloc(s.nGroups*sizeof(struct group_des));
  s.groupDesPtr = g;
  FILE* bmpStream = fdopen(fd_bitmap, "w");
  for(int i = 0; i < s.nGroups; i++){
    g[i].nInodes = s.inodesPerGroup;
    if(s.nInodes%s.inodesPerGroup && i == s.nInodes/s.inodesPerGroup){
      //If it's the last block group, assign any remaining blocks into the
      //group
      g[i].nInodes = s.nInodes%s.inodesPerGroup;
    }
    else{
      //Otherwise, assign it the value of the number of blocks per group
      g[i].nInodes = s.inodesPerGroup;
    }

    if(s.nBlocks%s.blocksPerGroup && i == s.nBlocks/s.blocksPerGroup){
      //If it's the last block group, assign any remaining blocks into the
      //group
      g[i].nBlocks = s.nBlocks%s.blocksPerGroup;
    }
    else{
      //Otherwise, assign it the value of the number of blocks per group
      g[i].nBlocks = s.blocksPerGroup;
    }
    load_group_des(&g[i], fd, i);
    print_group_des(&g[i], groupstream);

	  
    //Print blockmap 
    void* blockMap = load_map(g[i].blockMapBlock*blockSize, fd);
    print_block_map(blockMap,
		    i*blocksPerGroup, g[i].nBlocks,
		    bmpStream,g[i].blockMapBlock);
    free(blockMap);
    //Print inodemap
    blockMap = load_map(g[i].inodeMapBlock*blockSize, fd);
    int* inodeNums = print_inode_map(blockMap, bmpStream, &g[i]);

    //Print the value of all of the allocated inodes
    inode_t inodes = load_all_inodes(inodeNums, fd,  &g[i], i);
    print_all_inodes(&g[i], inodestream);

    //Load the directory entries into the appropriate inodes
    load_group_dirs(&g[i], fd);
    //Print the appropriate directory entries from the inodes
    print_group_dirs(&g[i], directorystream);

    //Load the indirect block entries into the group
    load_indir_ents(&g[i], fd);
    //Print indirect block entries from group
    print_indir_ents(&g[i], indirectstream);
  }
  //Print block bitmap
	
  //ETC
  return 0;
}

