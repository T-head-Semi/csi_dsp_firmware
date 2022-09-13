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

#ifndef __VECTORIZATIONFUNCTIONSDEF__
#define __VECTORIZATIONFUNCTIONSDEF__

//--------------------------------
// Reference Implementations
//--------------------------------
#define VECTOR_LENGTH  1536
#define WIDTH          (VECTOR_LENGTH + 32)
#define HEIGHT         18

#ifdef __cplusplus

extern "C" {
#endif
void Add(short *vec1, short *vec2, short *sum, int len);
void Mul(short *vec1, short *vec2, short *sum, int len);
void Filt3Tap(short *coeff, short *vecin, short *vecout, int veclen);
void Filt5Tap(short *coeff, short *vecin, short *vecout, int veclen);
void Filter3x3(short *filtin, short *filtout, int width, int widthExt, int height);

//--------------------------------
// Auto Vectorization version
//--------------------------------

void AddVectorAuto(short *pvec1, short *pvec2, short *psum, int veclen);
void MulVectorAuto(short *pvec1, short *pvec2, short *psum, int veclen);
void Add200VectorAuto(short *pvec1, short *pvec2, short *psum, int veclen);
void Filt3TapVectorAuto(short *pcoeff, short *pvecin, short *pvecout, int veclen);
void Filt5TapVectorAuto(short *pcoeff, short *pvecin, short *pvecout, int veclen);
void Filt3x3VectorAuto(short *filtin, short *filtout, int width, int widthExt, int height);

//--------------------------------
// IVP32 Implementations
//--------------------------------

void xvAdd(short *pvec1, short *pvec2, short *psum, int veclen);
void xvMul(short *pvec1, short *pvec2, short *psum, int veclen);
void xvFilt3Tap(short *pcoeff, short *pvecin, short *pvecout, int veclen);
void xvFilt5Tap(short *pcoeff, short *pvecin, short *pvecout, int veclen);
void xvFilter3x3(short *filtin, short *filtout, int width, int widthExt, int height);


#ifdef __cplusplus

}
#endif
#endif /*__VECTORIZATIONFUNCTIONSDEF__ */
