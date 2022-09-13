/*
 * test_common.h
 *
 *  Created on: 2021Äê1ÔÂ23ÈÕ
 *      Author: Administrator
 */

#ifndef TEST_COMMON_H
#define TEST_COMMON_H

#ifdef __cplusplus

extern "C" {

#endif

#define FPGA_95P          1
#define IDMA_DRAM0                     __attribute__ ((section(".dram0.data")))
#define IDMA_DRAM1                     __attribute__ ((section(".dram1.data")))
#define DRAM0_USER                     __attribute__ ((section(".dram0_user.bss")))
#define DRAM1_USER                     __attribute__ ((section(".dram1_user.bss")))
#define SIZE_PER_DRAM                  128*1024
#define ALIGNTO64

typedef enum{

  	MEMORY_ACCESS_RD = 0,
  	MEMORY_ACCESS_WR,
  	MEMORY_ACCESS_WD

}direction;


typedef struct{
	char* test;
	union{
		int      reference_int;
		float    reference_float;
	};
}testReference;


extern char _memmap_mem_sram_start;
extern char _memmap_seg_sram1_end;
extern char _memmap_mem_sram_end;
extern char _memmap_mem_srom_start;
extern char _memmap_mem_srom_end;
extern char _memmap_mem_sram_part2_start;
extern char _memmap_mem_sram_part2_end;
extern char _memmap_mem_sram_part3_start;
extern char _memmap_mem_sram_part3_end;
extern char _memmap_mem_dram0_end;
extern char _memmap_mem_dram1_end;

extern char * allocat_memory_from_dram_user(int dram_sg,unsigned int size);


#ifdef __cplusplus

}

#endif


#endif /*  */
