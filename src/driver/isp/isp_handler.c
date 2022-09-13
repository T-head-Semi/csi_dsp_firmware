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
#include<stdlib.h>
#include<string.h>
#include "isp_common.h"
#include "isp_hw.h"
#include "dsp_ps_debug.h"
Sisp_handler g_isp_handler ISP_SECTION;

int32_t isp_initialize(uint16_t id) {
    if (id > MAX_ISP) {
        return EHW_ERROR_PARMA_INVALID;
    }
    memset(&g_isp_handler, 0x0, sizeof(g_isp_handler));
    g_isp_handler.id = id;
    return 0;
}

int32_t isp_handler_config(uint16_t hor_res, uint16_t ver_res,
						   csi_dsp_img_fmt_e raw_type, uint16_t line_value,
                           uint64_t buffer_addr, uint32_t buffer_size,uint16_t line_size) {
    int32_t ret;
    ret = ispc_base_info_update(&(g_isp_handler.base), hor_res, ver_res,
    							raw_type, line_value, buffer_addr,
                                buffer_size, line_size);
    if (ret != EHW_OK) {
    	g_isp_handler.status=ret;
        return ret;
    }
    g_isp_handler.base.line_num_used_per_frame=g_isp_handler.base.line_num_per_frame;
    const uint8_t start_side = 0;
    g_isp_handler.customer_ping_pong_flag = start_side;
    g_isp_handler.produce_ping_pong_flag = start_side;
    memcpy((void *)&g_isp_handler.info[start_side],(void *) &g_isp_handler.base,
           sizeof(Sisp_control_info));
    reset_fifo(&g_isp_handler.fifo[start_side],
               g_isp_handler.base.line_num_per_buffer, 0,g_isp_handler.base.line_num_per_frame);
    isp_register_interrupt(g_isp_handler.id);
    return EHW_OK;
}

int isp_get_status(int id)
{
	return g_isp_handler.status;
}
//Eisp_bool isp_get_next_n_line(const uint16_t line,Sisp_buffer buffer_list[MAX_BUFFER_BLOCK]) {
//
//    uint8_t ping_pong_side = g_isp_handler.customer_ping_pong_flag;
//    Sisp_control_info *isp_info = &g_isp_handler.info[ping_pong_side];
//    Sfifo *isp_fifo = &g_isp_handler.fifo[ping_pong_side];
//
//    if (is_empty_fifo(isp_fifo)) {
//    	g_isp_handler->status=EISP_INPUT_EMPRY;
//        return EISP_BOOL_FASE;
//    }
//    if(is_enough_element_to_pop_fifo(isp_fifo,line)<0)
//    {
//    	g_isp_handler->status=EISP_ERROR_POP_FAIL;
//    	return EISP_BOOL_FASE;
//    }
//    if(ispc_get_entry_to_pop(&g_isp_handler,line,buffer_list)<0)
//    {
//    	g_isp_handler->status=EISP_ERROR_POP_FAIL;
//    	return EISP_BOOL_FASE;
//    }
//
//    return EISP_BOOL_TRUE;
//}
void isp_fifo_reset(uint16_t id)
{
	uint8_t reset_side = g_isp_handler.produce_ping_pong_flag;
    Sisp_control_info *info = &g_isp_handler.info[reset_side];
    Sfifo *fifo = &g_isp_handler.fifo[reset_side];
	g_isp_handler.customer_ping_pong_flag =reset_side;
	g_isp_handler.produce_ping_pong_flag = reset_side;
	uint16_t base_index =
        (fifo->offset + info->line_num_used_per_frame) % fifo->depth;

	memcpy((void *)&g_isp_handler.info[reset_side], (void *)&g_isp_handler.base,
		   sizeof(Sisp_control_info));
	g_isp_handler.interact_counter = 0;
	reset_fifo(&g_isp_handler.fifo[reset_side],
			g_isp_handler.base.line_num_per_buffer,base_index,g_isp_handler.base.line_num_per_frame);
}


void isp_recovery_state_update(uint16_t id )
{
	switch(g_isp_handler.status)
	{
		case EHW_RECOVERYING:
			isp_fifo_reset(id);
			g_isp_handler.status = EHW_RECOVERY_DONE;
			break;
		case EHW_RECOVERY_DONE:
			g_isp_handler.status = EHW_OK;
			break;
		default:
			g_isp_handler.status = EHW_RECOVERYING;
			break;
	}

}


Eisp_bool isp_get_next_entry(Sisp_buffer buffer_list[MAX_BUFFER_BLOCK]) {
    uint16_t line;
    uint8_t ping_pong_side = g_isp_handler.customer_ping_pong_flag;
    Sisp_control_info *isp_info = &g_isp_handler.info[ping_pong_side];
    Sfifo *isp_fifo = &g_isp_handler.fifo[ping_pong_side];

    if (g_isp_handler.status != EHW_OK) {
        return EISP_BOOL_FASE;
    }

    if (is_empty_fifo(isp_fifo)) {
        return EISP_BOOL_FASE;
    }

    if(is_enough_element_to_pop_fifo(isp_fifo,isp_info->line_num_per_entry ))
    {
    	line =isp_info->line_num_per_entry;
    }
    else if(is_enough_element_to_pop_fifo(isp_fifo,isp_info->line_num_in_last_entry ))
    {
    	line =isp_info->line_num_in_last_entry;
    }
    else
    {
    	g_isp_handler.status=EHW_ERROR_POP_FAIL;
    	ps_err("%s,fail get entry\n",__func__);
    	return EISP_BOOL_FASE;
    }

    if(ispc_get_entry_to_pop(&g_isp_handler,line,buffer_list)<0)
    {
    	g_isp_handler.status=EHW_ERROR_POP_FAIL;
    	return EISP_BOOL_FASE;
    }
    return EISP_BOOL_TRUE;
}


int16_t isp_get_pushed_line_num()
{
	return ispc_get_pushed_line_num(&g_isp_handler);
}

int16_t isp_get_poped_line_num()
{
	return ispc_get_poped_line_num(&g_isp_handler);
}

uint32_t isp_pop_n_line(uint16_t line_num) {
    return ispc_pop_new_line(&g_isp_handler,line_num);
}

Eisp_bool isp_is_last_entry_pop_in_frame() {
    uint8_t ping_pong_side = g_isp_handler.customer_ping_pong_flag;
    Sisp_control_info *isp_info = &g_isp_handler.info[ping_pong_side];
    Sfifo *isp_fifo = &g_isp_handler.fifo[ping_pong_side];
    return (is_last_entry_to_pop_fifo(isp_fifo,isp_info->line_num_in_last_entry));
}

Eisp_bool isp_is_last_entry_push_in_frame() {
    uint8_t ping_pong_side = g_isp_handler.produce_ping_pong_flag;
    Sisp_control_info *isp_info = &g_isp_handler.info[ping_pong_side];
    Sfifo *isp_fifo = &g_isp_handler.fifo[ping_pong_side];
    return (
        is_last_entry_to_push_fifo(isp_fifo,isp_info->line_num_in_last_entry));
}

int32_t isp_get_line_size() {
    uint8_t ping_pong_side = g_isp_handler.customer_ping_pong_flag;
    Sisp_control_info *isp_info = &g_isp_handler.info[ping_pong_side];
    return isp_info->size_per_line;
}
uint32_t isp_get_frame_size()
{
	  uint8_t ping_pong_side = g_isp_handler.customer_ping_pong_flag;
	  Sisp_control_info *isp_info = &g_isp_handler.info[ping_pong_side];
	  return isp_info->line_num_per_frame *isp_info->size_per_line;
}

uint32_t isp_get_frame_height()
{
	  uint8_t ping_pong_side = g_isp_handler.customer_ping_pong_flag;
	  Sisp_control_info *isp_info = &g_isp_handler.info[ping_pong_side];
	  return isp_info->line_num_per_frame;
}
Eisp_bool isp_is_all_entry_done()
{
	  uint8_t ping_pong_side = g_isp_handler.customer_ping_pong_flag;
	  Sisp_control_info *isp_info = &g_isp_handler.info[ping_pong_side];
	  Sfifo *isp_fifo = &g_isp_handler.fifo[ping_pong_side];
	  return is_last_entry_pop_fifo(isp_fifo);
}
