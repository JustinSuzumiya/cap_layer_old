#include "lru.h"
#include "hash.h"
#include "cap_layer.h"

void initLRU()
{
	UINT16 i;
	for(i=0;i!=NUM_LRUBUFFER;++i)
	{
		set_lru_prev(i, i-1);
		set_lru_next(i, i+1);
		set_lru_lpn(i, INVALID_ID);
		set_lru_in_cap(i, 0);
		/*
		LRU_list[i].prev = i-1;
		LRU_list[i].next = i+1;
		LRU_list[i].lpn  = INVALID_ID ;
		*/
	}
	LRU_head = INVALID_ID;
	LRU_tail = INVALID_ID;
	LRU_size = 0;
	
	set_lru_prev(0,NUM_LRUBUFFER-1);
	set_lru_next((NUM_LRUBUFFER - 1)  , 0);
	free_head = 0;
	free_tail = NUM_LRUBUFFER - 1;
	uart_printf("LRU initialize");
}

UINT16 getFromFreeList()
{
	UINT16 id = free_head;
	// get free node from free_head
	/*
	LRU_list[LRU_list[free_head].next].prev = LRU_list[free_head].prev;
	LRU_list[LRU_list[free_head].prev].next = LRU_list[free_head].next;
	*/
	while(get_lru_in_cap(free_head) != 0)
	{
		free_head = get_lru_next(free_head);
		free_tail = get_lru_next(free_tail);
	}
	set_lru_prev(get_lru_next(free_head), get_lru_prev(free_head));
	set_lru_next(get_lru_prev(free_head), get_lru_next(free_head));
	if(LRU_size == (NUM_LRUBUFFER - 1))
	{
		free_head = INVALID_ID; 
		free_tail = INVALID_ID ;
	}
	else
	{
		//free_head=LRU_list[free_head].next;
		free_head = get_lru_next(free_head);//LRU_list[free_head].page_next;
	}
	
	++LRU_size;
	//uart_printf("after insert size:%d", LRU_size);
	return id;
}
void recycleFreeList(UINT16 const id)
{
	if(LRU_size == NUM_LRUBUFFER)
	{
		free_head = id ; 
		free_tail = id ;
	}
	UINT16 new_id = id; // add free node to head
	set_lru_next(free_tail,new_id);
	set_lru_prev(free_head,new_id);
	set_lru_prev(new_id,free_tail);
	set_lru_next(new_id,free_head);
	/*
	LRU_list[free_tail].next=new_id;
	LRU_list[free_head].prev=new_id;
	LRU_list[new_id].prev=free_tail;
	LRU_list[new_id].next=free_head;
	*/
	free_tail = new_id;
	
	--LRU_size;
	//uart_printf("after delete size:%d", LRU_size);
}
/*
UINT16 insertLRU(UINT32 const lpn, UINT16 id) // insert node in front of id   
{
	UINT16 new_id = getFromFreeList();
	//insertHash(lpn, new_id ); 在ftl.c處理
	
	if( emptyLRU() ) // LRU list is empty
	{
		LRU_head=new_id;
		LRU_tail=new_id;
		/*
		LRU_list[new_id].next=new_id;
		LRU_list[new_id].prev=new_id;
		
		
			set_lru_next(new_id,new_id);
			set_lru_prev(new_id,new_id);
	}
	else
	{
		LRU_list[LRU_list[id].prev].next = new_id;
		LRU_list[new_id].prev = LRU_list[id].prev;
		
		LRU_list[new_id].next = id;
		LRU_list[id].prev = new_id;
		
		if(id == LRU_head)
			LRU_head = new_id;
	}

	LRU_list[new_id].lpn = lpn;
	
	return new_id; //return insert id
}
*/
UINT16 insertHeadLRU(UINT32 const lpn) 
{
	//uart_printf("===insertLRU===lpn = %d",lpn);

	//if(!fullLRU()) // have free node 在ftl.c處理
	{	
		UINT16 new_id = getFromFreeList();
		//insertHash(lpn, new_id ); 在ftl.c處理
		
		if( LRU_size == 1) // LRU list is empty
		{
			LRU_head=new_id;
			LRU_tail=new_id;
			/*
			LRU_list[new_id].next=new_id;
			LRU_list[new_id].prev=new_id;
			*/
			
			set_lru_next(new_id,new_id);
			set_lru_prev(new_id,new_id);
			//LRU_list[new_id]=new_id;
		}
		else
		{
			/*
			LRU_list[LRU_head].prev=new_id;
			LRU_list[LRU_tail].next=new_id;
			
			LRU_list[new_id].next=LRU_head;
			LRU_list[new_id].prev=LRU_tail;
			*/
			set_lru_prev(LRU_head,new_id);
			set_lru_next(LRU_tail,new_id);

			set_lru_next(new_id,LRU_head);
			set_lru_prev(new_id,LRU_tail);
			LRU_head = new_id;
		}
		set_lru_lpn(new_id, lpn);
		//LRU_list[new_id].lpn = lpn;
		
		//uart_printf("insert lpn:%d at id:%d", lpn , findLRU(lpn));
		
		return new_id; //return insert id
	}
	/*
	else // LRU list full, evict from LRU_tail
	{
		return deleteLRU(LRU_tail);
	}*/
}

UINT16 insertTailLRU(UINT32 const lpn) 
{
	//uart_printf("===insertLRU===lpn = %d",lpn);

	//if(!fullLRU()) // have free node 在ftl.c處理
	{	
		UINT16 new_id = getFromFreeList();
		//insertHash(lpn, new_id ); 在ftl.c處理
		
		if( LRU_size == 1) // LRU list is empty
		{
			LRU_head=new_id;
			LRU_tail=new_id;
			/*
			LRU_list[new_id].next=new_id;
			LRU_list[new_id].prev=new_id;
			*/
			
			set_lru_next(new_id,new_id);
			set_lru_prev(new_id,new_id);
			//LRU_list[new_id]=new_id;
		}
		else
		{
			/*
			LRU_list[LRU_head].prev=new_id;
			LRU_list[LRU_tail].next=new_id;
			
			LRU_list[new_id].next=LRU_head;
			LRU_list[new_id].prev=LRU_tail;
			*/
			set_lru_prev(LRU_head,new_id);
			set_lru_next(LRU_tail,new_id);

			set_lru_next(new_id,LRU_head);
			set_lru_prev(new_id,LRU_tail);
			LRU_tail = new_id;
		}
		set_lru_lpn(new_id, lpn);
		//LRU_list[new_id].lpn = lpn;
		
		//uart_printf("insert lpn:%d at id:%d", lpn , findLRU(lpn));
		
		return new_id; //return insert id
	}
}

UINT16 insertHEAD_bankLRU(UINT32 const lpn)
{
	UINT32 bank = get_num_bank(lpn); 
	UINT16 id   = findLRU(lpn);
	ASSERT(id != 0xFFFF);
	if(g_last_page[bank].bank_size == 0)
	{
		g_last_page[bank].bank_head = id;
		g_last_page[bank].bank_tail = id;

		/*
		LRU_list[id].bank_next=id;
		LRU_list[id].bank_prev=id;
		*/
		set_lru_bank_next(id,id);
		set_lru_bank_prev(id,id);
	}
	else
	{
		/*
		LRU_list[g_last_page[bank].bank_head].bank_prev=id;
		LRU_list[g_last_page[bank].bank_tail].bank_next=id;
		*/
		set_lru_bank_prev(g_last_page[bank].bank_head,id);
		set_lru_bank_next(g_last_page[bank].bank_tail,id);
		/*
		LRU_list[id].bank_next=g_last_page[bank].bank_head;
		LRU_list[id].bank_prev=g_last_page[bank].bank_tail;
		*/
		set_lru_bank_next(id,g_last_page[bank].bank_head);
		set_lru_bank_prev(id,g_last_page[bank].bank_tail);
		
		g_last_page[bank].bank_head = id;
	}
	g_last_page[bank].bank_size ++ ;
}
UINT16 insertTAIL_bankLRU(UINT32 const lpn)
{
	UINT32 bank = get_num_bank(lpn); 
	UINT16 id   = findLRU(lpn);
	ASSERT(id   != 0xFFFF);
	if(g_last_page[bank].bank_size == 0)
	{
		g_last_page[bank].bank_head = id;
		g_last_page[bank].bank_tail = id;
	
		set_lru_bank_next(id,id);
		set_lru_bank_prev(id,id);
	}
	else
	{
		set_lru_bank_prev(g_last_page[bank].bank_head, id);
		set_lru_bank_next(g_last_page[bank].bank_tail, id);
	
		set_lru_bank_next(id, g_last_page[bank].bank_head);
		set_lru_bank_prev(id, g_last_page[bank].bank_tail);
		g_last_page[bank].bank_tail = id;
	}
	g_last_page[bank].bank_size ++ ;
}

UINT32 deleteLRU(UINT32 const victim_current)
{
	UINT32 victimID = victim_current;
	UINT32 victimLPN = get_lru_lpn(victimID);//LRU_list[victimID].lpn;
	//ASSERT(victimID != 0xFFFF);
	//if(LRU_head == LRU_tail) // LRU list becomes empty
	if(LRU_size == 1)
	{
		LRU_head = INVALID_ID;
		LRU_tail = INVALID_ID;
	}
	else if(victimID == LRU_head)
		LRU_head = get_lru_next(victimID);
	else if(victimID == LRU_tail)
		LRU_tail = get_lru_prev(victimID);

	set_lru_prev(get_lru_next(victimID),get_lru_prev(victimID));
	//LRU_list[LRU_list[victimID].next].prev = LRU_list[victimID].prev;
	set_lru_next(get_lru_prev(victimID),get_lru_next(victimID));
	//LRU_list[LRU_list[victimID].prev].next = LRU_list[victimID].next;	
	
	#if RMW_flag == 2
	_clr_bit_dram_all(wbuf_vpage_ADDR + victimID * SECTORS_PER_PAGE / 8);
	#endif
	
	recycleFreeList(victimID);
	return victimLPN; // for delete hash table entry
}

UINT32 delete_bankLRU(UINT32 const victim_current)
{
	UINT32 victimID = victim_current;
	UINT32 lpn = get_lru_lpn(victimID);
	UINT32 bank = get_num_bank(lpn);
	//ASSERT(victimID != 0xFFFF);
	if(g_last_page[bank].bank_size == 1)
	{
		g_last_page[bank].bank_head =INVALID_ID ; 
		g_last_page[bank].bank_tail =INVALID_ID ; 
	
	}
	else if(victimID == g_last_page[bank].bank_head)
		g_last_page[bank].bank_head = get_lru_bank_next(victimID);//LRU_list[victimID].bank_next;
	else if(victimID == g_last_page[bank].bank_tail)
		g_last_page[bank].bank_tail = get_lru_bank_prev(victimID);//LRU_list[victimID].bank_prev ;
	/*
	LRU_list[LRU_list[victimID].bank_next].bank_prev = LRU_list[victimID].bank_prev;
	LRU_list[LRU_list[victimID].bank_prev].bank_next = LRU_list[victimID].bank_next;
	*/
	set_lru_bank_prev(get_lru_bank_next(victimID), get_lru_bank_prev(victimID));	
	set_lru_bank_next(get_lru_bank_prev(victimID), get_lru_bank_next(victimID));

	g_last_page[bank].bank_size-- ; 
	
}


UINT16 findLRU(UINT32 const lpn)
{
	return findHash(lpn);
}


UINT16 updateLRU(UINT32 const lpn )
{
	//uart_printf("===updateLRU===");
	UINT16 id = findLRU(lpn);
	//uart_printf("endupdate");
	ASSERT(id != 0xFFFF);
	if(id == LRU_head) // the node is head, no need to update
		return id;
	else if(id == LRU_tail)
	{
		
		LRU_tail = get_lru_prev(LRU_tail);
	}
	
	set_lru_prev(get_lru_next(id),get_lru_prev(id));
	set_lru_next(get_lru_prev(id),get_lru_next(id));
	
	set_lru_next(id,LRU_head);
	set_lru_prev(id,LRU_tail);

	set_lru_prev(LRU_head,id);
	set_lru_next(LRU_tail,id);
	
	LRU_head = id ;

	return id;
}

UINT16 updateBANK(UINT32 const lpn )
{
	UINT16 id = findLRU(lpn);
	UINT32 bank = get_num_bank(lpn);
	ASSERT( id != 0xFFFF);
	if(id == g_last_page[bank].bank_head) //LRU_head) // the node is head, no need to update
		return id;
	else if(id == g_last_page[bank].bank_tail) //LRU_tail)
	{	
		//LRU_tail = LRU_list[LRU_tail].prev;
	g_last_page[bank].bank_tail = get_lru_bank_prev(id);//LRU_list[id].bank_prev ; 
	}
	
	set_lru_bank_prev(get_lru_bank_next(id),get_lru_bank_prev(id));
	set_lru_bank_next(get_lru_bank_prev(id),get_lru_bank_next(id));
	set_lru_bank_next(id,g_last_page[bank].bank_head);
	set_lru_bank_prev(id,g_last_page[bank].bank_tail);
	set_lru_bank_prev(g_last_page[bank].bank_head,id);
	set_lru_bank_next(g_last_page[bank].bank_tail,id);
	
	g_last_page[bank].bank_head = id ;

	return id;
}

UINT8 fullLRU()
{
	return LRU_size == real_size;
}
UINT8 emptyLRU()
{
	return LRU_size == 1;
}

UINT16 get_VBLK()
{
	return victim_blk;
}
UINT16 set_VBLK(UINT16 blk_num)
{
	return victim_blk = blk_num;
}
UINT16 get_VBANK()
{
	return victim_bank;
}
UINT16 set_VBANK(UINT16 bank_num)
{
	return victim_bank = bank_num;
}