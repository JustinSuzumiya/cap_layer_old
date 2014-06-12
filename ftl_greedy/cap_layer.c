#include "cap_layer.h"

void init_cap_queue()
{
	/*
		Q_DEPTH=4, NUM_BANKS=16
				bank0  bank1  bank2 ....  bank15
		d=0	    0      4      8		      60
		d=1	    1      5      9           61
		d=2	    2      6      10          62
		d=3	    3      7      11          63
	*/
	uart_printf("init_cap_queue");
	for(UINT8 bank=0 ; bank < NUM_BANKS; ++bank)
	{
		q_size[bank] = 0;
		q_head[bank] = bank*Q_DEPTH;
		//q_tail[bank] = bank*Q_DEPTH;
		//cap_free_head[bank] = bank * Q_DEPTH;
		//cap_free_tail[bank] = (bank + 1) * Q_DEPTH - 1;	
	}
	//cap_head = INVALID_ID;
	//cap_tail = INVALID_ID;
	/*
	mem_set_dram(P_LPN_ADDR, 0xFFFFFFFF, P_LPN_BYTES);
	mem_set_dram(P_BANK_STATE_ADDR, 0xFFFFFFFF, P_BANK_STATE_BYTES);
	
	for(int bank=0 ; bank < NUM_BANKS; ++bank)
	{
		uart_printf("bank = %d, cap_free_head= %d, cap_free_tail= %d", bank, cap_free_head[bank], cap_free_tail[bank]);
		uart_printf("bank = %d, q_head= %d, q_tail= %d", bank, q_head[bank], q_tail[bank]);
	}*/
}

//get free node and change the free_list
UINT16 cap_getFromFreeList(UINT8 bank)
{
	UINT16 new_id = (q_head[bank] + q_size[bank]) % Q_DEPTH + (bank * Q_DEPTH);
	++q_size[bank];
	return new_id;
}
void cap_recycleFreeList(UINT8 bank)
{
	//uart_printf("recycleFreeList id = %d", id);
	q_head[bank] = (q_head[bank] + 1) % Q_DEPTH + (bank * Q_DEPTH);
	--q_size[bank];
}

UINT16 cap_insert(UINT8 bank, struct cap_node node) 
{
	//uart_printf("cap_INSERT bank=%d",bank);
	
	//for(UINT8 bank = 0; bank < NUM_BANKS; ++bank )
	UINT8 victim;
	static UINT8 cnt = 0;
	//while((victim = selectVictim()) != 0xFF && currentPower() < LIMIT)
	{
		
	#if greedy == 0
		randomEvict();
		/*
		while(j-- && (LIMIT - currentPower())/WRITE_POWER )
			if((victim = selectVictim()) != 0xFF)
				cap_delete(victim);
		*/
	#else//greedy
		greedyEvict();
		/*
		while(j-- && q_size[buf[victim]] && (LIMIT - currentPower())/WRITE_POWER)
			cap_delete(buf[victim++]);
		*/
	#endif
		
	}
	if(q_size[bank] == Q_DEPTH)
	{
		//uart_printf("%d", q_size[bank]);
		//uart_printf("full");
		cap_delete(bank);
	}
	/*
	if(cap_is_full(bank))
	{
		//uart_printf("%d", q_size[bank]);
		cap_delete(bank);
	}*/
	//sort_cmd();
	UINT16 new_id = cap_getFromFreeList(bank); //cap_queue id
	UINT16 id; //LRU_list id
	/*
	if(node.type == NAND_PAGE_PROGRAM &&  //write
	   node.buf_addr >= LRU_ADDR && node.buf_addr < LRU_ADDR + LRU_BYTES)
	{
		id = findLRU(node.lpn);
		ASSERT( id != 0xFFFF);
		insert_cap_list(id);
	}*/
	//uart_printf("INSERT: id=%d",new_id);
	/***fill table***/
	write_dram_8(P_TYPE_ADDR + new_id*sizeof(UINT8), node.type);
	write_dram_8(P_BANK_ADDR + new_id*sizeof(UINT8), node.bank);
	//P_TYPE[new_id] = node.type;
	//P_BANK[new_id] = node.bank;
	if(node.type == NAND_PAGE_PTREAD_TO_HOST) //nand_page_ptread_to_host
	{
		//uart_printf("\nINSERT capping_read_id = %d\n", node.g_ftl_rw_buf_id);
		
		write_dram_32(P_VBLOCK_ADDR + new_id*sizeof(UINT32), node.vblock);
		write_dram_32(P_PAGE_NUM_ADDR + new_id*sizeof(UINT32), node.page_num);
		write_dram_32(P_SECT_OFFSET_ADDR + new_id*sizeof(UINT32), node.sect_offset);
		write_dram_32(P_NUM_SECTORS_ADDR + new_id*sizeof(UINT32), node.num_sectors);
		/*
		//need modify
		//write_dram_32(P_G_FTL_RW_BUF_ID_ADDR + new_id*sizeof(UINT32), node.g_ftl_rw_buf_id);
		P_VBLOCK[new_id] = node.vblock;
		P_PAGE_NUM[new_id] = node.page_num;
		P_SECT_OFFSET[new_id] = node.sect_offset;
		P_NUM_SECTORS[new_id] = node.num_sectors;
		*/
		//uart_printf("capping_read_id = %d", node.g_ftl_rw_buf_id);
	}
	else if(node.type == NAND_PAGE_PTREAD)//nand_page_ptread
	{
		
		write_dram_32(P_VBLOCK_ADDR + new_id*sizeof(UINT32), node.vblock);
		write_dram_32(P_PAGE_NUM_ADDR + new_id*sizeof(UINT32), node.page_num);
		write_dram_32(P_SECT_OFFSET_ADDR + new_id*sizeof(UINT32), node.sect_offset);
		write_dram_32(P_NUM_SECTORS_ADDR + new_id*sizeof(UINT32), node.num_sectors);
		write_dram_32(P_BUF_ADDR + new_id*sizeof(UINT32), node.buf_addr);
		write_dram_8(P_ISSUE_FLAG_ADDR + new_id*sizeof(UINT8), node.issue_flag);
		/*
		P_VBLOCK[new_id] = node.vblock;
		P_PAGE_NUM[new_id] = node.page_num;
		P_SECT_OFFSET[new_id] = node.sect_offset;
		P_NUM_SECTORS[new_id] = node.num_sectors;
		P_BUF[new_id] = node.buf_addr;
		P_ISSUE_FLAG[new_id] = node.issue_flag;
		*/
	}
	else if(node.type == NAND_PAGE_PTPROGRAM_FROM_HOST)//nand_page_ptprogram_from_host
	{
		
		write_dram_32(P_VBLOCK_ADDR + new_id*sizeof(UINT32), node.vblock);
		write_dram_32(P_PAGE_NUM_ADDR + new_id*sizeof(UINT32), node.page_num);
		write_dram_32(P_SECT_OFFSET_ADDR + new_id*sizeof(UINT32), node.sect_offset);
		write_dram_32(P_NUM_SECTORS_ADDR + new_id*sizeof(UINT32), node.num_sectors);
		/*
		P_VBLOCK[new_id] = node.vblock;
		P_PAGE_NUM[new_id] = node.page_num;
		P_SECT_OFFSET[new_id] = node.sect_offset;
		P_NUM_SECTORS[new_id] = node.num_sectors;
		*/
		//my_write_dram_32(P_G_FTL_RW_BUF_ID_ADDR + new_id*sizeof(UINT32), node.g_ftl_rw_buf_id);
		//uart_printf("capping_write_id = %d", node.g_ftl_rw_buf_id);
	}
	else if(node.type == NAND_PAGE_PTPROGRAM)//nand_page_ptprogram
	{
		
		write_dram_32(P_VBLOCK_ADDR + new_id*sizeof(UINT32), node.vblock);
		write_dram_32(P_PAGE_NUM_ADDR + new_id*sizeof(UINT32), node.page_num);
		write_dram_32(P_SECT_OFFSET_ADDR + new_id*sizeof(UINT32), node.sect_offset);
		write_dram_32(P_NUM_SECTORS_ADDR + new_id*sizeof(UINT32), node.num_sectors);
		write_dram_32(P_BUF_ADDR + new_id*sizeof(UINT32), node.buf_addr);
		/*
		P_VBLOCK[new_id] = node.vblock;
		P_PAGE_NUM[new_id] = node.page_num;
		P_SECT_OFFSET[new_id] = node.sect_offset;
		P_NUM_SECTORS[new_id] = node.num_sectors;
		P_BUF[new_id] = node.buf_addr;
		*/
	}
	else if(node.type == NAND_PAGE_PROGRAM)//nand_page_program
	{
		
		write_dram_32(P_VBLOCK_ADDR + new_id*sizeof(UINT32), node.vblock);
		write_dram_32(P_PAGE_NUM_ADDR + new_id*sizeof(UINT32), node.page_num);
		write_dram_32(P_BUF_ADDR + new_id*sizeof(UINT32), node.buf_addr);
		write_dram_16(P_ID_ADDR + new_id*sizeof(UINT16), node.id);
		/*
		P_VBLOCK[new_id] = node.vblock;
		P_PAGE_NUM[new_id] = node.page_num;
		P_BUF[new_id] = node.buf_addr;
		P_ID[new_id] = node.id;
		*/
		if(node.buf_addr >= LRU_ADDR && node.buf_addr < LRU_ADDR + LRU_BYTES)
			set_lru_in_cap(node.id, 1);
		//	LRU_list[node.id].in_cap_queue = 1;
	}
	else if(node.type == NAND_PAGE_COPYBACK)//nand_page_copyback
	{
		
		write_dram_32(P_SRC_VBLOCK_ADDR + new_id*sizeof(UINT32), node.src_vblock);
		write_dram_32(P_SRC_PAGE_ADDR + new_id*sizeof(UINT32), node.src_page);
		write_dram_32(P_DST_VBLOCK_ADDR + new_id*sizeof(UINT32), node.dst_vblock);
		write_dram_32(P_DST_PAGE_ADDR + new_id*sizeof(UINT32), node.dst_page);
		/*
		P_SRC_VBLOCK[new_id] = node.src_vblock;
		P_SRC_PAGE[new_id] = node.src_page;
		P_DST_VBLOCK[new_id] = node.dst_vblock;
		P_DST_PAGE[new_id] = node.dst_page;
		*/
	}
	else{//nand_block_erase
		write_dram_32(P_VBLOCK_ADDR + new_id*sizeof(UINT32), node.vblock);
		//P_VBLOCK[new_id] = node.vblock;
	}
	write_dram_32(P_LPN_ADDR + new_id*sizeof(UINT32), node.lpn);
	//P_LPN[new_id] = node.lpn;
	/***************/
	//uart_printf("=====cap_insert end====");
	return new_id;
}


UINT32 cap_delete(UINT8 __bank) //dispatch fn
{
	//uart_printf("=====cap_delete start====");
	//uart_printf("=====bank=%d====",__bank);
	//uart_printf("			DELETE: bank= %d",__bank);
	UINT8 type = 0;
	UINT32 bank = __bank;
	UINT32 vblock = 0;
	UINT32 page_num = 0;
	UINT32 sect_offset = 0;
	UINT32 num_sectors = 0;
	UINT32 buf_addr = 0;
	UINT32 g_ftl_rw_buf_id = 0;
	UINT32 src_vblock = 0;
	UINT32 src_page = 0;
	UINT32 dst_vblock = 0;
	UINT32 dst_page = 0;
	UINT16 p_next_node = 0;
	UINT16 p_prev_node = 0;
	UINT8 issue_flag = 0;
	UINT32 lpn = 0;
	UINT32 vpn = 0;
	UINT32 victimID = q_head[bank]; //cap_queue id
	UINT16 id; //LRU_list id
	//UINT32 victimID = q_tail[bank];
	static UINT8 in = 0;
	static UINT8 cnt = 0;
	if(q_size[bank] == 0)
		return;
	/*
	if(++cnt == 0)
	{
		if(in == 0)
		{
			ptimer_start_0();
			in = 1;
		}
		else
		{
			uart_printf("cap_delete time %u", ptimer_stop_and_uart_print_0()>>8);
			in = 0;
		}
	}*/
	
	/* now dispatch flash cmd , read the information of cmd first */
	
	type = read_dram_8(P_TYPE_ADDR + victimID*sizeof(UINT8));
	bank = read_dram_8(P_BANK_ADDR + victimID*sizeof(UINT8));
	lpn = read_dram_32(P_LPN_ADDR + victimID*sizeof(UINT32));
	/*
	type = P_TYPE[victimID];
	bank = P_BANK[victimID];
	lpn = P_LPN[victimID];
	*/
	if(type == NAND_PAGE_PTREAD_TO_HOST)
	{
		//uart_printf("dispatch_nand_page_ptread_to_host");
		
		vblock		= read_dram_32(P_VBLOCK_ADDR + victimID*sizeof(UINT32));
		page_num	= read_dram_32(P_PAGE_NUM_ADDR + victimID*sizeof(UINT32));
		sect_offset	= read_dram_32(P_SECT_OFFSET_ADDR + victimID*sizeof(UINT32));
		num_sectors	= read_dram_32(P_NUM_SECTORS_ADDR + victimID*sizeof(UINT32));
		/*
		vblock		= P_VBLOCK[victimID];
		page_num	= P_PAGE_NUM[victimID];
		sect_offset	= P_SECT_OFFSET[victimID];
		num_sectors	= P_NUM_SECTORS[victimID];
		*/
		//need modify
		//g_ftl_read_buf_id = read_dram_32(P_G_FTL_RW_BUF_ID_ADDR + victimID*sizeof(UINT32));
		//UINT32 temp	= read_dram_32(P_G_FTL_RW_BUF_ID_ADDR + victimID*sizeof(UINT32));
		
		//uart_printf("dispatch_bank=%d",bank);
		//uart_printf("dispatch_temp=%d",temp);
		UINT8 flag = 0;
		while(canEvict(0) != 1)
		{
			if(!flag)
			{
				uart_printf("ptread_to_host wait");
				flag = 1;
			}
				
		};
		nand_page_ptread_to_host(bank, vblock, page_num, sect_offset, num_sectors);
		set_bank_state(bank, 0);
	}
	else if(type == NAND_PAGE_PTREAD)
	{
		/*debug*/
		//uart_printf("dispatch_nand_page_ptread lpn: %d", lpn);
		
		vblock		= read_dram_32(P_VBLOCK_ADDR + victimID*sizeof(UINT32));
		page_num	= read_dram_32(P_PAGE_NUM_ADDR + victimID*sizeof(UINT32));
		sect_offset	= read_dram_32(P_SECT_OFFSET_ADDR + victimID*sizeof(UINT32));
		num_sectors	= read_dram_32(P_NUM_SECTORS_ADDR + victimID*sizeof(UINT32));
		buf_addr	= read_dram_32(P_BUF_ADDR + victimID*sizeof(UINT32));
		issue_flag	= read_dram_8(P_ISSUE_FLAG_ADDR + victimID*sizeof(UINT8));
		/*
		vblock		= P_VBLOCK[victimID];
		page_num	= P_PAGE_NUM[victimID];
		sect_offset	= P_SECT_OFFSET[victimID];
		num_sectors	= P_NUM_SECTORS[victimID];
		buf_addr	= P_BUF[victimID];
		issue_flag	= P_ISSUE_FLAG[victimID];
		*/
		//issue nand_page_ptread cmd
		while(canEvict(0) != 1);
		nand_page_ptread(bank, vblock, page_num, sect_offset, num_sectors, buf_addr, issue_flag);
		set_bank_state(bank, 0);
	}
	else if(type == NAND_PAGE_PTPROGRAM_FROM_HOST)//no use in write buffer version
	{
		//uart_printf("dispatch_nand_page_ptprogram_from_host");
		
		vblock		= read_dram_32(P_VBLOCK_ADDR + victimID*sizeof(UINT32));
		page_num	= read_dram_32(P_PAGE_NUM_ADDR + victimID*sizeof(UINT32));
		sect_offset	= read_dram_32(P_SECT_OFFSET_ADDR + victimID*sizeof(UINT32));
		num_sectors	= read_dram_32(P_NUM_SECTORS_ADDR + victimID*sizeof(UINT32));	
		/*
		vblock		= P_VBLOCK[victimID];
		page_num	= P_PAGE_NUM[victimID];
		sect_offset	= P_SECT_OFFSET[victimID];
		num_sectors	= P_NUM_SECTORS[victimID];
		*/
		//UINT32 temp2 = read_dram_32(P_G_FTL_RW_BUF_ID_ADDR + victimID*sizeof(UINT32));
		//uart_printf("dispatch__write_buf_id = %d",temp2);
		//issue nand_page_ptprogram_from_host cmd
		while(canEvict(1) != 1);
		nand_page_ptprogram_from_host(bank, vblock, page_num, sect_offset, num_sectors);
		set_bank_state(bank, 1);
	}
	else if(type == NAND_PAGE_PTPROGRAM)
	{	
		//uart_printf("dispatch_nand_page_ptprogram lpn: %d", lpn);
		
		vblock		= read_dram_32(P_VBLOCK_ADDR + victimID*sizeof(UINT32));
		page_num	= read_dram_32(P_PAGE_NUM_ADDR + victimID*sizeof(UINT32));
		sect_offset	= read_dram_32(P_SECT_OFFSET_ADDR + victimID*sizeof(UINT32));
		num_sectors	= read_dram_32(P_NUM_SECTORS_ADDR + victimID*sizeof(UINT32));
		buf_addr	= read_dram_32(P_BUF_ADDR + victimID*sizeof(UINT32));
		/*
		vblock		= P_VBLOCK[victimID];
		page_num	= P_PAGE_NUM[victimID];
		sect_offset	= P_SECT_OFFSET[victimID];
		num_sectors	= P_NUM_SECTORS[victimID];
		buf_addr	= P_BUF[victimID];
		*/
		//issue nand_page_ptprogram
		while(canEvict(1) != 1);
		nand_page_ptprogram(bank, vblock, page_num, sect_offset, num_sectors, buf_addr);
		set_bank_state(bank, 1);
	}
	else if(type == NAND_PAGE_PROGRAM)//nand_page_program, only issue from RMW write
	{
		//uart_printf("dispatch_nand_page_program lpn: %d", lpn);
		
		vblock		= read_dram_32(P_VBLOCK_ADDR + victimID*sizeof(UINT32));
		page_num	= read_dram_32(P_PAGE_NUM_ADDR + victimID*sizeof(UINT32));
		buf_addr	= read_dram_32(P_BUF_ADDR + victimID*sizeof(UINT32));
		/*
		vblock		= P_VBLOCK[victimID];
		page_num	= P_PAGE_NUM[victimID];
		buf_addr	= P_BUF[victimID];
		*/
		//uart_printf("merge start");
		//RMW buffer merge
		//if(!(_tst_bit_dram_more(P_CAP_BITMAP_ADDR + SECTORS_PER_PAGE * bank / 8)))
		//{	
			//RMW_merge(bank);
			//buf_addr = P_CAP_BUFFER_ADDR + bank * BYTES_PER_PAGE;
		//}
		//uart_printf("merge end");
		while(canEvict(1) != 1);
		nand_page_program(bank, vblock, page_num, buf_addr);
		set_bank_state(bank, 1);
		if(buf_addr >= LRU_ADDR && buf_addr < LRU_ADDR + LRU_BYTES)
		{
			id = read_dram_16(P_ID_ADDR + victimID*sizeof(UINT16));
			//id = P_ID[victimID];
			ASSERT(id != 0xFFFF);
			set_lru_in_cap(id, 0);
			//LRU_list[id].in_cap_queue = 0;
			//deleteHash(lpn);
		}
		//uart_printf("program end");
	}
	else if(type == NAND_PAGE_COPYBACK)
	{
		//uart_printf("dispatch_nand_page_copyback");
		
		src_vblock	= read_dram_32(P_SRC_VBLOCK_ADDR + victimID*sizeof(UINT32));
		src_page	= read_dram_32(P_SRC_PAGE_ADDR + victimID*sizeof(UINT32));
		dst_vblock	= read_dram_32(P_DST_VBLOCK_ADDR + victimID*sizeof(UINT32));
		dst_page	= read_dram_32(P_SRC_PAGE_ADDR + victimID*sizeof(UINT32));
		/*
		src_vblock	= P_SRC_VBLOCK[victimID];
		src_page	= P_SRC_PAGE[victimID];
		dst_vblock	= P_DST_VBLOCK[victimID];
		dst_page	= P_DST_PAGE[victimID];
		*/
		//issue copyback cmd
		while(canEvict(1) != 1);
		nand_page_copyback(bank, src_vblock, src_page, dst_vblock, dst_page);
		set_bank_state(bank, 1);
	}
	else //NAND_BLOCK_ERASE
	{
		//uart_printf("dispatch_nand_block_erase");
		vblock	= read_dram_32(P_VBLOCK_ADDR + victimID*sizeof(UINT32));
		//vblock = P_VBLOCK[victimID];
		//issue erase cmd
		while(canEvict(2) != 1);
		nand_block_erase(bank, vblock);
		set_bank_state(bank, 2);
	}
	write_dram_32(P_LPN_ADDR + victimID*sizeof(UINT32), 0xFFFFFFFF);
	//P_LPN[victimID] = 0xFFFFFFFF;
	/*
	if(type == NAND_PAGE_PROGRAM &&  //write
	   buf_addr >= LRU_ADDR && buf_addr < LRU_ADDR + LRU_BYTES)
	{
		id = findLRU(lpn);
		delete_cap_list(id);
		deleteHash(lpn);
		recycleFreeList(id);
	}*/
	cap_recycleFreeList(__bank); //change free_list pointer
	
	//uart_printf("=====cap_delete end====");
}
/*
void RMW_merge(UINT8 bank)
{
	UINT32 src, dst;
	//uart_printf("RMW_MERGE START");
	for(int i=0; i<SECTORS_PER_PAGE; i++)
	{

		if(tst_bit_dram(P_CAP_BITMAP_ADDR + bank * SECTORS_PER_PAGE / 8, i))
		{
			src = P_CAP_W_BUFFER_ADDR + bank*BYTES_PER_PAGE + i * BYTES_PER_SECTOR;
		}
		else
			src = P_CAP_R_BUFFER_ADDR + bank*BYTES_PER_PAGE + i * BYTES_PER_SECTOR;
		dst = P_CAP_BUFFER_ADDR + bank*BYTES_PER_PAGE + i * BYTES_PER_SECTOR;		
		mem_copy(dst, src, BYTES_PER_SECTOR);
		
	}
	//uart_printf("RMW_MERGE END");
}*/

inline UINT8 cap_is_full(UINT8 bank)
{
	//return free_head==free_tail;
	return q_size[bank] == Q_DEPTH;
}
/*
void insert_cap_list(UINT16 id)
{
	if(cap_head == INVALID_ID)
	{
		cap_head = id;
		cap_tail = id;
		LRU_list[id].next=id;
		LRU_list[id].prev=id;
	}
	else
	{
		LRU_list[cap_head].prev=id;
		LRU_list[cap_tail].next=id;
		
		LRU_list[id].next=cap_head;
		LRU_list[id].prev=cap_tail;
		
		cap_head=id;
	}
}

void delete_cap_list(UINT16 id)
{
	if(cap_queue_size() == 1)
	{
		cap_head = INVALID_ID;
		cap_tail = INVALID_ID;
	}
	else if(id == cap_head)
		cap_head = LRU_list[id].next;
	else if(id == cap_tail)
		cap_tail = LRU_list[id].prev;
		
	LRU_list[LRU_list[id].next].prev = LRU_list[id].prev;
	LRU_list[LRU_list[id].prev].next = LRU_list[id].next;
}*/
UINT16 cap_queue_size()
{
	UINT16 size = 0;
	for(int bank=0; bank != NUM_BANKS; ++bank)
		size += q_size[bank];
	return size;
}

UINT16 findCap(UINT32 lpn)
{
	for(UINT8 i=0; i!=P_MAX ; ++i)
		if(lpn == read_dram_32(P_LPN_ADDR + i*sizeof(UINT32)))
			return read_dram_16(P_ID_ADDR + i*sizeof(UINT16));
	/*
		if(lpn == P_LPN[i])
			return P_ID[i];
		
	*/
	
	return 0xFFFF;
}

UINT16 currentPower()
{
	UINT16 power = 0;
	for(UINT8 bank = 0; bank != NUM_BANKS; ++bank)
	{
		if( _BSP_FSM(REAL_BANK(bank)) != BANK_IDLE )
		{
			UINT8 operaion = read_dram_8(P_BANK_STATE_ADDR + bank*sizeof(UINT8));
			//UINT8 operaion = P_BANK_STATE[bank];
			if(operaion == 0) //read
				power += READ_POWER;
			else if(operaion == 1)//write
				power += WRITE_POWER;
			else if(operaion == 2)//erase
				power += ERASE_POWER;
		}
	}
	return power;
}

UINT8 canEvict(UINT8 operation)
{
	UINT8 power = 0;
	if(operation == 0)//read
		power = READ_POWER;
	else if(operation == 1)//write
		power = WRITE_POWER;
	else if(operation == 2)//erase
		power = ERASE_POWER;
	if(currentPower() + power > LIMIT)
		return 0; //can't evict
	else 
		return 1;
}

inline void set_bank_state(UINT8 bank, UINT8 operation)
{
	write_dram_8(P_BANK_STATE_ADDR + bank*sizeof(UINT8), operation);
	//P_BANK_STATE[bank] = operation;
}

inline UINT32 randNum()
{
	static UINT32 seed = 1103515245;
	const UINT32 c = 1103515245, d = 12345;
	seed = (c*seed + d);
	return seed & 0xFFFF;
}

/*
UINT8 selectVictim()
{
	UINT8 queueBank[NUM_BANKS];
	UINT8 j = 0;
	for(UINT8 i = 0 ; i != NUM_BANKS ; ++i)
	{
		if(q_size[i])
			queueBank[j++] = i;
	}
	if(j = 0)
		return 0xFF;
	UINT8 victim = randNum() % j ;
	return queueBank[victim];
}
*/

void randomEvict()
{
	UINT8 j = 0;
	UINT8 buf[NUM_BANKS];
	for(UINT8 i = 0 ; i != NUM_BANKS ; ++i)
	{
		if(q_size[i] && (_BSP_FSM(REAL_BANK(i)) == BANK_IDLE))
		{
			buf[j] = i;
			++j;
		}
	}
	for(UINT8 i = 0 ; i != j ; ++i)
		SWAP(UINT8, buf[i], buf[randNum() % j]);
	
	UINT8 idle = (LIMIT - currentPower())/WRITE_POWER ;
	if(j > idle)
		j = idle;
	
	for(UINT8 i = 0 ; i != j ; ++i)
		cap_delete(buf[i]);
}

void rrEvict()
{
	static UINT8 index = 0;
	UINT8 j = 0;
	UINT8 buf[NUM_BANKS];

	UINT8 idle = (LIMIT - currentPower())/WRITE_POWER ;
	
	for(UINT8 i = 0 ; i != NUM_BANKS ; ++i)
	{
		if(q_size[i] && (_BSP_FSM(REAL_BANK(i)) == BANK_IDLE))
			++j;
	}
	
	for(UINT8 i = 0 ; i != NUM_BANKS ; ++i)
	{
		if(q_size[index] && (_BSP_FSM(REAL_BANK(index)) == BANK_IDLE))
		{
			cap_delete(index);
			index = (index + 1) % (NUM_BANKS);
			if(--j == 0)
				break;
		}
	}
	
}

void greedyEvict()
{
	UINT8 j = 0;
	UINT8 buf[NUM_BANKS], buf_size[NUM_BANKS];
	static UINT8 cnt = 0;
	for(UINT8 i = 0 ; i != NUM_BANKS ; ++i)
	{
		if(q_size[i] && (_BSP_FSM(REAL_BANK(i)) == BANK_IDLE))
		{
			buf[j] = i;
			buf_size[j] = g_last_page[i].bank_size;
			++j;
		}
	}
	if(j == 0)
		return ;
	for(UINT8 i = 0 ; i != j - 1 ; ++i)
	{	
		UINT8 swapped = 0;
		for(UINT8 k = j - 1 ; k != i ; --k)
			if(buf_size[k] > buf_size[k-1])
			{
				SWAP(UINT8, buf_size[k], buf_size[k-1]);
				SWAP(UINT8, buf[k], buf[k-1]);
				/*
				UINT8 tmp = buf_size[k];
				buf_size[k] = buf_size[k-1];
				buf_size[k-1] = tmp;

				tmp = buf[k];
				buf[k] = buf[k-1];
				buf[k-1 ] = tmp;
				*/
				swapped = 1;
			}
		if(!swapped)
			break;
	}
	/*
	if(j > 1 && cnt == 0)
	{
		for(UINT8 i = 0 ; i != j ; ++i)
			uart_printf("%u: %u", buf[i], buf_size[i]);
		uart_printf("\n");
	}*/
	UINT8 idle = (LIMIT - currentPower())/WRITE_POWER;
	if(j > idle)
		j = idle;
	for(UINT8 i = 0 ; i != j ; ++i)
	{
		cap_delete(buf[i]);
	}
	++cnt;
}

