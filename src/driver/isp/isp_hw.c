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
#include <stdio.h>
#include "isp_hw.h"
#include "isp_common.h"
#include "dsp_ps_debug.h"
#include <xtensa/config/core.h>


#define  ISP_INT_DEBUG
#define XCHAL_ISP_INTERRUPT_MASK
#define XCHAL_ISP_INTERRUPT_LEVEL  15

static uint32_t isp_int_tick[1536];
static int32_t isp_int_cnt =0;
extern Sisp_handler g_isp_handler;

void isp_interrupt_handler(void *data) {
	uint32_t mi3= READ_ISP_MI3_INT(g_isp_handler.id);
	uint16_t line_entry = READ_ISP_N_COUNTER_REG(g_isp_handler.id);
	uint32_t mi = READ_ISP_MI_INT(g_isp_handler.id);
	uint32_t mi2 = READ_ISP_MI2_INT(g_isp_handler.id);
#ifdef ISP_INT_DEBUG
	isp_int_tick[isp_int_cnt++%1536]=mi3;
	isp_int_tick[isp_int_cnt++%1536]=mi;
	isp_int_tick[isp_int_cnt++%1536]=READ_ISP_MI1_INT(g_isp_handler.id);
	isp_int_tick[isp_int_cnt++%1536]=mi2;//ps_l32((volatile void *)ISP_MI_MIS2);
	isp_int_tick[isp_int_cnt++%1536]=0xf0000000|line_entry;
#endif

//	isp_int_tick[isp_int_cnt++]=line_entry;//READ_ISP_BUFFER_START_OFFSET_REG();
	if(mi3&ISP_N_LINES_INT_MASK)
    {
//		ps_s32((uint32_t)(0x8), (volatile void *)ISP_N_INTERRUPT_CLEAR_REG);
		CLEAR_ISP_MI3_INT(g_isp_handler.id,ISP_N_LINES_INT_MASK);
		ispc_push_new_line(&g_isp_handler,line_entry-ispc_get_pushed_line_num(&g_isp_handler));
		g_isp_handler.current_N_value = line_entry;

    }
	if(mi2&ISP_PPW_FRAME_DONE)
	{
		if((isp_get_status(g_isp_handler.id)==EHW_RECOVERY_DONE))
		{
			isp_recovery_state_update(g_isp_handler.id);
		}
		else if(isp_get_status(g_isp_handler.id)==EHW_RECOVERYING)
		{
			isp_fifo_reset(0);
		}
		CLEAR_ISP_MI2_INT(g_isp_handler.id,ISP_PPW_FRAME_DONE);
	}
	if(mi&ISP_AE_INT_MASK)
	{
		CLEAR_ISP_MI_INT(g_isp_handler.id,ISP_AE_INT_MASK);
		SET_ISP_ISP_INT(g_isp_handler.id,0x1<<8);
	}
    else
    {

    	 WRITE_ISP_CLEAR_INTERRUPT_REG(g_isp_handler.id);
    }

    return;
}
int32_t isp_register_interrupt(uint16_t id) {

    int32_t ret = 0;
    uint32_t int_num = id==0?XCHAL_ISP0_INTERRUPT:XCHAL_ISP1_INTERRUPT;
    // Normally none of these calls should fail. We don't check them
    // individually, any one failing will cause this function to return failure.
	xthal_interrupt_sens_set(int_num, 1); //0~ eage 1~ level
	xthal_interrupt_pri_set(int_num,XCHAL_ISP_INTERRUPT_LEVEL);
    ret += xtos_set_interrupt_handler(int_num,
                                      isp_interrupt_handler, NULL, NULL);
    ret += xtos_interrupt_enable(int_num);
    ps_debug("register ISP interrupt %d %s,trace buf:0x%x\n",int_num,ret==0?"success":"fail",isp_int_tick);
    return ret;

}

int32_t isp_enable_inerrupt(uint16_t id){
    int32_t ret = 0;
    uint32_t int_num = id==0?XCHAL_ISP0_INTERRUPT:XCHAL_ISP1_INTERRUPT;
    ret = xtos_interrupt_enable(int_num);
    ps_debug("Enable ISP interrupt %s\n",ret==0?"success":"fail");
    return ret;
}

int32_t isp_disable_inerrupt(uint16_t id) {
    int32_t ret = 0;
    uint32_t int_num = id==0?XCHAL_ISP0_INTERRUPT:XCHAL_ISP1_INTERRUPT;
    ret = xtos_interrupt_disable(int_num);
    ps_debug("disable ISP interrupt %s\n",ret==0?"success":"fail");
    return ret;
}

int isp_get_irq_num(uint16_t id)
{
	return id==0?XCHAL_ISP0_INTERRUPT:XCHAL_ISP1_INTERRUPT;
}
