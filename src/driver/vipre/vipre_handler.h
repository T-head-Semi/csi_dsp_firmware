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

#ifndef VIPRE_HANDLER_H
#define VIPRE_HANDLER_H
#include "vipre_hw.h"



typedef enum {

	EVIPRE_MODE_M_FRAME,
	EVIPRE_MODE_N_LINE,

}EVIPRE_MODE;

//typedef enum{
//	EVIPRE_RAW_FMT_RAW6,
//	EVIPRE_RAW_FMT_RAW8,
//	EVIPRE_RAW_FMT_RAW10,
//	EVIPRE_RAW_FMT_RAW12,
//}EVIPRE_RAW_FMT;

typedef struct{
	uint16_t buffer_num;
    uint16_t size_per_line;
//    uint16_t stride_per_line;
    uint16_t line_num_per_buffer;
	uint16_t pixs_per_line;
	csi_dsp_img_fmt_e data_fmt;
	struct {
		  uint64_t buffer_addr[4];
	}channel_buffer[MAX_VIPRE_CHANNEL];

}Svipre_buffer_list;

typedef struct {

    uint16_t line_num_per_entry;
    uint16_t line_num_per_frame;
    uint16_t line_num_in_last_entry;
    uint16_t size_per_line;
    uint16_t line_num_per_buffer;
    uint16_t line_num_used_per_frame;
	struct {
		  uint64_t buffer_addr;
	}channel_buffer[MAX_VIPRE_CHANNEL];

} Svipre_n_line_info;

typedef struct {

    uint8_t customer_ping_pong_flag;
    uint8_t produce_ping_pong_flag;
    uint16_t active_channle;
    uint16_t current_NM;
    int16_t status;
    EVIPRE_MODE vipre_mode;
    union
    {
    	Svipre_n_line_info info[PING_PONG];
        Svipre_buffer_list buffer_info[PING_PONG];
    };

    Sfifo fifo[PING_PONG];
    union
    {
    	Svipre_n_line_info base;
    	Svipre_buffer_list base_buffer_info;
    };
    uint32_t *frame_meta_reg;
    frame_meta_t  meta[MAX_VIPRE_BUFFER];

} Svipre_handler;


typedef struct{

	uint16_t channel_num;
	uint16_t line_num;
	uint16_t fmt;
	uint32_t line_size;
	uint32_t size;
	uint64_t start_addr[MAX_VIPRE_CHANNEL];
}Svipre_buffer;

extern Eisp_bool vipre_push_new_entry();
extern Eisp_bool vipre_is_last_entry_push_in_frame();
extern void vipre_set_overflow();
extern Eisp_bool vipre_push_new_line(uint16_t element);
extern void vipre_store_frame_meta(Sfifo *vipre_fifo );
#endif /*  */
