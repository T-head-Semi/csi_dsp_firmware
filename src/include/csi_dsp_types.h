/********************************************************************************************

 ************************************************************************************************/
#ifndef _CSI_DSP_TYPES_H_
#define _CSI_DSP_TYPES_H_

#define _DL_DEBUG_





#undef bool
#define bool char
#define TBOOL char
#define TTRUE  1
#define TFALSE 0
#undef true
#define true   1
#undef false 
#define false  0

#ifndef NULL
#define NULL 0
#endif
#ifndef null
#define null 0
#endif

typedef void TVOID;

typedef char   TINT8;
typedef char   int8;
typedef unsigned char TUINT8;
typedef unsigned char uint8;
typedef char   TCHAR;
                                        
typedef signed short    TINT16;
typedef unsigned short  TUINT16;
typedef signed short    TSHORT;
typedef unsigned short  TUSHORT;
                                          
typedef signed int   TINT;
typedef unsigned int TUINT;
typedef signed int   TINT32;
typedef signed int   int32;
typedef unsigned int TUINT32;
typedef unsigned int uint32;
                                          
typedef signed long   TLONG;
typedef unsigned long TULONG;
                                          
typedef signed long long   TLONGLONG;
typedef unsigned long long TULONGLONG;
                                          
typedef float TFLOAT;

typedef TUINT8 DMA_MEM_T;

typedef void*   T_PTR;
/*****************************************************************************************
    definition
*****************************************************************************************/
/*****************************************************************************************
    enum
*****************************************************************************************/

//typedef enum _VPU_MSG_ENUM_ {
//    VPU_MSG_SUCEED           = 0,
//    VPU_MSG_ERR_PARAMETER,
//    VPU_MSG_ADL_WR_RING_OVERFLOW,
//    //
//    VPU_MSG_MAX,
//} VPU_MSG_ENUM;
//
///*****************************************************************************************
//    structure
//*****************************************************************************************/
//typedef struct _vpu_size_ {
//    TUINT32 width;  /* pixel */
//    TUINT32 height; /* pixel */
//    TUINT32 stride; /* byte */
//}VPU_SIZE;
//
//typedef struct _vpu_roi_ {
//    TUINT32 width;
//    TUINT32 height;
//    TUINT32 stride;
//}VPU_ROI;
//
//typedef struct _vpu_imgsize_ {
//    TUINT32 width;  /* pixel */
//    TUINT32 height; /* pixel */
//}VPU_IMGSIZE;
//
//typedef struct _vpu_offset_ {
//    TUINT32 x;
//    TUINT32 y;
//}VPU_OFFSET;
////
//
//typedef void*   TM_OBJ;
//
//typedef struct _tm_size_ {
//    TUINT32 width;  /* pixel */
//    TUINT32 height; /* pixel */
//    TUINT32 stride; /* byte */
//}TM_SIZE;
//
//typedef struct _tm_roi_ {
//    TUINT32 width;
//    TUINT32 height;
//    TUINT32 stride;
//}TM_ROI;
//
//typedef struct _tm_imgsize_ {
//    TUINT32 width;  /* pixel */
//    TUINT32 height; /* pixel */
//}TM_IMGSIZE;
//
//typedef struct _tm_offset_ {
//    TUINT32 x;
//    TUINT32 y;
//}TM_OFFSET;
///*****************************************************************************************
//    callback function
//*****************************************************************************************/
//typedef TBOOL   (*CBF_NOTIFY)(TVOID* user, TINT32 const msg);
//typedef TBOOL   (*CBF_DATA)(TVOID* user, TINT32 const msg);
//

#endif  // _DL_TYPES_H_

