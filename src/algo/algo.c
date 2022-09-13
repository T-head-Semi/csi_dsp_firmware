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
#include <stdlib.h>
#include <math.h>
#include "algo.h"
#include "ialgo.h"
#include "xrp_api.h"
#include "csi_dsp_post_process_defs.h"
#include "dmaif.h"

#if defined(DSP_TEST) || defined(DSP_IP_TEST) || !defined(USE_SUBFUNC_OBJ)
#define LM_DRAM0(align)
#define LM_DRAM1(align)
#else

#define LM_DRAM0(align) __attribute__((aligned(align), section(".dram0.data")))
#define LM_DRAM1(align) __attribute__((aligned(align), section(".dram1.data")))
#endif
// 50K空间
#define BUFFER_SIZE  (51200)

static unsigned char image_in_char_tdma0[BUFFER_SIZE]    	LM_DRAM0(64);
static unsigned char image_in_char_tdma1[BUFFER_SIZE]    	LM_DRAM1(64);
static unsigned char image_out_char_tdma0[BUFFER_SIZE]    	LM_DRAM0(64);
static unsigned char image_out_char_tdma1[BUFFER_SIZE]    	LM_DRAM1(64);

#define  LIB_NAME  ("lib")

typedef struct cb_args{
    char lib_name[8];
    char setting[16];
    uint32_t frame_id;
    uint64_t timestap;
	uint32_t  temp_projector;   //投射器温度
	uint32_t  temp_sensor;     //sensor 温度
}cb_args_t;

int dsp_dummy_algo(csi_dsp_algo_param_t* param)
{
	if(!param || param->buffer_num <2 )
	{
		return -1;
	}

	struct csi_dsp_buffer *bufs = (struct csi_dsp_buffer *)param->buffer_struct_ptr;
#if 0
	if(idma_copy_ext2ext(bufs[0].planes[0].buf_phy, bufs[1].planes[0].buf_phy,bufs[0].planes[0].size,
			image_in_char_tdma0,sizeof(image_in_char_tdma0)))
	{
		return -1;
	}
#else
	int loop;
	TMDMA* idma_obj =dmaif_getDMACtrl();
	uint64_t src,dst;
	int tcm_size = sizeof(image_in_char_tdma0);
	int size = tcm_size;
	int index =0 ;
    int fragment_size ;
    int loop_num ;
    static uint64_t temp_buf=0;
    if(param->buffer_num==3)
    {
    	if(temp_buf !=bufs[2].planes[0].buf_phy)
    	{
    		temp_buf = bufs[2].planes[0].buf_phy;
    		printf("update temp_buf:0x%lx\n",*((uint32_t*)temp_buf));
    	}
    }
    for(index=0;index<bufs->plane_count;index++)
    {
    	fragment_size = bufs[0].planes[index].size % tcm_size;
    	loop_num = bufs[0].planes[index].size / tcm_size;
    	loop_num = fragment_size ? loop_num + 1 : loop_num;
		src = bufs[0].planes[index].buf_phy;
		dst = bufs[1].planes[index].buf_phy;
		size = tcm_size;
		for (loop = 0; loop < loop_num; loop++) {

			uint64_t dst1_wide =
				((uint64_t)0xffffffff) & ((uint64_t)image_in_char_tdma0);
			uint64_t src1_wide = (uint64_t)src + size * loop;

			uint64_t dst2_wide = (uint64_t)dst + size * loop;
			if (loop == (loop_num - 1) && fragment_size) {
				size = fragment_size;
			}
			idma_obj->load(NULL,dst1_wide,src1_wide,size,size,1,size,1);
			idma_obj->waitLoadDone(NULL);
			idma_obj->store(NULL,dst2_wide,dst1_wide,size,size,1,size,1);
			idma_obj->waitStoreDone(NULL);
		}
    }
    static frame_count =0;
    cb_args_t *report_data;
    if(param->extra_param_ptr!=NULL &&
		param->extra_param_ptr->report_buf!=NULL&&
		param->extra_param_ptr->report_buf_size>= sizeof(*report_data))
    {
    	report_data = (cb_args_t *)param->extra_param_ptr->report_buf;
//    	memcpy(param->extra_param_ptr->report_buf,&frame_count,sizeof(frame_count));
    	report_data->frame_id = param->extra_param_ptr->frame_meta.frame_id;
    	report_data->timestap = param->extra_param_ptr->frame_meta.timestap;
    	report_data->temp_projector = param->extra_param_ptr->frame_meta.temp_projector;
    	report_data->temp_sensor = param->extra_param_ptr->frame_meta.temp_sensor;
    	/*loopback the setting*/
    	if(param->set_buf_size!=0 && param->set_buf_ptr!=NULL)
    	{
    		memcpy(&report_data->setting[0],param->set_buf_ptr,16);
    	}
    	memcpy(&report_data->lib_name[0],LIB_NAME,8);
    	param->extra_param_ptr->report_size = sizeof(*report_data);
    }
    frame_count++;

#endif
	return 0;

}


algo_func dsp_dummy_algo_wrapper(csi_dsp_algo_param_t* param)
{
	param->buf_desc_num =3;

	return dsp_dummy_algo;
}

