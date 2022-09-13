/********************************************************************************************

 ************************************************************************************************/
#ifndef _DMA_IF_H_
#define _DMA_IF_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "csi_dsp_types.h"
#include "tileManager.h"


/****************************************************************************************
    definition
****************************************************************************************/
#define _ADDR_ALIGN_ (128)

#define _IN_
#define _OUT_

/*********************************
 * Definitions for Image/Tile Size
 ********************************/
#define IMG_WIDTH   	 640
#define IMG_HEIGHT 		 64
#define BORDER_HOR       IVP_SIMD_WIDTH
#define BORDER_VER       1
#define TILE_WIDTH       (IMG_WIDTH) 
#define TILE_HEIGHT      (4)
#define TILE_WIDTH_EXT  (TILE_WIDTH  +  2*(BORDER_HOR))
#define TILE_HEIGHT_EXT (TILE_HEIGHT +  2*(BORDER_VER))
#define TILE_SIZE_IN    (TILE_WIDTH_EXT * TILE_HEIGHT_EXT)
#define TILE_SIZE_OUT   (TILE_WIDTH     * TILE_HEIGHT)
#if  ( 1 ==  _USE_WAITI_ )
#define INTERRUPT_ON_COMPLETION 1
#else
#define INTERRUPT_ON_COMPLETION 0
#endif
/******************************************
 * The symbols below define the padding 
 * requirements for arrays and image data.
 *****************************************/
#define HOR_PAD (IVP_SIMD_WIDTH)
#define VER_PAD (2)
//guarantee that image stride is multiple of 256
#define IMG_STRIDE     (((IMG_WIDTH) +2*(HOR_PAD)+255)/256*256)
#define IMG_HEIGHT_EXT ((IMG_HEIGHT)+2*(VER_PAD))

#define STACK (1024*6)
#define DRAM_USAGE (2*2*(TILE_WIDTH_EXT*TILE_HEIGHT_EXT + TILE_WIDTH*TILE_HEIGHT)+STACK)
#define DRAM_SIZE (64*1024)

/****************************************************************************************
    enum
****************************************************************************************/
typedef enum tm_dma_direction {
    DMA_DIR_IN = 0,
    DMA_DIR_OUT,
    DMA_DIR_NOT_SUPPORT,
    DMA_DIR_MAX = DMA_DIR_NOT_SUPPORT
}tm_dma_direction;

//typedef enum DL_DMA_DESC_IDX {
//    DL_DMA_DESC_IDX_0 = 0,
//    DL_DMA_DESC_IDX_1,
//    DL_DMA_DESC_IDX_2,
//    DL_DMA_DESC_IDX_3,
//    DL_DMA_DESC_IDX_4,
//    DL_DMA_DESC_IDX_5,
//    DL_DMA_DESC_IDX_6,
//    DL_DMA_DESC_IDX_7,
//    DL_DMA_DESC_IDX_MAX
//}DL_DMA_DESC_IDX;


/****************************************************************************************
    function
****************************************************************************************/
typedef TBOOL (*pfNoArg)();
typedef TBOOL (*pfCfg)(xvTile *, uint32_t ,uint32_t);
typedef TBOOL (*pfOneArg)(xvTile *);
typedef TBOOL (*pfWaitDone)(xvTile *,xvTile *);
typedef TBOOL   (*pfStart)(TUINT32, TUINT32);
typedef TUINT32 (*pfDataMv)(xvTile * , uint64_t, uint64_t, uint32_t , uint32_t , uint32_t , uint32_t , uint32_t );
typedef xvTile *(*pfSetup)(uint64_t ,uint32_t ,int32_t , int32_t , int32_t , uint8_t , uint8_t , uint8_t , uint32_t ,
			  	  int32_t , int32_t , uint16_t , int32_t , uint16_t , uint16_t , int32_t , uint16_t , int32_t );


typedef TBOOL (*pfAlign_check)(xvTile * ,uint64_t, uint64_t, TUINT32 , TUINT32 , TUINT32 , TUINT32 , tm_dma_direction );
typedef TBOOL (*pfGet_alignment)(_IN_ xvTile *, _IN_ TUINT32 , _OUT_ TUINT32*, _OUT_ TUINT32*, _OUT_ TUINT32*);
typedef T_PTR (*pfAllocateTcm)(TUINT32 ,TUINT32 ,TUINT32 );
typedef TBOOL (*pfFreeTcm)(T_PTR);
/****************************************************************************************
    structure
****************************************************************************************/
typedef struct TMDMA
{
    pfNoArg init;
    pfNoArg uninit;
    pfSetup setup;
    pfOneArg release;
    pfCfg config;

    pfDataMv  load;
    pfDataMv  store;
    pfDataMv  copy;

    pfStart start;
    pfNoArg stop;
    pfNoArg stall;
    pfWaitDone waitDone;
    pfOneArg waitLoadDone;
    pfOneArg waitStoreDone;
    pfNoArg isDone;

    pfAlign_check align_check;
    pfGet_alignment get_alignment;
    
    pfAllocateTcm allocateTcm;
    pfFreeTcm freeTcm;
//    /* for byte per row and stride */
//    TUINT32 alignBitDep;
}TMDMA;

/****************************************************************************************
    interface
****************************************************************************************/
#ifdef __cplusplus
#define EXTERNC_S extern "C" {
#define EXTERNC_E }
#else
#define EXTERNC_S
#define EXTERNC_E
#endif

EXTERNC_S
TMDMA *dmaif_getDMACtrl(void);
EXTERNC_E
/****************************************************************************************

****************************************************************************************/

#ifdef __cplusplus
}
#endif

#endif
