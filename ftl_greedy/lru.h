#ifndef LRU_H
#define LRU_H

#define INVALID_ID 0xFFFF

typedef struct node
{
	UINT16 prev;
	UINT16 next;
	UINT32 lpn;
	UINT16 bank_prev;
	UINT16 bank_next;
}node;
typedef struct bank_list
{
	UINT16 bank_head;
	UINT16 bank_tail; 
	UINT16 bank_size;
}bank_list;
//node LRU_list[NUM_LRUBUFFER];
bank_list      g_last_page[NUM_BANKS];
//node free_list[NUM_LRUBUFFER+1]; // 最後一個node用來判度full
UINT16 LRU_size;

UINT16 LRU_head;
UINT16 LRU_tail;
UINT16 free_head;
UINT16 free_tail;
UINT16 victim_blk;
UINT16 victim_bank;


void initLRU();
UINT16 getFromFreeList();
void recycleFreeList(UINT16 const id);

UINT16 insertLRU(UINT32 const lpn, UINT16 id); // insert at id's next node
UINT16 insertHEAD_bankLRU(UINT32 const lpn) ;
UINT16 insertHeadLRU(UINT32 const lpn);
UINT32 deleteLRU(UINT32 const lpn);
UINT32 deleteTailLRU();

UINT32 delete_bankLRU(UINT32 const lpn);
UINT16 findLRU(UINT32 const lpn);
UINT16 updateLRU(UINT32 const lpn);
UINT8 fullLRU();
UINT8 emptyLRU();

void LRUBlkPromote(UINT16 head, UINT16 tail);
void LRUBlkDemote(UINT16 head, UINT16 tail);
void LRUCut(UINT16 head, UINT16 tail);

UINT16 get_VBLK();
UINT16 set_VBLK(UINT16 blk_num);
void check_LRU();

UINT16 insertTAIL_bankLRU(UINT32 const lpn);
#endif