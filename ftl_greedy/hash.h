#ifndef HASH_H
#define HASH_H

#define INVALID_INDEX 0xFFFFFFFF

/*
#define HASH_TABLE_SIZE 4096


typedef struct HashTable
{
	UINT32 lpn;
	UINT32 id;
}HashTable;

HashTable hashTable[HASH_TABLE_SIZE];
HashTable hashTable[4096]; */

void initHASH();
void insertHash(UINT32 const lpn, UINT16 id);
UINT16 findHash(UINT32 const lpn);
void deleteHash(UINT32 const lpn);
//void updateHash(UINT32 const lpn, UINT16 id);
//UINT16 getHash(UINT32 const lpn);

void insertBlk(UINT32 const blk_num, UINT16 head, UINT16 tail);
UINT32 findBlk(UINT32 const blk_num);
void deleteBlk(UINT32 const blk_num);
void updateBlk(UINT32 const blk_num, UINT16 head, UINT16 tail);
void set_num_bank(UINT32 const lpn, UINT32 const bank) ;
UINT32 get_num_bank(UINT32 const lpn);
UINT16 getBlk(UINT32 const blk_num);
void   set_lru_next(UINT16 id, UINT16 value);
void   set_lru_prev(UINT16 id, UINT16 value);
void   set_lru_lpn(UINT16 id,UINT32 lpn);
UINT16 get_lru_next(UINT16 id);
UINT16 get_lru_prev(UINT16 id);
UINT32 get_lru_lpn(UINT16 id);
void   set_lru_bank_next(UINT16 id, UINT16 value);
void   set_lru_bank_prev(UINT16 id, UINT16 value);
UINT16 get_lru_bank_next(UINT16 id);
UINT16 get_lru_bank_prev(UINT16 id);

void set_lru_in_cap(UINT16 id, UINT8 value);
UINT8 get_lru_in_cap(UINT16 id);

#endif