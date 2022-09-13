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

#ifndef ALGO_H
#define ALGO_H
#include "ialgo.h"
enum IMAGE_FORMAT{
	IMAGE_RAW8 = 1,
	IMAGE_RAW10= 2,
};
struct algo_param {
	uint64_t image_in[2]; 	// 输入图片地址
	uint64_t image_out[2]; 	// 输出图片地址
	uint64_t report;
	int cols;			// 图片列数
	int rows;			// 图片行数
	int chan;			// 图片通道数
	int format;			// 图片格式, Raw8(uchar) 和 RAW10(unsigned short)
	int stride;     //row size
	int start_row;		// 传入图片的开始行
	int stop_row;		// 传入图片的结束行
	float gamma;		// float参数
	float coef1;
	float coef2;
	float coef3;
	float coef4;
	short beta;			// short型参数
	short beta1;
	short beta2;
	short beta3;
	short beta4;
	int reserve[8];		// 保留
};


// gamma process
//void gamma_raw8(unsigned char *image_in, unsigned char *image_out, int height, int width);

void gamma_raw10(unsigned short *image_in, unsigned short *image_out, int height, int width);

void gamma_raw10_slow(unsigned short *image_ddr_in, unsigned short *image_ddr_out,  int height, int width);

int gamma_wrapper(struct algo_param * params);

algo_func dsp_dummy_algo_wrapper(csi_dsp_algo_param_t* param);

int dsp_dummy_algo(csi_dsp_algo_param_t* param);
#endif /*  */


