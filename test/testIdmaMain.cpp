
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _XOS
#define IDMA_APP_USE_XOS // will use inlined disable/enable int
#endif
#ifdef _XTOS
#define IDMA_APP_USE_XTOS // will use inlined disable/enable int
#endif



//#include "CppUTest/CommandLineTestRunner.h"
#include "CppUTest/TestHarness.h"
#include "dmaif.h"

#ifdef DSP_TEST

#define IDMA_DEBUG 1
// IDMA_BUFFER_DEFINE(task_write, 1, IDMA_64B_DESC);
#define TCM_SIZE (32 * 1024)
#define HOR_RES 4096
#define VER_RES 3072

#define SIZE_PER_PEX_BIT 8

#define LINE_SIZE ((HOR_RES * SIZE_PER_PEX_BIT + 7) / 8)

#define N_LINE 16
static char temp_bufer_tcm0[TCM_SIZE] __attribute__((section(".dram0.data")));
static char temp_bufer_tcm1[TCM_SIZE] __attribute__((section(".dram1.data")));
#define MAX_ENTRY 256
uint32_t post_isp_tick[MAX_ENTRY];
uint32_t isp_int_tick[MAX_ENTRY];
int32_t post_isp_cnt;
int32_t isp_int_cnt;



int compare(char *dst, char *src, uint32_t size) {
    int i;
    for (i = 0; i < size; i++) {
        if (src[i] != dst[i]) {
            printf("Compare fail,expect:%d,actrul:%d,at:%d\n", src[i], dst[i],
                   i);
            return -1;
        }
    }
    return 0;
}

#define FRAME_NUM 2
#define N_BUFFER_NUM 2
char dst_pic[FRAME_NUM][VER_RES * LINE_SIZE];
char source_pic[FRAME_NUM][VER_RES * LINE_SIZE];


TEST_GROUP(TileIdmaTest){

    void setup(){
    	int ind1, ind2,ind3;
    	const int step=64;
    	idma_obj =dmaif_getDMACtrl();
    	idma_obj->init();
		for (ind1 = 0; ind1 < FRAME_NUM; ind1++) {
			for (ind2 = 0; ind2 < VER_RES; ind2++) {
				for(ind3 = 0;ind3 < LINE_SIZE; ind3+=step)
				{
					source_pic[ind1][LINE_SIZE * ind2+ind3] = (char)(rand() % 256);
				}

				dst_pic[ind1][LINE_SIZE * ind2] = (char)((rand() + 1) % 256);
				dst_pic[ind1][LINE_SIZE * (ind2 + 1) - 1] = (char)((rand() + 1) % 256);
			}
		}
}
    int dumy_process(xvTile * out_tile,xvTile * in_tile)
    {
    	if(!in_tile || !out_tile)
    	{
    		return -1;
    	}
    	uint32_t size_in = XV_TILE_GET_PITCH_IN_BYTES(in_tile)*XV_TILE_GET_HEIGHT(in_tile);
    	uint32_t size_out = XV_TILE_GET_PITCH_IN_BYTES(out_tile)*XV_TILE_GET_HEIGHT(out_tile);
    	uint32_t size = size_in>size_out?size_out:size_in;
    	memcpy((void*)XV_TILE_GET_DATA_PTR(out_tile),(void*)XV_TILE_GET_DATA_PTR(in_tile),size);
    	return 0;
    }

void teardown() {

	idma_obj->uninit();
	idma_obj=NULL;
}
TMDMA * idma_obj;

};


TEST(TileIdmaTest, TestOneFrameTileBaseProcess_1) {

    uint32_t frame_counter = 0;
    TBOOL ret = TTRUE;
    unsigned int width=640;
    unsigned int height=480;
    unsigned int pitch=640;
    unsigned int pixRes=1;
    unsigned int  tile_width =64;
    unsigned int  tile_height =16;
    unsigned int  tile_pitch=64;
    unsigned int x=0,y=0;
    xvTile *in_tile_obj;
    xvTile *out_tile_obj;
    uint32_t data_size;

    in_tile_obj = idma_obj->setup((uint64_t)&source_pic[0][0],VER_RES * LINE_SIZE,width,height,width,pixRes,1,0,0,
					tile_width*tile_height*pixRes,tile_width,tile_height,tile_width,0,0,0,XV_TILE_U8,DATA_ALIGNED_32);
    out_tile_obj = idma_obj->setup((uint64_t)&dst_pic[0][0],VER_RES * LINE_SIZE,width,height,width,pixRes,1,0,0,
					tile_width*tile_height*pixRes,tile_width,tile_height,tile_width,0,0,0,XV_TILE_U8,DATA_ALIGNED_32);
    do
    {
    	ret =idma_obj->load(in_tile_obj, 0,0,0,0,0,0, 1);
    	ret &=idma_obj->waitLoadDone(in_tile_obj);
        if(0!=dumy_process(out_tile_obj,in_tile_obj))
        {
        	ret = TFALSE;
        	break;
        }
        ret &=idma_obj->store(out_tile_obj, 0,0,0,0,0,0, 1);

        ret &=idma_obj->waitStoreDone(out_tile_obj);
        x+=tile_width;
        if(x>=width)
        {
        	x=0;
        	y+=tile_height;
        	if(y>=height)
        	{
        		y=0;
        	}
        }
//        XV_TILE_SET_X_COORD(in_tile_obj,x);
//        XV_TILE_SET_X_COORD(out_tile_obj,x);
        idma_obj->config(in_tile_obj,x,y);
//        XV_TILE_SET_Y_COORD(in_tile_obj,y);
//        XV_TILE_SET_Y_COORD(out_tile_obj,y);
        idma_obj->config(out_tile_obj,x,y);
    }while(!((in_tile_obj->x==0)&&(in_tile_obj->y==0)));
    ret &=idma_obj->release(in_tile_obj);
    ret &=idma_obj->release(out_tile_obj);
    CHECK_TRUE(ret);
    ret = compare((char *)dst_pic, (char *)source_pic, width * height*pixRes);
    CHECK_EQUAL_ZERO(ret);

}

TEST(TileIdmaTest, TestOneFrameNoTileBaseProcess_1) {

    int loop;
    unsigned int width=640;
    unsigned int height=480;
    unsigned int pitch=640;
    unsigned int pixRes=1;
    unsigned int  tile_width =64;
    unsigned int  tile_height =16;
    unsigned int  tile_pitch=64;
    int buf_tcm_size = tile_pitch*tile_height*pixRes;
    int frame_size = height*pitch*pixRes;
//	int buf_out_tcm_size = buf_in_tcm_size;
    int fragment_size = frame_size % buf_tcm_size;
    int loop_num = frame_size / buf_tcm_size;
    TBOOL ret = TTRUE;
    int size = buf_tcm_size;
    loop_num = fragment_size ? loop_num + 1 : loop_num;
    uint32_t frame_counter = 0;
    uint32_t start,end;
    uint32_t data_size;
    uint32_t *buf_in_ptr;
//    uint32_t *buf_out_ptr;
    buf_in_ptr = (uint32_t *)idma_obj->allocateTcm(buf_tcm_size,0,32);
    for (loop = 0; loop < loop_num; loop++) {

        uint64_t dst1_wide =
            ((uint64_t)0xffffffff) & ((uint64_t)buf_in_ptr);
        uint64_t src1_wide = (uint64_t)source_pic + size * loop;

        uint64_t dst2_wide = (uint64_t)dst_pic + size * loop;
        if (loop == (loop_num - 1) && fragment_size) {
            size = fragment_size;
        }
	data_size= idma_obj->load(NULL,dst1_wide,src1_wide,size,size,1,size,1);
	ret &= data_size==0?TFALSE:TTRUE;
	data_size= idma_obj->store(NULL,dst2_wide,dst1_wide,size,size,1,size,1);
	ret &= data_size==0?TFALSE:TTRUE;
	ret &= idma_obj->waitDone(NULL,NULL);
    }
    ret &= idma_obj->freeTcm(buf_in_ptr);
    CHECK_TRUE(ret);
    ret = compare((char *)dst_pic, (char *)source_pic, width * height*pixRes);
    CHECK_EQUAL_ZERO(ret);
}
#endif
//int main(int argc, char **argv) { return RUN_ALL_TESTS(argc, argv); }

