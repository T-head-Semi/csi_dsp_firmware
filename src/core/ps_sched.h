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
#include "xrp_api.h"
#include "xrp_dsp_hw.h"
#include "xrp_dsp_user.h"
#include "dsp_ps_ns.h"
#include "ialgo.h"
#include "csi_dsp_task_defs.h"
#include "csi_dsp_post_process_defs.h"
#include "csi_dsp_types.h"
#define PS_MAX_TASK_TYPE   16

#define DSP_REPORT_TO_HOST_FLAG  0x10000000



enum task_result
{
	PS_TASK_SUCCESS = 0x0000000,
	PS_TASK_FAILURE = 0x0000001,
	PS_TASK_SUCCESS_WITH_REPORT = PS_TASK_SUCCESS|DSP_REPORT_TO_HOST_FLAG,
	PS_TASK_FAILURE_WITH_REPORT = PS_TASK_FAILURE|DSP_REPORT_TO_HOST_FLAG,

};

struct dsp_task_report{
	uint32_t report_id;
	uint32_t *buf;
	uint32_t buf_size;
};

typedef enum task_result (task_func)(void* context);


typedef struct frame_struct
{
  uint64_t pFrameBuff;
  uint32_t frameBuffSize;
  int32_t  frameWidth;
  int32_t  frameHeight;
  int32_t  framePitch;    // frame gaps base on byte
  int32_t  pitch;         //width pitch base on byte;
  uint8_t  pixelRes;
  uint32_t fmt;
  uint8_t  numChannels;
  uint8_t  paddingType;
  uint32_t paddingVal;
}frame_struct_t;

typedef struct entry_struct{
    uint64_t  pBuff;
	uint16_t  x;
	uint16_t  y;
    int16_t   width;
    int16_t   height;
    int16_t   fmt;
    int16_t   pitch;
    uint32_t  frame_height;
    csi_dsp_frame_meta_t meta;
}entry_struct_t;

typedef struct dsp_fe_handler{
	enum csi_dsp_fe_type type;
	int id;
	int (*get_state)(int id);
	void (*update_state)(int id);
	int (*get_next_entry)(int id,entry_struct_t *ctrl_info);
	int (*entry_done)(int id,entry_struct_t *ctrl_info);
}dsp_fe_handler_t;

typedef struct dsp_algo_handler{
	int algo_id;
	char* name;
	csi_dsp_algo_param_t *param;
	algo_func algo_process;

}dsp_algo_handler_t;

typedef struct dsp_be_handler{
	enum csi_dsp_be_type type;
	int id;
	int (*get_state)(int id);
	void (*update_state)(int id);
	int (*get_next_entry)(int id,void *ctrl_info);
	int (*entry_start)(int id,void *ctrl_info);
}dsp_be_handler_t;


typedef enum task_status{
	DSP_TASK_STATUS_FREE,     // the task is not allocted
	DSP_TASK_STATUS_IDEL,	// the task is allocted,but not config, not ready to run
	DSP_TASK_STATUS_READY,  // the task is allocted and config, ready to run
	DSP_TASK_STATUS_RUNING,
	DSP_TASK_STATUS_INVALID

}task_status_e;

typedef struct dsp_task{
	uint16_t prio;
	csi_dsp_task_mode_e type;
	task_status_e status;
	dsp_fe_handler_t task_fe;
	dsp_be_handler_t task_be;
	dsp_algo_handler_t task_algo;
	task_func *task_pro;
//	xrp_command_handler *command_handler;
	unsigned char  nsid[XRP_NAMESPACE_ID_SIZE];
	struct dsp_task_report report_info;
	bool (*task_empty_check)();
	int32_t irq_num;
	void (*intr_handler)(void * data);
	void * intr_arg;
}dsp_task_t;



typedef struct  {
	struct xrp_device *device;
	uint16_t task_num;
	dsp_task_t *task_handler[PS_MAX_TASK_TYPE];

}s_schduler_t;

typedef enum buffer_status{
	DSP_BUF_STATUS_FREE =-40,     // the buf is not allocted
	DSP_BUF_STATUS_INVALID,
	DSP_BUF_STATUS_NO_MEMORY,
	DSP_BUF_STATUS_ALLOCTED = 0,	// the buf is allocted,normal running
}buffer_status_e;

typedef struct _sw_be_handler{
	uint8_t id;
	uint8_t buf_num;
	uint8_t head_id;
	int8_t valid_buf_num;
	int status;
	uint32_t report_size;
	void * report;
	struct csi_dsp_buffer *bufs;
}sw_be_handler_t;

extern enum xrp_status ps_run_command(void *context,
                                   const void *in_data, size_t in_data_size,
                                   void *out_data, size_t out_data_size,
                                   struct xrp_buffer_group *buffer_group);

extern enum xrp_status ps_run_isp_algo_command(void *context,
                                   const void *in_data, size_t in_data_size,
                                   void *out_data, size_t out_data_size,
                                   struct xrp_buffer_group *buffer_group);

extern task_func ps_process_isp_task;
extern int init_scheduler(struct xrp_device *device);
extern int ps_task_shedule(struct xrp_device *device);
extern enum xrp_status ps_common_command(void *context,
        const void *in_data, size_t in_data_size,
        void *out_data, size_t out_data_size,
        struct xrp_buffer_group *buffer_group);

extern task_func  ps_process_xrp_task;

extern struct xrp_device * ps_xrp_device_int(int idx);

extern int ps_register_cmd_handler(unsigned char * nsid,xrp_command_handler *cmd_handler,void * context);

extern int ps_wait_host_sync();
extern int ps_send_report(uint32_t id);
extern int ps_report_avaiable();
extern int ps_task_interrupt_unregister(int32_t pri,uint32_t intrnum);

extern int ps_get_isp_irq(uint16_t id);

extern int ps_task_interrupt_register_nsid(unsigned char* nsid);

extern int ps_task_enable_common_task();
extern int ps_get_vipre_irq();
extern void sw_be_init();
extern enum xrp_status ps_vipre_config_message_handler(void* in_data,void* out_data);
extern enum xrp_status ps_sw_task_out_config_message_handler(void* in_data,void* out_data,void * report_buf,uint32_t report_buf_size);
extern enum xrp_status ps_isp_config_message_handler(void* in_data,void* context);
extern enum xrp_status sw_be_asgin_buf(int id ,sw_be_config_par* cfg);
extern enum xrp_status ps_post_isp_config_message_handler(void* in_data,void* out_data);
extern int ps_fill_report(void *buffer,csi_dsp_report_e type,void*payload,size_t size);
extern int ps_exception_init();
extern int csi_dsp_result_report(void* report_buf,void*result,size_t result_sz,void* extra,size_t extra_sz);
extern bool ps_is_vipre_task_empty();
