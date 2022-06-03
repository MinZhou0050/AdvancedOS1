// Name: Min Zhou
// Date: 4/19/2022

#include <cpmfsys.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

// define diskstruct type
typedef struct diskStruct { 
uint8_t status;
char  name[8];
char  extension[3];
uint8_t XL;
uint8_t BC; 
uint8_t XH; 
uint8_t RC;  
uint8_t blocks[BLOCKS_PER_EXTENT];
} DiskStructType;

// allocates and populates memory
DirStructType *mkDirStruct(int index,uint8_t *e)
{
	e += index * sizeof(Extent);
	
	DirStructType* res = (DirStructType*)malloc(sizeof(DirStructType));
	if(res) {
		DiskStructType* p = (DiskStructType*)e;
		res->status = p->status;
		
		memset(res->name, 0, sizeof(res->name));
		int i = 0;
		for(i = 0; i < 8; ++i) {
			if(p->name[i] == ' ') break;
			res->name[i] = p->name[i];
		}
		
		memset(res->extension, 0, sizeof(res->extension));
		i = 0;
		for(i = 0; i < 3; ++i) {
			if(p->extension[i] == ' ') break;
			res->extension[i] = p->extension[i];
		}
		
		res->XL = p->XL;
		res->BC = p->BC;
		res->XH = p->XH;
		res->RC = p->RC;
		
		memcpy(res->blocks, p->blocks, sizeof(res->blocks));
	}
	return res;
}

// write contents back to block
void writeDirStruct(DirStructType *d, uint8_t index, uint8_t *e)
{
	e += index * sizeof(Extent);
	DiskStructType* p = (DiskStructType*)e;
	
	p->status = d->status;
	
	memset(p->name, ' ', sizeof(p->name));
	int i = 0;
	for(i = 0; d->name[i]; ++i) {
		p->name[i] = d->name[i];
	}
	
	memset(p->extension, ' ', sizeof(p->extension));
	for(i = 0; d->extension[i]; ++i) {
		p->extension[i] = d->extension[i];
	}
	
	p->XL = d->XL;
	p->BC = d->BC;
	p->XH = d->XH;
	p->RC = d->RC;
	
	memcpy(p->blocks, d->blocks, sizeof(d->blocks));
}

// define global data structure freelist
bool freelist[256];

// block_0 free = trun, busy = false
void makeFreeList()
{
	memset(freelist, false, sizeof(freelist));
	freelist[0] = true;
	
	uint8_t block0[1024];
	blockRead(block0, 0);
	
	DiskStructType* p = (DiskStructType*)block0;
	int i = 0;
	for(i = 0; i < 32; ++i, ++p) {
		if(p->status != 0xe5) {
			int j;
			for(j = 0; j < 16; ++j) {
				if(p->blocks[j]) {
					freelist[p->blocks[j]] = true;
				}
			}
		}
	}
}

// print out the content of the free list
void printFreeList()
{
	printf("FREE BLOCK LIST: (* means in-use)\n");
	int i = 0;
	for(i = 0; i < 16; ++i) {
		printf("%3x:", i * 16);
		int j;
		for(j = 0; j < 16; ++j) {
			if(freelist[i * 16 + j]) {
				printf(" *");
			}
			else {
				printf(" .");
			}
		}
		printf(" \n");
	}
}

// print the file directory to stdout
void cpmDir()
{
	printf("DIRECTORY LISTING\n");
	
	uint8_t block0[1024];
	blockRead(block0, 0);
	
	int i = 0;
	for(i = 0; i < 32; ++i) {
		DirStructType *d = mkDirStruct(i, block0);
		if(!d) continue;
		
		if(d->status != 0xe5) {
			int count = 0;
			int j;
			for(j = 0; j < 16; ++j) {
				if(d->blocks[j]) {
					++count;
				}
			}
			int len = (count - 1) * 1024 + d->RC * 128 + d->BC;
			
			printf("%s.%s %d\n", d->name, d->extension, len);
		}
		free(d);
	}
}


// check character
bool validChar(char ch)
{
	if(ch >= '0' && ch <= '9') return true;
	if(ch >= 'a' && ch <= 'z') return true;
	if(ch >= 'A' && ch <= 'Z') return true;
	return false;
}


// check legal name
bool checkLegalName(char *name)
{
	int idx = 0, i = 0;
	for(i = 0; i < 8; ++i, ++idx) {
		char ch = name[idx];
		if(ch == '.' || ch == 0) break;
		if(!validChar(ch)) return false;
	}
	if(idx == 0) return false;
	if(!name[idx]) return true;
	++idx;
	
	for(i = 0; i < 3; ++i, ++idx) {
		char ch = name[idx];
		if(ch == 0) break;
		if(!validChar(ch)) return false;
	}
	if(name[idx]) return false;
	
	return true;
}

// error codes for next five functions (not all errors apply to all 5 functions)  
/* 
    0 -- normal completion
   -1 -- source file not found
   -2 -- invalid  filename
   -3 -- dest filename already exists 
   -4 -- insufficient disk space 
*/ 

int findExtentWithName(char *name, uint8_t *block0)
{
	if(!checkLegalName(name)) return -1;
	
	char* name_t = strdup(name);
	char* n = name_t;
	char* e = "";
	
	int i = 0;
	for(i = 0; name_t[i]; ++i) {
		if(name_t[i] == '.') {
			name_t[i] = 0;
			e = &name_t[i + 1];
			break;
		}
	}
	
	for(i = 0; i < 32; ++i) {
		DirStructType *d = mkDirStruct(i, block0);
		if(!d) continue;
		
		if(d->status != 0xe5 && !strcmp(d->name, n) && !strcmp(d->extension, e)) {
			free(d);
			free(name_t);
			return i;
		}
		free(d);
	}
	free(name_t);
	return -1;
}

// rename file
int cpmRename(char *oldName, char * newName)
{
	uint8_t block0[1024];
	blockRead(block0, 0);
	
	if(!checkLegalName(oldName) || !checkLegalName(newName)) {
		return -2;
	}
	int s = findExtentWithName(oldName, block0);
	if(s < 0) {
		return -1;
	}
	int d = findExtentWithName(newName, block0);
	if(d >= 0) {
		return -3;
	}
	
	char* newname_t = strdup(newName);
	char* n = newname_t;
	char* e = "";
	
	int i = 0;
	for(i = 0; newname_t[i]; ++i) {
		if(newname_t[i] == '.') {
			newname_t[i] = 0;
			e = &newname_t[i + 1];
			break;
		}
	}
	
	DirStructType* p = mkDirStruct(s, block0);
	strcpy(p->name, n);
	strcpy(p->extension, e);
	writeDirStruct(p, s, block0);
	
	blockWrite(block0, 0);	
	
	free(newname_t);
	return 0;
}

// delete file
int  cpmDelete(char * name)
{
	uint8_t block0[1024];
	blockRead(block0, 0);
	
	if(!checkLegalName(name)) {
		return -2;
	}
	int s = findExtentWithName(name, block0);
	if(s < 0) {
		return -1;
	}
	
	DirStructType* p = mkDirStruct(s, block0);
	int j;
	for(j = 0; j < 16; ++j) {
		if(p->blocks[j]) {
			freelist[p->blocks[j]] = false;
		}
	}
	
	memset(p, 0, sizeof(DirStructType));
	p->status = 0xe5;
	
	writeDirStruct(p, s, block0);
	
	blockWrite(block0, 0);	
	return 0;
}