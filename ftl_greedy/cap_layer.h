#ifndef CAP_LAYER_H
#define CAP_LAYER_H

#include "lru.h"
#include "hash.h"

#define INVALID_ID 0xFFFF

#define cap 1

#define READ_POWER 30
#define WRITE_POWER 40
#define ERASE_POWER 30

#define LIMIT 320

enum {NAND_PAGE_PTREAD_TO_HOST, NAND_PAGE_PTREAD, NAND_PAGE_PTPROGRAM_FROM_HOST
	, NAND_PAGE_PTPROGRAM, NAND_PAGE_PROGRAM, NAND_PAGE_COPYBACK, NAND_BLOCK_ERASE};

typedef struct cap_node
{
	UINT8 type;//0:read,1:write,2:erase,3:copyback 
	UINT16 id;//not need
	UINT32 bank;// not need
	UINT32 vblock;
	UINT32 page_num;
	UINT32 sect_offset;
	UINT32 num_sectors;
	UINT32 buf_addr;
	UINT32 g_ftl_rw_buf_id;//g_ftl_write_buf_id or g_ftl_read_buf_id
	//only for copyback
	UINT32 src_vblock;
	UINT32 src_page;
	UINT32 dst_vblock;
	UINT32 dst_page;
//	UINT16 p_next_node;//ID
//	UINT16 p_prev_node;
	UINT16 issue_flag;
	UINT32 lpn;
}cap_node;

typedef struct record
{
	UINT32 bank;
	UINT32 id;
}record;

extern UINT32 g_ftl_read_buf_id;
extern UINT32 g_ftl_write_buf_id;
extern node LRU_list[NUM_LRUBUFFER];

UINT8 q_size[NUM_BANKS];
UINT8 q_head[NUM_BANKS];
UINT16 cap_head;
UINT16 cap_tail;
//UINT16 currentPower;
//UINT8 q_tail[NUM_BANKS];
//UINT16 cap_free_head[NUM_BANKS];
//UINT16 cap_free_tail[NUM_BANKS];

void init_cap_queue();
UINT16 cap_getFromFreeList(UINT8 bank);
void cap_recycleFreeList(UINT8 bank);
UINT16 cap_insert(UINT8 bank, struct cap_node node) ;
UINT32 cap_delete(UINT8 __bank);
inline UINT8 cap_is_full(UINT8 bank);
void RMW_merge(UINT8 bank);
//void insert_cap_list(UINT16 id);
UINT16 cap_queue_size();
UINT16 findCap(UINT32 lpn);
UINT16 currentPower();
UINT8 canEvict(UINT8 operation);
void set_bank_state(UINT8 bank, UINT8 operation);
inline UINT32 randNum();
UINT8 selectVictim();
#endif