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
#include "vipre_hw.h"
#include "isp_common.h"
#include "vipre_handler.h"
#include "dsp_ps_debug.h"
#include <xtensa/config/core.h>

#define DEBUG_VIPRE_INT
#define XCHAL_VIPRE_INTERRUPT XCHAL_EXTINT0_NUM
#define XCHAL_VIPRE_INTERRUPT_MASK
#define XCHAL_VIPRE_INTERRUPT_LEVEL  15

static uint32_t vipre_int_tick[1024];
static int32_t vipre_int_cnt;
extern Svipre_handler g_vipre_handler ISP_SECTION;

void vipre_interrupt_handler(void *data) {

		uint32_t vipre_int = READ_VIPRE_INTERRUPT_STATUS_REG(0);
		uint32_t vipre_buffer_status = READ_VIPRE_BUFFER_REG(0);
#ifdef DEBUG_VIPRE_INT
		vipre_int_tick[vipre_int_cnt++&0x3ff]=XT_RSR_CCOUNT();
		vipre_int_tick[vipre_int_cnt++&0x3ff]=vipre_int;
		vipre_int_tick[vipre_int_cnt++&0x3ff]= vipre_buffer_status;//READ_VIPRE_NLINE_COUNTER_REG(0);
//		if(g_vipre_handler.frame_meta_reg)
//			vipre_int_tick[vipre_int_cnt++&0x3ff] = ps_l32(g_vipre_handler.even_odd_sync);
#endif

	if((vipre_int & VIPRE_FRAME_DONE_INT_MASK))
    {

		vipre_push_new_entry();

		CLEAR_VIPRE_INTERRUPT_REG(0,VIPRE_FRAME_DONE_INT_MASK);
    }
	else if((vipre_int & VIPRE_N_LINE_DONE_INT_MASK))
	{
		CLEAR_VIPRE_INTERRUPT_REG(0,VIPRE_N_LINE_DONE_INT_MASK);

		g_vipre_handler.current_NM = READ_VIPRE_NLINE_COUNTER_REG(0);

		vipre_push_new_line(g_vipre_handler.current_NM);

	}
    else if((vipre_int & VIPRE_OVERFLOW_INT_MASK))
    {
//    	g_vipre_handler.current_NM = READ_VIPRE_NLINE_COUNTER_REG(0);
//		uint32_t value= ps_l32((volatile void *)VIPRE_N_LINE_READ_CHAN0_REG);
    	CLEAR_VIPRE_INTERRUPT_REG(0,VIPRE_OVERFLOW_INT_MASK);
    	vipre_pop_new_entry(1);
//    	ps_err("VIPRE HW overflow N line:%d,N entry:%d\n",g_vipre_handler.current_NM,value);
    }
    else
    {
    	CLEAR_VIPRE_INTERRUPT_REG(0,vipre_int);
    }

    return;
}
int32_t vipre_register_interrupt(uint16_t id) {

    int32_t ret = 0;

    // Normally none of these calls should fail. We don't check them
    // individually, any one failing will cause this function to return failure.
	xthal_interrupt_sens_set(XCHAL_VIPRE_INTERRUPT, 1); //0~ eage 1~ level
	xthal_interrupt_pri_set(XCHAL_VIPRE_INTERRUPT,XCHAL_VIPRE_INTERRUPT_LEVEL);
    ret += xtos_set_interrupt_handler(XCHAL_VIPRE_INTERRUPT,
                                      vipre_interrupt_handler, NULL, NULL);
    ret += xtos_interrupt_enable(XCHAL_VIPRE_INTERRUPT);
    ps_debug("enable vipre interrupt %s,trace buf:0x%x\n",ret==0?"success":"fail",vipre_int_tick);
    return ret;

}

int32_t vipre_disable_inerrupt(uint16_t id) {
    int32_t ret = 0;
    ret = xtos_interrupt_disable(XCHAL_VIPRE_INTERRUPT);
    ps_debug("disable vipre interrupt %s,trace buf:0x%x\n",ret==0?"success":"fail",vipre_int_tick);
    return ret;
}


int vipre_get_irq_num()
{
	return XCHAL_VIPRE_INTERRUPT;
}
