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
#ifdef IVP
#include <xtensa/tie/xt_ivpn.h>
#endif
#include <stdint.h>
#if defined(__XTENSA__)
#include <sys/times.h>
#include <xtensa/sim.h>
#endif
#include "commonDef.h"

// 3x3 filter coefficients
#define FX_MAX  3
#define FY_MAX  3
extern int16_t filtercoeff[FX_MAX][FY_MAX];

/* ********************************************************************
*	FUNCTION:AddVectorAuto
*	DESCRIPTION:AutoVectorised Function
*				This function find the addition of two short numbers.
*	INPUTS:
                int16_t *pvec1	Input Vector1
                int16_t *pvec2  Input Vector2
                int32_t veclen  Length of the Input Vectors
*	OUTPUTS:
                int16_t *psum	Output Vector
**********************************************************************/

void AddVectorAuto(int16_t *pvec1, int16_t *pvec2, int16_t *psum, int32_t veclen)
{
  int16_t(*__restrict pv1) = pvec1;
  int16_t(*__restrict pv2) = pvec2;
  int16_t(*__restrict ps)  = psum;
  int32_t indx;
#pragma aligned (pv1, 64)    // indicating arrays (pointers) are all aligned to 64bytes.
#pragma aligned (pv2, 64)    // this will improve compiler's auto vectorization.
#pragma aligned (ps, 64)     // see section 4.7.2 of Xtensa C/C++ Compiler User's Guide.
  for (indx = 0; indx < veclen; indx++)
  {
    ps[indx] = pv1[indx] + pv2[indx];
  }
  return;
}

/* ********************************************************************
*	FUNCTION: MulVectorAuto
*	DESCRIPTION:AutoVectorised Function
*				This function find the multiplication of two short numbers.
*	INPUTS:
                int16_t *pvec1	Input Vector1
                int16_t *pvec2  Input Vector2
                int32_t veclen  Length of the Input Vectors
*	OUTPUTS:
                int16_t *psum	Output Vector
**********************************************************************/
void MulVectorAuto(int16_t *pvec1, int16_t *pvec2, int16_t *psum, int32_t veclen)
{
  int16_t(*__restrict pv1) = pvec1;
  int16_t(*__restrict pv2) = pvec2;
  int16_t(*__restrict ps)  = psum;
  int32_t indx;
#pragma aligned (pv1, 64)    // indicating arrays (pointers) are all aligned to 64bytes.
#pragma aligned (pv2, 64)    // this will improve compiler's auto vectorization.
#pragma aligned (ps, 64)     // see section 4.7.2 of Xtensa C/C++ Compiler User's Guide.

  for (indx = 0; indx < veclen; indx++)
  {
    ps[indx] = pv1[indx] * pv2[indx];
  }
  return;
}

/* ********************************************************************
*	FUNCTION: Filt3TapVectorAuto
*	DESCRIPTION:AutoVectorised Function
*				This function performs the 3 tap filtering operation.
*	INPUTS:
                int16_t *pcoeff		 Filter coefficients
                int16_t *pvecin      Input Vector
                int32_t  veclen      Vector length
*	OUTPUTS:
                int16_t *pvecout	 Output Vector
**********************************************************************/
void Filt3TapVectorAuto(int16_t *pcoeff, int16_t *pvecin, int16_t *pvecout, int32_t veclen)
{
  int16_t(*__restrict pc) = pcoeff;
  int16_t(*__restrict pv) = pvecin;
  int16_t(*__restrict pf) = pvecout;
  int16_t temp;
  int32_t indx;
#pragma aligned (pv, 64)     // this will improve compiler's auto vectorization.
#pragma aligned (pf, 64)     // see section 4.7.2 of Xtensa C/C++ Compiler User's Guide.
  for (indx = 0; indx < veclen; indx++)
  {
    temp     = (pv[indx] * pc[0]);
    temp    += (pv[indx + 1] * pc[1]);
    temp    += (pv[indx + 2] * pc[2]);
    pf[indx] = temp;
  }
  return;
}

/* ********************************************************************
*	FUNCTION: Filt5TapVectorAuto
*	DESCRIPTION:AutoVectorised Function
*				This function performs the 5 tap filtering operation.
*	INPUTS:
                int16_t *pcoeff		Filter coefficients
                int16_t *pvecin     Input Vector
                int32_t  veclen     Vector length
*	OUTPUTS:
                int16_t *pvecout	Output Vector
**********************************************************************/
void Filt5TapVectorAuto(int16_t *pcoeff, int16_t *pvecin, int16_t *pvecout, int32_t veclen)
{
  int16_t(*__restrict pc) = pcoeff;
  int16_t(*__restrict pv) = pvecin;
  int16_t(*__restrict pf) = pvecout;
  int16_t temp;
  int32_t indx;
#pragma aligned (pv, 64)     // this will improve compiler's auto vectorization.
#pragma aligned (pf, 64)     // see section 4.7.2 of Xtensa C/C++ Compiler User's Guide.
  for (indx = 0; indx < veclen; indx++)
  {
    temp     = (pv[indx - 2] * pc[0]);
    temp    += (pv[indx - 1] * pc[1]);
    temp    += (pv[indx] * pc[2]);
    temp    += (pv[indx + 1] * pc[3]);
    temp    += (pv[indx + 2] * pc[4]);
    pf[indx] = temp;
  }
  return;
}

/* ********************************************************************
*	FUNCTION: Filt3x3VectorAuto
*	DESCRIPTION:AutoVectorised Function
*				This function performs the 3x3 filtering operation.
*	INPUTS:
                int16_t *filtin
                int32_t  width
                int32_t  widthExt
                int32_t	 height
*	OUTPUTS:
                int16_t *filtout	Output
**********************************************************************/
void Filt3x3VectorAuto(int16_t *filtin, int16_t *filtout, int32_t width, int32_t widthExt, int32_t height)
{
  int32_t ix, iy, indx;
  int16_t(*__restrict pc);
  int16_t(*__restrict pv);
  int16_t(*__restrict pf);
  int16_t temp16;

  iy = 0;

  for (iy = 0; iy < height; iy++)
  {
    pv = (int16_t *) &filtin[iy * widthExt];
    pc = (int16_t *) &filtercoeff[0][0];
    pf = (int16_t *) &filtout[iy * width];;
        #pragma aligned (pv, 64)     // this will improve compiler's auto vectorization.
        #pragma aligned (pf, 64)     // see section 4.7.2 of Xtensa C/C++ Compiler User's Guide.

    for (ix = 0; ix < width; ix++)
    {
      temp16  = (pv[ix] * pc[0]);
      temp16 += (pv[ix + 1] * pc[1]);
      temp16 += (pv[ix + 2] * pc[2]);
      pf[ix]  = temp16;
    }

    for (indx = 0; indx < 2; indx++)
    {
      pv = (int16_t *) &filtin[(iy + indx + 1) * widthExt];
      pc = (int16_t *) &filtercoeff[indx + 1][0];
#pragma aligned (pv, 64)     // this will improve compiler's auto vectorization.
#pragma aligned (pf, 64)     // see section 4.7.2 of Xtensa C/C++ Compiler User's Guide.
      for (ix = 0; ix < width; ix++)
      {
        temp16  = pf[ix];
        temp16 += (pv[ix] * pc[0]);
        temp16 += (pv[ix + 1] * pc[1]);
        temp16 += (pv[ix + 2] * pc[2]);
        pf[ix]  = temp16;
      }
    }
  }
  return;
}

