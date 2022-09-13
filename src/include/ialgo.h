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

#ifndef IALGO_H
#define IALGO_H

typedef struct csi_dsp_frame_meta{
	uint32_t  frame_id;
	uint32_t  temp_projector;   //Í¶ÉäÆ÷ÎÂ¶È
	uint32_t  temp_sensor;     //sensor ÎÂ¶È
	uint64_t  timestap;
}csi_dsp_frame_meta_t;

typedef struct csi_dsp_extra_param{
	uint16_t  start_line;
	uint16_t  end_line;
    uint32_t  total_line;
	uint16_t  report_buf_size;
	uint16_t  report_size;
	uint32_t  report_buf;
	csi_dsp_frame_meta_t  frame_meta;
}csi_dsp_extra_param_t;

typedef struct csi_dsp_algo_param {

	uint32_t	version;
/*********************utility**********************************/
	int     (*printf_func)(const char *fmt,...);
	void * (*get_idma_ctrl)();
/****************read only by host filled by dsp********************/
	uint16_t   buf_desc_num;
	uint16_t   info_num_prop_des;
	uint16_t   set_num_prop_des;
	void 	  *buf_desc_ptr;
	void 	  *info_prop_ptr;
	uint32_t  info_prop_length;
	void 	  *info_desc_ptr;
	void 	  *set_desc_ptr;


/****************read only by dsp filled by host********************/
	uint32_t         buffer_num;       //size of buffer array
	void 	        *buffer_struct_ptr; // buffer array
	void	        *set_buf_ptr;   //setting buffer
	uint32_t         set_buf_size; // set_buf_ptr buffer size
	csi_dsp_extra_param_t*	 extra_param_ptr;   //param for HW mode

}csi_dsp_algo_param_t;




typedef int (*algo_func)(csi_dsp_algo_param_t* context);
typedef algo_func (*wrapper_func)(void* context);
#endif /*  */


