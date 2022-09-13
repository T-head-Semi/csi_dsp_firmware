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
#ifndef ISP_HW_H
#define ISP_HW_H
#include "isp_common.h"

#define XCHAL_ISP0_INTERRUPT   XCHAL_EXTINT8_NUM
#define XCHAL_ISP1_INTERRUPT   XCHAL_EXTINT7_NUM


#define ISP_N_LINES_INT_MASK (0x08)
#define ISP_AE_INT_MASK (0x01<<2)
#define ISP0_BASE_ADDR    (VI_SYSREG_BASE+0x100000)
#define ISP1_BASE_ADDR	  (VI_SYSREG_BASE+0x110000)

#define ISP_N_INTERRUPT_STATUS_REG  (0x56d8)
#define ISP_FRAME_BUFFER_START_REG  (0x5560)
#define ISP_N_INTERRUPT_CLEAR_REG 	(0x56dc)
#define ISP_N_COUNTER_REG 			(0x56b0)
#define ISP_N_INTERRUPT_CLEAR_MASK  (0x1<<3)
#define MI_ICR              		(0x16d8)
#define ISP_MI_MIS          		(0x16d0)
#define MI_ICR1             		(0x16dc)
#define ISP_MI_MIS1         		(0x16d4)
#define MI_ICR2             		(0x16f4)
#define ISP_MI_MIS2					(0x16f0)
#define MI_PP_Y_OFFS_CNT    		(0x5574)
#define ISP_PPW_FRAME_DONE          (0x1<<11)
#define ISP_ISP_ISR            	    (0x05cc)
#ifdef __cplusplus
extern "C" {
#endif
void isp_interrupt_handler(void* data);

int32_t isp_register_interrupt(uint16_t id);

ALWAYS_INLINE uint16_t READ_ISP_N_COUNTER_REG(uint16_t id) {
	uint32_t base_addr = id==0?ISP0_BASE_ADDR:ISP1_BASE_ADDR;
    return ps_l32((volatile void *)(ISP_N_COUNTER_REG+base_addr));
}

ALWAYS_INLINE uint16_t READ_ISP_BUFFER_START_OFFSET_REG(uint16_t id) {
	uint32_t base_addr = id==0?ISP0_BASE_ADDR:ISP1_BASE_ADDR;
    return ps_l32((volatile void *)(ISP_FRAME_BUFFER_START_REG+base_addr));
}

ALWAYS_INLINE uint32_t READ_ISP_MI3_INT(int32_t isp_id) {
	uint32_t base_addr = isp_id==0?ISP0_BASE_ADDR:ISP1_BASE_ADDR;
    return (ps_l32((volatile void *)(ISP_N_INTERRUPT_STATUS_REG+base_addr)));
}

ALWAYS_INLINE uint16_t READ_ISP_MI_INT(uint16_t id) {
	uint32_t base_addr = id==0?ISP0_BASE_ADDR:ISP1_BASE_ADDR;
    return ps_l32((volatile void *)(ISP_MI_MIS+base_addr));
}

ALWAYS_INLINE uint16_t READ_ISP_MI1_INT(uint16_t id) {
	uint32_t base_addr = id==0?ISP0_BASE_ADDR:ISP1_BASE_ADDR;
    return ps_l32((volatile void *)(ISP_MI_MIS1+base_addr));
}

ALWAYS_INLINE uint16_t READ_ISP_MI2_INT(uint16_t id) {
	uint32_t base_addr = id==0?ISP0_BASE_ADDR:ISP1_BASE_ADDR;
    return ps_l32((volatile void *)(ISP_MI_MIS2+base_addr));
}

ALWAYS_INLINE void CLEAR_ISP_MI_INT(uint16_t id,uint32_t value) {
	uint32_t base_addr = id==0?ISP0_BASE_ADDR:ISP1_BASE_ADDR;
    return ps_s32(value,(volatile void *)(MI_ICR+base_addr));
}

ALWAYS_INLINE void CLEAR_ISP_MI2_INT(uint16_t id,uint32_t value) {
	uint32_t base_addr = id==0?ISP0_BASE_ADDR:ISP1_BASE_ADDR;
    return ps_s32(value,(volatile void *)(MI_ICR2+base_addr));
}

ALWAYS_INLINE void CLEAR_ISP_MI3_INT(int32_t isp_id,uint32_t value){
	uint32_t base_addr = isp_id==0?ISP0_BASE_ADDR:ISP1_BASE_ADDR;
	ps_s32(value, (volatile void *)(ISP_N_INTERRUPT_CLEAR_REG+base_addr));
}

ALWAYS_INLINE void SET_ISP_ISP_INT(int32_t isp_id,uint32_t value){
	uint32_t base_addr = isp_id==0?ISP0_BASE_ADDR:ISP1_BASE_ADDR;
	ps_s32(value, (volatile void *)(ISP_ISP_ISR+base_addr));
}


ALWAYS_INLINE void WRITE_ISP_CLEAR_INTERRUPT_REG(int32_t isp_id) {
	uint32_t base_addr = isp_id==0?ISP0_BASE_ADDR:ISP1_BASE_ADDR;
	//ps_s32((uint32_t)(0xffffffff), (volatile void *)ISP_N_INTERRUPT_CLEAR_REG);
	ps_s32((uint32_t)(0xffffffff),(volatile void *)(MI_ICR+base_addr));
	ps_s32((uint32_t)(0xffffffff),(volatile void *)(MI_ICR1+base_addr));
	ps_s32((uint32_t)(0xffffffff),(volatile void *)(MI_ICR2+base_addr));

}



void isp_fifo_reset(uint16_t id);

#ifdef __cplusplus
}
#endif
#endif
