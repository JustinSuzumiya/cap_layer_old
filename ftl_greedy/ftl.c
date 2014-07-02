// Copyright 2011 INDILINX Co., Ltd.
//
// This file is part of Jasmine.
//
// Jasmine is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// Jasmine is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with Jasmine. See the file COPYING.
// If not, see <http://www.gnu.org/licenses/>.
//
// GreedyFTL source file
//
// Author; Sang-Phil Lim (SKKU VLDB Lab.)
//
// - support POR
//  + fixed metadata area (Misc. block/Map block)
//  + logging entire FTL metadata when each ATA commands(idle/ready/standby) was issued
//

#include "jasmine.h"
#include "lru.c"
#include "hash.c"
#include "cap_layer.c"
//----------------------------------
// macro
//----------------------------------

#define VC_MAX              0xCDCD
#define MISCBLK_VBN         0x1 // vblock #1 <- misc metadata
#define MAPBLKS_PER_BANK    (((PAGE_MAP_BYTES / NUM_BANKS) + BYTES_PER_PAGE - 1) / BYTES_PER_PAGE)
#define META_BLKS_PER_BANK  (1 + 1 + MAPBLKS_PER_BANK) // include block #0, misc block

// the number of sectors of misc. metadata info.
#define NUM_MISC_META_SECT  ((sizeof(misc_metadata) + BYTES_PER_SECTOR - 1)/ BYTES_PER_SECTOR)
#define NUM_VCOUNT_SECT     ((VBLKS_PER_BANK * sizeof(UINT16) + BYTES_PER_SECTOR - 1) / BYTES_PER_SECTOR)

//----------------------------------
// metadata structure
//----------------------------------
typedef struct _ftl_statistics
{
//    UINT32 gc_cnt;
//    UINT32 page_wcount; // page write count
	UINT32 erase_cnt;
	UINT32 user_write;
	UINT32 flash_write;
} ftl_statistics;

typedef struct _misc_metadata
{
    UINT32 cur_write_vpn; // physical page for new write
    UINT32 cur_miscblk_vpn; // current write vpn for logging the misc. metadata
    UINT32 cur_mapblk_vpn[MAPBLKS_PER_BANK]; // current write vpn for logging the age mapping info.
    UINT16 gc_vblock; // vblock number for garbage collection
    UINT32 free_blk_cnt; // total number of free block count
    //UINT32 lpn_list_of_cur_vblock[PAGES_PER_BLK]; // logging lpn list of current write vblock for GC
    //UINT32 lpn_list_of_gc_vblock[PAGES_PER_BLK];
    UINT16 gc_backup;
    UINT16 gc_backup_for_gc;
    UINT8 cur_freevpn ;
    UINT8 gc_vblock_vcount;
    UINT8  gcing;
    UINT8  gc_over;
    UINT16 victim_block ;
    UINT16 reserve_vcount ;
    UINT32 src_page ;
//    UINT32 target_block ;
//   UINT32 target_page ;
//    UINT8  you_need_read ;
} misc_metadata; // per bank

//----------------------------------
// FTL metadata (maintain in SRAM)
//----------------------------------
static misc_metadata  g_misc_meta[NUM_BANKS];
static ftl_statistics g_ftl_statistics[NUM_BANKS];
static UINT32		  g_bad_blk_count[NUM_BANKS];


// SATA read/write buffer pointer id
UINT32 				  g_ftl_read_buf_id;
UINT32 				  g_ftl_write_buf_id;

//----------------------------------
// NAND layout
//----------------------------------
// block #0: scan list, firmware binary image, etc.
// block #1: FTL misc. metadata
// block #2 ~ #31: page mapping table
// block #32: a free block for gc
// block #33~: user data blocks

//----------------------------------
// macro functions
//----------------------------------
#define is_full_all_blks(bank)  (g_misc_meta[bank].free_blk_cnt == 3)
#define inc_full_blk_cnt(bank)  (g_misc_meta[bank].free_blk_cnt--)
#define dec_full_blk_cnt(bank)  (g_misc_meta[bank].free_blk_cnt++)
#define inc_mapblk_vpn(bank, mapblk_lbn)    (g_misc_meta[bank].cur_mapblk_vpn[mapblk_lbn]++)
#define inc_miscblk_vpn(bank)               (g_misc_meta[bank].cur_miscblk_vpn++)

// page-level striping technique (I/O parallelism)
//#define get_num_bank(lpn)             ((lpn) % NUM_BANKS)
#define get_bad_blk_cnt(bank)         (g_bad_blk_count[bank])
#define get_cur_write_vpn(bank)       (g_misc_meta[bank].cur_write_vpn)
#define set_new_write_vpn(bank, vpn)  (g_misc_meta[bank].cur_write_vpn = vpn)
#define get_gc_vblock(bank)           (g_misc_meta[bank].gc_vblock)
#define set_gc_vblock(bank, vblock)   (g_misc_meta[bank].gc_vblock = vblock)
//#define set_lpn(bank, page_num, lpn)  (g_misc_meta[bank].lpn_list_of_cur_vblock[page_num] = lpn)
//#define get_lpn(bank, page_num)       (g_misc_meta[bank].lpn_list_of_cur_vblock[page_num])
#define set_lpn(bank, page_num, lpn)	write_dram_32(LPN_LIST_OF_CUR_VBLOCK_ADDR + (bank*PAGES_PER_BLK+page_num) * sizeof(UINT32), lpn)
#define get_lpn(bank, page_num)		read_dram_32(LPN_LIST_OF_CUR_VBLOCK_ADDR + (bank*PAGES_PER_BLK+page_num) * sizeof(UINT32))
#define get_miscblk_vpn(bank)         (g_misc_meta[bank].cur_miscblk_vpn)
#define set_miscblk_vpn(bank, vpn)    (g_misc_meta[bank].cur_miscblk_vpn = vpn)
#define get_mapblk_vpn(bank, mapblk_lbn)      (g_misc_meta[bank].cur_mapblk_vpn[mapblk_lbn])
#define set_mapblk_vpn(bank, mapblk_lbn, vpn) (g_misc_meta[bank].cur_mapblk_vpn[mapblk_lbn] = vpn)
#define CHECK_LPAGE(lpn)              ASSERT((lpn) < NUM_LPAGES)
#define CHECK_VPAGE(vpn)              ASSERT((vpn) < (VBLKS_PER_BANK * PAGES_PER_BLK))

//----------------------------------
// FTL internal function prototype
//----------------------------------
static void   format(void);
static void   write_format_mark(void);
static void   sanity_check(void);
static void   load_pmap_table(void);
static void   load_misc_metadata(void);
static void   init_metadata_sram(void);
static void   load_metadata(void);
static void   logging_pmap_table(void);
static void   logging_misc_metadata(void);
static void   write_page(UINT32 const lpn, UINT32 const sect_offset, UINT32 const num_sectors);
static void   set_vpn(UINT32 const lpn, UINT32 const vpn);
static void   set_fake_vpn(UINT32 const lpn, UINT32 const vpn);
static UINT32 garbage_collection(UINT32 const bank);
static void   set_vcount(UINT32 const bank, UINT32 const vblock, UINT32 const vcount);
static BOOL32 is_bad_block(UINT32 const bank, UINT32 const vblock);
static BOOL32 check_format_mark(void);
static UINT32 get_vcount(UINT32 const bank, UINT32 const vblock);
static UINT32 get_vpn(UINT32 const lpn);
static UINT32 get_vt_vblock(UINT32 const bank);
static UINT32 assign_new_write_vpn(UINT32 const bank);
static UINT32 check_GC(UINT32 const bank);

//LRU data
//extern node LRU_list[NUM_LRUBUFFER];
extern bank_list  g_last_page[NUM_BANKS];
//extern node free_list[NUM_LRUBUFFER+1];
extern UINT16 LRU_head;
extern UINT16 LRU_tail;
extern UINT16 free_head;
extern UINT16 free_tail;
extern UINT16 LRU_size;

static void sanity_check(void)
{
    UINT32 dram_requirement = RD_BUF_BYTES + WR_BUF_BYTES + COPY_BUF_BYTES + FTL_BUF_BYTES
                              + HIL_BUF_BYTES + TEMP_BUF_BYTES + BAD_BLK_BMP_BYTES + PAGE_MAP_BYTES + VCOUNT_BYTES
                              + LRU_BYTES + HASH_TABLE_BYTES + BLK_TABLE_BYTES
                              + gc_last_page_BYTES + bank_list_BYTES + last_page_BYTES + wbuf_vpage_BYTES + RMW_page_BYTES
                              + RMW_target_bank_BYTES + lru_list_lpn_BYTES + lru_list_prev_BYTES + lru_list_next_BYTES
                              + lru_list_bank_prev_BYTES + lru_list_bank_next_BYTES + lru_list_in_cap_BYTES
                              + P_TYPE_BYTES + P_BANK_BYTES + P_VBLOCK_BYTES + P_PAGE_NUM_BYTES + P_SECT_OFFSET_BYTES + P_NUM_SECTORS_BYTES
                              + P_BUF_ADDR_BYTES + P_G_FTL_RW_BUF_ID_BYTES + P_SRC_VBLOCK_BYTES + P_SRC_PAGE_BYTES + P_DST_VBLOCK_BYTES
                              + P_DST_PAGE_BYTES + P_NEXT_NODE_BYTES + P_PREV_NODE_BYTES + P_ISSUE_FLAG_BYTES + P_LPN_BYTES
                              + P_ID_BYTES + P_BANK_STATE_BYTES;
                              /*+ P_CAP_BUFFER_BYTES + WBUF_VDATA_BYTES + P_CAP_R_BUFFER_BYTES + P_CAP_W_BUFFER_BYTES
                              + P_CAP_BITMAP_BYTES + P_P2L_BUFFER_BYTES + P_L2P_BUFFER_BYTES;*/

    if ((dram_requirement > DRAM_SIZE) || // DRAM metadata size check
            (sizeof(misc_metadata) > BYTES_PER_PAGE)) // misc metadata size check
    {
        led_blink();
        uart_printf("no enough dram");
        while (1);
    }
}
static void build_bad_blk_list(void)
{
    UINT32 bank, num_entries, result, vblk_offset;
    scan_list_t* scan_list = (scan_list_t*) TEMP_BUF_ADDR;

    mem_set_dram(BAD_BLK_BMP_ADDR, NULL, BAD_BLK_BMP_BYTES);

    disable_irq();

    flash_clear_irq();

    for (bank = 0; bank < NUM_BANKS; bank++)
    {
        SETREG(FCP_CMD, FC_COL_ROW_READ_OUT);
        SETREG(FCP_BANK, REAL_BANK(bank));
        SETREG(FCP_OPTION, FO_E);
        SETREG(FCP_DMA_ADDR, (UINT32) scan_list);
        SETREG(FCP_DMA_CNT, SCAN_LIST_SIZE);
        SETREG(FCP_COL, 0);
        SETREG(FCP_ROW_L(bank), SCAN_LIST_PAGE_OFFSET);
        SETREG(FCP_ROW_H(bank), SCAN_LIST_PAGE_OFFSET);

        SETREG(FCP_ISSUE, NULL);
        while ((GETREG(WR_STAT) & 0x00000001) != 0);
        while (BSP_FSM(bank) != BANK_IDLE);

        num_entries = NULL;
        result = OK;

        if (BSP_INTR(bank) & FIRQ_DATA_CORRUPT)
        {
            result = FAIL;
        }
        else
        {
            UINT32 i;

            num_entries = read_dram_16(&(scan_list->num_entries));

            if (num_entries > SCAN_LIST_ITEMS)
            {
                result = FAIL;
            }
            else
            {
                for (i = 0; i < num_entries; i++)
                {
                    UINT16 entry = read_dram_16(scan_list->list + i);
                    UINT16 pblk_offset = entry & 0x7FFF;

                    if (pblk_offset == 0 || pblk_offset >= PBLKS_PER_BANK)
                    {
#if OPTION_REDUCED_CAPACITY == FALSE
                        result = FAIL;
#endif
                    }
                    else
                    {
                        write_dram_16(scan_list->list + i, pblk_offset);
                    }
                }
            }
        }

        if (result == FAIL)
        {
            num_entries = 0;  // We cannot trust this scan list. Perhaps a software bug.
        }
        else
        {
            write_dram_16(&(scan_list->num_entries), 0);
        }

        g_bad_blk_count[bank] = 0;

        for (vblk_offset = 1; vblk_offset < VBLKS_PER_BANK; vblk_offset++)
        {
            BOOL32 bad = FALSE;

#if OPTION_2_PLANE
            {
                UINT32 pblk_offset;

                pblk_offset = vblk_offset * NUM_PLANES;

                // fix bug@jasmine v.1.1.0
                if (mem_search_equ_dram(scan_list, sizeof(UINT16), num_entries + 1, pblk_offset) < num_entries + 1)
                {
                    bad = TRUE;
                }

                pblk_offset = vblk_offset * NUM_PLANES + 1;

                // fix bug@jasmine v.1.1.0
                if (mem_search_equ_dram(scan_list, sizeof(UINT16), num_entries + 1, pblk_offset) < num_entries + 1)
                {
                    bad = TRUE;
                }
            }
#else
            {
                // fix bug@jasmine v.1.1.0
                if (mem_search_equ_dram(scan_list, sizeof(UINT16), num_entries + 1, vblk_offset) < num_entries + 1)
                {
                    bad = TRUE;
                }
            }
#endif

            if (bad)
            {
                g_bad_blk_count[bank]++;
                set_bit_dram(BAD_BLK_BMP_ADDR + bank*(VBLKS_PER_BANK/8 + 1), vblk_offset);
            }
        }
    }
}

void ftl_open(void)
{
    // debugging example 1 - use breakpoint statement!
    /* *(UINT32*)0xFFFFFFFE = 10; */

    /* UINT32 volatile g_break = 0; */
    /* while (g_break == 0); */
	ptimer_start_scale0(); //for setRand()
    led(0);
    sanity_check();
    //----------------------------------------
    // read scan lists from NAND flash
    // and build bitmap of bad blocks
    //----------------------------------------
    build_bad_blk_list();

    //----------------------------------------
    // If necessary, do low-level format
    // format() should be called after loading scan lists, because format() calls is_bad_block().
    //----------------------------------------
    /* 	if (check_format_mark() == FALSE) */
    if (TRUE)
    {
        uart_print("do format");
        format();
        uart_print("end format");
        initLRU();
        initHASH();
        init_cap_queue();
    }
    // load FTL metadata
    else
    {
        load_metadata();
    }
    g_ftl_read_buf_id = 0;
    g_ftl_write_buf_id = 0;

    // This example FTL can handle runtime bad block interrupts and read fail (uncorrectable bit errors) interrupts
    flash_clear_irq();

    SETREG(INTR_MASK, FIRQ_DATA_CORRUPT | FIRQ_BADBLK_L | FIRQ_BADBLK_H);
    SETREG(FCONF_PAUSE, FIRQ_DATA_CORRUPT | FIRQ_BADBLK_L | FIRQ_BADBLK_H);

    enable_irq();
	setRand(ptimer_stop_and_uart_print_scale0());//setRand()
}
void ftl_flush(void)
{
    /* ptimer_start(); */
    logging_pmap_table();
    logging_misc_metadata();
    /* ptimer_stop_and_uart_print(); */
}
// Testing FTL protocol APIs
void ftl_test_write(UINT32 const lba, UINT32 const num_sectors)
{
    ASSERT(lba + num_sectors <= NUM_LSECTORS);
    ASSERT(num_sectors > 0);

    ftl_write(lba, num_sectors);
}
UINT32 user_read=0;

void ftl_read(UINT32 const lba, UINT32 const num_sectors)
{
    UINT32 remain_sects, num_sectors_to_read;
    UINT32 lpn, sect_offset;
    UINT32 bank, vpn;

    lpn          = lba / SECTORS_PER_PAGE;
    sect_offset  = lba % SECTORS_PER_PAGE;
    remain_sects = num_sectors;

    while (remain_sects != 0)
    {
        if ((sect_offset + remain_sects) < SECTORS_PER_PAGE)
        {
            num_sectors_to_read = remain_sects;
        }
        else
        {
            num_sectors_to_read = SECTORS_PER_PAGE - sect_offset;
        }
        bank = get_num_bank(lpn); // page striping
        vpn  = get_vpn(lpn);
        //CHECK_VPAGE(vpn);

        UINT16 id = findLRU(lpn);
        //LRU hit
        if(id != 0xFFFF)
        {
            UINT32 next_read_buf_id = (g_ftl_read_buf_id + 1) % NUM_RD_BUFFERS;

            mem_copy(RD_BUF_PTR(g_ftl_read_buf_id) + (sect_offset * BYTES_PER_SECTOR),
                     LRU_ADDR + (id* BYTES_PER_PAGE) + (sect_offset * BYTES_PER_SECTOR),
                     num_sectors_to_read * BYTES_PER_SECTOR);

#if OPTION_FTL_TEST == 0
            while (next_read_buf_id == GETREG(SATA_RBUF_PTR));	// wait if the read buffer is full (slow host)
#endif

            flash_finish();

            SETREG(BM_STACK_RDSET, next_read_buf_id);	// change bm_read_limit
            SETREG(BM_STACK_RESET, 0x02);				// change bm_read_limit

            g_ftl_read_buf_id = next_read_buf_id;
        }
        else if( (id = findCap(lpn)) != 0xFFFF)
        {
            UINT32 next_read_buf_id = (g_ftl_read_buf_id + 1) % NUM_RD_BUFFERS;

            mem_copy(RD_BUF_PTR(g_ftl_read_buf_id) + (sect_offset * BYTES_PER_SECTOR),
                     LRU_ADDR + (id* BYTES_PER_PAGE) + (sect_offset * BYTES_PER_SECTOR),
                     num_sectors_to_read * BYTES_PER_SECTOR);

#if OPTION_FTL_TEST == 0
            while (next_read_buf_id == GETREG(SATA_RBUF_PTR));	// wait if the read buffer is full (slow host)
#endif

            flash_finish();

            SETREG(BM_STACK_RDSET, next_read_buf_id);	// change bm_read_limit
            SETREG(BM_STACK_RESET, 0x02);				// change bm_read_limit

            g_ftl_read_buf_id = next_read_buf_id;
        }
        //LRU miss
        else
        {
            if (vpn != NULL && vpn < VBLKS_PER_BANK * PAGES_PER_BLK)
            {
                ++user_read;
                //uart_printf("user_read=%d",user_read);
                while(canEvict(0) != 1);
                nand_page_ptread_to_host(bank,
                                         vpn / PAGES_PER_BLK,
                                         vpn % PAGES_PER_BLK,
                                         sect_offset,
                                         num_sectors_to_read);
                set_bank_state(bank, 0);

            }
            // The host is requesting to read a logical page that has never been written to.
            else
            {
                UINT32 next_read_buf_id = (g_ftl_read_buf_id + 1) % NUM_RD_BUFFERS;

#if OPTION_FTL_TEST == 0
                while (next_read_buf_id == GETREG(SATA_RBUF_PTR));	// wait if the read buffer is full (slow host)
#endif

                // fix bug @ v.1.0.6
                // Send 0xFF...FF to host when the host request to read the sector that has never been written.
                // In old version, for example, if the host request to read unwritten sector 0 after programming in sector 1, Jasmine would send 0x00...00 to host.
                // However, if the host already wrote to sector 1, Jasmine would send 0xFF...FF to host when host request to read sector 0. (ftl_read() in ftl_xxx/ftl.c)
                mem_set_dram(RD_BUF_PTR(g_ftl_read_buf_id) + sect_offset*BYTES_PER_SECTOR,
                             0xFFFFFFFF, num_sectors_to_read*BYTES_PER_SECTOR);

                flash_finish();

                SETREG(BM_STACK_RDSET, next_read_buf_id);	// change bm_read_limit
                SETREG(BM_STACK_RESET, 0x02);				// change bm_read_limit

                g_ftl_read_buf_id = next_read_buf_id;
            }
        }
        sect_offset   = 0;
        remain_sects -= num_sectors_to_read;
        lpn++;
    }
}
UINT32 flash_write=0, gc_write=0, user_write=0, flash_read, erase_count=0,lefthole=0,righthole=0,no_writeflag=1 , limit = 0;
UINT32 ff_flag = 0 ;
//UINT32 merge_cnt = 0;
void ftl_write(UINT32 const lba, UINT32 const num_sectors)
{
    UINT32 remain_sects, num_sectors_to_write;
    UINT32 lpn, sect_offset;
    static UINT8 setTimer = 0;
    if(!setTimer)
    {
        ptimer_start();
        setTimer = 1;
    }
    lpn          = lba / SECTORS_PER_PAGE;
    sect_offset  = lba % SECTORS_PER_PAGE;
    remain_sects = num_sectors;

    //uart_printf("lba=%d",lba);
    limit += num_sectors ;

    while (remain_sects != 0)
    {
#if OPTION_FTL_TEST == 0
        while (g_ftl_write_buf_id == GETREG(SATA_WBUF_PTR));
#endif

        if ((sect_offset + remain_sects) < SECTORS_PER_PAGE)
        {
            ff_flag = 1 ;
            num_sectors_to_write = remain_sects;
        }
        else
        {
            num_sectors_to_write = SECTORS_PER_PAGE - sect_offset;
        }
        // single page write individually
        ++user_write;

        write_page(lpn, sect_offset, num_sectors_to_write);
        g_ftl_write_buf_id = (g_ftl_write_buf_id + 1 ) % NUM_WR_BUFFERS;
        SETREG(BM_STACK_WRSET, g_ftl_write_buf_id);     		// change bm_write_limit
        SETREG(BM_STACK_RESET, 0x01);                           // change bm_write_limit

        sect_offset   = 0;
        remain_sects -= num_sectors_to_write;
        lpn++;
    }

//	if(limit >= 4194346)
    if(lba == 10000005)
    {
        UINT32 totalTime = ptimer_stop_and_uart_print();
        long double execTime = 0; //ms
        execTime = erase_count*3.171 + gc_write*1.5255 + flash_write*1.319 + (flash_read )*0.7755;
        //uart_printf("idle time: %lf", 1-execTime / ((long double)totalTime/1000*NUM_BANKS));
		uart_printf("total time: %u", totalTime);
		uart_printf("more: %u", more);
		uart_printf("less: %u", less);
        /*if(g_ftl_statistics[0].gc_cnt>1000)
        	uart_printf("gc_cnt bank[0]=%d",g_ftl_statistics[0].gc_cnt);
        if(g_ftl_statistics[1].gc_cnt>1000)
        	uart_printf("gc_cnt bank[1]=%d",g_ftl_statistics[1].gc_cnt);
        uart_printf("user_write=%d",user_write);
        
        uart_printf("user_write=%d",user_write);
        uart_printf("flash_write = %d", flash_write);
        uart_printf("gc_write = %d", gc_write);
        uart_printf("flash_read+user_read = %d", flash_read + user_read);
        uart_printf("erase_count = %d", erase_count);
	 	uart_printf("merge_cnt = %d", merge_cnt);	
	 	*/
        /*
        uart_printf("left hole=%d",lefthole);
        uart_printf("right hole=%d",righthole);
        
        for(UINT8 i = 0 ; i != NUM_BANKS ; ++i)
		{
			uart_printf("bank %u", i);
			uart_printf("user_write: %u", g_ftl_statistics[i].user_write);
			uart_printf("flash_write: %u", g_ftl_statistics[i].flash_write);
			uart_printf("erase_cnt: %u", g_ftl_statistics[i].erase_cnt);
			uart_printf("wamp: %f", ((float)g_ftl_statistics[i].user_write+g_ftl_statistics[i].flash_write)/g_ftl_statistics[i].user_write);
		}*/
    }

}
//UINT32 target = 0;
UINT32 RMW_page_read = NUM_BANKS;
static UINT8 cnt = 0;
static UINT8 in = 0;
static UINT32 time = 0;
static void write_page(UINT32 const lpn, UINT32 const sect_offset, UINT32 const num_sectors)
{

    //uart_printf("write lpn: %d",lpn);
    CHECK_LPAGE(lpn);
    ASSERT(sect_offset < SECTORS_PER_PAGE);
    ASSERT(num_sectors > 0 && num_sectors <= SECTORS_PER_PAGE);

    UINT32 bank, old_vpn, new_vpn;
    UINT32 vblock, page_num, page_offset, column_cnt;

    bank        = get_num_bank(lpn); // page striping
    page_offset = sect_offset;
    column_cnt  = num_sectors;

    //new_vpn  = assign_new_write_vpn(bank);


    //CHECK_VPAGE (old_vpn);
    //CHECK_VPAGE (new_vpn);
    //ASSERT(old_vpn != new_vpn);
    old_vpn  = get_vpn(lpn);
    vblock   = old_vpn / PAGES_PER_BLK;
    page_num = old_vpn % PAGES_PER_BLK;

    //  g_ftl_statistics[bank].page_wcount++;
    UINT32 page_id = findLRU(lpn) ;
    //LRU hit
    if(page_id != 0xFFFF && page_id == LRU_head)
    {
    	//++merge_cnt;
        //copy from SATA_W_BUF to LRU_BUF
        //if(no_writeflag)
        if(user_write <= 3)
            mem_copy(LRU_ADDR + (page_id* BYTES_PER_PAGE) + (sect_offset * BYTES_PER_SECTOR),
                     WR_BUF_PTR(g_ftl_write_buf_id) + (sect_offset * BYTES_PER_SECTOR) ,
                     num_sectors * BYTES_PER_SECTOR
                    );

#if RMW_flag == 2
        _set_bit_dram_more(wbuf_vpage_ADDR + page_id * SECTORS_PER_PAGE / 8, sect_offset , num_sectors);
#endif

        //updateLRU(lpn);

#if static_binding
        //updateBANK(lpn);
#endif

    }
    //LRU miss
    else
    {
#if RMW_flag == 1
        if(RMW_page_read != NUM_BANKS)
        {
            while ((GETREG(WR_STAT) & 0x00000001) != 0);
            while(BSP_FSM(RMW_page_read) != BANK_IDLE) ;

            RMW_page_read = NUM_BANKS ;
        }
#endif

        //if full, select LRU block which is not current gc_block or current write_block
        UINT8 all_cap_full = 0;
        while(fullLRU())
        {
            //uart_printf("size=%d",LRU_size);
            UINT32 vlpn ;
            UINT32 vbank ;
            //UINT32 rbank;

            for(int i = 0 ; i < NUM_BANKS; i ++ )
            {
	        	#if greedy == 0
					#if random == 1
						#if backlogged == 1
							randombEvict();
						#else
							randomEvict();
						#endif
					#else //rr
						#if backlogged == 1
							rrbEvict();
						#else
							rrEvict();
						#endif
					#endif
				#else//greedy
					greedyEvict();
				#endif
                vbank = i;// target ;
                //uart_printf("%d", vbank);
                if((_BSP_FSM(REAL_BANK(vbank)) != BANK_IDLE) || q_size[vbank] == Q_DEPTH)
                {
                    //target = (target+1) % NUM_BANKS;
                    continue;
                }


                UINT32 last_page_flag = check_GC(vbank) ;
                if(last_page_flag)
                {
                    //target = (target+1) % NUM_BANKS;
                    continue ;
                }
                if(g_misc_meta[vbank].gcing == 1)
                {
                    g_misc_meta[vbank].gcing = garbage_collection(vbank) ;
                    //target = (target+1) % NUM_BANKS;
                    continue ;
                }
                /*
                else if(g_misc_meta[vbank].gcing == 3)
                {
                	uart_printf("o.o");
                #if cap
                	cap_node node;
                	node.type = NAND_PAGE_PROGRAM;
                	node.bank = vbank;
                	node.vblock = g_misc_meta[vbank].target_block;
                	node.page_num = g_misc_meta[vbank].target_page;
                	node.buf_addr = COPY_BUF(vbank);
                	cap_insert(vbank, node);
                #else
                	nand_page_program(vbank,
                					g_misc_meta[vbank].target_block,
                					g_misc_meta[vbank].target_page,
                					COPY_BUF(vbank));
                #endif
                	g_misc_meta[vbank].gcing = 1 ;
                	target = (target+1) % NUM_BANKS;
                	continue ;

                }
                */
#if static_binding
                if(g_last_page[vbank].bank_tail == 0xFFFF)
                {
                    //target = (target+1) % NUM_BANKS;
                    continue;
                }
#endif
                //target = (target+1) % NUM_BANKS;
                ///////////

                UINT32 victim_current ;

                if(!all_cap_full && cap_is_full(vbank))
                    continue;
				

				
				
#if static_binding
                vlpn = get_lru_lpn(g_last_page[vbank].bank_tail);//LRU_list[g_last_page[vbank].bank_tail].lpn;
                victim_current = g_last_page[vbank].bank_tail;



#endif

                ASSERT(g_misc_meta[vbank].gcing == 0);
                UINT32 old_bank = get_num_bank(vlpn)  ;
                UINT32 ovpn = get_vpn(vlpn);
                //	ASSERT(get_num_bank(vlpn) == vbank);
                ////////////////
                if(!(_tst_bit_dram_more(wbuf_vpage_ADDR + SECTORS_PER_PAGE * victim_current / 8)))
                {
                    if(vbank == old_bank)
                    {
			#if cap
                        cap_node node;
                        node.type = NAND_PAGE_PTREAD;
                        node.bank = vbank;
                        node.vblock = ovpn / PAGES_PER_BLK;
                        node.page_num = ovpn % PAGES_PER_BLK;
                        node.sect_offset = 0;
                        node.num_sectors = SECTORS_PER_PAGE;
                        node.buf_addr = LRU_ADDR + victim_current * BYTES_PER_PAGE;
                        node.issue_flag = 0;
                        node.lpn = 0xFFFFFFFF;
                        cap_insert(vbank, node);
			#else
                        nand_page_ptread(vbank,
                                         ovpn / PAGES_PER_BLK,
                                         ovpn % PAGES_PER_BLK,
                                         0,
                                         SECTORS_PER_PAGE,
                                         LRU_ADDR + victim_current * BYTES_PER_PAGE,
                                         0);
			#endif
                        ++flash_read;
                        _set_bit_dram_more(wbuf_vpage_ADDR + victim_current * SECTORS_PER_PAGE / 8, 0 , SECTORS_PER_PAGE);
                    }
                    else
                    {
                        uart_printf("vbank = %d", vbank);
                        uart_printf("old_bank = %d", old_bank);
                        ASSERT(static_binding != 1);
                        insertTAIL_bankLRU(vlpn);
                  //      g_misc_meta[vbank].you_need_read = 1 ;
                    }

                }
                else
                {
                    new_vpn  = assign_new_write_vpn(vbank);
                    UINT32 new_block = new_vpn / PAGES_PER_BLK;
                    UINT32 new_page_num = new_vpn % PAGES_PER_BLK;

                    ASSERT(get_vcount(vbank,new_block) < (PAGES_PER_BLK - 1));
                    //if(no_writeflag)
                    //uart_printf("%d", vlpn);
					
				#if cap
                    cap_node node;
                    node.type = NAND_PAGE_PROGRAM;
                    node.bank = vbank;
                    node.vblock = new_block;
                    node.page_num = new_page_num;
                    node.buf_addr = LRU_ADDR + (victim_current * BYTES_PER_PAGE);
                    node.id = victim_current;
                    node.lpn = vlpn;
                    cap_insert(vbank, node);
				#else
                    nand_page_program(vbank,
                                      new_block,
                                      new_page_num,
                                      LRU_ADDR + (victim_current * BYTES_PER_PAGE)
                                     );
				#endif
					++g_ftl_statistics[vbank].user_write;
                    ++flash_write;

                    if(get_gc_vblock(old_bank) == get_vpn(vlpn)/PAGES_PER_BLK)
                    {
                        g_misc_meta[old_bank].gc_vblock_vcount--;
                    }
                    else
                    {
                        set_vcount(old_bank, get_vpn(vlpn)/PAGES_PER_BLK, get_vcount(old_bank, get_vpn(vlpn)/PAGES_PER_BLK) - 1);
                    }
                    if(vbank!= old_bank)
                        set_num_bank(vlpn , vbank);

                    set_lpn(vbank, new_page_num, vlpn);
                    set_vpn(vlpn, new_vpn);
                    set_vcount(vbank, new_block, get_vcount(vbank, new_block) + 1);

#if static_binding
                    delete_bankLRU(victim_current);
#else
                    write_dram_32(RMW_target_bank_ADDR + victim_current * sizeof(UINT32), NUM_BANKS);
                    write_dram_32((RMW_page_ADDR + vbank * sizeof(UINT32)), NUM_LPAGES);
#endif

                    deleteLRU(victim_current);
                    //deleteLRU(vlpn);
                    deleteHash(victim_current);
                    //deleteHash(vlpn);
                    //break;
                }
            }
            all_cap_full = 1;
        }
        // if old data already exist,
        //-------------------------------------------------------------
        //uart_printf("whole page read");
        //ptimer_start();
        old_vpn  = get_vpn(lpn);
        vblock   = old_vpn / PAGES_PER_BLK;
        page_num = old_vpn % PAGES_PER_BLK;
        bank        = get_num_bank(lpn);
        UINT16 id = insertHeadLRU(lpn);
        insertHash(lpn, id);

#if static_binding
        insertHEAD_bankLRU(lpn);
#endif

        if (old_vpn != NULL )
        {
            //--------------------------------------------------------------------------------------
            // `Partial programming'
            // we could not determine whether the new data is loaded in the SATA write buffer.
            // Thus, read the left/right hole sectors of a valid page and copy into the write buffer.
            // And then, program whole valid data
            //--------------------------------------------------------------------------------------
#if RMW_flag == 1
            if (num_sectors != SECTORS_PER_PAGE)
            {

                if(ff_flag)
                {
                    RMW_page_read = bank ;

                }
#if cap
                cap_node node;
                node.type = NAND_PAGE_PTREAD;
                node.bank = bank;
                node.vblock = vblock;
                node.page_num = page_num;
                node.sect_offset = 0;
                node.num_sectors = SECTORS_PER_PAGE;
                node.buf_addr = LRU_ADDR + id * BYTES_PER_PAGE;
                node.issue_flag = 0;
                node.lpn = 0xFFFFFFFF;
                cap_insert(bank, node);
#else
                nand_page_ptread(bank,
                                 vblock,
                                 page_num,
                                 0,
                                 SECTORS_PER_PAGE,
                                 LRU_ADDR + (id * BYTES_PER_PAGE),
                                 0);
#endif
                ++flash_read;
                ff_flag = 0 ;

            }

#elif RMW_flag == 2
            _set_bit_dram_more(wbuf_vpage_ADDR + id * SECTORS_PER_PAGE / 8,sect_offset ,num_sectors);
#endif

        }

        //if(no_writeflag)
        if(user_write <= 3)
            mem_copy(LRU_ADDR + (id * BYTES_PER_PAGE),
                     WR_BUF_PTR(g_ftl_write_buf_id),
                     BYTES_PER_PAGE
                    );

    }
    ff_flag = 0 ;
}
// get vpn from PAGE_MAP
static UINT32 get_vpn(UINT32 const lpn)
{
    CHECK_LPAGE(lpn);
    return read_dram_32(PAGE_MAP_ADDR + lpn * sizeof(UINT32));
}
// set vpn to PAGE_MAP
static void set_vpn(UINT32 const lpn, UINT32 const vpn)
{
    CHECK_LPAGE(lpn);
    ASSERT(vpn >= (META_BLKS_PER_BANK * PAGES_PER_BLK) && vpn < (VBLKS_PER_BANK * PAGES_PER_BLK));

    write_dram_32(PAGE_MAP_ADDR + lpn * sizeof(UINT32), vpn);
}
static void set_fake_vpn(UINT32 const lpn, UINT32 const vpn)
{
    CHECK_LPAGE(lpn);
    //ASSERT(vpn >= (META_BLKS_PER_BANK * PAGES_PER_BLK) && vpn < (VBLKS_PER_BANK * PAGES_PER_BLK));

    write_dram_32(PAGE_MAP_ADDR + lpn * sizeof(UINT32), vpn + VBLKS_PER_BANK * PAGES_PER_BLK);
}
// get valid page count of vblock
static UINT32 get_vcount(UINT32 const bank, UINT32 const vblock)
{
    UINT32 vcount;

    ASSERT(bank < NUM_BANKS);
    ASSERT((vblock >= META_BLKS_PER_BANK) && (vblock < VBLKS_PER_BANK));

    vcount = read_dram_16(VCOUNT_ADDR + (((bank * VBLKS_PER_BANK) + vblock) * sizeof(UINT16)));
    ASSERT((vcount < PAGES_PER_BLK) || (vcount == VC_MAX));

    return vcount;
}
// set valid page count of vblock
static void set_vcount(UINT32 const bank, UINT32 const vblock, UINT32 const vcount)
{
    ASSERT(bank < NUM_BANKS);
    ASSERT((vblock >= META_BLKS_PER_BANK) && (vblock < VBLKS_PER_BANK));
    if((vcount > PAGES_PER_BLK) && (vcount != VC_MAX))
    {
        uart_printf("bank : %d , vblock : %d , vcount : %d" , bank , vblock , vcount ) ;
        uart_printf("gc_block: %d , gc_backup : %d , lb : %d " , get_gc_vblock(bank)  , g_misc_meta[bank].gc_backup , g_misc_meta[bank].gc_backup_for_gc);
    }
    ASSERT((vcount < PAGES_PER_BLK) || (vcount == VC_MAX));

    write_dram_16(VCOUNT_ADDR + (((bank * VBLKS_PER_BANK) + vblock) * sizeof(UINT16)), vcount);
}
static UINT32 assign_new_write_vpn(UINT32 const bank)
{
    ASSERT(bank < NUM_BANKS);

    UINT32 write_vpn;
    UINT32 vblock;

    write_vpn = get_cur_write_vpn(bank);
    vblock    = write_vpn / PAGES_PER_BLK;

    set_new_write_vpn(bank, write_vpn + 1);
    return write_vpn;


}

static UINT32 check_GC(UINT32 const bank)
{
    UINT32 write_vpn;
    UINT32 vblock;
    if((g_misc_meta[bank].gcing == 1 )||(g_misc_meta[bank].gcing == 3 ))
    {
        return 0;
    }
    write_vpn = get_cur_write_vpn(bank);
    vblock    = write_vpn / PAGES_PER_BLK;

    // NOTE: if next new write page's offset is
    // the last page offset of vblock (i.e. PAGES_PER_BLK - 1),
    if ((write_vpn % PAGES_PER_BLK) == (PAGES_PER_BLK - 1))
    {
        // then, because of the flash controller limitation
        // (prohibit accessing a spare area (i.e. OOB)),
        // thus, we persistenly write a lpn list into last page of vblock.
        //mem_copy(gc_last_page_ADDR + bank * BYTES_PER_PAGE, g_misc_meta[bank].lpn_list_of_cur_vblock, sizeof(UINT32) * PAGES_PER_BLK);
        mem_copy(gc_last_page_ADDR + bank * BYTES_PER_PAGE, 
        	LPN_LIST_OF_CUR_VBLOCK_ADDR + bank * PAGES_PER_BLK * sizeof(UINT32), 
        	sizeof(UINT32) * PAGES_PER_BLK);
        // fix minor bug
#if 0//cap
        cap_node node;
        node.type = NAND_PAGE_PTPROGRAM;
        node.bank = bank;
        node.vblock = vblock;
        node.page_num = PAGES_PER_BLK - 1;
        node.sect_offset = 0;
        node.num_sectors = ((sizeof(UINT32) * PAGES_PER_BLK + BYTES_PER_SECTOR - 1 ) / BYTES_PER_SECTOR);
        node.buf_addr = gc_last_page_ADDR + bank * BYTES_PER_PAGE;
        node.lpn = 0xFFFFFFFF;
        cap_insert(bank, node);

#else
#if cap
        while(canEvict(1) != 1);
#endif
        nand_page_ptprogram(bank,
                            vblock,
                            PAGES_PER_BLK - 1,
                            0,
                            ((sizeof(UINT32) * PAGES_PER_BLK + BYTES_PER_SECTOR - 1 ) / BYTES_PER_SECTOR),
                            gc_last_page_ADDR + bank * BYTES_PER_PAGE);
        set_bank_state(bank, 1);
#endif
        ++flash_write;
        while ((GETREG(WR_STAT) & 0x00000001) != 0);
        //mem_set_sram(g_misc_meta[bank].lpn_list_of_cur_vblock, 0x00000000, sizeof(UINT32) * PAGES_PER_BLK);
	mem_set_dram(LPN_LIST_OF_CUR_VBLOCK_ADDR + bank * PAGES_PER_BLK * sizeof(UINT32), 0x00000000, sizeof(UINT32) * PAGES_PER_BLK);
        inc_full_blk_cnt(bank);

        // do garbage collection if necessary
        if (is_full_all_blks(bank))
        {
            g_misc_meta[bank].gcing = 1;

            return 1;
        }
        do
        {
            vblock++;
            set_new_write_vpn(bank, vblock * PAGES_PER_BLK );
            ASSERT(vblock != VBLKS_PER_BANK);
        }
        while (get_vcount(bank, vblock) == VC_MAX);
        return 1 ;
    }
    return 0 ;
}

static BOOL32 is_bad_block(UINT32 const bank, UINT32 const vblk_offset)
{
    if (tst_bit_dram(BAD_BLK_BMP_ADDR + bank*(VBLKS_PER_BANK/8 + 1), vblk_offset) == FALSE)
    {
        return FALSE;
    }
    return TRUE;
}

UINT16 gc_count  = 0;
static UINT32 garbage_collection(UINT32 const bank)
{

    gc_count++ ;

    // ASSERT(bank < NUM_BANKS);
    // g_ftl_statistics[bank].gc_cnt++;

    UINT32 src_lpn;
    UINT32 vt_vblock;
    UINT32 backup_vblock;
    UINT32 gc_backup_vblock;
    UINT32 free_vpn;
    UINT32 vcount; // valid page count in victim block
    UINT32 src_page;
    UINT32 test_page;
    UINT32 test_lpn ;
    UINT32 gc_vblock;
    UINT32 test;
    UINT8  flag = 0;

    UINT16 reserve_vcount = 0;
    //ASSERT(no_use_bank_block != NUM_VBLKS);
    //[bank].gc_cnt++;
    //ASSERT( no_use_bank==bank);


    while(1)
    {

        if(g_misc_meta[bank].victim_block == NUM_VBLKS)
        {
            vt_vblock = get_vt_vblock(bank);
            g_misc_meta[bank].victim_block  = vt_vblock;
#if 0
#else
            while(canEvict(0) != 1);
            nand_page_ptread(bank,
                             vt_vblock,
                             PAGES_PER_BLK - 1,
                             0,
                             ((sizeof(UINT32) * PAGES_PER_BLK + BYTES_PER_SECTOR - 1 ) / BYTES_PER_SECTOR),
                             last_page_ADDR + sizeof(UINT32) * bank * PAGES_PER_BLK,
                             2);
            set_bank_state(bank, 0);
#endif
            ++flash_read;
            //nand_page_ptread(bank, vt_vblock, PAGES_PER_BLK - 1, 0,
            //((sizeof(UINT32) * PAGES_PER_BLK + BYTES_PER_SECTOR - 1 ) / BYTES_PER_SECTOR),FTL_BUF(bank), RETURN_WHEN_DONE);
            //		mem_copy(g_misc_meta[bank].lpn_list_of_cur_vblock,FTL_BUF(bank), sizeof(UINT32) * PAGES_PER_BLK);

            return 1 ;
        }
        else
        {
            vt_vblock = g_misc_meta[bank].victim_block ;
            //mem_copy(g_misc_meta[bank].lpn_list_of_cur_vblock,gc_last_page_ADDR + bank * BYTES_PER_PAGE, sizeof(UINT32) * PAGES_PER_BLK);
        }
        // get victim block

        ASSERT((vt_vblock >= META_BLKS_PER_BANK) && (vt_vblock < VBLKS_PER_BANK));
        vcount    = get_vcount(bank, vt_vblock);


        //uart_printf("vt_block : %d ,vcount : %d" , vt_vblock , vcount);
        gc_vblock = get_gc_vblock(bank);
        backup_vblock = g_misc_meta[bank].gc_backup ;
        gc_backup_vblock = g_misc_meta[bank].gc_backup_for_gc ;

        //////////////////////

        free_vpn  = gc_vblock * PAGES_PER_BLK + g_misc_meta[bank].cur_freevpn;
        flag = 0;


        UINT16 test_flag = 0 ;
        src_page = g_misc_meta[bank].src_page ;
        /*
        for (src_page = 0; src_page < (PAGES_PER_BLK - 1); src_page++)
        {
        */
        while(src_page != 127)
        {
            if(g_misc_meta[bank].cur_freevpn == 127)
            {
                //uart_printf("test==127");
                //  uart_printf("run out gc block  : %d" , g_misc_meta[bank].gc_vblock_vcount);
                //uart_printf("change");
                //mem_copy(FTL_BUF(bank), g_misc_meta[bank].lpn_list_of_gc_vblock, sizeof(UINT32) * PAGES_PER_BLK);
                mem_copy(FTL_BUF(bank), LPN_LIST_OF_GC_VBLOCK_ADDR + bank * PAGES_PER_BLK * sizeof(UINT32), sizeof(UINT32) * PAGES_PER_BLK);
                UINT32 vblock = (free_vpn / PAGES_PER_BLK) ;
#if 0//cap
                cap_node node;
                node.type = NAND_PAGE_PTPROGRAM;
                node.bank = bank;
                node.vblock = vblock;
                node.page_num = PAGES_PER_BLK - 1;
                node.sect_offset = 0;
                node.num_sectors =  (sizeof(UINT32) * PAGES_PER_BLK + BYTES_PER_SECTOR - 1 ) / BYTES_PER_SECTOR;
                node.buf_addr = FTL_BUF(bank);
                cap_insert(bank, node);
#else
#if cap
                while(canEvict(1) != 1);
#endif
                nand_page_ptprogram(bank,
                                    vblock,
                                    PAGES_PER_BLK - 1,
                                    0,
                                    ((sizeof(UINT32) * PAGES_PER_BLK + BYTES_PER_SECTOR - 1 ) / BYTES_PER_SECTOR),
                                    FTL_BUF(bank)
                                   );
                set_bank_state(bank, 1);
#endif
                ++flash_write;
                // mem_set_sram(g_misc_meta[bank].lpn_list_of_gc_vblock, 0x00000000, sizeof(UINT32) * PAGES_PER_BLK);
                g_misc_meta[bank].gc_backup = gc_backup_vblock;
                set_vcount(bank,vblock,g_misc_meta[bank].gc_vblock_vcount);
                set_gc_vblock(bank,backup_vblock);
                free_vpn = backup_vblock * PAGES_PER_BLK ;
                flag = 1;
                g_misc_meta[bank].gc_over = 1;
                g_misc_meta[bank].cur_freevpn = 0;
                g_misc_meta[bank].gc_vblock_vcount = 0;
                return 1 ;
            }
            // get lpn of victim block from a read lpn list

            // src_lpn = get_lpn(bank, src_page);
            src_lpn = read_dram_32(last_page_ADDR + sizeof(UINT32)*(bank * PAGES_PER_BLK + src_page));
            //uart_printf("%d " , g_misc_meta[bank].cur_freevpn);

            CHECK_LPAGE(src_lpn);
            CHECK_VPAGE(get_vpn(src_lpn));

            //UINT32 findequ = mem_search_equ_dram(LRU_table_ADDR,sizeof(UINT32),NUM_LRU_TABLE,src_lpn) ;
            // determine whether the page is valid or not

            if ((get_vpn(src_lpn) !=
                    ((vt_vblock * PAGES_PER_BLK) + src_page)) || (get_num_bank(src_lpn) != bank))
            {
                // invalid page
                src_page++;
                continue;
            }

            //uart_printf("src_page : %d" , src_page);

            // ASSERT(get_lpn(bank, src_page) != INVALID);
            CHECK_LPAGE(src_lpn);
            //if page is in write buffer ,copy to flash

            UINT32 copy_flag = 0;
#if cap
            cap_node node;
            node.type = NAND_PAGE_COPYBACK;
            node.bank = bank;
            node.src_vblock = vt_vblock;
            node.src_page = src_page;
            node.dst_vblock = free_vpn / PAGES_PER_BLK;
            node.dst_page = free_vpn % PAGES_PER_BLK;
            node.lpn = 0xFFFFFFFF;
            cap_insert(bank, node);
#else
            nand_page_copyback(bank,
                               vt_vblock,
                               src_page,
                               free_vpn / PAGES_PER_BLK,
                               free_vpn % PAGES_PER_BLK);
#endif
			++g_ftl_statistics[bank].flash_write;
	/*
            if(copy_flag)
            {
                g_misc_meta[bank].target_block =  free_vpn / PAGES_PER_BLK ;
                g_misc_meta[bank].target_page =  free_vpn % PAGES_PER_BLK ;
            }
      */
            gc_write++;

            //  ASSERT((free_vpn / PAGES_PER_BLK) == gc_vblock);
            // update metadata
            set_vpn(src_lpn, free_vpn);
            //   set_lpn(bank, (free_vpn % PAGES_PER_BLK), src_lpn);
            UINT32 page_num = free_vpn % PAGES_PER_BLK;
            //g_misc_meta[bank].lpn_list_of_gc_vblock[page_num] = src_lpn;
            write_dram_32(LPN_LIST_OF_GC_VBLOCK_ADDR + (bank*PAGES_PER_BLK + page_num)*sizeof(UINT32), src_lpn);
            //	uart_printf("gc_count : %d vcount : %d ,src_page : %d cur_freevpn : %d" , gc_count, vcount , src_page , g_misc_meta[bank].cur_freevpn);
            src_page++;
            g_misc_meta[bank].src_page = src_page ;
            free_vpn ++;
            g_misc_meta[bank].cur_freevpn ++ ;
            g_misc_meta[bank].gc_vblock_vcount ++ ;
            if(!copy_flag)
                return 1;
            else
                return 3;
        }
        //	mem_set_sram(g_misc_meta[bank].lpn_list_of_cur_vblock, 0x00000000, sizeof(UINT32) * PAGES_PER_BLK);

        // 3. erase victim block
#if cap
        cap_node node;
        node.type = NAND_BLOCK_ERASE;
        node.bank = bank;
        node.vblock = vt_vblock;
        node.lpn = 0xFFFFFFFF;
        cap_insert(bank, node);
#else
        nand_block_erase(bank, vt_vblock);
#endif
		++g_ftl_statistics[bank].erase_cnt;
        erase_count++;
        g_misc_meta[bank].src_page = 0;
        /*     uart_printf("gc page count : %d", vcount); */
        g_misc_meta[bank].victim_block = NUM_VBLKS;

        // 4. update metadata

        if(g_misc_meta[bank].gc_over  == 0)//gc_vblock is not full
        {
            set_new_write_vpn(bank, (g_misc_meta[bank].gc_backup_for_gc * PAGES_PER_BLK) ); // set a free page for new write

            set_vcount(bank,g_misc_meta[bank].gc_backup_for_gc,0);

            g_misc_meta[bank].gc_backup_for_gc = vt_vblock;
            set_vcount(bank, vt_vblock, VC_MAX);

            g_misc_meta[bank].reserve_vcount = PAGES_PER_BLK;
            //	uart_printf("erase count : %d vcount : %d vt_vblock : %d bank : %d" , erase_count,vcount , vt_vblock, bank);

            dec_full_blk_cnt(bank);
            return 0;
            //uart_printf("end =%d" , gc_count);

        }
        else
        {
            g_misc_meta[bank].gc_backup_for_gc = vt_vblock;
            set_vcount(bank, vt_vblock, VC_MAX);
            g_misc_meta[bank].gc_over = 0;
        }
        return 1 ;
    }
}
//-------------------------------------------------------------
// Victim selection policy: Greedy
//
// Select the block which contain minumum valid pages
//-------------------------------------------------------------
static UINT32 get_vt_vblock(UINT32 const bank)
{
    ASSERT(bank < NUM_BANKS);

    UINT32 vblock;

    // search the block which has mininum valid pages
    vblock = mem_search_min_max(VCOUNT_ADDR + (bank * VBLKS_PER_BANK * sizeof(UINT16)),
                                sizeof(UINT16),
                                VBLKS_PER_BANK,
                                MU_CMD_SEARCH_MIN_DRAM);

    ASSERT(is_bad_block(bank, vblock) == FALSE);
    ASSERT(vblock >= META_BLKS_PER_BANK && vblock < VBLKS_PER_BANK);
    ASSERT(get_vcount(bank, vblock) < (PAGES_PER_BLK - 1));

    return vblock;
}
static void format(void)
{
    UINT32 bank, vblock, vcount_val;

    ASSERT(NUM_MISC_META_SECT > 0);
    ASSERT(NUM_VCOUNT_SECT > 0);

    uart_printf("Total FTL DRAM metadata size: %d KB", DRAM_BYTES_OTHER / 1024);

    uart_printf("VBLKS_PER_BANK: %d", VBLKS_PER_BANK);
    uart_printf("LBLKS_PER_BANK: %d", NUM_LPAGES / PAGES_PER_BLK / NUM_BANKS);
    uart_printf("META_BLKS_PER_BANK: %d", META_BLKS_PER_BANK);
    uart_printf("LRU_BUF_SIZE: %d KB", LRU_BYTES / 1024);
    uart_printf("HASH_TABLE_SIZE: %d KB", HASH_TABLE_BYTES / 1024);
    uart_printf("BLK_TABLE_SIZE: %d KB", BLK_TABLE_BYTES / 1024);
	
    uart_printf("g_misc_meta: %d B", sizeof(g_misc_meta ) );
    //----------------------------------------
    // initialize DRAM metadata
    //----------------------------------------
    mem_set_dram(PAGE_MAP_ADDR, NULL, PAGE_MAP_BYTES);
    mem_set_dram(VCOUNT_ADDR, NULL, VCOUNT_BYTES);
    mem_set_dram(LRU_ADDR, 0xFFFFFFFF, LRU_BYTES);
    mem_set_dram(HASH_TABLE_ADDR, 0xFFFFFFFF, HASH_TABLE_BYTES);
    mem_set_dram(BLK_TABLE_ADDR, 0xFFFFFFFF, BLK_TABLE_BYTES);
    //mem_set_dram(P_LPN_ADDR, 0xFFFFFFFF, P_LPN_BYTES);
    //mem_set_dram(P_BANK_STATE_ADDR, 0xFFFFFFFF, P_BANK_STATE_BYTES);
    //assign fake vpn
    /*UINT32 lpn=0;
    UINT32 vpn=0;

    for( ; lpn!=NUM_LPAGES ; ++lpn, ++vpn)
    {
    	set_fake_vpn(lpn, vpn);
    	if((vpn+2)%128==0)
    		++vpn;
    }*/
    //----------------------------------------
    // erase all blocks except vblock #0
    //----------------------------------------
    for (vblock = MISCBLK_VBN; vblock < VBLKS_PER_BANK; vblock++)
    {
        for (bank = 0; bank < NUM_BANKS; bank++)
        {
            vcount_val = VC_MAX;
            if (is_bad_block(bank, vblock) == FALSE)
            {
                nand_block_erase(bank, vblock);
                vcount_val = 0;
            }
            write_dram_16(VCOUNT_ADDR + ((bank * VBLKS_PER_BANK) + vblock) * sizeof(UINT16),
                          vcount_val);
        }
    }
    //----------------------------------------
    // initialize SRAM metadata
    //----------------------------------------
    init_metadata_sram();

    // flush metadata to NAND
    logging_pmap_table();
    logging_misc_metadata();

    write_format_mark();
    led(1);
    uart_print("format complete");

    //---------------------------------------

    for( int i =  0 ; i < NUM_BANKS ; i++)
    {
		g_ftl_statistics[i].erase_cnt = g_ftl_statistics[i].flash_write = g_ftl_statistics[i].user_write = 0;

        UINT32 first_vpn =	get_cur_write_vpn(i) ;
        UINT32 first_block = first_vpn	/ PAGES_PER_BLK ;
        UINT32 group =1;
        UINT32 current = 0 ;
        UINT32 first_lpn = i * group;
		/*
        if(i == 0)
        {
            for(int r = 0 ; r < 16 ; r++)
            {
                do
                {
                    first_block++;
                }
                while (get_vcount(i, first_block) == VC_MAX);
                inc_full_blk_cnt(i);
                set_vcount(i,first_block,VC_MAX);
            }
            first_block = first_vpn	/ PAGES_PER_BLK ;
        }
        if(i == 1)
        {
            for(int r = 0 ; r < 1 ; r++)
            {
                do
                {
                    first_block++;
                }
                while (get_vcount(i, first_block) == VC_MAX);
                inc_full_blk_cnt(i);
                set_vcount(i,first_block,VC_MAX);
            }
            first_block = first_vpn	/ PAGES_PER_BLK ;
        }
        */
        uart_printf("group : %d" , group);
        uart_printf("free block : %d" , g_misc_meta[i].free_blk_cnt);
        while(g_misc_meta[i].free_blk_cnt > 34)
        {
            UINT32 kk = 0;


            for( int k = 0 ; k <127 ; k++)
            {
                //uart_printf("first lpn : %d , vpn : %d" , first_lpn , first_block * PAGES_PER_BLK + k);

                if(first_lpn >= NUM_LPAGES)
                {
                    set_vcount(i,first_block,k);
                    set_new_write_vpn(i,first_block * PAGES_PER_BLK + k);
                    uart_printf("free block : %d" , g_misc_meta[i].free_blk_cnt);
                    uart_printf("new vpn : %d" , get_cur_write_vpn(i) % PAGES_PER_BLK);
                    uart_printf("k1 : %d" , k);
                    break ;
                }
                kk++ ;
                //nand_page_program(i,first_block,k,FTL_BUF(i));
                //g_misc_meta[i].lpn_list_of_cur_vblock[k] = first_lpn ; // set lpn
                set_lpn(i, k, first_lpn);
                set_num_bank(first_lpn,i) ;
                set_vpn(first_lpn, first_block * PAGES_PER_BLK + k);	 // set vpn

                if((current % group) == (group -1))
                    first_lpn += ((NUM_BANKS - 1) * group + 1);
                else
                    first_lpn++ ;
                current++;

            }
            if((first_lpn >= NUM_LPAGES)&&(kk!=127))
                break ;
            //mem_copy(FTL_BUF(i), g_misc_meta[i].lpn_list_of_cur_vblock, sizeof(UINT32) * PAGES_PER_BLK);
	    mem_copy(FTL_BUF(i), LPN_LIST_OF_CUR_VBLOCK_ADDR + i*PAGES_PER_BLK*sizeof(UINT32), sizeof(UINT32) * PAGES_PER_BLK);         
            nand_page_ptprogram_for_format(i, first_block, PAGES_PER_BLK - 1, 0,
                                           ((sizeof(UINT32) * PAGES_PER_BLK + BYTES_PER_SECTOR - 1 ) / BYTES_PER_SECTOR), FTL_BUF(i));
            inc_full_blk_cnt(i);
            //uart_printf("full_cnt : %d" , g_misc_meta[i].free_blk_cnt);
            set_vcount(i,first_block,127);
            do
            {
                first_block++;


            }while (get_vcount(i, first_block) == VC_MAX);
            set_new_write_vpn(i, first_block * PAGES_PER_BLK);



            if(first_lpn >= NUM_LPAGES)
            {
                uart_printf("k2 ");
                uart_printf("free block : %d" , g_misc_meta[i].free_blk_cnt);
                uart_printf("new vpn : %d" , get_cur_write_vpn(i));

                break ;
            }
        }
		uart_printf("free block : %d" , g_misc_meta[i].free_blk_cnt);
    }
    //-----------------------------

}
static void init_metadata_sram(void)
{
    UINT32 bank;
    UINT32 vblock;
    UINT32 mapblk_lbn;
    for(int i = 0 ; i< NUM_LRUBUFFER ; i++)
    {
        write_dram_32(RMW_target_bank_ADDR + i * sizeof(UINT32),NUM_BANKS);
    }
    //----------------------------------------
    // initialize misc. metadata
    //----------------------------------------
    for (bank = 0; bank < NUM_BANKS; bank++)
    {
        g_misc_meta[bank].free_blk_cnt = VBLKS_PER_BANK - META_BLKS_PER_BANK;
        g_misc_meta[bank].free_blk_cnt -= get_bad_blk_cnt(bank);

        g_misc_meta[bank].cur_freevpn = 0;
        g_misc_meta[bank].gc_vblock_vcount = 0;
        g_misc_meta[bank].src_page = 0;
        g_misc_meta[bank].victim_block = NUM_VBLKS ;
        g_misc_meta[bank].reserve_vcount = PAGES_PER_BLK ;
        g_misc_meta[bank].gcing = 0;
        g_misc_meta[bank].gc_over = 0;
        //g_misc_meta[bank].you_need_read = 0 ;
        write_dram_32(RMW_page_ADDR + bank * sizeof(UINT32),NUM_LPAGES);
        ////

        g_last_page[bank].bank_head = 0xFFFF;
        g_last_page[bank].bank_tail = 0xFFFF;
        g_last_page[bank].bank_size = 0;

        ////

        // NOTE: vblock #0,1 don't use for user space
        write_dram_16(VCOUNT_ADDR + ((bank * VBLKS_PER_BANK) + 0) * sizeof(UINT16), VC_MAX);
        write_dram_16(VCOUNT_ADDR + ((bank * VBLKS_PER_BANK) + 1) * sizeof(UINT16), VC_MAX);

        //----------------------------------------
        // assign misc. block
        //----------------------------------------
        // assumption: vblock #1 = fixed location.
        // Thus if vblock #1 is a bad block, it should be allocate another block.
        set_miscblk_vpn(bank, MISCBLK_VBN * PAGES_PER_BLK - 1);
        ASSERT(is_bad_block(bank, MISCBLK_VBN) == FALSE);

        vblock = MISCBLK_VBN;

        //----------------------------------------
        // assign map block
        //----------------------------------------
        mapblk_lbn = 0;
        while (mapblk_lbn < MAPBLKS_PER_BANK)
        {
            vblock++;
            ASSERT(vblock < VBLKS_PER_BANK);
            if (is_bad_block(bank, vblock) == FALSE)
            {
                set_mapblk_vpn(bank, mapblk_lbn, vblock * PAGES_PER_BLK);
                write_dram_16(VCOUNT_ADDR + ((bank * VBLKS_PER_BANK) + vblock) * sizeof(UINT16), VC_MAX);
                mapblk_lbn++;
            }
        }
        //----------------------------------------
        // assign free block for gc
        //----------------------------------------
        do
        {
            vblock++;
            // NOTE: free block should not be secleted as a victim @ first GC
            write_dram_16(VCOUNT_ADDR + ((bank * VBLKS_PER_BANK) + vblock) * sizeof(UINT16), VC_MAX);
            // set free block
            set_gc_vblock(bank, vblock);

            ASSERT(vblock < VBLKS_PER_BANK);
        }
        while(is_bad_block(bank, vblock) == TRUE);
        do
        {
            vblock++;
            // NOTE: free block should not be secleted as a victim @ first GC
            write_dram_16(VCOUNT_ADDR + ((bank * VBLKS_PER_BANK) + vblock) * sizeof(UINT16), VC_MAX);
            // set free block
            g_misc_meta[bank].gc_backup = vblock;

            ASSERT(vblock < VBLKS_PER_BANK);
        }
        while(is_bad_block(bank, vblock) == TRUE);
        do
        {
            vblock++;
            // NOTE: free block should not be secleted as a victim @ first GC
            write_dram_16(VCOUNT_ADDR + ((bank * VBLKS_PER_BANK) + vblock) * sizeof(UINT16), VC_MAX);
            // set free block
            g_misc_meta[bank].gc_backup_for_gc= vblock;

            ASSERT(vblock < VBLKS_PER_BANK);
        }
        while(is_bad_block(bank, vblock) == TRUE);
        //----------------------------------------
        // assign free vpn for first new write
        //----------------------------------------
        do
        {
            vblock++;
            // ?�재 next vblock부???��????�이?�를 ?�?��? ?��?
            set_new_write_vpn(bank, vblock * PAGES_PER_BLK);
            ASSERT(vblock < VBLKS_PER_BANK);
        }
        while(is_bad_block(bank, vblock) == TRUE);
    }
}
// logging misc + vcount metadata
static void logging_misc_metadata(void)
{
    UINT32 misc_meta_bytes = NUM_MISC_META_SECT * BYTES_PER_SECTOR; // per bank
    UINT32 vcount_addr     = VCOUNT_ADDR;
    UINT32 vcount_bytes    = NUM_VCOUNT_SECT * BYTES_PER_SECTOR; // per bank
    UINT32 vcount_boundary = VCOUNT_ADDR + VCOUNT_BYTES; // entire vcount data
    UINT32 bank;

    flash_finish();

    for (bank = 0; bank < NUM_BANKS; bank++)
    {
        inc_miscblk_vpn(bank);

        // note: if misc. meta block is full, just erase old block & write offset #0
        if ((get_miscblk_vpn(bank) / PAGES_PER_BLK) != MISCBLK_VBN)
        {
            nand_block_erase(bank, MISCBLK_VBN);
            set_miscblk_vpn(bank, MISCBLK_VBN * PAGES_PER_BLK); // vpn = 128
        }
        // copy misc. metadata to FTL buffer
        mem_copy(FTL_BUF(bank), &g_misc_meta[bank], misc_meta_bytes);

        // copy vcount metadata to FTL buffer
        if (vcount_addr <= vcount_boundary)
        {
            mem_copy(FTL_BUF(bank) + misc_meta_bytes, vcount_addr, vcount_bytes);
            vcount_addr += vcount_bytes;
        }
    }
    // logging the misc. metadata to nand flash
    for (bank = 0; bank < NUM_BANKS; bank++)
    {
        nand_page_ptprogram(bank,
                            get_miscblk_vpn(bank) / PAGES_PER_BLK,
                            get_miscblk_vpn(bank) % PAGES_PER_BLK,
                            0,
                            NUM_MISC_META_SECT + NUM_VCOUNT_SECT,
                            FTL_BUF(bank));
    }
    flash_finish();
}
static void logging_pmap_table(void)
{
    UINT32 pmap_addr  = PAGE_MAP_ADDR;
    UINT32 pmap_bytes = BYTES_PER_PAGE; // per bank
    UINT32 mapblk_vpn;
    UINT32 bank;
    UINT32 pmap_boundary = PAGE_MAP_ADDR + PAGE_MAP_BYTES;
    BOOL32 finished = FALSE;

    for (UINT32 mapblk_lbn = 0; mapblk_lbn < MAPBLKS_PER_BANK; mapblk_lbn++)
    {
        flash_finish();

        for (bank = 0; bank < NUM_BANKS; bank++)
        {
            if (finished)
            {
                break;
            }
            else if (pmap_addr >= pmap_boundary)
            {
                finished = TRUE;
                break;
            }
            else if (pmap_addr + BYTES_PER_PAGE >= pmap_boundary)
            {
                finished = TRUE;
                pmap_bytes = (pmap_boundary - pmap_addr + BYTES_PER_SECTOR - 1) / BYTES_PER_SECTOR * BYTES_PER_SECTOR ;
            }
            inc_mapblk_vpn(bank, mapblk_lbn);

            mapblk_vpn = get_mapblk_vpn(bank, mapblk_lbn);

            // note: if there is no free page, then erase old map block first.
            if ((mapblk_vpn % PAGES_PER_BLK) == 0)
            {
                // erase full map block
                nand_block_erase(bank, (mapblk_vpn - 1) / PAGES_PER_BLK);

                // next vpn of mapblk is offset #0
                set_mapblk_vpn(bank, mapblk_lbn, ((mapblk_vpn - 1) / PAGES_PER_BLK) * PAGES_PER_BLK);
                mapblk_vpn = get_mapblk_vpn(bank, mapblk_lbn);
            }
            // copy the page mapping table to FTL buffer
            mem_copy(FTL_BUF(bank), pmap_addr, pmap_bytes);

            // logging update page mapping table into map_block
            nand_page_ptprogram(bank,
                                mapblk_vpn / PAGES_PER_BLK,
                                mapblk_vpn % PAGES_PER_BLK,
                                0,
                                pmap_bytes / BYTES_PER_SECTOR,
                                FTL_BUF(bank));
            pmap_addr += pmap_bytes;
        }
        if (finished)
        {
            break;
        }
    }
    flash_finish();
}
// load flushed FTL metadta
static void load_metadata(void)
{
    load_misc_metadata();
    load_pmap_table();
}
// misc + VCOUNT
static void load_misc_metadata(void)
{
    UINT32 misc_meta_bytes = NUM_MISC_META_SECT * BYTES_PER_SECTOR;
    UINT32 vcount_bytes    = NUM_VCOUNT_SECT * BYTES_PER_SECTOR;
    UINT32 vcount_addr     = VCOUNT_ADDR;
    UINT32 vcount_boundary = VCOUNT_ADDR + VCOUNT_BYTES;

    UINT32 load_flag = 0;
    UINT32 bank, page_num;
    UINT32 load_cnt = 0;

    flash_finish();

    disable_irq();
    flash_clear_irq();	// clear any flash interrupt flags that might have been set

    // scan valid metadata in descending order from last page offset
    for (page_num = PAGES_PER_BLK - 1; page_num != ((UINT32) -1); page_num--)
    {
        for (bank = 0; bank < NUM_BANKS; bank++)
        {
            if (load_flag & (0x1 << bank))
            {
                continue;
            }
            // read valid metadata from misc. metadata area
            nand_page_ptread(bank,
                             MISCBLK_VBN,
                             page_num,
                             0,
                             NUM_MISC_META_SECT + NUM_VCOUNT_SECT,
                             FTL_BUF(bank),
                             RETURN_ON_ISSUE);
        }
        flash_finish();

        for (bank = 0; bank < NUM_BANKS; bank++)
        {
            if (!(load_flag & (0x1 << bank)) && !(BSP_INTR(bank) & FIRQ_ALL_FF))
            {
                load_flag = load_flag | (0x1 << bank);
                load_cnt++;
            }
            CLR_BSP_INTR(bank, 0xFF);
        }
    }
    ASSERT(load_cnt == NUM_BANKS);

    for (bank = 0; bank < NUM_BANKS; bank++)
    {
        // misc. metadata
        mem_copy(&g_misc_meta[bank], FTL_BUF(bank), sizeof(misc_metadata));

        // vcount metadata
        if (vcount_addr <= vcount_boundary)
        {
            mem_copy(vcount_addr, FTL_BUF(bank) + misc_meta_bytes, vcount_bytes);
            vcount_addr += vcount_bytes;

        }
    }
    enable_irq();
}
static void load_pmap_table(void)
{
    UINT32 pmap_addr = PAGE_MAP_ADDR;
    UINT32 temp_page_addr;
    UINT32 pmap_bytes = BYTES_PER_PAGE; // per bank
    UINT32 pmap_boundary = PAGE_MAP_ADDR + (NUM_LPAGES * sizeof(UINT32));
    UINT32 mapblk_lbn, bank;
    BOOL32 finished = FALSE;

    flash_finish();

    for (mapblk_lbn = 0; mapblk_lbn < MAPBLKS_PER_BANK; mapblk_lbn++)
    {
        temp_page_addr = pmap_addr; // backup page mapping addr

        for (bank = 0; bank < NUM_BANKS; bank++)
        {
            if (finished)
            {
                break;
            }
            else if (pmap_addr >= pmap_boundary)
            {
                finished = TRUE;
                break;
            }
            else if (pmap_addr + BYTES_PER_PAGE >= pmap_boundary)
            {
                finished = TRUE;
                pmap_bytes = (pmap_boundary - pmap_addr + BYTES_PER_SECTOR - 1) / BYTES_PER_SECTOR * BYTES_PER_SECTOR;
            }
            // read page mapping table from map_block
            nand_page_ptread(bank,
                             get_mapblk_vpn(bank, mapblk_lbn) / PAGES_PER_BLK,
                             get_mapblk_vpn(bank, mapblk_lbn) % PAGES_PER_BLK,
                             0,
                             pmap_bytes / BYTES_PER_SECTOR,
                             FTL_BUF(bank),
                             RETURN_ON_ISSUE);
            pmap_addr += pmap_bytes;
        }
        flash_finish();

        pmap_bytes = BYTES_PER_PAGE;
        for (bank = 0; bank < NUM_BANKS; bank++)
        {
            if (temp_page_addr >= pmap_boundary)
            {
                break;
            }
            else if (temp_page_addr + BYTES_PER_PAGE >= pmap_boundary)
            {
                pmap_bytes = (pmap_boundary - temp_page_addr + BYTES_PER_SECTOR - 1) / BYTES_PER_SECTOR * BYTES_PER_SECTOR;
            }
            // copy page mapping table to PMAP_ADDR from FTL buffer
            mem_copy(temp_page_addr, FTL_BUF(bank), pmap_bytes);

            temp_page_addr += pmap_bytes;
        }
        if (finished)
        {
            break;
        }
    }
}
static void write_format_mark(void)
{
    // This function writes a format mark to a page at (bank #0, block #0).

#ifdef __GNUC__
    extern UINT32 size_of_firmware_image;
    UINT32 firmware_image_pages = (((UINT32) (&size_of_firmware_image)) + BYTES_PER_FW_PAGE - 1) / BYTES_PER_FW_PAGE;
#else
    extern UINT32 Image$$ER_CODE$$RO$$Length;
    extern UINT32 Image$$ER_RW$$RW$$Length;
    UINT32 firmware_image_bytes = ((UINT32) &Image$$ER_CODE$$RO$$Length) + ((UINT32) &Image$$ER_RW$$RW$$Length);
    UINT32 firmware_image_pages = (firmware_image_bytes + BYTES_PER_FW_PAGE - 1) / BYTES_PER_FW_PAGE;
#endif

    UINT32 format_mark_page_offset = FW_PAGE_OFFSET + firmware_image_pages;

    mem_set_dram(FTL_BUF_ADDR, 0, BYTES_PER_SECTOR);

    SETREG(FCP_CMD, FC_COL_ROW_IN_PROG);
    SETREG(FCP_BANK, REAL_BANK(0));
    SETREG(FCP_OPTION, FO_E | FO_B_W_DRDY);
    SETREG(FCP_DMA_ADDR, FTL_BUF_ADDR); 	// DRAM -> flash
    SETREG(FCP_DMA_CNT, BYTES_PER_SECTOR);
    SETREG(FCP_COL, 0);
    SETREG(FCP_ROW_L(0), format_mark_page_offset);
    SETREG(FCP_ROW_H(0), format_mark_page_offset);

    // At this point, we do not have to check Waiting Room status before issuing a command,
    // because we have waited for all the banks to become idle before returning from format().
    SETREG(FCP_ISSUE, NULL);

    // wait for the FC_COL_ROW_IN_PROG command to be accepted by bank #0
    while ((GETREG(WR_STAT) & 0x00000001) != 0);

    // wait until bank #0 finishes the write operation
    while (BSP_FSM(0) != BANK_IDLE);
}
static BOOL32 check_format_mark(void)
{
    // This function reads a flash page from (bank #0, block #0) in order to check whether the SSD is formatted or not.

#ifdef __GNUC__
    extern UINT32 size_of_firmware_image;
    UINT32 firmware_image_pages = (((UINT32) (&size_of_firmware_image)) + BYTES_PER_FW_PAGE - 1) / BYTES_PER_FW_PAGE;
#else
    extern UINT32 Image$$ER_CODE$$RO$$Length;
    extern UINT32 Image$$ER_RW$$RW$$Length;
    UINT32 firmware_image_bytes = ((UINT32) &Image$$ER_CODE$$RO$$Length) + ((UINT32) &Image$$ER_RW$$RW$$Length);
    UINT32 firmware_image_pages = (firmware_image_bytes + BYTES_PER_FW_PAGE - 1) / BYTES_PER_FW_PAGE;
#endif

    UINT32 format_mark_page_offset = FW_PAGE_OFFSET + firmware_image_pages;
    UINT32 temp;

    flash_clear_irq();	// clear any flash interrupt flags that might have been set

    SETREG(FCP_CMD, FC_COL_ROW_READ_OUT);
    SETREG(FCP_BANK, REAL_BANK(0));
    SETREG(FCP_OPTION, FO_E);
    SETREG(FCP_DMA_ADDR, FTL_BUF_ADDR); 	// flash -> DRAM
    SETREG(FCP_DMA_CNT, BYTES_PER_SECTOR);
    SETREG(FCP_COL, 0);
    SETREG(FCP_ROW_L(0), format_mark_page_offset);
    SETREG(FCP_ROW_H(0), format_mark_page_offset);

    // At this point, we do not have to check Waiting Room status before issuing a command,
    // because scan list loading has been completed just before this function is called.
    SETREG(FCP_ISSUE, NULL);

    // wait for the FC_COL_ROW_READ_OUT command to be accepted by bank #0
    while ((GETREG(WR_STAT) & 0x00000001) != 0);

    // wait until bank #0 finishes the read operation
    while (BSP_FSM(0) != BANK_IDLE);

    // Now that the read operation is complete, we can check interrupt flags.
    temp = BSP_INTR(0) & FIRQ_ALL_FF;

    // clear interrupt flags
    CLR_BSP_INTR(0, 0xFF);

    if (temp != 0)
    {
        return FALSE;	// the page contains all-0xFF (the format mark does not exist.)
    }
    else
    {
        return TRUE;	// the page contains something other than 0xFF (it must be the format mark)
    }
}

// BSP interrupt service routine
void ftl_isr(void)
{
    UINT32 bank;
    UINT32 bsp_intr_flag;

    uart_print("BSP interrupt occured...");
    // interrupt pending clear (ICU)
    SETREG(APB_INT_STS, INTR_FLASH);

    for (bank = 0; bank < NUM_BANKS; bank++)
    {
        while (BSP_FSM(bank) != BANK_IDLE);
        // get interrupt flag from BSP
        bsp_intr_flag = BSP_INTR(bank);

        if (bsp_intr_flag == 0)
        {
            continue;
        }
        UINT32 fc = GETREG(BSP_CMD(bank));
        // BSP clear
        CLR_BSP_INTR(bank, bsp_intr_flag);

        // interrupt handling
        if (bsp_intr_flag & FIRQ_DATA_CORRUPT)
        {
            uart_printf("BSP interrupt at bank: 0x%x", bank);
            uart_print("FIRQ_DATA_CORRUPT occured...");
        }
        if (bsp_intr_flag & (FIRQ_BADBLK_H | FIRQ_BADBLK_L))
        {
            uart_printf("BSP interrupt at bank: 0x%x", bank);
            if (fc == FC_COL_ROW_IN_PROG || fc == FC_IN_PROG || fc == FC_PROG)
            {
                uart_print("find runtime bad block when block program...");
            }
            else
            {
                uart_printf("find runtime bad block when block erase...vblock #: %d", GETREG(BSP_ROW_H(bank)) / PAGES_PER_BLK);
                ASSERT(fc == FC_ERASE);
            }
        }
    }
}
