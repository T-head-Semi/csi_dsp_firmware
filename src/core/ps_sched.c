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
#include <xtensa/config/core.h>
#include <xtensa/xtruntime.h>

#if HAVE_THREADS_XOS
#include <xtensa/xos.h>
#endif
#include "dsp_ps_debug.h"
#include "vi_hw_api.h"
#include "xrp_api.h"
#include "xrp_dsp_hw.h"
#include "xrp_dsp_user.h"
#include "dsp_ps_ns.h"
#include "csi_dsp_task_defs.h"
#include "csi_dsp_post_process_defs.h"
#include "ps_sched.h"
#include "algo.h"
#include "test_def.h"
#include "dsp_loader.h"
#include "dmaif.h"
#include "dsp_utils.h"

#ifdef DSP_PROFILE_ENBALE
char profile_buf[128*1024];
DSP_PROFILE_CUSTOM_DECLATR(task)
DSP_PROFILE_CUSTOM_DECLATR(algo)
DSP_PROFILE_CUSTOM_DECLATR(intr)
DSP_PROFILE_CUSTOM_DECLATR(cmd)
DSP_PROFILE_CUSTOM_DECLATR(cmdU)
#endif

#define PS_CRITICAL_SECTION __attribute__((section(".dram0.data")))

s_schduler_t g_scheduler PS_CRITICAL_SECTION;

volatile char g_task_rdy_grp PS_CRITICAL_SECTION;
volatile char g_task_rdy_Tbl[8] PS_CRITICAL_SECTION;

int __stack_size = 0x1400;

char task_ummap[256] PS_CRITICAL_SECTION =
{
		0, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
		4, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
		5, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
		4, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
		6, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
		4, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
		5, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
		4, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
		7, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
		4, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
		5, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
		4, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
		6, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
		4, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
		5, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
		4, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0
};


dsp_task_t  task_table[PS_MAX_TASK_TYPE] PS_CRITICAL_SECTION={
		{0 , CSI_DSP_TASK_HW, DSP_TASK_STATUS_FREE,  {CSI_DSP_FE_TYPE_INVALID,-1,NULL,NULL,NULL,NULL},{CSI_DSP_BE_TYPE_INVALID,-1,NULL,NULL,NULL,NULL},{0,NULL,NULL,NULL},NULL,NULL,{0,NULL,0},NULL,NULL,NULL,NULL},
		{1 , CSI_DSP_TASK_HW, DSP_TASK_STATUS_FREE,  {CSI_DSP_FE_TYPE_INVALID,-1,NULL,NULL,NULL,NULL},{CSI_DSP_BE_TYPE_INVALID,-1,NULL,NULL,NULL,NULL},{0,NULL,NULL,NULL},NULL,NULL,{0,NULL,0},NULL,NULL,NULL,NULL},
		{2 , CSI_DSP_TASK_HW, DSP_TASK_STATUS_FREE,  {CSI_DSP_FE_TYPE_INVALID,-1,NULL,NULL,NULL,NULL},{CSI_DSP_BE_TYPE_INVALID,-1,NULL,NULL,NULL,NULL},{0,NULL,NULL,NULL},NULL,NULL,{0,0,0},NULL,NULL,NULL,NULL},
		{3 , CSI_DSP_TASK_SW, DSP_TASK_STATUS_FREE,  {CSI_DSP_FE_TYPE_INVALID,-1,NULL,NULL,NULL,NULL},{CSI_DSP_BE_TYPE_INVALID,-1,NULL,NULL,NULL,NULL},{0,NULL,NULL,NULL},NULL,XRP_PS_NSID_COMMON_CMD,{0,0,0},NULL,NULL,NULL,NULL},
		{4 , CSI_DSP_TASK_SW, DSP_TASK_STATUS_FREE,  {CSI_DSP_FE_TYPE_INVALID,-1,NULL,NULL,NULL,NULL},{CSI_DSP_BE_TYPE_INVALID,-1,NULL,NULL,NULL,NULL},{0,NULL,NULL,NULL},NULL,NULL,{0,0,0},NULL,NULL,NULL,NULL},
		{5 , CSI_DSP_TASK_SW, DSP_TASK_STATUS_FREE,  {CSI_DSP_FE_TYPE_INVALID,-1,NULL,NULL,NULL,NULL},{CSI_DSP_BE_TYPE_INVALID,-1,NULL,NULL,NULL,NULL},{0,NULL,NULL,NULL},NULL,NULL,{0,0,0},NULL,NULL,NULL,NULL},
		{6 , CSI_DSP_TASK_SW, DSP_TASK_STATUS_FREE,  {CSI_DSP_FE_TYPE_INVALID,-1,NULL,NULL,NULL,NULL},{CSI_DSP_BE_TYPE_INVALID,-1,NULL,NULL,NULL,NULL},{0,NULL,NULL,NULL},NULL,NULL,{0,0,0},NULL,NULL,NULL,NULL},
		{7 , CSI_DSP_TASK_SW, DSP_TASK_STATUS_FREE,  {CSI_DSP_FE_TYPE_INVALID,-1,NULL,NULL,NULL,NULL},{CSI_DSP_BE_TYPE_INVALID,-1,NULL,NULL,NULL,NULL},{0,NULL,NULL,NULL},NULL,NULL,{0,0,0},NULL,NULL,NULL,NULL},
		{8 , CSI_DSP_TASK_SW, DSP_TASK_STATUS_FREE,  {CSI_DSP_FE_TYPE_INVALID,-1,NULL,NULL,NULL,NULL},{CSI_DSP_BE_TYPE_INVALID,-1,NULL,NULL,NULL,NULL},{0,NULL,NULL,NULL},NULL,NULL,{0,0,0},NULL,NULL,NULL,NULL},
		{9 , CSI_DSP_TASK_SW, DSP_TASK_STATUS_FREE,  {CSI_DSP_FE_TYPE_INVALID,-1,NULL,NULL,NULL,NULL},{CSI_DSP_BE_TYPE_INVALID,-1,NULL,NULL,NULL,NULL},{0,NULL,NULL,NULL},NULL,NULL,{0,0,0},NULL,NULL,NULL,NULL},
		{10, CSI_DSP_TASK_SW, DSP_TASK_STATUS_FREE,  {CSI_DSP_FE_TYPE_INVALID,-1,NULL,NULL,NULL,NULL},{CSI_DSP_BE_TYPE_INVALID,-1,NULL,NULL,NULL,NULL},{0,NULL,NULL,NULL},NULL,NULL,{0,0,0},NULL,NULL,NULL,NULL},
		{11, CSI_DSP_TASK_SW, DSP_TASK_STATUS_FREE,  {CSI_DSP_FE_TYPE_INVALID,-1,NULL,NULL,NULL,NULL},{CSI_DSP_BE_TYPE_INVALID,-1,NULL,NULL,NULL,NULL},{0,NULL,NULL,NULL},NULL,NULL,{0,0,0},NULL,NULL,NULL,NULL},
		{12, CSI_DSP_TASK_SW, DSP_TASK_STATUS_FREE,  {CSI_DSP_FE_TYPE_INVALID,-1,NULL,NULL,NULL,NULL},{CSI_DSP_BE_TYPE_INVALID,-1,NULL,NULL,NULL,NULL},{0,NULL,NULL,NULL},NULL,NULL,{0,0,0},NULL,NULL,NULL,NULL},
		{13, CSI_DSP_TASK_SW, DSP_TASK_STATUS_FREE,  {CSI_DSP_FE_TYPE_INVALID,-1,NULL,NULL,NULL,NULL},{CSI_DSP_BE_TYPE_INVALID,-1,NULL,NULL,NULL,NULL},{0,NULL,NULL,NULL},NULL,NULL,{0,0,0},NULL,NULL,NULL,NULL},
		{14, CSI_DSP_TASK_SW, DSP_TASK_STATUS_FREE,  {CSI_DSP_FE_TYPE_INVALID,-1,NULL,NULL,NULL,NULL},{CSI_DSP_BE_TYPE_INVALID,-1,NULL,NULL,NULL,NULL},{0,NULL,NULL,NULL},NULL,NULL,{0,0,0},NULL,NULL,NULL,NULL},
		{15, CSI_DSP_TASK_SW, DSP_TASK_STATUS_FREE,  {CSI_DSP_FE_TYPE_INVALID,-1,NULL,NULL,NULL,NULL},{CSI_DSP_BE_TYPE_INVALID,-1,NULL,NULL,NULL,NULL},{0,NULL,NULL,NULL},NULL,NULL,{0,0,0},NULL,NULL,NULL,NULL}
};
#define   EXTRA_REPORT_SIZE         128
char extra_report[EXTRA_REPORT_SIZE] PS_CRITICAL_SECTION;
static csi_dsp_extra_param_t extra_param PS_CRITICAL_SECTION ={
		.report_buf = (uint32_t)&extra_report,
		.report_buf_size = EXTRA_REPORT_SIZE,
};

static csi_dsp_algo_param_t g_sw_algo_par;

static inline void set_ready_task(uint8_t pri)
{
	uint8_t lsb = pri&0x7;
	uint8_t msb = pri>>3&0x7;

	uint32_t intstate=XTOS_DISABLE_INTERRUPTS();
	g_task_rdy_Tbl[msb]|=0x1<<lsb;
	g_task_rdy_grp |=0x1<<msb;
	XTOS_RESTORE_INTERRUPTS(intstate);
}

static inline void clr_ready_task(uint8_t pri,bool (*is_done)())
{
	uint8_t lsb = pri&0x7;
	uint8_t msb = pri>>3&0x7;
	uint32_t intstate=XTOS_DISABLE_INTERRUPTS();

	if(is_done!=NULL && is_done()!=TTRUE)
	{
		XTOS_RESTORE_INTERRUPTS(intstate);
		return;
	}
	g_task_rdy_Tbl[msb]&=~(0x1<<lsb);
	if(g_task_rdy_Tbl[msb]==0)
	{
		g_task_rdy_grp &=~(0X1<<msb);
	}
	XTOS_RESTORE_INTERRUPTS(intstate);
}

static inline  dsp_task_t* get_ready_task()
{
	uint8_t	msb = task_ummap[g_task_rdy_grp];
	uint8_t lsb= task_ummap[g_task_rdy_Tbl[msb]];
	return &task_table[msb<<3|lsb];
}

static void
ps_task_trigger_intr_handler(void * arg)
{
  int32_t pri = (int32_t)arg;

  DSP_PROFILE_CUSTOM_ID_ADD(intr,pri)
  DSP_PROFILE_CUSTOM_START(intr)

  task_table[pri].intr_handler(task_table[pri].intr_arg);
  set_ready_task(pri);

  DSP_PROFILE_CUSTOM_END(intr)
}
static inline  dsp_task_t* get_free_sw_task()
{
	int task_num= PS_MAX_TASK_TYPE;
	int id;
	for(id=0;id<task_num;id++)
	{
		if(task_table[id].type&CSI_DSP_TASK_SW && task_table[id].status == DSP_TASK_STATUS_FREE)
		{
			return &task_table[id];
		}
	}
	return NULL;
}

static inline  dsp_task_t* get_free_hw_task()
{
	int task_num= PS_MAX_TASK_TYPE;
	int id;
	for(id=0;id<task_num;id++)
	{
		if(task_table[id].type&CSI_DSP_TASK_HW && task_table[id].status == DSP_TASK_STATUS_FREE)
		{
			return &task_table[id];
		}
	}
	return NULL;
}

static inline int  release_task(dsp_task_t* task)
{
	task->status = DSP_TASK_STATUS_FREE;
	return 0;
}

static int allocte_ns_id(int task_id,unsigned char *ns_id)
{
	unsigned char base_id[]=XRP_PS_NSID_COMMON_CMD;
	int i;
	for(i=0;i<XRP_NAMESPACE_ID_SIZE;i++)
	{
		ns_id[i]=base_id[i]+(char)task_id;
	}
	return 0;
}
static int dsp_get_latest_hw_task(uint16_t pri,uint32_t  intnum)
{
	int task_num= PS_MAX_TASK_TYPE;
	int id;
	for(id=0;id<task_num;id++)
	{
		if(pri!=id && task_table[id].type&CSI_DSP_TASK_HW && task_table[id].status == DSP_TASK_STATUS_RUNING)
		{
			if(task_table[id].irq_num == intnum)
			{
				return id;
			}
		}
	}
	return -1;
}

int ps_task_interrupt_register(uint16_t pri,uint32_t intrnum)
{
    int32_t ret = 0;
    void (*old_intr_handler)(void *data);
    uint32_t task_priority = (uint32_t )pri;
    // Normally none of these calls should fail. We don't check them
    // individually, any one failing will cause this function to return failure.
    ret = xtos_set_interrupt_handler(intrnum,
    				ps_task_trigger_intr_handler, (void *)task_priority, &old_intr_handler);
    if(old_intr_handler==NULL)
    {
        ps_err("previous interrupt(%d) hander is NULL\n",intrnum);
        return -1;
    }
    if(old_intr_handler == ps_task_trigger_intr_handler)
    {
    	task_table[task_priority].intr_arg = (void*)dsp_get_latest_hw_task(task_priority,intrnum);
    	if((int)task_table[pri].intr_arg == -1)
    	{
    		ps_err("add task on hw init fail, pri:%d,intr_num:%d\n",task_priority,intrnum);
    	}
    }
    else
    {
    	task_table[task_priority].intr_arg  = (void*)-1;
    }
    task_table[pri].intr_handler = old_intr_handler;
    xtos_interrupt_enable(intrnum);
   // ret += xtos_interrupt_enable(intrnum);
    ps_debug("register interrupt:%d to ps interrupt handler %s\n",intrnum,ret==0?"success":"fail");
    return ret;
}

int ps_task_interrupt_disable(uint16_t pri,uint32_t intrnum)
{
    int32_t ret = 0;
    void (*old_intr_handler)(void *data);
    uint32_t task_priority = pri;
    // Normally none of these calls should fail. We don't check them
    // individually, any one failing will cause this function to return failure.
    ret = xtos_set_interrupt_handler(intrnum,
    				task_table[pri].intr_handler, (void *)task_priority, &old_intr_handler);

	xtos_interrupt_disable(intrnum);
    ps_debug("disable task %d,interrupt\n",task_priority);
    return ret;

}
static inline dsp_task_t * ps_search_task_by_ns(unsigned char* nsid)
{
	int loop;
	for(loop=0;loop<g_scheduler.task_num;loop++)
	{
		if(memcmp(nsid,g_scheduler.task_handler[loop]->nsid,XRP_NAMESPACE_ID_SIZE)==0)
		{
			return g_scheduler.task_handler[loop];
		}
	}
	return NULL;
}
static inline dsp_task_t *dsp_search_task_by_id(int id)
{
	if(id>=PS_MAX_TASK_TYPE)
	{
		return NULL;
	}
	return g_scheduler.task_handler[id];
}

int ps_task_interrupt_register_nsid(unsigned char* nsid)
{
	dsp_task_t * task =ps_search_task_by_ns(nsid);
	if(NULL==task)
	{
		ps_err("Nsid register fail\n");
		return -1;
	}
	return ps_task_interrupt_register(task->prio,task->irq_num);
}

int ps_task_enable_common_task()
{
	unsigned char ns_id[]=XRP_PS_NSID_COMMON_CMD;
	if(ps_task_interrupt_register_nsid(ns_id))
	{
		ps_debug("fail to regster the common int\n");
		return -1;
	}
	xrp_interrupt_on();
	ps_debug("xrp command irq enable\n");
	return 0;
}

static enum task_result ps_idle_task(void* context)
{
	XT_WAITI(0);
	return PS_TASK_SUCCESS;
}

static bool ps_is_idle_task_done(void* context)
{
	return TFALSE;
}

extern int idma_inition();
int init_scheduler(struct xrp_device *device)
{
	int index;
	int common_task_flag =0;
	int task_num= sizeof(task_table)/sizeof(dsp_task_t);
	char common_id[]=XRP_PS_NSID_COMMON_CMD;
	memset((void*)&g_scheduler.task_handler,0x0,sizeof(g_scheduler.task_handler));
	g_scheduler.task_num=0;
	g_scheduler.device=device;
	if(task_num==0|task_num>PS_MAX_TASK_TYPE)
	{
		ps_err("task Initialize fail , task num :(%d) Error\n",task_num);
		return -1;
	}

	for(index=0;index<task_num;index++)
	{
        /*********The first SW task as common cmd task */
		if(common_task_flag==0&& task_table[index].type == CSI_DSP_TASK_SW)
		{
			memcpy(task_table[index].nsid,common_id,XRP_NAMESPACE_ID_SIZE);
			task_table[index].task_pro = ps_process_xrp_task;
			task_table[index].report_info.buf=NULL;
			task_table[index].report_info.buf_size=0;
			task_table[index].report_info.report_id=-1;
			task_table[index].irq_num = xrp_get_irq();
			if(0!=ps_register_cmd_handler(task_table[index].nsid,ps_common_command,NULL))
			{
				ps_err("Common task register fail\n");
				return -1;
			}
			g_scheduler.task_handler[g_scheduler.task_num++]=&task_table[index];
			task_table[index].status=DSP_TASK_STATUS_IDEL;
			common_task_flag = 1;

		}
		else
		{
			//g_scheduler.task_handler[task_table[index].prio] = &task_table[index];
			task_table[index].status=DSP_TASK_STATUS_FREE;
		}
	}
	/**the clear task schdule table***************/
	g_task_rdy_grp=0;
	memset((void*)&g_task_rdy_Tbl,0x0,sizeof(g_task_rdy_Tbl));

	/**the last task reigters as idle task*/
	dsp_task_t *idle_task = &task_table[task_num-1];
	idle_task->task_pro = ps_idle_task;
	idle_task->task_empty_check = ps_is_idle_task_done;
	idle_task->status = DSP_TASK_STATUS_IDEL;
	g_scheduler.task_handler[g_scheduler.task_num++]=idle_task;
	set_ready_task(idle_task->prio);

#if 0
//	idma_inition();
#else
	TMDMA* idma_obj =dmaif_getDMACtrl();
	idma_obj->init();
#endif
	sw_be_init();

#ifdef DSP_PROFILE_ENBALE
	dsp_profile_init(profile_buf,sizeof(profile_buf));
#endif

	return 0;
}

static int dsp_search_higher_prio_task(dsp_task_t *task)
{
	int loop=0;
	dsp_task_t *task_temp=NULL;
	for(loop=0;loop<g_scheduler.task_num;loop++)
	{
		task_temp = g_scheduler.task_handler[loop];
		if(task_temp!=NULL && task_temp!=task)
		{
			if(task_temp->task_fe.type == task->task_fe.type&&
					task_temp->prio<task->prio)
			{
				return task_temp->prio;
			}
		}
	}
	return -1;
}

static int dsp_add_task_to_scheduler(dsp_task_t* task)
{
	int loop=0;
	if(g_scheduler.task_num>=PS_MAX_TASK_TYPE)
	{
		for(loop=0;loop<g_scheduler.task_num;loop++)
		{
			if(g_scheduler.task_handler[loop]==NULL)
			{
				g_scheduler.task_handler[loop]=task;
				return loop;
			}
		}
		return -1;
	}
	g_scheduler.task_handler[g_scheduler.task_num]=task;
	return g_scheduler.task_num++;
}

static void dsp_remove_task_from_scheduler(int task_id)
{
	if(g_scheduler.task_num == (task_id+1))
	{
		g_scheduler.task_num--;
	}

	g_scheduler.task_handler[task_id]=NULL;
}
enum xrp_status ps_register_report_to_task(void *context)
{
	struct report_config_msg *msg= (struct report_config_msg *)context;
//	dsp_task_t * task =ps_search_task_by_ns(msg->task);
	dsp_task_t * task  = dsp_search_task_by_id(msg->report_id);  //report_id equal to task -id;
	if(NULL==task)
	{
		ps_err("Nsid register fail\n");
		return XRP_STATUS_FAILURE;
	}
	if(msg->report_id <0)
	{
		ps_err("report_id %d invalid\n",msg->report_id);
		return XRP_STATUS_FAILURE;
	}
	switch(msg->flag)
	{
		case CMD_SETUP:
			if(XRP_STATUS_SUCCESS
					!=xrp_get_report_buffer(g_scheduler.device,&task->report_info.buf,&task->report_info.buf_size))
			{
				ps_err("report get xrp buffer fail\n");
				return XRP_STATUS_FAILURE;
			}
			if(task->report_info.buf_size < msg->size)
			{
				ps_err("report %d request size :%d, exceed the limit:%d",msg->report_id,msg->size,task->report_info.buf_size);
				return XRP_STATUS_FAILURE;
			}
			task->report_info.buf_size = msg->size;
			task->report_info.report_id = msg->report_id;
			break;
		case CMD_RELEASE:
			task->report_info.report_id=-1;
			break;
		default:
			return XRP_STATUS_FAILURE;
	}

	ps_debug("task %d, register report:%d,report_buf:%x\n",task->nsid[0],task->report_info.report_id,task->report_info.buf);
	return XRP_STATUS_SUCCESS;

}


enum xrp_status ps_run_data_move_cmd(void *context)
{
	struct data_move_msg *msg= (struct data_move_msg *)context;
	ps_debug("src :%llx,dst:%llx,size:%d\n",msg->src_addr, msg->dst_addr, msg->size);
//	if(idma_copy_ext2ext(msg->src_addr, msg->dst_addr, msg->size,
//			image_in_char_tdma0,51200))
//	{
//		return XRP_STATUS_FAILURE;
//	}
	ps_debug("success\n");
	return XRP_STATUS_SUCCESS;
}

//enum xrp_status dsp_dummy_algo(csi_dsp_algo_param_t* param)
//{
//	if(!param || param->buffer_num <2 )
//	{
//		return XRP_STATUS_FAILURE;
//	}
//	struct csi_dsp_buffer *bufs;
//	if(idma_copy_ext2ext(bufs[0].planes[0].buf_phy, bufs[1].planes[0].buf_phy,bufs[0].planes[0].size,
//			image_in_char_tdma0,61440))
//	{
//		return XRP_STATUS_FAILURE;
//	}
//	return XRP_STATUS_SUCCESS;
//
//}
static inline void fill_hw_task_algo_param(csi_dsp_algo_param_t* param,entry_struct_t* in_info,
											struct csi_dsp_buffer  * out_info)
{
	struct csi_dsp_buffer *bufs;
	if(!param||!in_info||!out_info)
		return;
	if(param->buffer_num <2)
	{
		return;
	}
	bufs =(struct csi_dsp_buffer *) param->buffer_struct_ptr;
	bufs[0].height= in_info->height;
	bufs[0].width = in_info->width;
	bufs[0].plane_count =1;
	bufs[0].planes[0].buf_phy=in_info->pBuff;
	bufs[0].planes[0].stride = in_info->pitch;
	bufs[0].planes[0].size = in_info->height*in_info->pitch;
	if(in_info->height == in_info->frame_height)
	{
		param->extra_param_ptr->start_line =0;
		param->extra_param_ptr->end_line =0;
		param->extra_param_ptr->total_line =0;
	}
	else
	{
		param->extra_param_ptr->start_line = in_info->y;
		param->extra_param_ptr->end_line = in_info->y+in_info->height;
		param->extra_param_ptr->total_line = in_info->frame_height;
	}
//	if(in_info->meta)
	{
		param->extra_param_ptr->frame_meta.temp_projector = in_info->meta.temp_projector;
		param->extra_param_ptr->frame_meta.temp_sensor = in_info->meta.temp_sensor;
		param->extra_param_ptr->frame_meta.frame_id = in_info->meta.frame_id;
		param->extra_param_ptr->frame_meta.timestap = in_info->meta.timestap;
	}

	bufs[1].height= out_info->height;
	bufs[1].width = out_info->width;
	bufs[1].format = out_info->format;
	bufs[1].plane_count =out_info->plane_count;
	memcpy(bufs[1].planes,out_info->planes,sizeof(struct csi_dsp_plane)*out_info->plane_count);

}

static inline int fill_sw_task_algo_param(csi_dsp_algo_param_t* param,const void *in_data,
								struct xrp_buffer_group *buffer_group,entry_struct_t * out_info)
{
	struct csi_sw_task_req *req = (struct csi_sw_task_req *)in_data;
	int buf_idx;
	struct csi_dsp_buffer *buf;
	if(!param || !in_data)
	{
		return CSI_DSP_ERR_ILLEGAL_PARAM;
	}
	param->buffer_struct_ptr = &req->buffers[0];
	param->buffer_num = req->buffer_num;
	param->set_buf_ptr = (void*)req->sett_ptr;
	param->set_buf_size = req->sett_length;
	param->extra_param_ptr = NULL;
	if(out_info)
	{
		buf_idx = req->buffer_num-1;
		buf = &req->buffers[buf_idx];
		if(buf->type != CSI_DSP_BUF_ALLOC_FM)
		{
			return CSI_DSP_BUF_TYPE_ERR;
		}
		buf->format = out_info->fmt;
		buf->height = out_info->height;
		buf->width = out_info->width;
		buf->plane_count = 1;
		buf->planes[0].buf_phy = out_info->pBuff;
		buf->planes[0].stride = out_info->pitch;
	}

	return CSI_DSP_OK;

}

static inline enum xrp_status  dsp_process_sw_task(void *context,
											const void *in_data, size_t in_data_size,
											void *out_data, size_t out_data_size,
											struct xrp_buffer_group *buffer_group)
{
	dsp_task_t *task = (dsp_task_t *)context;
	entry_struct_t ctrl_info;
//	csi_dsp_algo_param_t param;
//
//	task->task_algo.param =&param;
	if(task->type == CSI_DSP_TASK_SW_TO_HW)
	{
		task->task_be.get_next_entry(task->task_be.id,&ctrl_info);
		fill_sw_task_algo_param(task->task_algo.param,in_data,buffer_group,&ctrl_info);

	}
	else
	{
		fill_sw_task_algo_param(task->task_algo.param,in_data,buffer_group,NULL);
	}

	if(!task->task_algo.algo_process)
	{
		*((csi_dsp_status_e *)out_data) = CSI_DSP_ALGO_INVALID;
		return XRP_STATUS_SUCCESS;
	}

	DSP_PROFILE_CUSTOM_START(algo)
	if(XRP_STATUS_SUCCESS !=task->task_algo.algo_process(task->task_algo.param))
	{
		*((csi_dsp_status_e *)out_data) = CSI_DSP_ALGO_ERR;
		return XRP_STATUS_SUCCESS;
	}
	DSP_PROFILE_CUSTOM_END(algo)

	if(task->type == CSI_DSP_TASK_SW_TO_HW)
	{
		if(task->task_be.get_state(task->task_be.id)!= 0)
		{
			*((csi_dsp_status_e *)out_data) = CSI_DSP_ALGO_ERR;
		}
		task->task_be.entry_start(task->task_be.id,&ctrl_info);
	}
	return XRP_STATUS_SUCCESS;
}

static enum task_result  dsp_process_hw_task(void* context)
{
	dsp_task_t *task = (dsp_task_t *)context;
	entry_struct_t in_info;
	struct csi_dsp_buffer out_info;
	task->task_algo.param->extra_param_ptr = &extra_param;
	task->task_algo.param->extra_param_ptr->report_size = 0;
	if(task->task_fe.get_next_entry(task->task_fe.id, &in_info))
	{
		if(task->task_fe.get_state(task->task_fe.id)==EHW_ERROR_OVER_FLOW)
		{
			task->task_fe.update_state(task->task_fe.id);
			task->task_be.update_state(task->task_be.id);
		}
		if(task->task_fe.get_state(task->task_fe.id)==EHW_RECOVERYING && task->task_be.get_state(task->task_be.id)==EHW_RECOVERY_DONE)
		{
			task->task_fe.update_state(task->task_fe.id);
			task->task_be.update_state(task->task_be.id);
			if(task->report_info.buf)
			{
				ps_fill_report(task->report_info.buf,CSI_DSP_HW_FRAME_DROP,NULL,0);
			}
			return PS_TASK_FAILURE_WITH_REPORT;
		}
		return PS_TASK_FAILURE;
	}
#if 0
	static uint8_t side=0;
	/*********************/
	if(in_info.y ==0)
	{
		side ^=1;
	}
	if(side == 0 && in_info.y >120)
	{
		return PS_TASK_SUCCESS;
	}
#endif
	if(task->task_be.get_next_entry(task->task_be.id,&out_info))
	{
		if(task->type == CSI_DSP_TASK_HW_TO_SW)
		{
			/**for back end issue such as no variable buffer drop the coming entry **/
			task->task_fe.entry_done(task->task_fe.id,&in_info);
			ps_wrn("be get entry fail\n");
		}
		return PS_TASK_FAILURE;
	}

/***************************to pack the algo param******************************/
	fill_hw_task_algo_param(task->task_algo.param,&in_info,&out_info);

	if(task->task_algo.algo_process == NULL)
	{
		return PS_TASK_FAILURE;
	}
	DSP_PROFILE_CUSTOM_START(algo)
	if(XRP_STATUS_FAILURE ==task->task_algo.algo_process(task->task_algo.param))
	{
//		*((csi_dsp_status_e*)out_data) = CSI_DSP_ALGO_ERR;
		ps_wrn("Algo run fail\n");
		return PS_TASK_FAILURE;
	}
	DSP_PROFILE_CUSTOM_END(algo)
	task->task_fe.entry_done(task->task_fe.id,&in_info);
	if(task->task_fe.get_state(task->task_fe.id)!= 0)
	{
		ps_wrn("Fe Error\n");
		return PS_TASK_FAILURE;
	}

	if(task->task_be.entry_start(task->task_be.id,&out_info))
	{

		if(task->task_be.get_state(task->task_be.id) < 0)
		{
			return PS_TASK_FAILURE_WITH_REPORT;
		}
		ps_wrn("Be trigger fail\n");
		return PS_TASK_FAILURE;
	}

	if(task->type == CSI_DSP_TASK_HW_TO_SW)
	{

		csi_dsp_result_report(task->report_info.buf,(void*)&out_info,sizeof(out_info),(void*)extra_param.report_buf,extra_param.report_size);
		return PS_TASK_SUCCESS_WITH_REPORT;
	}

	return PS_TASK_SUCCESS;
}

static enum xrp_status ps_create_task(void *req,void * resp)
{
	struct csi_dsp_task_create_req *msg= (struct csi_dsp_task_create_req *)req;
	struct csi_dsp_task_create_resp *reps_msg= (struct csi_dsp_task_create_resp *)resp;
	dsp_task_t* new_task;
	int task_id =-1;
	enum xrp_status status;
	ps_debug("Task type %d to create\n",msg->type);
	switch(msg->type)
	{
		case CSI_DSP_TASK_SW:
		case CSI_DSP_TASK_SW_TO_SW:
		case CSI_DSP_TASK_SW_TO_HW:
			new_task = get_free_sw_task();
			if(!new_task)
			{
				reps_msg->status = CSI_DSP_TASK_ALLOC_FAIL;
				return XRP_STATUS_SUCCESS;
			}
			allocte_ns_id(new_task->prio,new_task->nsid);
			xrp_device_register_namespace(g_scheduler.device,new_task->nsid,dsp_process_sw_task, new_task, &status);
			if(status!=XRP_STATUS_SUCCESS)
			{
				reps_msg->status = CSI_DSP_TASK_ALLOC_FAIL;
				return XRP_STATUS_SUCCESS;
			}

			break;

		case CSI_DSP_TASK_HW:
		case CSI_DSP_TASK_HW_TO_SW:
		case CSI_DSP_TASK_HW_TO_HW:
			new_task = get_free_hw_task();
			if(!new_task)
			{
				reps_msg->status = CSI_DSP_TASK_ALLOC_FAIL;
				return XRP_STATUS_SUCCESS;
			}
			allocte_ns_id(new_task->prio,new_task->nsid);
			new_task->task_pro = dsp_process_hw_task;

			break;
		default:
			reps_msg->status =CSI_DSP_ERR_ILLEGAL_PARAM;
			return XRP_STATUS_SUCCESS;
	}

	if(msg->type == CSI_DSP_TASK_SW_TO_SW)
	{
		new_task->status = DSP_TASK_STATUS_READY;
	}
	else
	{
		new_task->status = DSP_TASK_STATUS_IDEL;
	}
	new_task->type = msg->type;
	new_task->report_info.buf=NULL;
	new_task->report_info.buf_size=0;
	new_task->report_info.report_id=-1;
	new_task->task_fe.type= CSI_DSP_FE_TYPE_INVALID;
	new_task->task_be.type= CSI_DSP_BE_TYPE_INVALID;
	new_task->task_algo.algo_id=-1;
	new_task->task_algo.param = NULL;
//	g_scheduler.task_handler[task_id]=new_task;
	task_id = dsp_add_task_to_scheduler(new_task);
	if(task_id <0)
	{
		reps_msg->status = CSI_DSP_TASK_ADD_TO_SCHEDULER_FAIL;
		release_task(new_task);
		return XRP_STATUS_SUCCESS;
	}
	reps_msg->status = CSI_DSP_OK;
	reps_msg->task_id = task_id;
//	g_scheduler.task_num++;
	memcpy(reps_msg->task_ns,new_task->nsid,XRP_NAMESPACE_ID_SIZE);
	return XRP_STATUS_SUCCESS;
}


static enum xrp_status ps_destroy_task(void *req,void * resp)
{
	struct csi_dsp_task_free_req *msg = (struct csi_dsp_task_free_req*)req;
	struct csi_dsp_task_comm_resp *reps_msg= (struct csi_dsp_task_comm_resp *)resp;
	reps_msg->status = CSI_DSP_OK;
	enum xrp_status status;
	dsp_task_t * task =dsp_search_task_by_id(msg->task_id);
	if(!task)
	{
		reps_msg->status = CSI_DSP_TASK_NOT_VALID;
		return XRP_STATUS_SUCCESS;
	}
	dsp_remove_task_from_scheduler(msg->task_id);
	if(task->task_algo.param)
	{
		if(CSI_DSP_TASK_HW & task->type)
		{
			if(task->task_algo.param->buffer_struct_ptr)
				free(task->task_algo.param->buffer_struct_ptr);
			if(task->task_algo.param->set_buf_ptr)
				free(task->task_algo.param->set_buf_ptr);
		}
		free(task->task_algo.param);
	}
	task->task_algo.param = NULL;
	task->task_algo.algo_id =-1;
	task->task_algo.algo_process = NULL;
	if(CSI_DSP_TASK_SW & task->type)
	{
		xrp_device_unregister_namespace(g_scheduler.device,task->nsid,&status);
	}
	release_task(task);
	reps_msg->status = CSI_DSP_OK;
	return XRP_STATUS_SUCCESS;
}

static enum xrp_status dsp_task_start(void *req,void * resp){
	struct csi_dsp_task_start_req *msg= (struct csi_dsp_task_start_req *)req;
	struct csi_dsp_task_comm_resp *reps_msg= (struct csi_dsp_task_comm_resp *)resp;

	ps_debug("task %d to start\n",msg->task_id);
	dsp_task_t *task = dsp_search_task_by_id(msg->task_id);

	if(task == NULL)
	{
		reps_msg->status = CSI_DSP_ERR_ILLEGAL_PARAM;
		return XRP_STATUS_SUCCESS;
	}

	if(task->status == DSP_TASK_STATUS_RUNING)
	{
		reps_msg->status = CSI_DSP_TASK_ALREADY_RUNNING;
		return XRP_STATUS_SUCCESS;
	}
	else if(task->status ==DSP_TASK_STATUS_READY)
	{
		/*TODO enable task.. enable the HW INIT */
		if(0!=ps_task_interrupt_register(task->prio,task->irq_num))
		{
			return XRP_STATUS_FAILURE;
		}
		task->status = DSP_TASK_STATUS_RUNING;
		reps_msg->status = CSI_DSP_OK;
		return XRP_STATUS_SUCCESS;
	}
	else
	{
		reps_msg->status = CSI_DSP_TASK_START_FAIL;
		return XRP_STATUS_SUCCESS;
	}

}

static enum xrp_status dsp_task_stop(void *req,void * resp){
	struct csi_dsp_task_start_req *msg= (struct csi_dsp_task_start_req *)req;
	struct csi_dsp_task_comm_resp *reps_msg= (struct csi_dsp_task_comm_resp *)resp;

	ps_debug("task %d to stop\n",msg->task_id);
	dsp_task_t *task = dsp_search_task_by_id(msg->task_id);
	reps_msg->status = CSI_DSP_OK;
	if(task == NULL)
	{
		reps_msg->status = CSI_DSP_ERR_ILLEGAL_PARAM;
		return XRP_STATUS_SUCCESS;
	}

	if(task->status == DSP_TASK_STATUS_RUNING)
	{
		ps_task_interrupt_disable(task->prio,task->irq_num);
		task->status = DSP_TASK_STATUS_READY;
		return XRP_STATUS_SUCCESS;
	}
	else
	{
		return XRP_STATUS_SUCCESS;
	}

}

static inline void update_task_status(dsp_task_t * task )
{
	switch(task->type)
	{
		case CSI_DSP_TASK_SW_TO_HW:
			 if((task->status == DSP_TASK_STATUS_IDEL) &&
					 (task->task_be.type!=CSI_DSP_BE_TYPE_INVALID) &&
					 (task->task_algo.algo_process!=NULL))
			 {
				 task->status= DSP_TASK_STATUS_READY;
			 }
			 break;
		case CSI_DSP_TASK_HW_TO_SW:
			 if((task->status == DSP_TASK_STATUS_IDEL) &&
					 (task->task_fe.type!=CSI_DSP_FE_TYPE_INVALID) &&
					 (task->task_be.type!=CSI_DSP_BE_TYPE_INVALID)&&
					 (task->task_algo.algo_process!=NULL))
			 {
				 task->status= DSP_TASK_STATUS_READY;
			 }
			 break;
		case CSI_DSP_TASK_HW_TO_HW:
			 if((task->status == DSP_TASK_STATUS_IDEL) &&
					 (task->task_fe.type!=CSI_DSP_FE_TYPE_INVALID) &&
					 (task->task_be.type!=CSI_DSP_BE_TYPE_INVALID)&&
					 (task->task_algo.algo_process!=NULL))
			 {
				 task->status= DSP_TASK_STATUS_READY;
			 }
			 break;
		default:
			break;
	}
}

int high_pri_task =-1;
static bool dsp_vipre_task_check()
{
	if( ps_is_vipre_task_empty()== true || high_pri_task<0 )
	{
		return true;
	}
	set_ready_task(high_pri_task);
	return false;

}

static enum xrp_status dsp_task_fe_config(void *req,void * resp){
	struct csi_dsp_task_fe_para *msg= (struct csi_dsp_task_fe_para *)req;
	struct csi_dsp_task_comm_resp *reps_msg= (struct csi_dsp_task_comm_resp *)resp;
	reps_msg->status = CSI_DSP_OK;
	dsp_task_t * task =dsp_search_task_by_id(msg->task_id);

	if(!task)
	{
		reps_msg->status = CSI_DSP_TASK_NOT_VALID;
		return XRP_STATUS_SUCCESS;
	}
	ps_debug("FE %d to config\n",msg->frontend_type);
	switch(msg->frontend_type)
	{
		case CSI_DSP_FE_TYPE_CPU:
				break;
		case CSI_DSP_FE_TYPE_ISP:
				if(XRP_STATUS_SUCCESS!=ps_isp_config_message_handler((void*)&msg->isp_param,(void*)&task->task_fe))
				{
					reps_msg->status = CSI_DSP_FE_CONFIG_FAIL;
					return XRP_STATUS_SUCCESS;
				}

				task->irq_num = ps_get_isp_irq(msg->isp_param.id);

				break;
		case CSI_DSP_FE_TYPE_VIPRE:
			if(XRP_STATUS_SUCCESS!=ps_vipre_config_message_handler((void*)&msg->isp_param,(void*)&task->task_fe))
			{
				reps_msg->status = CSI_DSP_FE_CONFIG_FAIL;
				return XRP_STATUS_SUCCESS;
			}
			task->irq_num = ps_get_vipre_irq();
			high_pri_task = dsp_search_higher_prio_task(task);
			if(high_pri_task >=0)
			{
				task->task_empty_check = dsp_vipre_task_check;
			}
			else
			{
				task->task_empty_check = NULL;
			}

			break;
		default:
			reps_msg->status = CSI_DSP_ERR_ILLEGAL_PARAM;
			return XRP_STATUS_SUCCESS;
	}
	update_task_status(task);
	return XRP_STATUS_SUCCESS;

}

static enum xrp_status dsp_task_be_config(void *req,void * resp){
	struct csi_dsp_task_be_para *msg= (struct csi_dsp_task_be_para *)req;
	struct csi_dsp_task_comm_resp *reps_msg= (struct csi_dsp_task_comm_resp *)resp;

	dsp_task_t * task =dsp_search_task_by_id(msg->task_id);
	reps_msg->status = CSI_DSP_OK;

	if(!task)
	{
		reps_msg->status = CSI_DSP_TASK_NOT_VALID;
		return XRP_STATUS_SUCCESS;
	}
	ps_debug("Task be %d to config\n",msg->backend_type);
	switch(msg->backend_type)
	{
		case CSI_DSP_BE_TYPE_HOST:
			if(XRP_STATUS_SUCCESS!=  ps_sw_task_out_config_message_handler(&msg->sw_param,&task->task_be,task->report_info.buf,task->report_info.buf_size))
			{
				reps_msg->status = CSI_DSP_BE_CONFIG_FAIL;
				return XRP_STATUS_SUCCESS;
			}
			break;

		case CSI_DSP_BE_TYPE_POST_ISP:
			if(XRP_STATUS_SUCCESS!= ps_post_isp_config_message_handler(&msg->post_isp_param,&task->task_be))
			{
				reps_msg->status = CSI_DSP_BE_CONFIG_FAIL;
				return XRP_STATUS_SUCCESS;
			}
			break;
		default:
			reps_msg->status = CSI_DSP_ERR_ILLEGAL_PARAM;
			return XRP_STATUS_SUCCESS;
	}
	update_task_status(task);
	return XRP_STATUS_SUCCESS;
}
static inline int dsp_task_algo_setting_update(csi_dsp_algo_param_t *algo_param ,struct csi_dsp_algo_config_par *new_param)
{

	if((new_param->sett_ptr==NULL) ||(new_param->sett_length==0))
	{
		return 0;
	}
	if(algo_param->set_buf_ptr ==NULL || algo_param->set_buf_size!=new_param->sett_length)
	{
		ps_err("update setting invalid dst(0x%x,%d),src(0x%x,%d)\n",\
						algo_param->set_buf_ptr,algo_param->set_buf_size,new_param->sett_ptr,new_param->sett_length);
		return -1;
	}
	memcpy(algo_param->set_buf_ptr,(void*)new_param->sett_ptr,algo_param->set_buf_size);
	return 0;
}

static inline int dsp_task_algo_buf_update(csi_dsp_algo_param_t *algo_param ,struct csi_dsp_algo_config_par *new_param)
{
	int index,loop;
	struct csi_dsp_buffer *old_bufs;
	struct csi_dsp_buffer *new_bufs;
	if((new_param->bufs_ptr==NULL) ||(new_param->buf_num==0))
	{
		return 0;
	}
	if(new_param->buf_num>(algo_param->buffer_num-CSI_DSP_HW_TASK_EXTRA_BUF_START_INDEX))
	{
		return -1;
	}
	old_bufs = (struct csi_dsp_buffer *)algo_param->buffer_struct_ptr;
	new_bufs = (struct csi_dsp_buffer *)new_param->bufs_ptr;
	for(loop=0;loop<new_param->buf_num;loop++)
	{
		index = new_bufs[loop].buf_id;
		if(old_bufs[index].buf_id!=index)
		{
			return -1;
		}
		memcpy(&old_bufs[index],&new_bufs[loop],sizeof(struct csi_dsp_buffer));
	}
	ps_debug("exit");
	return 0;

}
static enum xrp_status dsp_task_algo_config(void *req,void * resp)
{
	struct csi_dsp_algo_config_par *msg= (struct csi_dsp_algo_config_par *)req;
	struct csi_dsp_task_comm_resp *resp_msg= (struct csi_dsp_task_comm_resp *)resp;

	dsp_task_t * task =dsp_search_task_by_id(msg->task_id);
	resp_msg->status = CSI_DSP_OK;
	struct csi_dsp_buffer *bufs;
	if(!task)
	{
		resp_msg->status = CSI_DSP_TASK_NOT_VALID;
		return XRP_STATUS_SUCCESS;
	}

	if(task->task_algo.param==NULL)
	{
		task->task_algo.param = malloc(sizeof(csi_dsp_algo_param_t));
		if(!task->task_algo.param )
		{
			resp_msg->status = CSI_DSP_MALLO_FAIL;
			return XRP_STATUS_SUCCESS;
		}
		memset(task->task_algo.param,0x0,sizeof(csi_dsp_algo_param_t));
	}
	else
	{
		/*for algo already load, this mean update algo param and buffer*/
		if((task->task_algo.algo_process!=NULL) && (msg->algo_ptr == NULL) && (msg->algo_id == -1))
		{
			if(dsp_task_algo_setting_update(task->task_algo.param,msg))
			{
				ps_err("Algo setting update fail\n");
				resp_msg->status = CSI_DSP_FAIL;
			}
			if(dsp_task_algo_buf_update(task->task_algo.param,msg))
			{
				ps_err("Algo buf update fail\n");
				resp_msg->status = CSI_DSP_FAIL;
			}
			return XRP_STATUS_SUCCESS;
		}
	}

	task->task_algo.algo_process=dsp_algo_load(msg->algo_id,msg->algo_ptr,task->task_algo.param);
	task->task_algo.algo_id=msg->algo_id;


	if(!task->task_algo.algo_process)
	{
		free(task->task_algo.param);
		task->task_algo.param = NULL;
		resp_msg->status = CSI_DSP_ALGO_INVALID;
		ps_err("Algo load fail\n");
		return XRP_STATUS_SUCCESS;
	}

//	/*TODO*/
//	task->task_algo.param->buf_desc_num =2;

	if(task->type&CSI_DSP_TASK_HW)
	{
		if(task->task_algo.param->buf_desc_num)
		{
			task->task_algo.param->buffer_num=task->task_algo.param->buf_desc_num;
			task->task_algo.param->buffer_struct_ptr=malloc(task->task_algo.param->buf_desc_num*sizeof(struct csi_dsp_buffer));
			if(!task->task_algo.param->buffer_struct_ptr)
			{
				free(task->task_algo.param);
				task->task_algo.param = NULL;
				resp_msg->status = CSI_DSP_MALLO_FAIL;
				return XRP_STATUS_SUCCESS;
			}
			memset(task->task_algo.param->buffer_struct_ptr,0x0,task->task_algo.param->buf_desc_num*sizeof(struct csi_dsp_buffer));
			if(msg->sett_ptr!=NULL && msg->sett_length !=0)
			{
				task->task_algo.param->set_buf_size = msg->sett_length;
				task->task_algo.param->set_buf_ptr = malloc(task->task_algo.param->set_buf_size);
				if(!task->task_algo.param->set_buf_ptr)
				{
					free(task->task_algo.param->buffer_struct_ptr);
					free(task->task_algo.param);
					task->task_algo.param = NULL;
					resp_msg->status = CSI_DSP_MALLO_FAIL;
					return XRP_STATUS_SUCCESS;
				}
				memcpy(task->task_algo.param->set_buf_ptr,(void*)msg->sett_ptr,task->task_algo.param->set_buf_size);
			}
			if(msg->bufs_ptr!=NULL && msg->buf_num != 0)
			{
				if((msg->buf_num+CSI_DSP_HW_TASK_EXTRA_BUF_START_INDEX)<=task->task_algo.param->buffer_num)
				{
					bufs = (struct csi_dsp_buffer *)task->task_algo.param->buffer_struct_ptr;
					memcpy(&bufs[CSI_DSP_HW_TASK_EXTRA_BUF_START_INDEX],msg->bufs_ptr,msg->buf_num*(sizeof(struct csi_dsp_buffer)));
				}
				else
				{
					free(task->task_algo.param->buffer_struct_ptr);
					free(task->task_algo.param->set_buf_ptr);
					free(task->task_algo.param);
					task->task_algo.param = NULL;
					resp_msg->status = CSI_DSP_ALGO_BUF_FAIL;
					ps_err("set buf_num:%d,algo buf_num:%d not match fail\n",msg->buf_num,task->task_algo.param->buffer_num);
					return XRP_STATUS_SUCCESS;
				}
			}
		}
		else
		{
			resp_msg->status = CSI_DSP_ALGO_BUF_FAIL;
			ps_err("algo buffer not defined\n");
			return XRP_STATUS_SUCCESS;
		}
	}

//	task->task_algo.param->buffer_num = msg->buf_num;
//	task->task_algo.param->buffer_struct_ptr =	msg->bufs_ptr;
//	task->task_algo.param->set_buf_ptr = msg->sett_ptr;
//	task->task_algo.param->set_buf_size = msg->sett_length;
	task->task_algo.param->get_idma_ctrl = dmaif_getDMACtrl;
	task->task_algo.param->printf_func = printf;

	update_task_status(task);
	return XRP_STATUS_SUCCESS;
}

static enum xrp_status csi_dsp_task_algo_load(void *in,void * out)
{
	csi_dsp_algo_load_req_t *req = (csi_dsp_algo_load_req_t *)in;
	csi_dsp_algo_load_resp_t *resp = (csi_dsp_algo_load_resp_t *)out;
	dsp_task_t * task =dsp_search_task_by_id(req->task_id);
	resp->status = CSI_DSP_OK;
	if(!task)
	{
		resp->status = CSI_DSP_TASK_NOT_VALID;
		return XRP_STATUS_SUCCESS;
	}
	task->task_algo.param = malloc(sizeof(csi_dsp_algo_param_t));
	if(!task->task_algo.param )
	{
		resp->status = CSI_DSP_MALLO_FAIL;
		return XRP_STATUS_SUCCESS;
	}
	memset(task->task_algo.param,0x0,sizeof(csi_dsp_algo_param_t));
	task->task_algo.algo_process = dsp_algo_load(req->algo_id,req->algo_ptr,task->task_algo.param);
	if(!task->task_algo.algo_process)
	{
		free(task->task_algo.param);
		resp->status = CSI_DSP_ALGO_LOAD_FAIL;
		return XRP_STATUS_SUCCESS;
	}
	task->task_algo.algo_id = req->algo_id;
	if(task->type&CSI_DSP_TASK_HW )
	{
		if(task->task_algo.param->buf_desc_num)
		{
			task->task_algo.param->buffer_num=task->task_algo.param->buf_desc_num;
			task->task_algo.param->buffer_struct_ptr=malloc(task->task_algo.param->buf_desc_num*sizeof(struct csi_dsp_buffer));
			if(!task->task_algo.param->buffer_struct_ptr)
			{
				free(task->task_algo.param);
				resp->status = CSI_DSP_ALGO_LOAD_FAIL;
				return XRP_STATUS_SUCCESS;
			}
		}
	}
	task->task_algo.param->get_idma_ctrl = dmaif_getDMACtrl;
	task->task_algo.param->printf_func = printf;

	return XRP_STATUS_SUCCESS;
}

static enum xrp_status dsp_ip_test_handler(void *req,void * resp)
{
	struct csi_dsp_ip_test_par *msg = (struct csi_dsp_ip_test_par *)req;
	char argv0[8];
	char argv1[8];
	char argv2[8];
	char *strargv[4];
	strargv[0]=argv0;
	strargv[1]=argv1;
	strargv[2]=argv2;
	strargv[3]=msg->case_goup;
	sprintf(strargv[1], "-v");
	sprintf(strargv[2], "-g");
	run_case(resp,msg->result_buf_size,3, strargv);
	return XRP_STATUS_SUCCESS;
}
static enum xrp_status dsp_backend_assgin_buf_handler(void *req,void * resp)
{
	struct csi_dsp_task_be_para *msg= (struct csi_dsp_task_be_para *)req;
	struct csi_dsp_task_comm_resp *reps_msg= (struct csi_dsp_task_comm_resp *)resp;
    DSP_PROFILE_CUSTOM_START(cmdU)
	dsp_task_t * task =dsp_search_task_by_id(msg->task_id);
	reps_msg->status = CSI_DSP_OK;

	if(!task)
	{
		reps_msg->status = CSI_DSP_TASK_NOT_VALID;
		return XRP_STATUS_SUCCESS;
	}

	if(XRP_STATUS_SUCCESS != sw_be_asgin_buf(task->task_be.id,&msg->sw_param))
	{
		reps_msg->status = CSI_DSP_BE_ERR;

	}
	DSP_PROFILE_CUSTOM_END(cmdU)
	return XRP_STATUS_SUCCESS;
}

enum xrp_status ps_common_command(void *context,
                                   const void *in_data, size_t in_data_size,
                                   void *out_data, size_t out_data_size,
                                   struct xrp_buffer_group *buffer_group)
{
	  (void)out_data_size;
	  (void)context;
	  size_t sz;
	  uint32_t cmd = *(const uint32_t *)in_data;
	  void * data =(void *)(in_data+4);
	  enum xrp_status status = XRP_STATUS_SUCCESS;

	  ps_debug("CMD %d received\n",cmd);
	  switch (cmd) {
	  	  case  PS_CMD_LOOPBACK_TEST:
	  		     sz = in_data_size> out_data_size?in_data_size:out_data_size;
	  		     memcpy(out_data,in_data+4,sz-4);
	  		     return XRP_STATUS_SUCCESS;
	  	  case PS_CMD_REPORT_CONFIG:
	  		  	 return ps_register_report_to_task(data);
	  	  case PS_CMD_DATA_MOVE:
	  		  	 return ps_run_data_move_cmd(data);
	  	  case PS_CMD_TASK_ALLOC:
	  		  	  return ps_create_task(data,out_data);
	  	  case PS_CMD_TASK_FREE:
	  		  	   return ps_destroy_task(data,out_data);
	  	  case PS_CMD_TASK_START:
	  		  	  return dsp_task_start(data,out_data);
	  	  case PS_CMD_TASK_STOP:
	  	  	  	  return dsp_task_stop(data,out_data);
	  	  case PS_CMD_FE_CONFIG:
	  		  	  return dsp_task_fe_config(data,out_data);
	  	  case PS_CMD_BE_CONFIG:
	  		  	  return dsp_task_be_config(data,out_data);
	  	  case PS_CMD_ALGO_CONFIG:
	  		  	  return dsp_task_algo_config(data,out_data);
	  	  case PS_CMD_DSP_IP_TEST:
	  		  	  return dsp_ip_test_handler(data,out_data);
	  	  case PS_CMD_BE_ASSGIN_BUF:
	  		  	  return dsp_backend_assgin_buf_handler(data,out_data);
	  	  case PS_CMD_ALGO_LOAD:
	  		  	  return csi_dsp_task_algo_load(data,out_data);
	  	  case PS_CMD_HEART_BEAT_REQ:
	  		  	  return XRP_STATUS_SUCCESS;
	  	  default:
	  		  	 return XRP_STATUS_FAILURE;

	  }
}

int ps_task_shedule(struct xrp_device *device)
{
	dsp_task_t* task;
	void* report_buf=NULL;
	int  size;
	enum task_result status;
	for(;;)
	{
		if(!g_task_rdy_grp)
		{
			continue;

		}
		task=get_ready_task();
		DSP_PROFILE_CUSTOM_ID_ADD(task,task->prio)
		DSP_PROFILE_CUSTOM_START(task);
		if(task->task_pro)
		{

			status=task->task_pro(task);
			if((status&DSP_REPORT_TO_HOST_FLAG))
			{
//				*((int *)task->report_info.buf)=task->report_info.report_id;
				xrp_send_report(device,task->report_info.report_id|DSP_REPORT_TO_HOST_FLAG);
			}
			clr_ready_task(task->prio,task->task_empty_check);
		}
		else
		{
			ps_wrn("ps no valid task handler\n");
		}
		DSP_PROFILE_CUSTOM_END(task);
	}
	ps_wrn("ps schedule exit");

}


int ps_task_interrupt_unregister(int32_t pri,uint32_t intrnum)
{
    int32_t ret = 0;

    // Normally none of these calls should fail. We don't check them
    // individually, any one failing will cause this function to return failure.
    ret = xtos_set_interrupt_handler(intrnum,
    				g_scheduler.task_handler[pri]->intr_handler, NULL, NULL);
    g_scheduler.task_handler[pri]->intr_handler=NULL;

    ps_debug("unregister ps interrupt %d\n",intrnum);
    return ret;
}

int ps_exception_init()
{
	enum xrp_status status;
	xrp_user_initialize(&status);
	return status==XRP_STATUS_SUCCESS ?0:-1;
}

