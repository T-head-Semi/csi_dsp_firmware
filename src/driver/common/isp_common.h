#ifndef ISP_COMMON_H
#define ISP_COMMON_H

#include <xtensa/config/core-isa.h>
#include <xtensa/hal.h>
#include <xtensa/xtensa-types.h>
#include <xtensa/xtruntime.h>
#include "csi_dsp_post_process_defs.h"
#define ALWAYS_INLINE __attribute__((always_inline)) static inline
#define ISP_SECTION __attribute__((section(".dram1.data")))
#define PING_PONG 2
#define MAX_ISP 2
#define VI_SYSREG_BASE  (0xFFC00000)
#define MAX_POST_ISP 1
#define MAX_HORIZONTAL 4096
#define MAX_VERTICAL 3192

#define MAX_BUFFER_BLOCK   2



#define LINES_MASK 0x1FFF
#define ISP_ENABLE_INTS
#define ISP_DISABLE_INTS


/**********************HW param define *************************/


typedef enum {
    EFIFO_STATUS_EMPTY = 0,
    EFIFO_STATUS_NOTMAL,
    EFIFO_STATUS_FULL,
    EFIFO_STATUS_MISS_PUSH,
    EFIFO_STATUS_REACH_MAX,
    EFIFO_STATUS_OVERFLOW
} Efifo_status;

typedef enum { EISP_BOOL_FASE = 0, EISP_BOOL_TRUE = 1 } Eisp_bool;


typedef enum {
    EHW_ERROR_PARMA_INVALID = -40,
    EHW_ERROR_OVER_FLOW,
    EHW_ERROR_POP_FAIL,
    EHW_ERROR_FRAME_SWITCH_FAIL,
    EHW_OK = 0,
    EHW_INPUT_EMPRY,
    EHW_RECOVERYING,
    EHW_RECOVERY_DONE,

} ehw_error_code;

typedef struct {
    uint16_t head;
    uint16_t tail;
    uint16_t depth;
    uint16_t max_index;

    int16_t offset;
    Efifo_status status;

} Sfifo;

typedef struct {

    uint16_t line_num_per_entry;
    uint16_t line_num_per_frame;
    uint16_t line_num_in_last_entry;
    uint16_t size_per_line;
    uint16_t line_num_per_buffer;
    uint16_t line_num_used_per_frame;
    csi_dsp_img_fmt_e format;
    uint64_t frame_buffer_addr;

} Sisp_control_info;

typedef struct {

    uint8_t customer_ping_pong_flag;
    uint8_t produce_ping_pong_flag;
    int16_t status;
    uint16_t id;
    uint16_t interact_counter;
    uint16_t current_N_value;
    Sisp_control_info info[PING_PONG];
    Sfifo fifo[PING_PONG];
    Sisp_control_info base;

} Sisp_handler;

typedef enum {
    EISP_RAW12_FORMAT_UNALGIN = 0,
    EISP_RAW12_FORMAT_ALGIN0 = 1,
    EISP_RAW12_FORMAT_ALGIN1,
    EISP_RAW8_FORMAT,
    EISP_RAW12_FORMAT_INVALID

} EIsp_raw_fomrat;


typedef struct{
	uint16_t fmt;
	uint16_t line_num;
	uint32_t size;
	uint32_t line_size;
	uint64_t start_addr;
}Sisp_buffer;


typedef struct {
    uint64_t frame_cnt;
    uint64_t frame_ts_us;
    int32_t floodlight_temperature;
    int32_t projection_temperature;
} frame_meta_t;

ALWAYS_INLINE void reset_fifo(Sfifo *ptr, uint16_t depth, int16_t base_index,uint16_t max) {
    ptr->head = 0;
    ptr->tail = 0;
    ptr->depth = depth;
    ptr->max_index = max;
    ptr->offset = base_index;
    ptr->status = EFIFO_STATUS_EMPTY;
}


extern Eisp_bool ispc_push_new_line(Sisp_handler *handler,uint16_t line_num);
extern Eisp_bool ispc_get_entry_to_push(const Sisp_handler *handler,const uint16_t line_num,
											Sisp_buffer buffer_list[MAX_BUFFER_BLOCK]);

extern Eisp_bool ispc_pop_new_line(Sisp_handler *handler,uint16_t line_num);
extern Eisp_bool ispc_get_entry_to_pop(const Sisp_handler *handler,const uint16_t line_num,
											Sisp_buffer buffer_list[MAX_BUFFER_BLOCK]);

extern int32_t ispc_base_info_update(Sisp_control_info *base_info,
                                     uint16_t hor_res, uint16_t ver_res,
                                     csi_dsp_img_fmt_e raw_type, uint16_t line_value,
                                     uint64_t buffer_addr,
                                     uint32_t buffer_size,
                                     uint16_t line_size);
extern Eisp_bool ispc_switch_buffer(Sisp_control_info *isp_info,
                                    Sisp_control_info *base);
extern uint32_t ispc_check_exeptional(Sisp_handler *handler);
//extern int32_t ispc_get_line_size(csi_dsp_img_fmt_e data_format,
//                                  uint16_t pexs_per_hor);
extern int16_t ispc_get_pushed_line_num(Sisp_handler *handler);
extern int16_t ispc_get_poped_line_num(Sisp_handler *handler);

static inline uint32_t ps_l32(volatile void *p)
{
	uint32_t v = *(const volatile uint32_t *)p;
	return v;
}

ALWAYS_INLINE void ps_s32(uint32_t v, volatile void *p)
{
	*(volatile uint32_t *)p = v;
}
ALWAYS_INLINE Eisp_bool is_full_fifo(Sfifo *ptr) {
    return (Eisp_bool)((ptr->tail - ptr->head) == ptr->depth);
}

ALWAYS_INLINE Eisp_bool is_enough_space_to_push_fifo(Sfifo *ptr,uint16_t element) {
    return (Eisp_bool)((ptr->tail +element- ptr->head) <= ptr->depth);
}

ALWAYS_INLINE Eisp_bool is_enough_element_to_pop_fifo(Sfifo *ptr,uint16_t element){
	return (Eisp_bool)((ptr->head +element) <= ptr->tail);
}

ALWAYS_INLINE Eisp_bool is_overflow_fifo(const Sfifo *ptr) {
    return (Eisp_bool)(ptr->status == EFIFO_STATUS_OVERFLOW);
}
ALWAYS_INLINE Eisp_bool is_empty_fifo(const Sfifo *ptr) {
    return (Eisp_bool)(ptr->tail == ptr->head);
}
ALWAYS_INLINE Eisp_bool push_fifo(Sfifo *ptr,uint16_t element,uint16_t offset) {
    ISP_DISABLE_INTS
    if (!is_enough_space_to_push_fifo(ptr,element+offset)) {
        ptr->status = EFIFO_STATUS_OVERFLOW;
        ISP_ENABLE_INTS;
        return EISP_BOOL_FASE;
    }
    ptr->tail +=element;
    ISP_ENABLE_INTS;
    return EISP_BOOL_TRUE;
}

ALWAYS_INLINE  Eisp_bool pop_fifo(Sfifo *ptr,uint16_t element) {
    ISP_DISABLE_INTS;
    if (is_empty_fifo(ptr) || is_overflow_fifo(ptr)||
    	!is_enough_element_to_pop_fifo(ptr,element)) {
        ISP_ENABLE_INTS
        return EISP_BOOL_FASE;
    }
    ptr->head +=element;
    ISP_ENABLE_INTS;
    return EISP_BOOL_TRUE;
}
ALWAYS_INLINE uint16_t get_depth_fifo(const Sfifo *ptr){return  ptr->depth;}

ALWAYS_INLINE uint16_t get_head_fifo(const Sfifo *ptr) {

    return (ptr->head + ptr->offset) % ptr->depth;
}

ALWAYS_INLINE uint16_t get_element_num_fifo(const Sfifo *ptr) {

    return ptr->tail-ptr->head;
}

ALWAYS_INLINE uint16_t get_free_num_fifo(const Sfifo *ptr) {

    return ptr->depth-get_element_num_fifo(ptr);
}

ALWAYS_INLINE int16_t get_head_index_fifo(const Sfifo *ptr) { return ptr->head; }



ALWAYS_INLINE int16_t get_tail_fifo(const Sfifo *ptr) {

    return (ptr->tail + ptr->offset) % ptr->depth;
}

ALWAYS_INLINE int16_t get_tail_index_fifo(Sfifo *ptr) { return ptr->tail; }

ALWAYS_INLINE Eisp_bool is_last_entry_push_fifo(const Sfifo *ptr) {
    return (Eisp_bool)(ptr->tail == ptr->max_index);
}

ALWAYS_INLINE Eisp_bool is_last_entry_to_push_fifo(const Sfifo *ptr,uint16_t elemnets) {
    return (Eisp_bool)(ptr->tail == (ptr->max_index - elemnets));
}
ALWAYS_INLINE Eisp_bool is_last_entry_pop_fifo(const Sfifo *ptr) {
    return (Eisp_bool)(ptr->head == ptr->max_index);
}

ALWAYS_INLINE Eisp_bool is_last_entry_to_pop_fifo(const Sfifo *ptr,uint16_t elemnets) {
    return (Eisp_bool)(ptr->head == (ptr->max_index - elemnets));
}

ALWAYS_INLINE uint32_t ispc_get_ver_res(Sisp_control_info *info) {
    return info->line_num_per_frame ;
}

ALWAYS_INLINE uint32_t ispc_get_line_num_in_buffer(Sisp_control_info *info) {
    return info->line_num_per_buffer;
}

ALWAYS_INLINE uint32_t ispc_get_frame_buffer_size(Sisp_control_info *info) {
    return ispc_get_line_num_in_buffer(info) * info->size_per_line;
}
#endif

