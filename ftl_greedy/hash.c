#include "lru.h"
#include "hash.h"

void initHASH()
{
	uart_printf("HASH initialize");
}

void insertHash(UINT32 const lpn, UINT16 id)
{
	write_dram_16(HASH_TABLE_ADDR + lpn * sizeof(UINT16) , id );
}

UINT16 findHash(UINT32 const lpn)
{
	return read_dram_16(HASH_TABLE_ADDR + lpn * sizeof(UINT16) );
}

void deleteHash(UINT32 const lpn)
{
	write_dram_16(HASH_TABLE_ADDR + lpn * sizeof(UINT16) , 0xFFFF );
}

void set_num_bank(UINT32 const lpn, UINT32 const bank)
{
	write_dram_32(bank_list_ADDR + lpn * sizeof(UINT32), bank) ;
}

UINT32 get_num_bank(UINT32 const lpn)
{
	return read_dram_32(bank_list_ADDR + lpn * sizeof(UINT32)) ; 
}
void set_lru_next(UINT16 id, UINT16 value)
{
	ASSERT(id < NUM_LRUBUFFER);
	write_dram_16(lru_list_next_ADDR + (id) * sizeof(UINT16), value);
}
void set_lru_prev(UINT16 id, UINT16 value)
{
	ASSERT(id < NUM_LRUBUFFER);
	write_dram_16(lru_list_prev_ADDR + (id) * sizeof(UINT16), value);
}
void set_lru_lpn(UINT16 id,UINT32 lpn)
{
	ASSERT(id < NUM_LRUBUFFER);
	write_dram_32(lru_list_lpn_ADDR + (id)*sizeof(UINT32), lpn);
}

UINT16 get_lru_next(UINT16 id)
{
	ASSERT(id < NUM_LRUBUFFER);
	return read_dram_16(lru_list_next_ADDR + id * sizeof(UINT16));
}
UINT16 get_lru_prev(UINT16 id)
{
	ASSERT(id < NUM_LRUBUFFER);
	return read_dram_16(lru_list_prev_ADDR + (id)* sizeof(UINT16));
}
UINT32 get_lru_lpn(UINT16 id)
{
	ASSERT(id < NUM_LRUBUFFER);
	return read_dram_32(lru_list_lpn_ADDR + (id) * sizeof(UINT32));
}

void insertBlk(UINT32 const blk_num, UINT16 head, UINT16 tail)
{
	//uart_printf("insertBlk");
	/*
	UINT16 key = getBlk(blk_num);
	
	write_dram_32(BLK_TABLE_ADDR + key * sizeof(UINT32) * 2 , blk_num );
	write_dram_32(BLK_TABLE_ADDR + key * sizeof(UINT32) * 2 + sizeof(UINT32), head<<16 | tail );
	*/
	//write_dram_32(BLK_TABLE_ADDR + ((blk_num>>16) * VBLKS_PER_BANK + (blk_num&0xFFFF)) * sizeof(UINT32), head<<16 | tail );
	write_dram_32(BLK_TABLE_ADDR + blk_num * sizeof(UINT32), head<<16 | tail );
}

UINT32 findBlk(UINT32 const blk_num)
{
/*
	UINT16 _blk_num = blk_num % BLK_TABLE_SIZE;
	UINT16 key = blk_num % BLK_TABLE_SIZE;
	
	do
	{
		if( read_dram_32(BLK_TABLE_ADDR + key * sizeof(UINT32) * 2) == blk_num )
			return read_dram_32(BLK_TABLE_ADDR + key * sizeof(UINT32) * 2 + sizeof(UINT32) ); //return head and tail
			
		key = (key+2) % BLK_TABLE_SIZE ; 
		//while(key>=100);
	}while( key != _blk_num );
	
	//uart_printf("Hash not found");
	return INVALID_INDEX;
*/
	//return read_dram_32(BLK_TABLE_ADDR + ((blk_num>>16) * VBLKS_PER_BANK + (blk_num&0xFFFF)) * sizeof(UINT32) );
	return read_dram_32(BLK_TABLE_ADDR + blk_num * sizeof(UINT32) );
}

void deleteBlk(UINT32 const blk_num)
{
/*
	UINT16 key = blk_num % BLK_TABLE_SIZE;

	while( read_dram_32(BLK_TABLE_ADDR + key * sizeof(UINT32) * 2) != blk_num )
		key = (key+2)%BLK_TABLE_SIZE ;
		
	write_dram_32(BLK_TABLE_ADDR + key * sizeof(UINT32) * 2 , INVALID_INDEX );
	write_dram_32(BLK_TABLE_ADDR + key * sizeof(UINT32) * 2 + sizeof(UINT32) , INVALID_INDEX );
*/
	//write_dram_32(BLK_TABLE_ADDR + ((blk_num>>16) * VBLKS_PER_BANK + (blk_num&0xFFFF)) * sizeof(UINT32), INVALID_INDEX);
	write_dram_32(BLK_TABLE_ADDR + blk_num * sizeof(UINT32), INVALID_INDEX);
}

void updateBlk(UINT32 const blk_num, UINT16 head, UINT16 tail)
{
/*
	//uart_printf("updateBlk");
	UINT16 key = blk_num % BLK_TABLE_SIZE;	
	while( read_dram_32(BLK_TABLE_ADDR + key * sizeof(UINT32) * 2) != blk_num )
		key = (key+2) % BLK_TABLE_SIZE ; 
		
	//write_dram_32(BLK_TABLE_ADDR + key * sizeof(UINT32) * 2 , blk_num );
	write_dram_32(BLK_TABLE_ADDR + key * sizeof(UINT32) * 2 + sizeof(UINT32), head<<16 | tail );
*/
	//write_dram_32(BLK_TABLE_ADDR + ((blk_num>>16) * VBLKS_PER_BANK + (blk_num&0xFFFF)) * sizeof(UINT32), head<<16 | tail );
	write_dram_32(BLK_TABLE_ADDR + blk_num * sizeof(UINT32), head<<16 | tail );
}
UINT16 getBlk(UINT32 const blk_num)
{
	UINT16 key = blk_num % BLK_TABLE_SIZE ;
	
	while(read_dram_32(BLK_TABLE_ADDR + key * sizeof(UINT32) * 2 ) != INVALID_INDEX)
	{
		key = (key+2)%BLK_TABLE_SIZE ; 
	}
	return key;
}

void set_lru_bank_next(UINT16 id, UINT16 value)
{
	ASSERT(id < NUM_LRUBUFFER);
	write_dram_16(lru_list_bank_next_ADDR + (id) * sizeof(UINT16),value);
}
void set_lru_bank_prev(UINT16 id, UINT16 value)
{
	ASSERT(id < NUM_LRUBUFFER);
	write_dram_16(lru_list_bank_prev_ADDR + (id) * sizeof(UINT16),value);
}
UINT16 get_lru_bank_next(UINT16 id)
{
	return read_dram_16(lru_list_bank_next_ADDR+ (id) * sizeof(UINT16));
}
UINT16 get_lru_bank_prev(UINT16 id)
{
	return read_dram_16(lru_list_bank_prev_ADDR+ (id) * sizeof(UINT16));
}

void set_lru_in_cap(UINT16 id, UINT8 value)
{
	ASSERT(id < NUM_LRUBUFFER);
	write_dram_8(lru_list_in_cap_ADDR + id * sizeof(UINT8), value);
}
UINT8 get_lru_in_cap(UINT16 id)
{
	return read_dram_8(lru_list_in_cap_ADDR + id * sizeof(UINT8));
}