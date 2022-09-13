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
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <xtensa/corebits.h>
#include <xtensa/xtruntime.h>
#if HAVE_THREADS_XOS
#include <xtensa/xos.h>
#endif
#include "dsp_ps_debug.h"
#include "xrp_api.h"
#include "xrp_dsp_hw.h"
#include "xrp_dsp_user.h"
#include "dsp_ps_ns.h"
#include "csi_dsp_task_defs.h"
#include "csi_dsp_post_process_defs.h"
#include "vi_hw_api.h"
#include "algo.h"
#include "ps_sched.h"
#include "idma_if.h"

#define MAX_SW_BE   3


static sw_be_handler_t g_sw_be_hands[MAX_SW_BE];



int ps_fill_report(void *buffer,csi_dsp_report_e type,void*payload,size_t size)
{
	csi_dsp_report_item_t *ptr = (csi_dsp_report_item_t *)buffer;
	if(buffer)
	{
		ptr->type=type;
		if(payload!=NULL&&size!=0)
		{
			memcpy(ptr->data,payload,size);
		}
		return DSP_REPORT_TO_HOST_FLAG;
	}
	return 0;
}

int csi_dsp_result_report(void* report_buf,void*result,size_t result_sz,void* extra,size_t extra_sz)
{
	csi_dsp_report_item_t *ptr = (csi_dsp_report_item_t *)report_buf;
	size_t filled_sz=0;
	if(ptr && ((result_sz+extra_sz)<MAX_REPORT_SIZE))
	{
		ptr->type=0;
		if(result!=NULL&&result_sz!=0)
		{
			memcpy(ptr->data,result,result_sz);
			filled_sz +=result_sz;
			ptr->type=CSI_DSP_REPORT_RESULT;
		}
		if(extra!=NULL&&extra_sz!=0)
		{
			memcpy(ptr->data+filled_sz,extra,extra_sz);
			filled_sz +=result_sz;
			ptr->type = ptr->type==CSI_DSP_REPORT_RESULT?CSI_DSP_REPORT_RESULT_WITH_EXRA_PARAM:CSI_DSP_REPORT_EXRA_PARAM;
		}
		return 0;
	}
	else
	{
		ps_err("invalid param for report filled\n");
		return -1;
	}

}

static int get_isp_next_entry(int id, entry_struct_t *ctrl_info){
	Sisp_buffer  isp_buffer_list[MAX_BUFFER_BLOCK];
	Sisp_buffer  p_isp_buffer;

	if (isp_get_next_entry(isp_buffer_list) != EISP_BOOL_TRUE)
	{
		return -1;
	}
	if(isp_buffer_list[1].size!=0)
	{
		return -1;
	}
	ctrl_info->x = 0;
	ctrl_info->y =isp_get_poped_line_num();
	ctrl_info->height= isp_buffer_list[0].line_num;
	ctrl_info->fmt = isp_buffer_list[1].fmt;
	ctrl_info->frame_height = isp_get_frame_height();
	ctrl_info->pitch= isp_buffer_list[0].line_size;
	ctrl_info->pBuff =isp_buffer_list[0].start_addr;
	memset(&ctrl_info->meta,0x0,sizeof(csi_dsp_frame_meta_t));
	return 0;
}

static int get_post_isp_next_entry(int id, struct csi_dsp_buffer * buf_entry)
{

	Sisp_buffer  p_isp_buffer;
	if (post_isp_get_next_entry(&p_isp_buffer) != EHW_OK)
	{
		return -1;
	}
//	ctrl_info->frame=NULL;
//	ctrl_info->x =0;
//	ctrl_info->y= isp_get_pushed_line_num();
//	ctrl_info->pitch = p_isp_buffer.line_size;
//	ctrl_info->pBuff = p_isp_buffer.start_addr;
//	ctrl_info->height =p_isp_buffer.line_num;

//	buf_entry->width = p_isp_buffer.
	buf_entry->height = p_isp_buffer.line_num;
	buf_entry->format = p_isp_buffer.fmt;
	buf_entry->plane_count=1;
	buf_entry->planes[0].buf_phy= p_isp_buffer.start_addr;
	buf_entry->planes[0].stride = p_isp_buffer.line_size;
	buf_entry->planes[0].size =  p_isp_buffer.line_size*p_isp_buffer.line_num;
	return 0;
}
static inline int isp_entry_done(int id ,entry_struct_t *ctrl_info)
{
	return isp_pop_n_line(ctrl_info->height);
}

enum xrp_status ps_isp_config_message_handler(void* in_data,void* context)
{
	sisp_config_par *config = (sisp_config_par *)in_data;
    if(isp_initialize(config->id))
    {
    	return XRP_STATUS_FAILURE;
    }
    if(isp_handler_config(config->hor,config->ver, config->data_fmt, config->line_in_entry,
                       (uint64_t)config->buffer_addr, config->buffer_size,config->line_stride))
    {
    	return XRP_STATUS_FAILURE;
    }
    ((dsp_fe_handler_t*)context)->type = CSI_DSP_FE_TYPE_ISP;
    ((dsp_fe_handler_t*)context)->id = config->id;
    ((dsp_fe_handler_t*)context)->get_next_entry = get_isp_next_entry;
    ((dsp_fe_handler_t*)context)->get_state = isp_get_status;
    ((dsp_fe_handler_t*)context)->entry_done = isp_entry_done;
    ((dsp_fe_handler_t*)context)->update_state= isp_recovery_state_update;

    return XRP_STATUS_SUCCESS;
}

static int get_vipre_next_entry(int id, entry_struct_t *ctrl_info){

	Svipre_buffer  buffer_list;
	frame_meta_t * vipre_fmeta;
	if (vipre_get_next_entry(id,&buffer_list)!= EISP_BOOL_TRUE)
	{
		return -1;
	}

	ctrl_info->frame_height = vipre_get_frame_height();
	ctrl_info->height= buffer_list.line_num;
	ctrl_info->pitch= buffer_list.line_size;
	ctrl_info->pBuff =buffer_list.start_addr[0];
	if(ctrl_info->frame_height == ctrl_info->height)
	{
		ctrl_info->x = -1;
		ctrl_info->y = -1;
	}
	else
	{
		/*TODO VIPRE N LINE¡¡£Í£Ï£Ä£Å*/
	}
	vipre_fmeta = vipre_get_frame_meta();
	if(vipre_fmeta)
	{
		ctrl_info->meta.frame_id = vipre_fmeta->frame_cnt;
		ctrl_info->meta.temp_projector = vipre_fmeta->projection_temperature;
		ctrl_info->meta.temp_sensor = vipre_fmeta->floodlight_temperature;
		ctrl_info->meta.timestap = vipre_fmeta->frame_ts_us;
	}
	else
	{
		memset(&ctrl_info->meta,0x0,sizeof(csi_dsp_frame_meta_t));
	}
	return 0;
}

enum xrp_status ps_vipre_config_message_handler(void* in_data,void* out_data)
{
	vipre_config_par_t *config = (vipre_config_par_t *)in_data;
	int ret =0;
    if(vipre_initialize())
    {
    	return XRP_STATUS_FAILURE;
    }
    if(config->line_num ==0)
    {
    	// M frame mode
    	ret =vipre_frame_mode_handler_config(config->act_channel,config->hor,config->ver,config->line_stride, config->data_fmt ,
    										 config->buffer_num,config->buffer_addr);
    }
    else
    {
    	   //N line mode
    	ret =vipre_handler_nline_config(config->act_channel,config->hor,config->ver,config->line_stride, config->data_fmt ,
    			config->line_num,config->buffer_addr, config->buffer_size);
    }
    if(ret)
    {
    	return XRP_STATUS_FAILURE;
    }
    ((dsp_fe_handler_t*)out_data)->id = config->id;
    ((dsp_fe_handler_t*)out_data)->type = CSI_DSP_FE_TYPE_VIPRE;
    ((dsp_fe_handler_t*)out_data)->get_next_entry = get_vipre_next_entry;
    ((dsp_fe_handler_t*)out_data)->get_state = vipre_check_exeptional;
    ((dsp_fe_handler_t*)out_data)->entry_done = vipre_pop_new_entry;
    ((dsp_be_handler_t*)out_data)->update_state = vipre_recovery_state_update;

    return XRP_STATUS_SUCCESS;
}
static inline int post_isp_entry_start(int id ,struct csi_dsp_buffer * buf_entry)
{
	return post_isp_push_entry(buf_entry->height);
}
enum xrp_status ps_post_isp_config_message_handler(void* in_data,void* out_data)
{
	sisp_config_par *config = (sisp_config_par *)in_data;
    if(post_isp_initialize(config->id))
    {
    	return XRP_STATUS_FAILURE;
    }
    if(post_isp_handler_config(config->hor,config->ver, config->data_fmt, config->line_in_entry,
                       (uint64_t)config->buffer_addr, config->buffer_size,config->line_stride))
    {
    	return XRP_STATUS_FAILURE;
    }
    ((dsp_be_handler_t*)out_data)->type = CSI_DSP_BE_TYPE_POST_ISP;
    ((dsp_be_handler_t*)out_data)->get_next_entry = get_post_isp_next_entry;
    ((dsp_be_handler_t*)out_data)->get_state = post_isp_get_status;
    ((dsp_be_handler_t*)out_data)->entry_start = post_isp_entry_start;
    ((dsp_be_handler_t*)out_data)->update_state = post_isp_recovery_state_update;
    return XRP_STATUS_SUCCESS;
}
void sw_be_init()
{
	int id;
	for(id=0;id<MAX_SW_BE;id++)
	{
		g_sw_be_hands[id].status = DSP_BUF_STATUS_FREE;
	}

}
static int get_sw_be_next_entry(int id, struct csi_dsp_buffer * buf_entry)
{
	if(g_sw_be_hands[id].status != DSP_BUF_STATUS_ALLOCTED)
	{
		return -1;
	}
	if(g_sw_be_hands[id].valid_buf_num <=0)
	{
		return -1;
	}
	int buf_id = g_sw_be_hands[id].head_id;
	memcpy(buf_entry,&g_sw_be_hands[id].bufs[buf_id],sizeof(struct csi_dsp_buffer));
	return 0;
}

int sw_be_get_exeptional(int id)
{
	return g_sw_be_hands[id].status ;
}

enum xrp_status sw_be_asgin_buf(int id ,sw_be_config_par* cfg)
{
	int i,buf_id;
	if((g_sw_be_hands[id].valid_buf_num+cfg->num_buf) >g_sw_be_hands[id].buf_num)
	{
		return XRP_STATUS_FAILURE;
	}
	buf_id = g_sw_be_hands[id].head_id+g_sw_be_hands[id].valid_buf_num;
	for(i=0;i<cfg->num_buf;i++)
	{
		memcpy(&g_sw_be_hands[id].bufs[(i+buf_id)%g_sw_be_hands[id].buf_num],&cfg->bufs[i],sizeof(struct csi_dsp_buffer));
	}
	g_sw_be_hands[id].valid_buf_num+=cfg->num_buf;
	if(	g_sw_be_hands[id].status == DSP_BUF_STATUS_NO_MEMORY && g_sw_be_hands[id].valid_buf_num>1)
	{
		g_sw_be_hands[id].status = DSP_BUF_STATUS_ALLOCTED;
	}
	return XRP_STATUS_SUCCESS;
}

static inline int sw_be_entry_start(int id ,void *extra_param)
{
	int buf_id = g_sw_be_hands[id].head_id;
	if(g_sw_be_hands[id].report ==NULL)
	{
		g_sw_be_hands[id].status = DSP_BUF_STATUS_INVALID;
		return -1;
	}

	if(g_sw_be_hands[id].status != DSP_BUF_STATUS_ALLOCTED)
	{
		return -1;
	}
	if(g_sw_be_hands[id].valid_buf_num <=1)
	{
		g_sw_be_hands[id].status = DSP_BUF_STATUS_NO_MEMORY;
		ps_fill_report(g_sw_be_hands[id].report,CSI_DSP_REPORT_NO_BUF,NULL,0);

		return -1;
	}

	if(buf_id>=g_sw_be_hands[id].buf_num)
	{
		g_sw_be_hands[id].status = DSP_BUF_STATUS_INVALID;
		return -1;
	}
	if(ps_report_avaiable() <0)
	{
		ps_fill_report(g_sw_be_hands[id].report,CSI_DSP_HW_FRAME_DROP,NULL,0);
		return -1;
	}
//	ps_fill_report(g_sw_be_hands[id].report,CSI_DSP_REPORT_RESULT,&g_sw_be_hands[id].bufs[buf_id],sizeof(struct csi_dsp_buffer));
	g_sw_be_hands[id].head_id = (g_sw_be_hands[id].head_id+1)%g_sw_be_hands[id].buf_num;
	g_sw_be_hands[id].valid_buf_num--;
	return 0;
}

void sw_recovery_state_update(int id )
{

	switch(g_sw_be_hands[id].status)
	{
		case EHW_RECOVERYING:
			g_sw_be_hands[id].status = EHW_RECOVERY_DONE;
			break;
		case EHW_ERROR_OVER_FLOW:
			g_sw_be_hands[id].status = EHW_RECOVERY_DONE;
			break;
		case EHW_RECOVERY_DONE:
			g_sw_be_hands[id].status = EHW_OK;
			break;
		default:
			g_sw_be_hands[id].status = EHW_RECOVERY_DONE;
			break;
	}

	ps_debug("update sw state:%d",g_sw_be_hands[id].status);

}

enum xrp_status ps_sw_task_out_config_message_handler(void* in_data,void* out_data,void * report_buf,uint32_t report_buf_size)
{
	sw_be_config_par* cfg = (sw_be_config_par* )in_data;
	int  id;
	for(id=0;id<MAX_SW_BE;id++)
	{
		if(g_sw_be_hands[id].status ==DSP_BUF_STATUS_FREE)
		{
			g_sw_be_hands[id].bufs = malloc(cfg->num_buf*sizeof(struct csi_dsp_buffer));
			if(g_sw_be_hands[id].bufs == NULL)
			{
				return XRP_STATUS_FAILURE;
			}
			g_sw_be_hands[id].id = id;
			g_sw_be_hands[id].buf_num = cfg->num_buf;
			g_sw_be_hands[id].head_id = 0;
			g_sw_be_hands[id].valid_buf_num = g_sw_be_hands[id].buf_num;
			g_sw_be_hands[id].report = report_buf;
			g_sw_be_hands[id].report_size = report_buf_size;
			g_sw_be_hands[id].status =DSP_BUF_STATUS_ALLOCTED;
			memcpy(g_sw_be_hands[id].bufs,cfg->bufs,sizeof(struct csi_dsp_buffer)*cfg->num_buf);
			((dsp_be_handler_t*)out_data)->id = id;
			((dsp_be_handler_t*)out_data)->type = CSI_DSP_BE_TYPE_HOST;
		    ((dsp_be_handler_t*)out_data)->get_next_entry = get_sw_be_next_entry;
		    ((dsp_be_handler_t*)out_data)->get_state = sw_be_get_exeptional;
		    ((dsp_be_handler_t*)out_data)->entry_start = sw_be_entry_start;
		    ((dsp_be_handler_t*)out_data)->update_state =sw_recovery_state_update;

			return XRP_STATUS_SUCCESS;
		}
	}
	return XRP_STATUS_FAILURE;
}

int ps_get_isp_irq(uint16_t id)
{
	return isp_get_irq_num(id);
}

int ps_get_vipre_irq()
{
	return vipre_get_irq_num();
}
bool ps_is_vipre_task_empty()
{
	return vipre_is_empty();
}
