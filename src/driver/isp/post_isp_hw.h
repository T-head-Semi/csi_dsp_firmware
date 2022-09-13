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
#ifndef POST_ISP_HW_H
#define POST_ISP_HW_H
#include "isp_common.h"


#define POST_ISP_BASE_ADDR   (VI_SYSREG_BASE+0x120000)
#define POST_ISP_ENTRY_INT_STATUS_REG  (POST_ISP_BASE_ADDR+0x56d8)
#define POST_ISP_ENTRY_INT_CLEAR_REG (POST_ISP_BASE_ADDR+0x56dc)
#define POST_ISP_VI_IRCL   (POST_ISP_BASE_ADDR+0x0014)

//#define Post_ISP_TRIGGER_REG (POST_ISP_BASE_ADDR +0x)
#define POST_ISP_BUS_IDLE_MASK   (0x1<<5)
#define POST_ISP_SOFT_RESET_MASK (0x1<<20)
#define POST_ISP_ENTRY_INT_CLEAR_MASK  (0x1<<0)
#define POST_ISP_MI_ICR              (POST_ISP_BASE_ADDR+0x16d8)
#define POST_ISP_MI_MIS              (POST_ISP_BASE_ADDR+0x16d0)
#define POST_ISP_MI_ICR1             (POST_ISP_BASE_ADDR+0x16dc)
#define POST_ISP_MI_MIS1              (POST_ISP_BASE_ADDR+0x16d4)
#define POST_ISP_MI_ICR2             (POST_ISP_BASE_ADDR+0x16f4)
#define POST_ISP_MI_MIS2              (POST_ISP_BASE_ADDR+0x16f0)
#define POST_ISP_ISP_ISR            	(POST_ISP_BASE_ADDR+0x05cc)
#define MI_MP_LINE_CNT    		(POST_ISP_BASE_ADDR+0x56b8)
#define POST_ISP_MI_CTRL              (POST_ISP_BASE_ADDR + 0x1300)
#define POST_ISP_TRIGGER_MASK  (0x3<<21)


#ifdef __cplusplus
extern "C" {
#endif
void post_isp_interrupt_handler(void *data);

int32_t post_isp_register_interrupt(uint16_t id);

//ALWAYS_INLINE uint16_t READ_POST_ISP_M_COUNTER_REG() {
//    return (XT_RER(POST_ISP_M_COUNTER_REG) & POST_ISP_M_LINES_MASK);
//}

ALWAYS_INLINE uint32_t READ_POST_ISP_M_INTERRUPT_STATUS_REG(int32_t id) {
		return ps_l32((volatile void *)POST_ISP_ENTRY_INT_STATUS_REG);
}

ALWAYS_INLINE void WRITE_POST_ISP_CLEAR_INTERRUPT_REG(int32_t id) {
	ps_s32((uint32_t)(0xffffffff),(volatile void *)POST_ISP_ENTRY_INT_CLEAR_REG);
//	ps_s32((uint32_t)(0xffffffff),(volatile void *)POST_ISP_MI_ICR);
//	ps_s32((uint32_t)(0xffffffff),(volatile void *)POST_ISP_MI_ICR1);
//	ps_s32((uint32_t)(0xffffffff),(volatile void *)POST_ISP_MI_ICR2);
}

ALWAYS_INLINE void CLEAR_POST_ISP_ENTRY_INT(int32_t id) {
	uint32_t value = ps_l32((volatile void *)POST_ISP_ENTRY_INT_CLEAR_REG) | POST_ISP_ENTRY_INT_CLEAR_MASK;
	ps_s32(value,(volatile void *)POST_ISP_ENTRY_INT_CLEAR_REG);
    return;
}

ALWAYS_INLINE void CLEAR_POST_ISP_BUS_IDLE_INT(int32_t id) {
	uint32_t value = ps_l32((volatile void *)POST_ISP_ENTRY_INT_CLEAR_REG) | POST_ISP_BUS_IDLE_MASK;
	ps_s32(value,(volatile void *)POST_ISP_ENTRY_INT_CLEAR_REG);
    return;
}

ALWAYS_INLINE void CLEAR_POST_ISP_BUS(int32_t id) {
	uint32_t value = ps_l32((volatile void *)POST_ISP_VI_IRCL) | POST_ISP_SOFT_RESET_MASK;
	ps_s32(value,(volatile void *)POST_ISP_VI_IRCL);
    return;
}

ALWAYS_INLINE void TRIGGER_POST_ISP_REG(int32_t id) {
	uint32_t value = ps_l32((volatile void *)POST_ISP_MI_CTRL);
	ps_s32((uint32_t)(value)|POST_ISP_TRIGGER_MASK,(volatile void *) POST_ISP_MI_CTRL);
	return;
}

ALWAYS_INLINE uint32_t GET_POST_ISP_BUS_IDLE_STATUS(int32_t id)
{
	return READ_POST_ISP_M_INTERRUPT_STATUS_REG(id) & POST_ISP_BUS_IDLE_MASK;
}

uint32_t post_isp_pop_entry();
#ifdef __cplusplus
}
#endif

#endif
