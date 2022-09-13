/*
 * Copyright (c) 2021 Alibaba Group. All rights reserved.
 * License-Identifier: Apache-2.0
 *
 * Licensed under the Apache License, Version 2.0 (the "License"); you may
 * not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */
#ifndef VIPRE_HW_H
#define VIPRE_HW_H
#include "isp_common.h"
#define S100P   1


#define MAX_VIPRE_CHANNEL 3
#define MAX_VIPRE_BUFFER 4
#define MAX_FRAME_BUFFER  4
#define VIPRE_BASE_ADDR   (VI_SYSREG_BASE+0x30000)

#define VIPRE_INTERRUPT_REG  (VIPRE_BASE_ADDR+0x248)
#ifdef S100P
#define VIPRE_INTERRUPT_STATUS_BIT_MASK  (0x1ff<<0)

#else
#define VIPRE_INTERRUPT_STATUS_BIT_MASK  (0x1f<<0)
#endif
#define VIPRE_INTERRUPT_CTRL_MASK     (0x3f<<16)
#define VIPRE_FRAME_DONE_INT_MASK  ( 0x1<<0)
#define VIPRE_N_LINE_DONE_INT_MASK (0x1<<1)
#define VIPRE_OVERFLOW_INT_MASK   (0x1<<2)
#define VIPRE_N_FRAME_END_INT_MASK  ( 0x1<<6)

#define VIPRE_M_FRAME_DONE_STATUS_REG  (VIPRE_BASE_ADDR+0x244)
#define VIPRE_ALL_ENTRY_DONE_REG (VIPRE_BASE_ADDR + 0x1c4)
#define VIPRE_M_N_READ_REPORT1_REG  (VIPRE_BASE_ADDR+ 0x1f4)
#define VIPRE_M_N_READ_REPORT2_REG  (VIPRE_BASE_ADDR+ 0x1f8)
#define VIPRE_M_FRAME_ID_MASK   (0x3<<13)

#define VIPRE_N_LINE_READ_CHAN0_REG  (VIPRE_BASE_ADDR+0x1c8)
#define VIPRE_N_LINE_READ_CHAN1_REG  (VIPRE_BASE_ADDR+0x1cc)
#define VIPRE_N_LINE_READ_CHAN2_REG  (VIPRE_BASE_ADDR+0x1d0)

#define VIPRE_N_LINE_COUNTER_REG1 (VIPRE_BASE_ADDR+ 0x1ec)
#define VIPRE_N_LINE_COUNTER_REG2 (VIPRE_BASE_ADDR+ 0x1f0)
#define VIPRE_N_LINE_COUNTER_MASK  (0x7ff)
#define VIPRE_N_LINE_CMPL_INT_MASK (0)

#define VIPRE_LINE_NUM_REG
#define VIPRE_BUFFER_INDEX_REG

#ifdef __cplusplus
extern "C" {
#endif

void vipre_interrupt_handler(void* data);

int32_t vipre_register_interrupt(uint16_t id);


ALWAYS_INLINE uint32_t READ_VIPRE_INTERRUPT_STATUS_REG(int32_t vipre_id) {
    return ps_l32((volatile void *)VIPRE_INTERRUPT_REG);
}

ALWAYS_INLINE void CLEAR_VIPRE_INTERRUPT_REG(int32_t vipre_id,uint32_t mask) {
//	uint32_t val= (ps_l32((volatile void *)VIPRE_INTERRUPT_REG)&(~VIPRE_INTERRUPT_STATUS_BIT_MASK))
//			       |(mask|(VIPRE_N_FRAME_END_INT_MASK<<(MAX_VIPRE_CHANNEL-1-vipre_id))&VIPRE_INTERRUPT_STATUS_BIT_MASK);
	uint32_t val= (ps_l32((volatile void *)VIPRE_INTERRUPT_REG)&(~VIPRE_INTERRUPT_STATUS_BIT_MASK))
			       |(mask|(0x7<<6)&VIPRE_INTERRUPT_STATUS_BIT_MASK);

	ps_s32((uint32_t)val, (volatile void *)VIPRE_INTERRUPT_REG);

}

ALWAYS_INLINE void CLEAR_VIPRE_BUFFER_REG(int32_t channel_id,uint16_t buffer_index)
{
	uint32_t mask = (0x1<<(MAX_VIPRE_BUFFER-1-(buffer_index+1)%MAX_VIPRE_BUFFER))<<((MAX_VIPRE_CHANNEL-1-channel_id)*MAX_VIPRE_BUFFER);
	ps_s32((uint32_t)mask, (volatile void *)VIPRE_M_FRAME_DONE_STATUS_REG);
}

ALWAYS_INLINE void SET_VIPRE_ALL_ENTRY_DONE_REG(int32_t channel_id)
{
	uint32_t mask = ps_l32((volatile void *)VIPRE_ALL_ENTRY_DONE_REG)^(0x1<<channel_id);
	ps_s32(mask,(volatile void *)VIPRE_ALL_ENTRY_DONE_REG);
}

//ALWAYS_INLINE void SET_VIPRE_NLINE_ENTRY_DONE_REG(int32_t vipre_id)
//{
//	uint32_t mask = ps_l32((volatile void *)VIPRE_ALL_ENTRY_DONE_REG)^(0x1<<vipre_id);
//	ps_s32(mask,(volatile void *)VIPRE_ALL_ENTRY_DONE_REG);
//}

ALWAYS_INLINE uint32_t READ_VIPRE_BUFFER_REG(int32_t channel_id)
{
//    uint32_t val=ps_l32((volatile void *)VIPRE_M_FRAME_DONE_STATUS_REG);
//    ps_s32(val,(volatile void *)VIPRE_M_FRAME_DONE_STATUS_REG);
//    return val;
	return ps_l32((volatile void *)VIPRE_M_FRAME_DONE_STATUS_REG)>>((MAX_VIPRE_CHANNEL-1-channel_id)*MAX_VIPRE_BUFFER)&0xf;
}

ALWAYS_INLINE void SET_VIPRE_M_FRAME_BUFFER_READ_DONE(int32_t channel_id,uint16_t buffer_index)
{
	uint32_t mask =0;
	switch(channel_id)
	{
		case 0:
			mask =ps_l32((volatile void *)VIPRE_M_N_READ_REPORT1_REG)|(buffer_index<<13&VIPRE_M_FRAME_ID_MASK);
			ps_s32((uint32_t)mask, (volatile void *)VIPRE_M_N_READ_REPORT1_REG);
			return;
		case 1:
			mask =ps_l32((volatile void *)VIPRE_M_N_READ_REPORT1_REG)|(buffer_index<<29);
			ps_s32((uint32_t)mask, (volatile void *)VIPRE_M_N_READ_REPORT1_REG);
			return;
		case 2:
			mask =ps_l32((volatile void *)VIPRE_M_N_READ_REPORT2_REG)|(buffer_index<<13&VIPRE_M_FRAME_ID_MASK);
			ps_s32((uint32_t)mask, (volatile void *)VIPRE_M_N_READ_REPORT2_REG);
			return;
		default:
			return;
	}

}

ALWAYS_INLINE void SET_VIPRE_N_LINE_READ_DONE(int32_t channel_id,uint16_t line)
{

	switch(channel_id)
	{
		case 0:
			ps_s32((uint32_t)line&0x1fff, (volatile void *)VIPRE_N_LINE_READ_CHAN0_REG);
			return;
		case 1:
				ps_s32((uint32_t)line&0x1fff, (volatile void *)VIPRE_N_LINE_READ_CHAN1_REG);
			return;
		case 2:
				ps_s32((uint32_t)line&0x1fff, (volatile void *)VIPRE_N_LINE_READ_CHAN2_REG);
			return;
		default:
			return;
	}


}


ALWAYS_INLINE uint32_t READ_VIPRE_NLINE_COUNTER_REG(int32_t channel_id)
{
	switch (channel_id)
	{
	     case 0:
	    	 return ps_l32((volatile void *)VIPRE_N_LINE_COUNTER_REG1)&VIPRE_N_LINE_COUNTER_MASK;
	     case 1:
	    	 return (ps_l32((volatile void *)VIPRE_N_LINE_COUNTER_REG1))>>16&VIPRE_N_LINE_COUNTER_MASK;
	     case 2:
	    	 return ps_l32((volatile void *)VIPRE_N_LINE_COUNTER_REG2)&VIPRE_N_LINE_COUNTER_MASK;
	     default:
	    	 return 0;
	}

}

#ifdef __cplusplus
}
#endif
#endif
