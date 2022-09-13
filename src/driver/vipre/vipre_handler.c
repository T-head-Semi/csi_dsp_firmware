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

#include "dsp_ps_debug.h"
#include "vipre_handler.h"

Svipre_handler g_vipre_handler ISP_SECTION;

int32_t vipre_initialize() {

    memset(&g_vipre_handler, 0x0, sizeof(g_vipre_handler));
    return 0;
}

int32_t vipre_handler_nline_config(uint32_t chan_num,uint16_t hor_res, uint16_t ver_res,
							uint16_t line_size, csi_dsp_img_fmt_e raw_type, uint16_t line_value,
                            uint64_t *buffer_addr_list, uint32_t buffer_size) {
    int32_t ret;
    const uint8_t start_side = 0;
    uint16_t channel=0;
//    uint16_t line_size = ispc_get_line_size(raw_type,
//                                            hor_res); // size_per_px * hor_res;
    if ((hor_res > MAX_HORIZONTAL) || (ver_res > MAX_VERTICAL) ||
        (line_value > LINES_MASK) || ((buffer_size % line_size) != 0)) {
    	ps_err("vipre config fail\n");
        return EHW_ERROR_PARMA_INVALID;
    }
    g_vipre_handler.frame_meta_reg = (uint32_t)buffer_addr_list[11];
//    if(g_vipre_handler.frame_meta_reg /*&& ps_l32(g_vipre_handler.frame_meta_reg)!=0*/)
//    {
//    	ps_err("vipre led_sync fail\n");
//		return EHW_ERROR_PARMA_INVALID;
//    }
    /*should check if there is issue if not the last N
     line cross the buffer bottom*/
	g_vipre_handler.active_channle=chan_num;
	g_vipre_handler.vipre_mode = EVIPRE_MODE_N_LINE;
	g_vipre_handler.status = EHW_OK;
	Svipre_n_line_info *base_info =&g_vipre_handler.base;
    uint16_t fragment_line = ver_res % line_value;
    base_info->line_num_per_entry = line_value;
    base_info->line_num_per_frame = ver_res;
    base_info->line_num_in_last_entry =
        fragment_line == 0 ? line_value : fragment_line;
    base_info->size_per_line = line_size;
    base_info->line_num_per_buffer = buffer_size / line_size;
    for(channel=0;channel<MAX_VIPRE_CHANNEL;channel++)
    {
    	if(g_vipre_handler.active_channle & (0x1<<channel))
    	{
    	    base_info->channel_buffer[channel].buffer_addr = buffer_addr_list[channel];
    	}
    }


    g_vipre_handler.base.line_num_used_per_frame=base_info->line_num_per_frame;

    g_vipre_handler.customer_ping_pong_flag = start_side;
    g_vipre_handler.produce_ping_pong_flag = start_side;
    memcpy((void *)&g_vipre_handler.info[start_side], &g_vipre_handler.base,
           sizeof(Svipre_n_line_info));
    reset_fifo(&g_vipre_handler.fifo[start_side],
               g_vipre_handler.base.line_num_per_buffer, 0,g_vipre_handler.base.line_num_per_frame);
    vipre_register_interrupt(0);
    return EHW_OK;
}

int32_t vipre_frame_mode_handler_config(uint32_t act_channel,uint16_t hor_res, uint16_t ver_res,uint16_t stride,
										csi_dsp_img_fmt_e raw_type,uint32_t buffer_num,uint64_t *buffer_addr_list)
{
    const uint8_t start_side = 0;
    int loop;
    uint16_t channel_num =0;
    uint16_t channel_list[MAX_VIPRE_CHANNEL]={0};
    g_vipre_handler.customer_ping_pong_flag = start_side;
    g_vipre_handler.produce_ping_pong_flag = start_side;
	g_vipre_handler.vipre_mode=EVIPRE_MODE_M_FRAME;
	g_vipre_handler.status = EHW_OK;
	if(act_channel>0x7)
	{
		ps_err("invalid channel id:%d\n",act_channel);
		return -1;
	}
	g_vipre_handler.active_channle=act_channel;
    for(loop=0;loop<MAX_VIPRE_CHANNEL;loop++)
    {
    	if(act_channel&(0x1<<loop))
    	{
    		channel_list[channel_num]=loop;
    		channel_num++;
    	}
    }

	if(buffer_num>MAX_VIPRE_BUFFER*channel_num)
	{
		ps_err("buffer number exceed limit:%d\n",buffer_num);
		return -1;
	}

	if(buffer_num%channel_num)
	{
		ps_err("buffer number is not match:%d,with channle num:%d\n",buffer_num,channel_num);
		return -1;
	}
    g_vipre_handler.frame_meta_reg = (uint32_t)buffer_addr_list[11];
//    if(g_vipre_handler.frame_meta_reg && ps_l32(g_vipre_handler.frame_meta_reg)!=0)
//    {
//    	ps_err("vipre led_sync fail\n");
//		return EHW_ERROR_PARMA_INVALID;
//    }
	g_vipre_handler.base_buffer_info.buffer_num=buffer_num/channel_num;
	uint16_t channle;
	for(loop=0;loop<buffer_num;loop++)
	{
        uint16_t channel_id = channel_list[loop/g_vipre_handler.base_buffer_info.buffer_num];
        uint16_t buffer_id= loop%g_vipre_handler.base_buffer_info.buffer_num;
		g_vipre_handler.base_buffer_info.channel_buffer[channel_id].buffer_addr[buffer_id]=buffer_addr_list[loop];
	}
	g_vipre_handler.base_buffer_info.data_fmt= raw_type;
	g_vipre_handler.base_buffer_info.line_num_per_buffer = ver_res;
	g_vipre_handler.base_buffer_info.pixs_per_line = hor_res;
	g_vipre_handler.base_buffer_info.size_per_line = stride;
	memcpy(&g_vipre_handler.buffer_info[start_side],&g_vipre_handler.base_buffer_info,sizeof(Svipre_buffer_list));
    reset_fifo(&g_vipre_handler.fifo[start_side],g_vipre_handler.base_buffer_info.buffer_num,0,g_vipre_handler.base_buffer_info.buffer_num);
    vipre_register_interrupt(0);
    return 0;

}

Eisp_bool vipre_push_new_entry()
{
	  if(g_vipre_handler.vipre_mode==EVIPRE_MODE_M_FRAME)
	  {
		  uint8_t ping_pong_side = g_vipre_handler.produce_ping_pong_flag;

		  Sfifo *vipre_fifo = &g_vipre_handler.fifo[ping_pong_side];
		  if (is_overflow_fifo(vipre_fifo)) {
		    	ps_err("vipre input is overflow\n");
		    	g_vipre_handler.status = EHW_ERROR_OVER_FLOW;
		        return EISP_BOOL_FASE;
		  }
		  vipre_store_frame_meta(vipre_fifo);
		  if (push_fifo(vipre_fifo,1,0) == EISP_BOOL_FASE) {
				ps_err("vipre push fail\n");
				  return EISP_BOOL_FASE;
			  }
		  if (is_last_entry_push_fifo(vipre_fifo)) {
				  ping_pong_side = ping_pong_side ^ 0x1;
				  memcpy(&g_vipre_handler.buffer_info[ping_pong_side],&g_vipre_handler.base_buffer_info,sizeof(Svipre_buffer_list));

				  g_vipre_handler.produce_ping_pong_flag = ping_pong_side;
				  reset_fifo(&g_vipre_handler.fifo[ping_pong_side],
						  g_vipre_handler.buffer_info[ping_pong_side].buffer_num,0,
						  g_vipre_handler.buffer_info[ping_pong_side].buffer_num);
			  }
       return EISP_BOOL_TRUE;
	  }
	  return EISP_BOOL_FASE;
}

Eisp_bool vipre_push_new_line(uint16_t element) {
    int16_t base_index;
	uint8_t ping_pong_side = g_vipre_handler.produce_ping_pong_flag;
	Sfifo *vipre_fifo = &g_vipre_handler.fifo[ping_pong_side];
	Svipre_n_line_info *info = &g_vipre_handler.info[ping_pong_side];
	uint16_t line_num;

    if (is_overflow_fifo(vipre_fifo)) {
    	ps_err("vipre input is overflow\n");
    	g_vipre_handler.status = EHW_ERROR_OVER_FLOW;
        return EISP_BOOL_FASE;
    }

	if(vipre_is_last_entry_push_in_frame()==EISP_BOOL_TRUE)
	{
	    line_num = info->line_num_in_last_entry;
	}
	else
	{
		 line_num = info->line_num_per_entry;
	}


    if (push_fifo(vipre_fifo,line_num,0) == EISP_BOOL_FASE) {
    	ps_err("vipre push fail\n");
    	g_vipre_handler.status = EHW_ERROR_OVER_FLOW;
        return EISP_BOOL_FASE;
    }
    if (is_last_entry_push_fifo(vipre_fifo)) {
//        if (ispc_switch_buffer(info, &handler->base)) {
//            base_index = 0;
//        } else {
            base_index =
                (vipre_fifo->offset + info->line_num_used_per_frame) % vipre_fifo->depth;
//        }
//    	base_index = 0;
        ping_pong_side = ping_pong_side ^ 0x1;
        memcpy(&g_vipre_handler.info[ping_pong_side], &g_vipre_handler.base,
               sizeof(Svipre_n_line_info));
        g_vipre_handler.produce_ping_pong_flag = ping_pong_side;
        reset_fifo(&g_vipre_handler.fifo[ping_pong_side],
        		g_vipre_handler.base.line_num_per_buffer,base_index,g_vipre_handler.base.line_num_per_frame);
    }
    return EISP_BOOL_TRUE;
}

int32_t vipre_check_exeptional(int id) {

    return g_vipre_handler.status;
}


Eisp_bool vipre_get_entry_to_pop(const uint16_t line_num,Svipre_buffer *buffer_list){

	uint16_t loop;
    uint8_t ping_pong_side = g_vipre_handler.customer_ping_pong_flag;
    Svipre_n_line_info *info = &g_vipre_handler.info[ping_pong_side];
    Sfifo *fifo = &g_vipre_handler.fifo[ping_pong_side];
    if (is_overflow_fifo(fifo)) {
//        printf("Err ISP input is overflow");
    	g_vipre_handler.status=EHW_ERROR_OVER_FLOW;
        return EISP_BOOL_FASE;
    }
    if (is_empty_fifo(fifo)) {
        return EISP_BOOL_FASE;
    }
    if( line_num >get_element_num_fifo(fifo)){
    	ps_err("vipre pop fail,expect:%d,actrule:%d\n",line_num,get_element_num_fifo(fifo));
    	return EISP_BOOL_FASE;
    }

	buffer_list->line_num = line_num;
	buffer_list->size = buffer_list->line_num*info->size_per_line;
	buffer_list->channel_num=0;

	for(loop=0;loop<MAX_VIPRE_CHANNEL;loop++)
	{
		if(g_vipre_handler.active_channle&(0x01<<loop))
		{
			buffer_list->start_addr[buffer_list->channel_num] = info->channel_buffer[loop].buffer_addr + get_head_fifo(fifo) * info->size_per_line;
			buffer_list->channel_num++;
		}
	}

    return EISP_BOOL_TRUE;

}

static Eisp_bool even_odd_frame_match(uint16_t id,uint16_t frame_id)
{
	uint16_t is_even = frame_id&0x1;
	switch(id)
	{
		case 0:     //EVEN and ODD
			return EISP_BOOL_TRUE;
		case 1:		//ODD
			if(is_even)
			{
				return EISP_BOOL_FASE;
			}
			else
			{
				return EISP_BOOL_TRUE;
			}
		case 2:    //EVEN
			if(is_even)
			{
				return EISP_BOOL_TRUE;
			}
			else
			{
				return EISP_BOOL_FASE;
			}
		default:
			return EISP_BOOL_FASE;
	}

}

Eisp_bool vipre_get_next_entry(uint16_t id,Svipre_buffer *buffer) {
    uint16_t line;
    uint16_t loop;
    uint8_t ping_pong_side = g_vipre_handler.customer_ping_pong_flag;

    Sfifo *vipre_fifo = &g_vipre_handler.fifo[ping_pong_side];
    if (is_empty_fifo(vipre_fifo)) {
        return EISP_BOOL_FASE;
    }

    if(g_vipre_handler.vipre_mode==EVIPRE_MODE_M_FRAME)
    {
    	buffer->channel_num=0;
    	if(even_odd_frame_match(id,get_head_fifo(vipre_fifo))==EISP_BOOL_FASE)
    	{
    		return EISP_BOOL_FASE;
    	}
    	for(loop=0;loop<MAX_VIPRE_CHANNEL;loop++)
    	{
    		if(g_vipre_handler.active_channle&(0x01<<loop))
    		{
    		   	buffer->start_addr[buffer->channel_num] = g_vipre_handler.buffer_info[ping_pong_side].channel_buffer[loop].buffer_addr[get_head_fifo(vipre_fifo)];
    		   	buffer->channel_num++;
    		}
    	}
//    	buffer->start_addr = g_vipre_handler.buffer_info[ping_pong_side].buffer_addr[get_head_fifo(vipre_fifo)];
    	buffer->line_num = g_vipre_handler.buffer_info[ping_pong_side].line_num_per_buffer;
    	buffer->size = g_vipre_handler.buffer_info[ping_pong_side].size_per_line*buffer->line_num;
    	buffer->line_size = g_vipre_handler.buffer_info[ping_pong_side].size_per_line;
    	buffer->fmt = g_vipre_handler.buffer_info[ping_pong_side].data_fmt;
    	return EISP_BOOL_TRUE;
    }
    else
    {
    	Svipre_n_line_info *vipre_info = &g_vipre_handler.info[ping_pong_side];
        if(is_enough_element_to_pop_fifo(vipre_fifo,vipre_info->line_num_per_entry ))
        {
        	line =vipre_info->line_num_per_entry;
        }
        else if(is_enough_element_to_pop_fifo(vipre_fifo,vipre_info->line_num_in_last_entry ))
        {
        	line =vipre_info->line_num_in_last_entry;
        }
        else
        {
        	ps_err("%s,fail get  entry\n",__func__);
        	return EISP_BOOL_FASE;
        }

        if(vipre_get_entry_to_pop(line,buffer)<0)
        {
        	return EISP_BOOL_FASE;
        }
        return EISP_BOOL_TRUE;
    }


}

int16_t vipre_get_pushed_line_num()
{
	Sfifo *fifo = &g_vipre_handler.fifo[g_vipre_handler.produce_ping_pong_flag];
	return get_tail_index_fifo(fifo);
}

int16_t vipre_get_poped_line_num()
{
	Sfifo *fifo = &g_vipre_handler.fifo[g_vipre_handler.customer_ping_pong_flag];
	return get_head_index_fifo(fifo);
}



Eisp_bool vipre_pop_new_entry(int16_t line_num)
{
    uint8_t ping_pong_side = g_vipre_handler.customer_ping_pong_flag;

    Sfifo *vipre_fifo = &g_vipre_handler.fifo[ping_pong_side];
    uint16_t loop_channel;
//    if (is_overflow_fifo(vipre_fifo)) {
//    	ps_err("vipre fifo overflow\n");
//        return EISP_BOOL_FASE;
//    }

    if (is_empty_fifo(vipre_fifo)) {
        return EISP_BOOL_FASE;
    }

	if(g_vipre_handler.vipre_mode == EVIPRE_MODE_M_FRAME)
	{

//	    CLEAR_VIPRE_BUFFER_REG(0,get_head_fifo(vipre_fifo));
//	    CLEAR_VIPRE_BUFFER_REG(1,get_head_fifo(vipre_fifo));
//	    CLEAR_VIPRE_BUFFER_REG(2,get_head_fifo(vipre_fifo));
//	    SET_VIPRE_M_FRAME_BUFFER_READ_DONE(0,get_head_fifo(vipre_fifo));
//	    SET_VIPRE_M_FRAME_BUFFER_READ_DONE(1,get_head_fifo(vipre_fifo));
//	    SET_VIPRE_M_FRAME_BUFFER_READ_DONE(2,get_head_fifo(vipre_fifo));
		uint16_t buffer_id=get_head_fifo(vipre_fifo);
        for(loop_channel=0;loop_channel<MAX_VIPRE_CHANNEL; loop_channel++)
        {
        	if(g_vipre_handler.active_channle &(0x1<<loop_channel))
        	{
        		CLEAR_VIPRE_BUFFER_REG(loop_channel,buffer_id);
        		SET_VIPRE_M_FRAME_BUFFER_READ_DONE(loop_channel,buffer_id);
        	}
        }

	    if (pop_fifo(vipre_fifo,1) < 0) {

	    	ps_err("vipre pop fail\n");
	    	g_vipre_handler.status = EHW_ERROR_POP_FAIL;
	        return EISP_BOOL_FASE;
	    }
//        /***********WR*****************************/
//	    if(is_last_entry_to_pop_fifo(vipre_fifo,1))
//	    {
//	    	SET_VIPRE_ALL_ENTRY_DONE_REG(g_vipre_handler.id);
//	    }
//	    /**********************/
	    if (is_last_entry_pop_fifo(vipre_fifo)) {

	    	SET_VIPRE_ALL_ENTRY_DONE_REG(0);
	        ping_pong_side = ping_pong_side ^ 0x01;
	        g_vipre_handler.customer_ping_pong_flag = ping_pong_side;
	        if (g_vipre_handler.customer_ping_pong_flag !=
	        	g_vipre_handler.produce_ping_pong_flag) {
	        	ps_err("ERR pingpong flag mismatch\n");
	        	g_vipre_handler.status = EHW_ERROR_FRAME_SWITCH_FAIL;
	            return EISP_BOOL_FASE;
	        }
	    }

	}
	else
	{

	    if (pop_fifo(vipre_fifo,line_num) < 0) {
	    	ps_err("vipre pop fail\n");
	    	g_vipre_handler.status = EHW_ERROR_POP_FAIL;
	        return EISP_BOOL_FASE;
	    }
        for(loop_channel=0;loop_channel<MAX_VIPRE_CHANNEL; loop_channel++)
        {
        	if(g_vipre_handler.active_channle &(0x1<<loop_channel))
        	{
				SET_VIPRE_N_LINE_READ_DONE(loop_channel,get_head_index_fifo(vipre_fifo));
				if(get_head_fifo(vipre_fifo)==0 ) //|| is_last_entry_pop_fifo(vipre_fifo)
				{
					SET_VIPRE_ALL_ENTRY_DONE_REG(loop_channel);
				}
        	}
        }

	    if (is_last_entry_pop_fifo(vipre_fifo)) {
	        ping_pong_side = ping_pong_side ^ 0x01;
	        g_vipre_handler.customer_ping_pong_flag = ping_pong_side;
	        if (g_vipre_handler.customer_ping_pong_flag !=
	        		g_vipre_handler.produce_ping_pong_flag) {
	        	ps_err("ERR pingpong flag mismatch\n");
	        	g_vipre_handler.status = EHW_ERROR_FRAME_SWITCH_FAIL;
	            return EISP_BOOL_FASE;
	        }
	    }
	}
    return EISP_BOOL_TRUE;
}

Eisp_bool vipre_is_last_entry_pop() {
    uint8_t ping_pong_side = g_vipre_handler.customer_ping_pong_flag;
    Svipre_n_line_info *vipre_info = &g_vipre_handler.info[ping_pong_side];
    Sfifo *vipre_fifo = &g_vipre_handler.fifo[ping_pong_side];
    if(g_vipre_handler.vipre_mode==EVIPRE_MODE_M_FRAME)
    {
    	return (is_last_entry_to_pop_fifo(vipre_fifo,1));
    }
    else
    {
        return (is_last_entry_to_pop_fifo(vipre_fifo,vipre_info->line_num_in_last_entry));
    }

}

uint32_t vipre_get_number_to_push()
{
    uint8_t ping_pong_side = g_vipre_handler.produce_ping_pong_flag;
    Svipre_n_line_info *vipre_info = &g_vipre_handler.info[ping_pong_side];

    if(g_vipre_handler.vipre_mode==EVIPRE_MODE_M_FRAME)
    {
    	return 1;
    }
    else
    {
        return vipre_info->line_num_in_last_entry;
    }
}

Eisp_bool vipre_is_last_entry_push_in_frame()
{
    uint8_t ping_pong_side = g_vipre_handler.produce_ping_pong_flag;

    Sfifo *vipre_fifo = &g_vipre_handler.fifo[ping_pong_side];
    if(g_vipre_handler.vipre_mode==EVIPRE_MODE_M_FRAME)
    {
        return (
            is_last_entry_to_push_fifo(vipre_fifo,1));
    }
    else
    {
    	Svipre_n_line_info *vipre_info = &g_vipre_handler.info[ping_pong_side];
        return (
            is_last_entry_to_push_fifo(vipre_fifo,vipre_info->line_num_in_last_entry));
    }

}

Eisp_bool vipre_is_last_entry_pop_in_frame() {
    uint8_t ping_pong_side = g_vipre_handler.customer_ping_pong_flag;

    Sfifo *vipre_fifo = &g_vipre_handler.fifo[ping_pong_side];
    if(g_vipre_handler.vipre_mode==EVIPRE_MODE_M_FRAME)
    {
        return (
            is_last_entry_to_pop_fifo(vipre_fifo,1));
    }
    else
    {
    	Svipre_n_line_info *vipre_info = &g_vipre_handler.info[ping_pong_side];
        return (
            is_last_entry_to_pop_fifo(vipre_fifo,vipre_info->line_num_in_last_entry));
    }

}


int32_t vipre_get_line_size() {
    uint8_t ping_pong_side = g_vipre_handler.customer_ping_pong_flag;
    if(g_vipre_handler.vipre_mode == EVIPRE_MODE_M_FRAME)
    {
    	return g_vipre_handler.buffer_info[ping_pong_side].size_per_line;
    }
    else
    {
    	Svipre_n_line_info *vipre_info = &g_vipre_handler.info[ping_pong_side];
        return vipre_info->size_per_line;
    }

}
uint32_t vipre_get_frame_size()
{
	uint8_t ping_pong_side = g_vipre_handler.customer_ping_pong_flag;
	if(g_vipre_handler.vipre_mode == EVIPRE_MODE_M_FRAME)
	{
		Svipre_buffer_list *vipre_info = &g_vipre_handler.buffer_info[ping_pong_side];
		return vipre_info->line_num_per_buffer*vipre_info->size_per_line;
	}
	else
	{
		Svipre_n_line_info *vipre_info = &g_vipre_handler.info[ping_pong_side];
				return vipre_info->line_num_per_frame *vipre_info->size_per_line;
	}

}
uint32_t vipre_get_frame_height()
{
	  uint8_t ping_pong_side = g_vipre_handler.customer_ping_pong_flag;
	  if(g_vipre_handler.vipre_mode == EVIPRE_MODE_M_FRAME)
	  {
			Svipre_buffer_list *vipre_info = &g_vipre_handler.buffer_info[ping_pong_side];
			return vipre_info->line_num_per_buffer;
	  }
	  else
	  {
		    Svipre_n_line_info *vipre_info = &g_vipre_handler.info[ping_pong_side];
			return vipre_info->line_num_per_frame;
	  }
}

Eisp_bool vipre_is_all_entry_done()
{
	  uint8_t ping_pong_side = g_vipre_handler.customer_ping_pong_flag;
		    Sfifo *vipre_fifo = &g_vipre_handler.fifo[ping_pong_side];
	return is_last_entry_pop_fifo(vipre_fifo);
}

Eisp_bool vipre_is_frame_mode()
{
	return g_vipre_handler.vipre_mode == EVIPRE_MODE_M_FRAME;
}
void vipre_set_overflow()
{
	g_vipre_handler.status = EHW_ERROR_OVER_FLOW;
}

void vipre_recovery_state_update(uint16_t id )
{
	if(g_vipre_handler.vipre_mode == EVIPRE_MODE_M_FRAME)
	{
		switch(g_vipre_handler.status)
		{
//			case EHW_RECOVERYING:
//				g_vipre_handler.status = EHW_RECOVERY_DONE;
//				break;
			case EHW_ERROR_OVER_FLOW:
				vipre_pop_new_entry(1);
				g_vipre_handler.status = EHW_RECOVERYING;
				break;
			case EHW_RECOVERY_DONE:
			case EHW_RECOVERYING:
				g_vipre_handler.status = EHW_OK;
				break;
			default:

				break;
		}
	}
	ps_debug("update vipre state:%d\n",g_vipre_handler.status);

}

void vipre_store_frame_meta( Sfifo *vipre_fifo )
{
    uint32_t index;

	index = get_tail_fifo(vipre_fifo);
	if(index<MAX_VIPRE_BUFFER && g_vipre_handler.frame_meta_reg)
	{
		memcpy(&g_vipre_handler.meta[index],g_vipre_handler.frame_meta_reg,sizeof(frame_meta_t));
	}else
	{
		ps_wrn("invalid index\n");
	}

}

frame_meta_t * vipre_get_frame_meta()
{
    uint8_t ping_pong_side = g_vipre_handler.customer_ping_pong_flag;
    Sfifo *vipre_fifo = &g_vipre_handler.fifo[ping_pong_side];
    uint32_t index;
	if(g_vipre_handler.vipre_mode == EVIPRE_MODE_M_FRAME)
	{
		index = get_head_fifo(vipre_fifo);
		if(index<MAX_VIPRE_BUFFER )
		{
			return &g_vipre_handler.meta[index];
		}else
		{
			ps_wrn("invalid index\n");
			return NULL;
		}
	}
	return NULL;
}


Eisp_bool vipre_is_empty()
{
    uint8_t ping_pong_side = g_vipre_handler.customer_ping_pong_flag;
    Svipre_n_line_info *info = &g_vipre_handler.info[ping_pong_side];
    Sfifo *fifo = &g_vipre_handler.fifo[ping_pong_side];
	return is_empty_fifo(fifo);
}
