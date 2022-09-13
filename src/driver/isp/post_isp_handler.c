#include<stdlib.h>
#include<string.h>
#include "isp_common.h"
#include "post_isp_hw.h"
#include "dsp_ps_debug.h"
Sisp_handler g_post_isp_handler ISP_SECTION;

int32_t post_isp_initialize(uint16_t id) {
    if (id > MAX_POST_ISP) {
        return EHW_ERROR_PARMA_INVALID;
    }
    memset((void *)&g_post_isp_handler, 0x0, sizeof(g_post_isp_handler));
    g_post_isp_handler.id = id;
    return EHW_OK;
}


int32_t post_isp_handler_config(uint16_t hor_res, uint16_t ver_res,
								csi_dsp_img_fmt_e raw_type, uint16_t line_value,
                                uint64_t buffer_addr, uint32_t buffer_size,uint16_t line_size) {
    int32_t ret;
    ret = ispc_base_info_update(&(g_post_isp_handler.base), hor_res, ver_res,
    							raw_type, line_value, buffer_addr,
                                buffer_size, line_size);
    if (ret != EHW_OK) {
    	g_post_isp_handler.status=ret;
        return ret;
    }
    g_post_isp_handler.base.line_num_used_per_frame=  g_post_isp_handler.base.line_num_per_frame -
    					g_post_isp_handler.base.line_num_in_last_entry+g_post_isp_handler.base.line_num_per_entry;
    const uint8_t start_side = 0;
    g_post_isp_handler.customer_ping_pong_flag = start_side;
    g_post_isp_handler.produce_ping_pong_flag = start_side;
    memcpy((void *)&g_post_isp_handler.info[start_side],(void *) &g_post_isp_handler.base,
           sizeof(Sisp_control_info));
    reset_fifo(&g_post_isp_handler.fifo[start_side],
               g_post_isp_handler.base.line_num_per_buffer, 0,g_post_isp_handler.base.line_num_per_frame);
    post_isp_register_interrupt(g_post_isp_handler.id);
    return EHW_OK;
}


int post_isp_get_status(uint16_t id)
{
	return g_post_isp_handler.status;
}
void post_isp_soft_reset(uint16_t id){

	const uint8_t start_side = 0;
	post_isp_disable_inerrupt(0);
    g_post_isp_handler.customer_ping_pong_flag = start_side;
    g_post_isp_handler.produce_ping_pong_flag = start_side;
	reset_fifo(&g_post_isp_handler.fifo[start_side],
	               g_post_isp_handler.base.line_num_per_buffer, 0,g_post_isp_handler.base.line_num_per_frame);
}

//int32_t post_isp_hard_reset(uint16_t id){
//     post_isp_disable_inerrupt(0);
//}
//

int32_t post_isp_get_next_entry(Sisp_buffer *entry_info) {
    uint32_t bock_size;
    uint8_t ping_pong_side = g_post_isp_handler.produce_ping_pong_flag;
    Sisp_control_info *post_isp_info = &g_post_isp_handler.info[ping_pong_side];
    Sfifo *post_isp_fifo = &g_post_isp_handler.fifo[ping_pong_side];

    int16_t index = get_tail_fifo(post_isp_fifo);
    if (index < 0 || index%post_isp_info->line_num_per_entry!=0) {
    	ps_err("invalid param\n");
        entry_info->line_num =0;
        entry_info->size =0;
        g_post_isp_handler.status=EHW_ERROR_PARMA_INVALID;
        return EHW_ERROR_PARMA_INVALID;
    }
    entry_info->start_addr= post_isp_info->frame_buffer_addr + index * post_isp_info->size_per_line;
    entry_info->line_num = is_last_entry_to_push_fifo(post_isp_fifo,post_isp_info->line_num_in_last_entry)
                ? post_isp_info->line_num_in_last_entry
                : post_isp_info->line_num_per_entry;
    entry_info->size = entry_info->line_num*post_isp_info->size_per_line;
    entry_info->line_size = post_isp_info->size_per_line;
    entry_info->fmt = post_isp_info->format;
    return EHW_OK;
}


uint32_t post_isp_pop_entry()
{
    uint8_t ping_pong_side = g_post_isp_handler.customer_ping_pong_flag;
    Sisp_control_info *post_isp_info = &g_post_isp_handler.info[ping_pong_side];
    Sfifo *post_isp_fifo = &g_post_isp_handler.fifo[ping_pong_side];
    uint16_t line_num = post_isp_info->line_num_per_entry;
    if(is_last_entry_to_pop_fifo(post_isp_fifo,post_isp_info->line_num_in_last_entry))
    {
    	line_num = post_isp_info->line_num_in_last_entry;
    }
    return ispc_pop_new_line(&g_post_isp_handler,line_num);

}

int16_t post_isp_get_pushed_line_num()
{
	return ispc_get_pushed_line_num(&g_post_isp_handler);
}

int16_t post_isp_get_poped_line_num()
{
	return ispc_get_poped_line_num(&g_post_isp_handler);
}



int32_t post_isp_push_entry(uint16_t line_num) {

	Eisp_bool ret = ispc_push_new_line(&g_post_isp_handler,line_num);
	if(ret==EISP_BOOL_TRUE)
	{
		TRIGGER_POST_ISP_REG(0);
		g_post_isp_handler.current_N_value = ispc_get_pushed_line_num(&g_post_isp_handler);
		return 0;
	}
	else
	{
		ps_err("Post ISP push fail\n");
		return -1;
	}


}
Eisp_bool post_isp_is_last_block_in_frame() {
    uint8_t ping_pong_side = g_post_isp_handler.produce_ping_pong_flag;
    Sisp_control_info *post_isp_info = &g_post_isp_handler.info[ping_pong_side];
    Sfifo *post_isp_fifo = &g_post_isp_handler.fifo[ping_pong_side];
    return (is_last_entry_to_push_fifo(post_isp_fifo,post_isp_info->line_num_in_last_entry));
}

Eisp_bool post_isp_is_last_block_pop_in_frame() {
    uint8_t ping_pong_side = g_post_isp_handler.customer_ping_pong_flag;
    Sisp_control_info *post_isp_info = &g_post_isp_handler.info[ping_pong_side];
    Sfifo *post_isp_fifo = &g_post_isp_handler.fifo[ping_pong_side];
    return (is_last_entry_to_pop_fifo(post_isp_fifo,post_isp_info->line_num_in_last_entry));
}
int32_t post_isp_get_line_size() {
    uint8_t ping_pong_side = g_post_isp_handler.produce_ping_pong_flag;
    Sisp_control_info *post_isp_info = &g_post_isp_handler.info[ping_pong_side];
    return post_isp_info->size_per_line;
}

uint32_t post_isp_get_frame_size()
{
	  uint8_t ping_pong_side = g_post_isp_handler.customer_ping_pong_flag;
	    Sisp_control_info *post_isp_info = &g_post_isp_handler.info[ping_pong_side];
	    return post_isp_info->line_num_per_frame*post_isp_info->size_per_line;
}

Eisp_bool post_isp_is_full()
{
	  uint8_t ping_pong_side = g_post_isp_handler.produce_ping_pong_flag;
	   Sfifo *post_isp_fifo = &g_post_isp_handler.fifo[ping_pong_side];
	  return is_full_fifo(post_isp_fifo);
}

Eisp_bool post_isp_is_empty()
{
	  uint8_t ping_pong_side = g_post_isp_handler.produce_ping_pong_flag;
	   Sfifo *post_isp_fifo = &g_post_isp_handler.fifo[ping_pong_side];
	  return is_empty_fifo(post_isp_fifo);
}


void post_isp_fifo_reset(uint16_t id )
{
	uint8_t reset_side = g_post_isp_handler.produce_ping_pong_flag;

    Sisp_control_info *info = &g_post_isp_handler.info[reset_side];
    Sfifo *fifo = &g_post_isp_handler.fifo[reset_side];
    g_post_isp_handler.customer_ping_pong_flag =reset_side;
    g_post_isp_handler.produce_ping_pong_flag = reset_side;
	uint16_t base_index =
        (fifo->offset + info->line_num_used_per_frame) % fifo->depth;

	memcpy((void *)&g_post_isp_handler.info[reset_side], (void *)&g_post_isp_handler.base,
		   sizeof(Sisp_control_info));
	g_post_isp_handler.interact_counter = 0;
	g_post_isp_handler.current_N_value = 0;
	reset_fifo(&g_post_isp_handler.fifo[reset_side],
			g_post_isp_handler.base.line_num_per_buffer,base_index,g_post_isp_handler.base.line_num_per_frame);
//	g_post_isp_handler.status = EHW_OK;
}

void post_isp_recovery_state_update(uint16_t id )
{
	switch(g_post_isp_handler.status)
	{
		case EHW_RECOVERYING:
			post_isp_fifo_reset(id);
			g_post_isp_handler.status = EHW_RECOVERY_DONE;
			break;
		case EHW_ERROR_OVER_FLOW:
			break;
		case EHW_RECOVERY_DONE:
			g_post_isp_handler.status = EHW_OK;
			break;
		default:
			TRIGGER_POST_ISP_REG(0);
			g_post_isp_handler.current_N_value += g_post_isp_handler.base.line_num_per_entry;
			g_post_isp_handler.status = EHW_RECOVERYING;
			break;
	}

}
