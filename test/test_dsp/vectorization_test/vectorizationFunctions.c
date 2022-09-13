/*
 * Copyright (c) 2020 by Cadence Design Systems, Inc.  ALL RIGHTS RESERVED.
 * These coded instructions, statements, and computer programs are the
 * copyrighted works and confidential proprietary information of
 * Cadence Design Systems Inc.  They may be adapted and modified by bona fide
 * purchasers for internal use, but neither the original nor any adapted
 * or modified version may be disclosed or distributed to third parties
 * in any manner, medium, or form, in whole or in part, without the prior
 * written consent of Cadence Design Systems Inc.  This software and its
 * derivatives are to be executed solely on products incorporating a Cadence
 * Design Systems processor.
 */


/* *********************************************************************************
 * FILE:  vectorizationFunctions.c
 *
 * DESCRIPTION:
 *
 *    This file contains auto vectorised, manually vectorised  and un vectorised implementations for
 *	  functions such as Add Mul 3-tap Filter 5-tap Filter 3X3 Filter
 * ********************************************************************************* */

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include "commonDef.h"
#ifdef IVP
#include <xtensa/tie/xt_ivpn.h>
#endif
#include <stdint.h>
#if defined(__XTENSA__)
#include <sys/times.h>
#include <xtensa/sim.h>
#endif


// 3x3 filter coefficients
#define FX_MAX  3
#define FY_MAX  3
extern int16_t filtercoeff[FX_MAX][FY_MAX];
/* ********************************************************************
*	FUNCTION:Add
*	DESCRIPTION:Reference Function
*				This function find the addition of two short numbers.
*	INPUTS:
                int16_t *vec1	Input Vector1
                int16_t *vec2	Input Vector2
                int32_t len		Length of the Input Vectors
*	OUTPUTS:
                int16_t *sum	Output Vector
**********************************************************************/


void Add(int16_t *vec1, int16_t *vec2, int16_t *sum, int32_t len)
{
  int32_t indx;

  for (indx = 0; indx < len; indx++)
  {
    sum[indx] = vec1[indx] + vec2[indx];
  }

  return;
}

#ifdef IVP
/* ********************************************************************
*	FUNCTION:xvAdd
*	DESCRIPTION:Manual Vectorised Function
*				This function find the addition of two short numbers.
*	INPUTS:
                int16_t *pvec1	Input Vector1
                int16_t *pvec2  Input Vector2
                int32_t veclen  Length of the Input Vectors
*	OUTPUTS:
                int16_t *psum	Output Vector
**********************************************************************/


void xvAdd(int16_t *pvec1, int16_t *pvec2, int16_t *psum, int32_t veclen)
{
  xb_vecNx16 vecData1;
  xb_vecNx16 vecData2;
  xb_vecNx16 vecResult;

  xb_vecNx16 * __restrict pvecData1;
  xb_vecNx16 * __restrict pvecData2;
  xb_vecNx16 * __restrict pvecResult;
  int32_t ix;
  pvecData1  = (xb_vecNx16 *) pvec1;
  pvecData2  = (xb_vecNx16 *) pvec2;
  pvecResult = (xb_vecNx16 *) psum;
  for (ix = 0; ix < (veclen / IVP_SIMD_WIDTH); ix++)
  {
    vecData1      = *pvecData1++;
    vecData2      = *pvecData2++;
    vecResult     = vecData1 + vecData2;
    *pvecResult++ = vecResult;
  }
  return;
}

#endif
/* ********************************************************************
*	FUNCTION:Mul
*	DESCRIPTION:Reference Function
*				This function find the multiplication of two short numbers.
*	INPUTS:
                int16_t *vec1	Input Vector1
                int16_t *vec2	Input Vector2
                int32_t len		Length of the Input Vectors
*	OUTPUTS:
                int16_t *sum	Output Vector
**********************************************************************/

void  Mul(int16_t *vec1, int16_t *vec2, int16_t *sum, int32_t len)
{
  int32_t indx;

  for (indx = 0; indx < len; indx++)
  {
    sum[indx] = vec1[indx] * vec2[indx];
  }

  return;
}

#ifdef IVP
/* ********************************************************************
*	FUNCTION:xvMul
*	DESCRIPTION:Manual Vectorised Function
*				This function find the multiplication of two short numbers.
*	INPUTS:
                int16_t *pvec1	Input Vector1
                int16_t *pvec2  Input Vector2
                int32_t veclen  Length of the Input Vectors
*	OUTPUTS:
                int16_t *psum	Output Vector
**********************************************************************/
void  xvMul(int16_t *pvec1, int16_t *pvec2, int16_t *psum, int32_t veclen)
{
  xb_vecNx16 vecData1;
  xb_vecNx16 vecData2;
  xb_vecNx16 vecResult;

  xb_vecNx16 * __restrict pvecData1;
  xb_vecNx16 * __restrict pvecData2;
  xb_vecNx16 * __restrict pvecResult;
  int32_t ix;
  pvecData1  = (xb_vecNx16 *) pvec1;
  pvecData2  = (xb_vecNx16 *) pvec2;
  pvecResult = (xb_vecNx16 *) psum;
  for (ix = 0; ix < (veclen / IVP_SIMD_WIDTH); ix++)
  {
    vecData1      = *pvecData1++;
    vecData2      = *pvecData2++;
    vecResult     = IVP_MULNX16PACKL(vecData1, vecData2);
    *pvecResult++ = vecResult;
  }
  return;
}

#endif
/* ********************************************************************
*	FUNCTION: Filt3Tap
*	DESCRIPTION:Reference Function
*				This function performs the 3 tap filtering operation.
*	INPUTS:
                int16_t *coeff		Filter coefficients
                int16_t *vecin		Input Vector
                int32_t  veclen		Vector length
*	OUTPUTS:
                int16_t *vecout		Output Vector
**********************************************************************/
void  Filt3Tap(int16_t *coeff, int16_t *vecin, int16_t *vecout, int32_t veclen)
{
  int32_t indx;
  for (indx = 0; indx < veclen; indx++)
  {
    vecout[indx] = (vecin[indx] * coeff[0]) +
                   (vecin[indx + 1] * coeff[1]) +
                   (vecin[indx + 2] * coeff[2]);
  }

  return;
}

#ifdef IVP
/* ********************************************************************
*	FUNCTION: xvFilt3Tap
*	DESCRIPTION:Manual Vectorised Function
*				This function performs the 3 tap filtering operation.
*	INPUTS:
                int16_t *pcoeff		Filter coefficients
                int16_t *pvecin     Input Vector
                int32_t  veclen     Vector length
*	OUTPUTS:
                int16_t *pvecout	Output Vector
**********************************************************************/
void  xvFilt3Tap(int16_t *pcoeff, int16_t *pvecin, int16_t *pvecout, int32_t veclen)
{
  xb_vecNx16 vecData0;
  xb_vecNx16 vecData1;
  xb_vecNx16 vecData2;
  xb_vecNx16 vecResult;
  xb_vecNx16 vecCoeff0;
  xb_vecNx16 vecCoeff1;
  xb_vecNx16 vecCoeff2;

  xb_vecNx16 * __restrict pvecData1;
  xb_vecNx16 * __restrict pvecResult;
  int32_t indx;
  vecCoeff0 = pcoeff[0];
  vecCoeff1 = pcoeff[1];
  vecCoeff2 = pcoeff[2];

  pvecData1  = (xb_vecNx16 *) pvecin;
  pvecResult = (xb_vecNx16 *) pvecout;
  vecData0   = *pvecData1++;

  for (indx = 0; indx < (veclen / IVP_SIMD_WIDTH); indx++)
  {
    vecData1  = *pvecData1++;
    vecResult = IVP_MULNX16PACKL(vecData0, vecCoeff0);
    vecData2  = IVP_SELNX16I(vecData1, vecData0, 0);
    IVP_MULANX16PACKL(vecResult, vecData2, vecCoeff1);
    vecData2 = IVP_SELNX16I(vecData1, vecData0, 1);
    IVP_MULANX16PACKL(vecResult, vecData2, vecCoeff2);
    *pvecResult++ = vecResult;
    vecData0      = vecData1;
  }

  return;
}

#endif
/* ********************************************************************
*	FUNCTION: Filt5Tap
*	DESCRIPTION:Reference Function
*				This function performs the 5 tap filtering operation.
*	INPUTS:
                int16_t *coeff		 Filter coefficients
                int16_t *vecin       Input Vector
                int32_t  veclen      Vector length
*	OUTPUTS:
                int16_t *vecout	     Output Vector
**********************************************************************/
void  Filt5Tap(int16_t *coeff, int16_t *vecin, int16_t *vecout, int32_t veclen)
{
  int32_t indx;

  for (indx = 0; indx < veclen; indx++)
  {
    vecout[indx] = (vecin[indx - 2] * coeff[0]) +
                   (vecin[indx - 1] * coeff[1]) +
                   (vecin[indx] * coeff[2]) +
                   (vecin[indx + 1] * coeff[3]) +
                   (vecin[indx + 2] * coeff[4]);
  }

  return;
}

#ifdef IVP
/* ********************************************************************
*	FUNCTION: xvFilt5Tap
*	DESCRIPTION:Manual Vectorised Function
*				This function performs the 5 tap filtering operation.
*	INPUTS:
                int16_t *pcoeff		Filter coefficients
                int16_t *pvecin     Input Vector
                int32_t  veclen     Vector length
*	OUTPUTS:
                int16_t *pvecout	Output Vector
**********************************************************************/

void  xvFilt5Tap(int16_t *pcoeff, int16_t *pvecin, int16_t *pvecout, int32_t veclen)
{
  xb_vecNx16 vecData0;
  xb_vecNx16 vecData1;
  xb_vecNx16 vecData2;
  xb_vecNx16 vecResult;
  xb_vecNx16 vecCoeff0;
  xb_vecNx16 vecCoeff1;
  xb_vecNx16 vecCoeff2;
  xb_vecNx16 vecCoeff3;
  xb_vecNx16 vecCoeff4;

  xb_vecNx16 * __restrict pvecData1;
  xb_vecNx16 * __restrict pvecResult;
  int32_t indx;
  vecCoeff0 = pcoeff[0];
  vecCoeff1 = pcoeff[1];
  vecCoeff2 = pcoeff[2];
  vecCoeff3 = pcoeff[3];
  vecCoeff4 = pcoeff[4];

  pvecData1  = (xb_vecNx16 *) &pvecin[-32];
  pvecResult = (xb_vecNx16 *) pvecout;
  vecData0   = *pvecData1++;
  vecData1   = *pvecData1++;

  for (indx = 0; indx < (veclen / IVP_SIMD_WIDTH); indx++)
  {
    vecData2  = IVP_SELNX16I(vecData1, vecData0, 9);
    vecResult = IVP_MULNX16PACKL(vecData2, vecCoeff0);
    vecData2  = IVP_SELNX16I(vecData1, vecData0, 8);
    IVP_MULANX16PACKL(vecResult, vecData2, vecCoeff1);
    IVP_MULANX16PACKL(vecResult, vecData1, vecCoeff2);
    vecData0 = vecData1;
    vecData1 = *pvecData1++;
    vecData2 = IVP_SELNX16I(vecData1, vecData0, 0);
    IVP_MULANX16PACKL(vecResult, vecData2, vecCoeff3);
    vecData2 = IVP_SELNX16I(vecData1, vecData0, 1);
    IVP_MULANX16PACKL(vecResult, vecData2, vecCoeff4);
    *pvecResult++ = vecResult;
  }
  return;
}

#endif
/* ********************************************************************
*	FUNCTION: Filter3x3
*	DESCRIPTION:Reference Function
*				This function performs the 3x3 tap filtering operation.
*	INPUTS:
                int16_t *filtin
                int32_t  width
                int32_t  widthExt
                int32_t	 height
*	OUTPUTS:
                int16_t *filtout	Output
**********************************************************************/

void  Filter3x3(int16_t *filtin, int16_t *filtout, int32_t width, int32_t widthExt, int32_t height)
{
  int32_t ix, iy, ix1, iy1, indx, indx1;
  int16_t coeffS;
  int16_t filtsum;

  for (ix = 0; ix < width; ix++)
  {
    for (iy = 0; iy < height; iy++)
    {
      indx    = (iy * (width)) + ix;
      filtsum = 0;
      for (iy1 = iy; iy1 < iy + 3; iy1++)
      {
        for (ix1 = ix; ix1 < ix + 3; ix1++)
        {
          indx1    = (iy1 * (widthExt)) + ix1;
          coeffS   = filtercoeff[iy1 - iy][ix1 - ix];
          filtsum += coeffS * filtin[indx1];
        }
      }
      filtout[indx] = filtsum;
    }
  }
  return;
}

#ifdef IVP
/* ********************************************************************
*	FUNCTION: xvFilter3x3
*	DESCRIPTION:Manual Vectorised Function
*				This function performs the 3x3 tap filtering operation.
*	INPUTS:
                int16_t *filtin		Input Image
                int32_t  width		Image width
                int32_t  widthExt	Extended image width
                int32_t	 height		Image height
*	OUTPUTS:
                int16_t *filtout	Output Image
**********************************************************************/
void  xvFilter3x3(int16_t *filtin, int16_t *filtout, int32_t width, int32_t widthExt, int32_t height)
{
  xb_vecNx16 vecData0;
  xb_vecNx16 vecData1;
  xb_vecNx16 vecData2;
  xb_vecNx16 vecResult;
  xb_vecNx16 vecCoeff0;
  xb_vecNx16 vecCoeff1;
  xb_vecNx16 vecCoeff2;
  xb_vecNx16 * __restrict pvecData1;
  xb_vecNx16 * __restrict pvecResult;

  int32_t ix, iy, indx;
  uintptr_t Uptrfiltin, Uptrfiltintemp, Uptrfiltout;
  uintptr_t Uptrcoeff, Uptrcoefftemp;
  int16_t(*__restrict pc);


  iy             = 0;
  Uptrfiltin     = (uintptr_t) filtin;
  Uptrfiltintemp = (uintptr_t) filtin;
  Uptrfiltout    = (uintptr_t) filtout;
  Uptrcoeff      = (uintptr_t) &filtercoeff[0][0];

  for (iy = 0; iy < height; iy++)
  {
    Uptrfiltintemp = Uptrfiltin;
    Uptrcoefftemp  = Uptrcoeff;

    pc        = (int16_t *) Uptrcoefftemp;
    vecCoeff0 = pc[0];
    vecCoeff1 = pc[1];
    vecCoeff2 = pc[2];

    pvecData1  = (xb_vecNx16 *) Uptrfiltintemp;
    pvecResult = (xb_vecNx16 *) Uptrfiltout;
    vecData0   = *pvecData1++;

    for (ix = 0; ix < (width / IVP_SIMD_WIDTH); ix++)
    {
      vecData1  = *pvecData1++;
      vecResult = IVP_MULNX16PACKL(vecData0, vecCoeff0);
      vecData2  = IVP_SELNX16I(vecData1, vecData0, 0);
      IVP_MULANX16PACKL(vecResult, vecData2, vecCoeff1);
      vecData2 = IVP_SELNX16I(vecData1, vecData0, 1);
      IVP_MULANX16PACKL(vecResult, vecData2, vecCoeff2);
      *pvecResult++ = vecResult;
      vecData0      = vecData1;
    }

    for (indx = 0; indx < 2; indx++)
    {
      Uptrfiltintemp += (widthExt << 1);
      Uptrcoefftemp  += (3 << 1);
      pc              = (int16_t *) Uptrcoefftemp;
      vecCoeff0       = pc[0];
      vecCoeff1       = pc[1];
      vecCoeff2       = pc[2];

      pvecData1  = (xb_vecNx16 *) Uptrfiltintemp;
      pvecResult = (xb_vecNx16 *) Uptrfiltout;
      vecData0   = *pvecData1++;

      for (ix = 0; ix < (width / IVP_SIMD_WIDTH); ix++)
      {
        vecData1  = *pvecData1++;
        vecResult = *pvecResult;
        IVP_MULANX16PACKL(vecResult, vecData0, vecCoeff0);
        vecData2 = IVP_SELNX16I(vecData1, vecData0, 0);
        IVP_MULANX16PACKL(vecResult, vecData2, vecCoeff1);
        vecData2 = IVP_SELNX16I(vecData1, vecData0, 1);
        IVP_MULANX16PACKL(vecResult, vecData2, vecCoeff2);
        *pvecResult++ = vecResult;
        vecData0      = vecData1;
      }
    }

    Uptrfiltin  += (widthExt << 1);
    Uptrfiltout += (width << 1);
  }
  return;
}

#endif
