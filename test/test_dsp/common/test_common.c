/*
 * test_common.c
 *
 *  Created on: 2021��1��23��
 *      Author: Administrator
 */
#include "test_common.h"
#include <stdio.h>
char    dram0_user_start __attribute__ ((section(".dram0_user.bss")));
char    dram1_user_start __attribute__ ((section(".dram1_user.bss")));

char * allocat_memory_from_dram_user(int dram_sg,unsigned int size)
{
	char * start_addr;
	unsigned int mem_size;
	switch(dram_sg)
	{
	     case 0:
	    	 start_addr = &dram0_user_start;
	    	 mem_size= &_memmap_mem_dram0_end - &dram0_user_start;
//	    	 printf("\n dram0:user range(0x%x---0x%x)\n",&dram0_user_start,&_memmap_mem_dram0_end);
	    	 break;
	     case 1:
	    	 start_addr =  &dram1_user_start;
	    	 mem_size= &_memmap_mem_dram1_end - &dram1_user_start;
//	    	 printf("\n dram1:user range(0x%x---0x%x)\n", &dram1_user_start,&_memmap_mem_dram1_end);
	    	 break;
	     default:
	    	 printf("Err no such as dram_sg:%d type\n ",dram_sg);
	    	 return 0;

	}

	if(size <= mem_size)
	{
		return start_addr;
	}
	else
	{
		printf("Error there is no enough space in dram%d:%d,for need:%d",dram_sg,mem_size,size);
		return 0;
	}
}
