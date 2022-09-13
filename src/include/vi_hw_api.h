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
#ifndef VI_HW_API_H
#define VI_HW_API_H

#ifdef __cplusplus
extern "C" {
#endif

#include "../common/isp_common.h"
#include "../driver/vipre/vipre_handler.h"
int32_t isp_initialize(uint16_t id);

int32_t isp_handler_config(uint16_t hor_res, uint16_t ver_res,
							EIsp_raw_fomrat raw_type, uint16_t line_value,
                           uint64_t buffer_addr, uint32_t buffer_size,uint16_t line_size);

int isp_get_status(int id);
Eisp_bool isp_get_next_entry(Sisp_buffer buffer_list[MAX_BUFFER_BLOCK]);

int16_t isp_get_pushed_line_num();
int16_t isp_get_poped_line_num();
Eisp_bool isp_is_last_entry_pop_in_frame();
Eisp_bool isp_is_all_entry_done();
Eisp_bool isp_is_last_entry_push_in_frame();
uint32_t isp_pop_n_line(uint16_t line_num);
int32_t isp_get_line_size();
uint32_t isp_get_frame_size();
uint32_t isp_get_frame_height();
int32_t isp_enable_inerrupt(uint16_t id);
int32_t isp_disable_inerrupt(uint16_t id);
int isp_get_irq_num(uint16_t id);
void isp_recovery_state_update(uint16_t id );
int32_t post_isp_initialize(uint16_t id);

int32_t post_isp_handler_config(uint16_t hor_res, uint16_t ver_res,
                                uint16_t size_per_px, uint16_t line_value,
                                uint64_t buffer_addr, uint32_t buffer_size,uint16_t line_size);


int32_t post_isp_get_next_entry(Sisp_buffer *entry_info);
int32_t post_isp_push_entry(uint16_t line_num);
Eisp_bool post_isp_is_last_block_in_frame();
Eisp_bool post_isp_is_last_block_pop_in_frame();
int32_t post_isp_get_line_size();
uint32_t post_isp_get_frame_size( );
Eisp_bool post_isp_is_full();
Eisp_bool post_isp_is_empty();
int16_t post_isp_get_pushed_line_num();
int16_t post_isp_get_poped_line_num();
void post_isp_soft_reset(uint16_t id);
int32_t post_isp_enable_inerrupt(uint16_t id);
int32_t post_isp_disable_inerrupt(uint16_t id);
int post_isp_get_status(int id);
void post_isp_recovery_state_update(uint16_t id );

int32_t vipre_initialize();
int32_t vipre_handler_nline_config(uint32_t chan_num,uint16_t hor_res, uint16_t ver_res,
							uint16_t line_size, csi_dsp_img_fmt_e raw_type, uint16_t line_value,
                            uint64_t *buffer_addr_list, uint32_t buffer_size);
int32_t vipre_frame_mode_handler_config(uint32_t act_channel,uint16_t hor_res, uint16_t ver_res,uint16_t stride,
										csi_dsp_img_fmt_e raw_type,uint32_t buffer_num,uint64_t *buffer_addr_list);
int32_t vipre_check_exeptional(int id);
Eisp_bool vipre_pop_new_entry(int16_t line_num);
int vipre_get_irq_num();
uint32_t vipre_get_frame_height();
int32_t vipre_check_exeptional(int id);
void vipre_recovery_state_update(uint16_t id );
Eisp_bool vipre_get_next_entry(uint16_t id,Svipre_buffer *buffer);
frame_meta_t * vipre_get_frame_meta();
Eisp_bool vipre_is_empty();
#ifdef __cplusplus
}
#endif

#endif
