/*
 * idma_wraper.c
 *
 *  Created on: 2021Äê5ÔÂ14ÈÕ
 *      Author: DELL
 */
#include <xtensa/hal.h>
#include <xtensa/idma.h>
#include "dsp_ps_debug.h"
// Define this to enable the wide address API.
#define IDMA_USE_WIDE_ADDRESS_COMPILE 1
// Define this to enable the large Description  API.
#define IDMA_USE_64B_DESC 1
//#define IDMA_DEBUG 1
static IDMA_BUFFER_DEFINE(idma_copy, 2, IDMA_64B_DESC);

static int do_cnt;
static int trans_cnt=0;
static uint64_t idma_hb[1536];
static void
idmaErrCB(const idma_error_details_t* data)
{
  idma_error_details_t* error = idma_error_details(IDMA_CHAN_FUNC_ARG);
  uint32_t idma_status = (uint32_t) READ_IDMA_REG(0, IDMA_REG_STATUS);
  printf("COPY FAILED, status :0x%x,Error 0x%x at desc:%p, PIF src/dst=%x/%x\n", idma_status,error->err_type, (void*)error->currDesc, error->srcAddr, error->dstAddr);
}


static void
idmaDoneCB(void* data)
{
	do_cnt=1;
}

int idma_inition()
{
	   idma_init(0, MAX_BLOCK_16, 32, TICK_CYCLES_8, 100000, idmaErrCB);
	   return 0;
}
int idma_copy_ext2ext(uint64_t src_addr, uint64_t dst_addr, uint32_t block_size,
							 uint32_t temp_buffer,uint32_t buffer_size) {
    // Construct wide pointers for the 3 buffers. Note that the addresses for
    // the two buffers in local memory must not have any of the high bits set or
    // else they'll look like sysmem addresses.

    int loop;
    int fragment_size = block_size % buffer_size;
    int loop_num = block_size / buffer_size;
    int ret = 0;
    int size = buffer_size;
    loop_num = fragment_size ? loop_num + 1 : loop_num;
    uint32_t frame_counter = 0;
    uint32_t start,end;
//    src_addr = 0xcce06000;
    // idma_log_handler(idmaLogHander);
//    idma_init(0, MAX_BLOCK_16, 32, TICK_CYCLES_8, 100000, idmaErrCB);
    for (loop = 0; loop < loop_num; loop++) {

        uint64_t dst1_wide =
            ((uint64_t)0xffffffff) & ((uint64_t)temp_buffer);
        uint64_t src1_wide = src_addr + size * loop;
        uint64_t dst2_wide = dst_addr + size * loop;
        if (loop == (loop_num - 1) && fragment_size) {
            size = fragment_size;
        }

        idma_init_task(idma_copy, IDMA_64B_DESC, 2, idmaDoneCB, idma_copy);

        idma_add_desc64_wide(idma_copy, &dst1_wide, &src1_wide, size, 0);
        idma_add_desc64_wide(idma_copy, &dst2_wide, &dst1_wide, size,
                             DESC_NOTIFY_W_INT);
        do_cnt=0;
        ret = idma_schedule_task(idma_copy);
        start = XT_RSR_CCOUNT();
        if (ret != IDMA_OK) {
        	ps_debug("FAILED to schedule taskh: %d\n", ret);
            return -1;
        }
        idma_hw_wait_all();
        while(do_cnt==0)
        {
        	;
        }
//        idma_hb[trans_cnt++]=src_addr;
//        idma_hb[trans_cnt++]=dst_addr;
        end = XT_RSR_CCOUNT();

    }
    return 0;
}
