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
// GreedyFTL header file
//
// Author; Sang-Phil Lim (SKKU VLDB Lab.)
//

#ifndef FTL_H
#define FTL_H


/////////////////
// DRAM buffers
/////////////////

#define NUM_RW_BUFFERS		((DRAM_SIZE - DRAM_BYTES_OTHER) / BYTES_PER_PAGE - 1)
#define NUM_RD_BUFFERS		(((NUM_RW_BUFFERS / 8) + NUM_BANKS - 1) / NUM_BANKS * NUM_BANKS)
#define NUM_WR_BUFFERS		(NUM_RW_BUFFERS - NUM_RD_BUFFERS)
#define NUM_COPY_BUFFERS	NUM_BANKS_MAX
#define NUM_FTL_BUFFERS		NUM_BANKS
#define NUM_HIL_BUFFERS		1
#define NUM_TEMP_BUFFERS	1

#define real_size        	768
#define LRU_BUF_SIZE		1280
#define RMW_flag			2			// 1 : RMW_buffer  2 : RMW_evict
#define static_binding		1

#define NUM_LRUBUFFER		LRU_BUF_SIZE
#define HASH_TABLE_SIZE		(LRU_BUF_SIZE*2)
#define BLK_TABLE_SIZE		(LRU_BUF_SIZE*2)

#define Q_DEPTH				1
#define P_MAX				(Q_DEPTH * NUM_BANKS)

#define DRAM_BYTES_OTHER	((NUM_COPY_BUFFERS + NUM_FTL_BUFFERS + NUM_HIL_BUFFERS + NUM_TEMP_BUFFERS) * BYTES_PER_PAGE \
+ BAD_BLK_BMP_BYTES + PAGE_MAP_BYTES + VCOUNT_BYTES + LRU_BYTES + HASH_TABLE_BYTES\
+ BLK_TABLE_BYTES + gc_last_page_BYTES+bank_list_BYTES+last_page_BYTES+wbuf_vpage_BYTES+RMW_page_BYTES\
+ RMW_target_bank_BYTES + lru_list_lpn_BYTES + lru_list_next_BYTES + lru_list_prev_BYTES + lru_list_bank_prev_BYTES + lru_list_bank_next_BYTES + lru_list_in_cap_BYTES\
+ P_TYPE_BYTES + P_BANK_BYTES + P_VBLOCK_BYTES + P_PAGE_NUM_BYTES + P_SECT_OFFSET_BYTES + P_NUM_SECTORS_BYTES + P_BUF_ADDR_BYTES + P_G_FTL_RW_BUF_ID_BYTES\
+ P_SRC_VBLOCK_BYTES + P_SRC_PAGE_BYTES + P_DST_VBLOCK_BYTES + P_DST_PAGE_BYTES + P_NEXT_NODE_BYTES + P_PREV_NODE_BYTES + P_ISSUE_FLAG_BYTES + P_LPN_BYTES + P_ID_BYTES + P_BANK_STATE_BYTES\
+ LPN_LIST_OF_CUR_VBLOCK_BYTES + LPN_LIST_OF_GC_VBLOCK_BYTES)
//+ P_CAP_BUFFER_BYTES + WBUF_VDATA_BYTES + P_CAP_BITMAP_BYTES + P_CAP_R_BUFFER_BYTES + P_CAP_W_BUFFER_BYTES + P_P2L_BUFFER_BYTES + P_L2P_BUFFER_BYTES)

#define WR_BUF_PTR(BUF_ID)	(WR_BUF_ADDR + ((UINT32)(BUF_ID)) * BYTES_PER_PAGE)
#define WR_BUF_ID(BUF_PTR)	((((UINT32)BUF_PTR) - WR_BUF_ADDR) / BYTES_PER_PAGE)
#define RD_BUF_PTR(BUF_ID)	(RD_BUF_ADDR + ((UINT32)(BUF_ID)) * BYTES_PER_PAGE)
#define RD_BUF_ID(BUF_PTR)	((((UINT32)BUF_PTR) - RD_BUF_ADDR) / BYTES_PER_PAGE)

#define _COPY_BUF(RBANK)	(COPY_BUF_ADDR + (RBANK) * BYTES_PER_PAGE)
#define COPY_BUF(BANK)		_COPY_BUF(REAL_BANK(BANK))
#define FTL_BUF(BANK)       (FTL_BUF_ADDR + ((BANK) * BYTES_PER_PAGE))

///////////////////////////////
// DRAM segmentation
///////////////////////////////

#define RD_BUF_ADDR			DRAM_BASE										// base address of SATA read buffers
#define RD_BUF_BYTES		(NUM_RD_BUFFERS * BYTES_PER_PAGE)

#define WR_BUF_ADDR			(RD_BUF_ADDR + RD_BUF_BYTES)					// base address of SATA write buffers
#define WR_BUF_BYTES		(NUM_WR_BUFFERS * BYTES_PER_PAGE)

#define COPY_BUF_ADDR		(WR_BUF_ADDR + WR_BUF_BYTES)					// base address of flash copy buffers
#define COPY_BUF_BYTES		(NUM_COPY_BUFFERS * BYTES_PER_PAGE)

#define FTL_BUF_ADDR		(COPY_BUF_ADDR + COPY_BUF_BYTES)				// a buffer dedicated to FTL internal purpose
#define FTL_BUF_BYTES		(NUM_FTL_BUFFERS * BYTES_PER_PAGE)

#define HIL_BUF_ADDR		(FTL_BUF_ADDR + FTL_BUF_BYTES)					// a buffer dedicated to HIL internal purpose
#define HIL_BUF_BYTES		(NUM_HIL_BUFFERS * BYTES_PER_PAGE)

#define TEMP_BUF_ADDR		(HIL_BUF_ADDR + HIL_BUF_BYTES)					// general purpose buffer
#define TEMP_BUF_BYTES		(NUM_TEMP_BUFFERS * BYTES_PER_PAGE)

#define BAD_BLK_BMP_ADDR	(TEMP_BUF_ADDR + TEMP_BUF_BYTES)				// bitmap of initial bad blocks
#define BAD_BLK_BMP_BYTES	(((NUM_VBLKS / 8) + DRAM_ECC_UNIT - 1) / DRAM_ECC_UNIT * DRAM_ECC_UNIT)

#define PAGE_MAP_ADDR		(BAD_BLK_BMP_ADDR + BAD_BLK_BMP_BYTES)			// page mapping table
#define PAGE_MAP_BYTES		((NUM_LPAGES * sizeof(UINT32) + BYTES_PER_SECTOR - 1) / BYTES_PER_SECTOR * BYTES_PER_SECTOR)

#define VCOUNT_ADDR			(PAGE_MAP_ADDR + PAGE_MAP_BYTES)
#define VCOUNT_BYTES		((NUM_BANKS * VBLKS_PER_BANK * sizeof(UINT16) + BYTES_PER_SECTOR - 1) / BYTES_PER_SECTOR * BYTES_PER_SECTOR)

#define LRU_ADDR			(VCOUNT_ADDR + VCOUNT_BYTES)
#define LRU_BYTES			(LRU_BUF_SIZE * BYTES_PER_PAGE)

#define HASH_TABLE_ADDR		(LRU_ADDR + LRU_BYTES  )
#define HASH_TABLE_BYTES	((NUM_LPAGES * sizeof(UINT16) + BYTES_PER_SECTOR - 1 ) / BYTES_PER_SECTOR * BYTES_PER_SECTOR)
//#define HASH_TABLE_BYTES	((HASH_TABLE_SIZE * sizeof(UINT32) * 2 + BYTES_PER_SECTOR - 1) / BYTES_PER_SECTOR * BYTES_PER_SECTOR)

#define BLK_TABLE_ADDR		(HASH_TABLE_ADDR + HASH_TABLE_BYTES  )
#define BLK_TABLE_BYTES		(((NUM_VBLKS)  * sizeof(UINT32) + BYTES_PER_SECTOR - 1) / BYTES_PER_SECTOR * BYTES_PER_SECTOR)
//#define BLK_TABLE_BYTES		((BLK_TABLE_SIZE * sizeof(UINT32) * 2 + BYTES_PER_SECTOR - 1) / BYTES_PER_SECTOR * BYTES_PER_SECTOR)
// #define BLKS_PER_BANK		VBLKS_PER_BANK
#define gc_last_page_ADDR  (BLK_TABLE_ADDR + BLK_TABLE_BYTES)
#define gc_last_page_BYTES (BYTES_PER_PAGE * NUM_BANKS)


#define bank_list_ADDR           (gc_last_page_ADDR + gc_last_page_BYTES)
#define bank_list_BYTES			 ((NUM_LPAGES * sizeof(UINT32)+ BYTES_PER_SECTOR - 1) / BYTES_PER_SECTOR * BYTES_PER_SECTOR)

#define last_page_ADDR			(bank_list_ADDR+ bank_list_BYTES)
#define last_page_BYTES		((NUM_BANKS * PAGES_PER_BLK * sizeof(UINT32)+ BYTES_PER_SECTOR - 1) / BYTES_PER_SECTOR * BYTES_PER_SECTOR)

#define wbuf_vpage_ADDR		(last_page_ADDR+ last_page_BYTES)
#define wbuf_vpage_BYTES		(((NUM_LRUBUFFER * SECTORS_PER_PAGE / 8)+ BYTES_PER_SECTOR - 1)/ BYTES_PER_SECTOR * BYTES_PER_SECTOR)

#define RMW_page_ADDR		(wbuf_vpage_ADDR + wbuf_vpage_BYTES)
#define	RMW_page_BYTES		((NUM_BANKS* sizeof(UINT32)+ BYTES_PER_SECTOR - 1) / BYTES_PER_SECTOR * BYTES_PER_SECTOR)

#define RMW_target_bank_ADDR   (RMW_page_ADDR + RMW_page_BYTES)
#define RMW_target_bank_BYTES	 ((NUM_LRUBUFFER* sizeof(UINT32)+ BYTES_PER_SECTOR - 1) / BYTES_PER_SECTOR * BYTES_PER_SECTOR)

#define lru_list_lpn_ADDR    (RMW_target_bank_ADDR + RMW_target_bank_BYTES)
#define lru_list_lpn_BYTES	((NUM_LRUBUFFER * sizeof(UINT32) + 127) / 128 * 128)

#define lru_list_prev_ADDR   (lru_list_lpn_ADDR + lru_list_lpn_BYTES)
#define lru_list_prev_BYTES  ((NUM_LRUBUFFER * sizeof(UINT16) + 127) / 128 * 128)

#define lru_list_next_ADDR   (lru_list_prev_ADDR + lru_list_prev_BYTES)
#define lru_list_next_BYTES  ((NUM_LRUBUFFER * sizeof(UINT16) + 127) / 128 * 128)


#define lru_list_bank_prev_ADDR  (lru_list_next_ADDR + lru_list_next_BYTES)
#define lru_list_bank_prev_BYTES ((NUM_LRUBUFFER * sizeof(UINT16) + 127) / 128 * 128)

#define lru_list_bank_next_ADDR  (lru_list_bank_prev_ADDR + lru_list_bank_prev_BYTES)
#define lru_list_bank_next_BYTES ((NUM_LRUBUFFER * sizeof(UINT16) + 127) / 128 * 128)

#define lru_list_in_cap_ADDR	(lru_list_bank_next_ADDR + lru_list_bank_next_BYTES)
#define lru_list_in_cap_BYTES	((NUM_LRUBUFFER * sizeof(UINT8) + 127) / 128 * 128)

//////////////for capping////////////////////////

#define P_TYPE_ADDR			(lru_list_in_cap_ADDR + lru_list_in_cap_BYTES )
#define P_TYPE_BYTES		(sizeof(UINT8) * P_MAX + BYTES_PER_SECTOR - 1) / BYTES_PER_SECTOR * BYTES_PER_SECTOR

#define P_BANK_ADDR			(P_TYPE_ADDR + P_TYPE_BYTES)
#define P_BANK_BYTES		(sizeof(UINT8) * P_MAX + BYTES_PER_SECTOR - 1) / BYTES_PER_SECTOR * BYTES_PER_SECTOR

#define P_VBLOCK_ADDR		(P_BANK_ADDR + P_BANK_BYTES)
#define P_VBLOCK_BYTES		(sizeof(UINT32) * P_MAX + BYTES_PER_SECTOR - 1) / BYTES_PER_SECTOR * BYTES_PER_SECTOR

#define P_PAGE_NUM_ADDR		(P_VBLOCK_ADDR + P_VBLOCK_BYTES)
#define P_PAGE_NUM_BYTES	(sizeof(UINT32) * P_MAX + BYTES_PER_SECTOR - 1) / BYTES_PER_SECTOR * BYTES_PER_SECTOR

#define P_SECT_OFFSET_ADDR	(P_PAGE_NUM_ADDR + P_PAGE_NUM_BYTES)
#define P_SECT_OFFSET_BYTES	(sizeof(UINT32) * P_MAX + BYTES_PER_SECTOR - 1) / BYTES_PER_SECTOR * BYTES_PER_SECTOR

#define P_NUM_SECTORS_ADDR	(P_SECT_OFFSET_ADDR + P_SECT_OFFSET_BYTES)
#define P_NUM_SECTORS_BYTES	(sizeof(UINT32) * P_MAX + BYTES_PER_SECTOR - 1) / BYTES_PER_SECTOR * BYTES_PER_SECTOR

#define P_BUF_ADDR				(P_NUM_SECTORS_ADDR + P_NUM_SECTORS_BYTES)
#define P_BUF_ADDR_BYTES		(sizeof(UINT32) * P_MAX + BYTES_PER_SECTOR - 1) / BYTES_PER_SECTOR * BYTES_PER_SECTOR

#define P_G_FTL_RW_BUF_ID_ADDR	(P_BUF_ADDR + P_BUF_ADDR_BYTES)
#define P_G_FTL_RW_BUF_ID_BYTES	(sizeof(UINT32) * P_MAX + BYTES_PER_SECTOR - 1) / BYTES_PER_SECTOR * BYTES_PER_SECTOR

//for copyback
#define P_SRC_VBLOCK_ADDR	(P_G_FTL_RW_BUF_ID_ADDR + P_G_FTL_RW_BUF_ID_BYTES)
#define P_SRC_VBLOCK_BYTES	(sizeof(UINT32) * P_MAX + BYTES_PER_SECTOR - 1) / BYTES_PER_SECTOR * BYTES_PER_SECTOR

#define P_SRC_PAGE_ADDR		(P_SRC_VBLOCK_ADDR + P_SRC_VBLOCK_BYTES)
#define P_SRC_PAGE_BYTES	(sizeof(UINT32) * P_MAX + BYTES_PER_SECTOR - 1) / BYTES_PER_SECTOR * BYTES_PER_SECTOR

#define P_DST_VBLOCK_ADDR	(P_SRC_PAGE_ADDR + P_SRC_PAGE_BYTES)
#define P_DST_VBLOCK_BYTES	(sizeof(UINT32) * P_MAX + BYTES_PER_SECTOR - 1) / BYTES_PER_SECTOR * BYTES_PER_SECTOR

#define P_DST_PAGE_ADDR		(P_DST_VBLOCK_ADDR + P_DST_VBLOCK_BYTES)
#define P_DST_PAGE_BYTES	(sizeof(UINT32) * P_MAX + BYTES_PER_SECTOR - 1) / BYTES_PER_SECTOR * BYTES_PER_SECTOR

#define P_NEXT_NODE_ADDR	(P_DST_PAGE_ADDR + P_DST_PAGE_BYTES)
#define P_NEXT_NODE_BYTES	(sizeof(UINT16) * P_MAX + BYTES_PER_SECTOR - 1) / BYTES_PER_SECTOR * BYTES_PER_SECTOR

#define P_PREV_NODE_ADDR	(P_NEXT_NODE_ADDR + P_NEXT_NODE_BYTES)
#define P_PREV_NODE_BYTES	(sizeof(UINT16) * P_MAX + BYTES_PER_SECTOR - 1) / BYTES_PER_SECTOR * BYTES_PER_SECTOR

#define P_ISSUE_FLAG_ADDR	(P_PREV_NODE_ADDR + P_PREV_NODE_BYTES)
#define P_ISSUE_FLAG_BYTES	(sizeof(UINT8) * P_MAX + BYTES_PER_SECTOR - 1) / BYTES_PER_SECTOR * BYTES_PER_SECTOR

#define P_LPN_ADDR	(P_ISSUE_FLAG_ADDR + P_ISSUE_FLAG_BYTES)
#define P_LPN_BYTES	((sizeof(UINT32) * P_MAX + 127) / 128 * 128)

#define P_ID_ADDR	(P_LPN_ADDR + P_LPN_BYTES)
#define P_ID_BYTES	((sizeof(UINT16) * P_MAX + BYTES_PER_SECTOR - 1) / BYTES_PER_SECTOR * BYTES_PER_SECTOR)

#define P_BANK_STATE_ADDR	(P_ID_ADDR + P_ID_BYTES)
#define P_BANK_STATE_BYTES	((sizeof(UINT8) * NUM_BANKS + 127) /128 * 128)

#define LPN_LIST_OF_CUR_VBLOCK_ADDR (P_BANK_STATE_ADDR + P_BANK_STATE_BYTES)
#define LPN_LIST_OF_CUR_VBLOCK_BYTES (sizeof(UINT32) * PAGES_PER_BLK)

#define LPN_LIST_OF_GC_VBLOCK_ADDR (LPN_LIST_OF_CUR_VBLOCK_ADDR + LPN_LIST_OF_CUR_VBLOCK_BYTES)
#define LPN_LIST_OF_GC_VBLOCK_BYTES (sizeof(UINT32) * PAGES_PER_BLK)
/*
#define P_CAP_BUFFER_ADDR	(P_LPN_ADDR + P_LPN_BYTES)
#define P_CAP_BUFFER_BYTES	NUM_BANKS * BYTES_PER_PAGE

//write_buffer_valid_sector_bitmap
#define WBUF_VDATA_ADDR		(P_CAP_BUFFER_ADDR + P_CAP_BUFFER_BYTES)
#define WBUF_VDATA_BYTES	(((NUM_LRUBUFFER * SECTORS_PER_PAGE / 8) + BYTES_PER_SECTOR - 1) / BYTES_PER_SECTOR * BYTES_PER_SECTOR)

#define P_CAP_R_BUFFER_ADDR	(WBUF_VDATA_ADDR + WBUF_VDATA_BYTES)
#define P_CAP_R_BUFFER_BYTES	NUM_BANKS * BYTES_PER_PAGE

#define P_CAP_W_BUFFER_ADDR	(P_CAP_R_BUFFER_ADDR + P_CAP_R_BUFFER_BYTES)
#define P_CAP_W_BUFFER_BYTES	NUM_BANKS * BYTES_PER_PAGE

#define P_CAP_BITMAP_ADDR	(P_CAP_W_BUFFER_ADDR + P_CAP_W_BUFFER_BYTES)
#define P_CAP_BITMAP_BYTES	(((NUM_BANKS * SECTORS_PER_PAGE / 8) + BYTES_PER_SECTOR - 1) / BYTES_PER_SECTOR * BYTES_PER_SECTOR)

#define P_P2L_BUFFER_ADDR P_CAP_BITMAP_ADDR + P_CAP_BITMAP_BYTES
#define P_P2L_BUFFER_BYTES (NUM_FTL_BUFFERS * BYTES_PER_PAGE)

#define P_L2P_BUFFER_ADDR P_P2L_BUFFER_ADDR + P_P2L_BUFFER_BYTES
#define P_L2P_BUFFER_BYTES (NUM_FTL_BUFFERS * BYTES_PER_PAGE)
*/

///////////////////////////////
// FTL public functions
///////////////////////////////

void ftl_open(void);
void ftl_read(UINT32 const lba, UINT32 const num_sectors);
void ftl_write(UINT32 const lba, UINT32 const num_sectors);
void ftl_test_write(UINT32 const lba, UINT32 const num_sectors);
void ftl_flush(void);
void ftl_isr(void);

#endif //FTL_H
