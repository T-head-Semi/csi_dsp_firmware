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
#include "post_isp_hw.h"
#include "isp_common.h"
#include "vi_hw_api.h"
#include "dsp_ps_debug.h"
#include <xtensa/config/core.h>

#define POST_ISP_ENABLE_INTS()
#define POST_ISP_DISABLE_INTS()
#define  POST_ISP_INT_DEBUG
#define XCHAL_POST_ISP_INTERRUPT XCHAL_EXTINT4_NUM
#define XCHAL_POST_ISP_INTERRUPT_LEVEL  15
extern int32_t bus_idle_flag;
extern Sisp_handler g_post_isp_handler;
static uint32_t post_isp_tick[1536];
static int32_t post_isp_cnt=0;
static uint32_t isp_fram_start_tick=0;
static uint32_t isp_fram_start_index=0;
void post_isp_interrupt_handler(void *data) {

 //   g_post_isp_handler.current_N_value = READ_POST_ISP_M_COUNTER_REG();
//	isp_int_tick[isp_int_cnt++]=XT_RSR_CCOUNT();
#ifdef POST_ISP_INT_DEBUG
//	post_isp_tick[post_isp_cnt++]=0xdead;
	post_isp_tick[post_isp_cnt++]=READ_POST_ISP_M_INTERRUPT_STATUS_REG(0);
	post_isp_tick[post_isp_cnt++]=ps_l32((volatile void *)POST_ISP_MI_MIS);
	post_isp_tick[post_isp_cnt++]=ps_l32((volatile void *)MI_MP_LINE_CNT);
	post_isp_tick[post_isp_cnt++]=ps_l32((volatile void *)POST_ISP_MI_MIS2);
	post_isp_tick[post_isp_cnt++]=post_isp_get_poped_line_num();
	post_isp_cnt=post_isp_cnt&0x3ff;

#endif

	if(READ_POST_ISP_M_INTERRUPT_STATUS_REG(0)&0x1)
	{
		if(post_isp_get_status(0)== EHW_OK)
		{
			post_isp_pop_entry();
		}
		else if (post_isp_get_status(0)== EHW_RECOVERYING && (g_post_isp_handler.current_N_value<g_post_isp_handler.base.line_num_per_frame))
		{
			TRIGGER_POST_ISP_REG(0);
			g_post_isp_handler.current_N_value += g_post_isp_handler.base.line_num_per_entry;
		}

		CLEAR_POST_ISP_ENTRY_INT(0);
	}
	if (READ_POST_ISP_M_INTERRUPT_STATUS_REG(0)&(0x1<<5))
	{
		CLEAR_POST_ISP_BUS_IDLE_INT(0);
//		bus_idle_flag=1;   TODO  post isp reset handle
	}
	/*if mp_ycbcr_frame_end, remap to ISP int and trigger to CPU*/
	if(ps_l32((volatile void *)POST_ISP_MI_MIS)&0x1)
	{
		if (post_isp_get_status(0)== EHW_RECOVERYING)
		{
			post_isp_recovery_state_update(0);
		}
//		else
		{
			// map INT
			ps_s32(0x1<<8,(volatile void *)POST_ISP_ISP_ISR);
		}

		ps_s32((uint32_t)(0xffffffff),(volatile void *)POST_ISP_MI_ICR);
	}

	WRITE_POST_ISP_CLEAR_INTERRUPT_REG(0);


    return;
}
int32_t post_isp_register_interrupt(uint16_t id) {

    int32_t ret = 0;

    // Normally none of these calls should fail. We don't check them
    // individually, any one failing will cause this function to return failure.
    xthal_interrupt_sens_set(XCHAL_POST_ISP_INTERRUPT, 1);  //0~ eage 1~ level
    xthal_interrupt_pri_set(XCHAL_POST_ISP_INTERRUPT,XCHAL_POST_ISP_INTERRUPT_LEVEL);
    ret += xtos_set_interrupt_handler(XCHAL_POST_ISP_INTERRUPT,
                                      post_isp_interrupt_handler, NULL, NULL);
    ret += xtos_interrupt_enable(XCHAL_POST_ISP_INTERRUPT);
    ps_debug("enable Post ISP interrupt %s,trace buf:0x%x\n",ret==0?"success":"fail",post_isp_tick);
    return ret;
}

int32_t post_isp_enable_inerrupt(uint16_t id) {
    int32_t ret = 0;
    ret = xtos_interrupt_enable(XCHAL_POST_ISP_INTERRUPT);
    ps_debug("enable Post ISP interrupt %s,trace buf:0x%x\n",ret==0?"success":"fail",post_isp_tick);
    return ret;
}


int32_t post_isp_disable_inerrupt(uint16_t id) {
    int32_t ret = 0;
    ret = xtos_interrupt_disable(XCHAL_POST_ISP_INTERRUPT);
    return ret;
}


