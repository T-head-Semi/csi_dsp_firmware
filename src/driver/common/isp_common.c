#include <string.h>
#include "isp_common.h"
#include "dsp_ps_debug.h"

int32_t ispc_base_info_update(Sisp_control_info *base_info, uint16_t hor_res,
                              uint16_t ver_res, csi_dsp_img_fmt_e raw_type,
                              uint16_t line_value, uint64_t buffer_addr,
                              uint32_t buffer_size,uint16_t line_size) {

//    uint16_t line_size = ispc_get_line_size(raw_type,
//                                            hor_res); // size_per_px * hor_res;
    if ((hor_res > MAX_HORIZONTAL) || (ver_res > MAX_VERTICAL) ||
        (line_value > LINES_MASK) || ((buffer_size % line_size) != 0) ||
        (buffer_addr == 0)) {
        return EHW_ERROR_PARMA_INVALID;
    }
    /*should check if there is issue if not the last N
     line cross the buffer bottom*/


    uint16_t fragment_line = ver_res % line_value;
    base_info->line_num_per_entry = line_value;
    base_info->line_num_per_frame = ver_res;
    base_info->line_num_in_last_entry =
        fragment_line == 0 ? line_value : fragment_line;
    base_info->size_per_line = line_size;
    base_info->line_num_per_buffer = buffer_size / line_size;
    base_info->frame_buffer_addr = buffer_addr;
    base_info->format = raw_type;
    return EHW_OK;
}
uint32_t ispc_check_exeptional(Sisp_handler *handler) {
    uint8_t ping_pong_side = handler->customer_ping_pong_flag;
    Sfifo *fifo = &handler->fifo[ping_pong_side];
    if (is_overflow_fifo(fifo)) {
        //  printf("Err ISP input is ovetflow");
        return EHW_ERROR_OVER_FLOW;
    }
    return EHW_OK;
}
Eisp_bool ispc_get_entry_to_pop(const Sisp_handler *handler,const uint16_t line_num,Sisp_buffer buffer_list[MAX_BUFFER_BLOCK]){

    uint8_t ping_pong_side = handler->customer_ping_pong_flag;
    const Sisp_control_info *info = &handler->info[ping_pong_side];
    const Sfifo *fifo = &handler->fifo[ping_pong_side];
    uint16_t start_offset;
//    if (is_overflow_fifo(fifo)) {
////        printf("Err ISP input is overflow");
//        return EISP_BOOL_FASE;
//    }
    if (is_empty_fifo(fifo)) {
        return EISP_BOOL_FASE;
    }
    if(line_num>fifo->depth|| line_num >get_element_num_fifo(fifo)){
    	return EISP_BOOL_FASE;
    }
    start_offset= get_head_fifo(fifo);
    if(get_depth_fifo(fifo)>=start_offset+line_num)
    {
    	buffer_list[0].line_num = line_num;
    	buffer_list[0].line_size = info->size_per_line;
		buffer_list[0].start_addr = info->frame_buffer_addr + get_head_fifo(fifo) * info->size_per_line;
		buffer_list[0].size = buffer_list[0].line_num*info->size_per_line;
		buffer_list[0].fmt = info->format;
		buffer_list[1].line_num =0;
		buffer_list[1].start_addr = 0;
		buffer_list[1].size = 0;
    }
    else
    {
    	buffer_list[0].line_num = get_depth_fifo(fifo) - start_offset;
    	buffer_list[0].start_addr = info->frame_buffer_addr + get_head_fifo(fifo) * info->size_per_line;
    	buffer_list[0].size = buffer_list[0].line_num*info->size_per_line;
       	buffer_list[0].line_size = info->size_per_line;
       	buffer_list[0].fmt = info->format;
    	buffer_list[1].line_num = line_num - buffer_list[0].line_num;
    	buffer_list[1].start_addr = info->frame_buffer_addr;
    	buffer_list[1].size = buffer_list[1].line_num *info->size_per_line;
       	buffer_list[1].line_size = info->size_per_line;
       	buffer_list[1].fmt = info->format;

    }
    return EISP_BOOL_TRUE;

}


Eisp_bool ispc_pop_new_line(Sisp_handler *handler,uint16_t line_num) {
    uint8_t ping_pong_side = handler->customer_ping_pong_flag;
    Sisp_control_info *info = &handler->info[ping_pong_side];
    Sfifo *fifo = &handler->fifo[ping_pong_side];

    if (handler->status != EHW_OK) {
        return EISP_BOOL_FASE;
    }
    if (is_empty_fifo(fifo)) {
//    	handler->status=EHW_INPUT_EMPRY;
        return EISP_BOOL_FASE;
    }

    if (pop_fifo(fifo,line_num) < 0) {
       	handler->status=EHW_ERROR_POP_FAIL;
        return EISP_BOOL_FASE;
    }
    if(get_head_index_fifo(fifo)>info->line_num_per_frame)
    {
    	 handler->status=EHW_ERROR_POP_FAIL;
    	 return EISP_BOOL_FASE;
    }
        ping_pong_side = ping_pong_side ^ 0x01;
    if (is_last_entry_pop_fifo(fifo)) {
        handler->customer_ping_pong_flag = ping_pong_side;
        if (handler->customer_ping_pong_flag !=
            handler->produce_ping_pong_flag) {
        	ps_err("ERR pingpong flag mismatch\n");
        	handler->status=EHW_ERROR_FRAME_SWITCH_FAIL;
            return EISP_BOOL_FASE;
        }
    }
    return EISP_BOOL_TRUE;
}

Eisp_bool ispc_push_new_line(Sisp_handler *handler,uint16_t element) {
    int16_t base_index;
    uint8_t ping_pong_side = handler->produce_ping_pong_flag;
    Sisp_control_info *info = &handler->info[ping_pong_side];
    Sfifo *fifo = &handler->fifo[ping_pong_side];
    uint16_t offset=0;
    if (handler->status !=EHW_OK) {
        return EISP_BOOL_FASE;
    }
    if(element<info->line_num_per_entry ||element>info->line_num_per_entry*2 )
    {
    	if(!is_last_entry_to_push_fifo(fifo,info->line_num_per_entry))
    	{
			handler->status=EHW_ERROR_PARMA_INVALID;
			ps_err("new entry size:%d not meet expect\n",element);
			return EISP_BOOL_FASE;
    	}
    }
    if((handler->produce_ping_pong_flag != handler->customer_ping_pong_flag) &&
    		(handler->info[ping_pong_side].frame_buffer_addr==handler->info[handler->customer_ping_pong_flag].frame_buffer_addr))
    {
    	offset = get_element_num_fifo(&handler->fifo[handler->customer_ping_pong_flag]);
    }
    if (push_fifo(fifo,info->line_num_per_entry ,offset) == EISP_BOOL_FASE) {
    	handler->status=EHW_ERROR_OVER_FLOW;
    	ps_err("push fail\n");
        return EISP_BOOL_FASE;
    }
    if (is_last_entry_push_fifo(fifo)) {
        if (ispc_switch_buffer(info, &handler->base)) {
            base_index = 0;
        } else {
            base_index =
                (fifo->offset + info->line_num_used_per_frame) % fifo->depth;
        }
        ping_pong_side = ping_pong_side ^ 0x1;
        memcpy((void *)&handler->info[ping_pong_side], (void *)&handler->base,
               sizeof(Sisp_control_info));
        handler->interact_counter = 0;
        handler->produce_ping_pong_flag = ping_pong_side;
        reset_fifo(&handler->fifo[ping_pong_side],
                   handler->base.line_num_per_buffer,base_index,handler->base.line_num_per_frame);
    }
    return EISP_BOOL_TRUE;
}

Eisp_bool ispc_switch_buffer(Sisp_control_info *isp_info,
                             Sisp_control_info *base) {
    return (isp_info->frame_buffer_addr != base->frame_buffer_addr) ||
           (isp_info->line_num_per_frame != base->line_num_per_frame) ||
           (isp_info->line_num_in_last_entry != base->line_num_in_last_entry);
}

int16_t ispc_get_pushed_line_num(Sisp_handler *handler)
{
    uint8_t ping_pong_side = handler->produce_ping_pong_flag;
    Sfifo *fifo = &(handler->fifo[ping_pong_side]);

    return get_tail_index_fifo(fifo);
}

int16_t ispc_get_poped_line_num(Sisp_handler *handler)
{
    uint8_t ping_pong_side = handler->customer_ping_pong_flag;
    Sfifo *fifo = &(handler->fifo[ping_pong_side]);
    return get_head_index_fifo(fifo);
}


//int32_t ispc_get_line_size(csi_dsp_img_fmt_e data_format,
//                           uint16_t pexs_per_hor) {
//    int32_t size = 0;
//    switch (data_format) {
//    case EISP_RAW12_FORMAT_UNALGIN:
//        size = (pexs_per_hor * 12 + 7) / 8;
//        break;
//    case EISP_RAW12_FORMAT_ALGIN0:
//        size = (pexs_per_hor * 12 + 7) / 8;
//        break;
//    case EISP_RAW12_FORMAT_ALGIN1:
//        size = pexs_per_hor * 16 / 8;
//        break;
//    case EISP_RAW8_FORMAT :
//    	size = pexs_per_hor ;
//    	break;
//    default:
//    	ps_err("ERR invalid data format\n");
//        return -1;
//    }
//    return size;
//}
