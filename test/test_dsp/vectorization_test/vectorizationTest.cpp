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
 * FILE:  vectorizationExamplesMain.c
 *
 * DESCRIPTION:
 *
 *    This file contains test bench for auto vectorised, manually vectorised and
 *un vectorised implementations for functions such as Add Mul 3-tap Filter 5-tap
 *Filter 3X3 Filter
 *
 * *********************************************************************************
 */
#include "commonDef.h"
#include "vectorizationFunctionsDef.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#if defined(__XTENSA__)
#include <xtensa/sim.h>
#endif
#ifdef IVP
#include <xtensa/tie/xt_ivpn.h>
#endif
#include "CppUTest/TestHarness.h"
#include "test_common.h"

// 3x3 filter coefficients
#define FX_MAX 3
#define FY_MAX 3
#define TOLERANCE 1.1
// int16_t ALIGN64 ImageIn0[WIDTH * HEIGHT] _LOCAL_DRAM1_;
// int16_t ALIGN64 ImageIn1[WIDTH * HEIGHT] _LOCAL_DRAM0_;
// int16_t ALIGN64 ImageOut[WIDTH * HEIGHT] _LOCAL_DRAM0_;
// int16_t ALIGN64 ImageOutRef[WIDTH * HEIGHT] _LOCAL_DRAM1_;
#define ALGIN_TO_64(addr) (((uint32_t)addr + 63) & 0xffffffc0)
short ALIGN64 filtercoeff[FY_MAX][FX_MAX] DRAM1_USER = {
    {1, 3, 1}, {5, 12, 5}, {1, 3, 1}};
#ifdef DSP_IP_TEST
TEST_GROUP(VectorizationTest) {

  void setup() {

    int ind1, ind2;
    int16_t *tcm0 = (int16_t *)allocat_memory_from_dram_user(
        0, WIDTH * HEIGHT * sizeof(int16_t) + 64 * 2);
    int16_t *tcm1 = (int16_t *)allocat_memory_from_dram_user(
        1, WIDTH * HEIGHT * sizeof(int16_t) + 64 * 2);
    if(!tcm0||!tcm1)
    {
    	FAIL("FAIL NO TCM");
    	return;
    }

    ImageIn1 = (int16_t *)ALGIN_TO_64(tcm0);
    ImageOut =
        (int16_t *)ALGIN_TO_64(ImageIn1 + WIDTH * HEIGHT * sizeof(int16_t));
    ImageIn0 = (int16_t *)ALGIN_TO_64(tcm1);
    ImageOutRef =
        (int16_t *)ALGIN_TO_64(ImageIn0 + WIDTH * HEIGHT * sizeof(int16_t));
    ;
    // Generate two images
    for (ind1 = 0; ind1 < WIDTH * HEIGHT; ind1++) {
      ImageIn0[ind1] = rand() % 256;
      ImageIn1[ind1] = rand() % 256;
    }
  }
  void teardown() {}
  int16_t *ImageIn0;
  int16_t *ImageIn1;
  int16_t *ImageOut;
  int16_t *ImageOutRef;

};

TEST(VectorizationTest, VectorizationAddTest) {
  USER_DEFINED_HOOKS_STOP();
  int ind1, ind2;
  int check;
  long cycles_start, cycles_stop, cyclesIVP, cyclesAV;
#ifdef FPGA_95P
  float reference_vector_auto = 0.146;
  float reference_vecotr =  0.135;
#else
  float reference_vector_auto = 0.116;
  float reference_vecotr =0.119;
#endif

  //-----------------------------------------------
  // Addition Of Two Vectors Of Length VECTOR_LENGTH
  //-----------------------------------------------
  Add(ImageIn0, ImageIn1, ImageOutRef, VECTOR_LENGTH);

  USER_DEFINED_HOOKS_START();
  TIME_STAMP(cycles_start);
  AddVectorAuto(ImageIn0, ImageIn1, ImageOut, VECTOR_LENGTH);
  TIME_STAMP(cycles_stop);
  USER_DEFINED_HOOKS_STOP();
  cyclesAV = cycles_stop - cycles_start;

  MEMCMP_EQUAL(ImageOut, ImageOutRef, VECTOR_LENGTH * sizeof(int16_t));

  USER_DEFINED_HOOKS_START();
  TIME_STAMP(cycles_start);
  xvAdd(ImageIn0, ImageIn1, ImageOut, VECTOR_LENGTH);
  TIME_STAMP(cycles_stop);
  USER_DEFINED_HOOKS_STOP();
  cyclesIVP = cycles_stop - cycles_start;

  MEMCMP_EQUAL(ImageOut, ImageOutRef, VECTOR_LENGTH * sizeof(int16_t));

//  CHECK_COMPARE_TEXT((float)cyclesIVP / ((float)VECTOR_LENGTH), <=,
//		  reference_vecotr * TOLERANCE,"Vector fail");
//  CHECK_COMPARE_TEXT((float)cyclesAV / ((float)VECTOR_LENGTH), <=,
//		  reference_vector_auto * TOLERANCE,"Auto Vector fail");
}

TEST(VectorizationTest, VectorizationMulTest) {
  USER_DEFINED_HOOKS_STOP();
  int ind1, ind2;
  int check;
  long cycles_start, cycles_stop, cyclesIVP, cyclesAV;

#ifdef FPGA_95P
  float reference_vector_auto = 0.205;
  float reference_vecotr =  0.233;
#else
  float reference_vector_auto = 0.135;
  float reference_vecotr =0.134;
#endif
  //--------------------------------------------
  // Multiplication Of Two Vectors Of Length 256
  //--------------------------------------------
  Mul(ImageIn0, ImageIn1, ImageOutRef, VECTOR_LENGTH);

  USER_DEFINED_HOOKS_START();
  TIME_STAMP(cycles_start);
  MulVectorAuto(ImageIn0, ImageIn1, ImageOut, VECTOR_LENGTH);
  TIME_STAMP(cycles_stop);
  USER_DEFINED_HOOKS_STOP();
  cyclesAV = cycles_stop - cycles_start;

  MEMCMP_EQUAL(ImageOut, ImageOutRef, VECTOR_LENGTH * sizeof(int16_t));

  USER_DEFINED_HOOKS_START();
  TIME_STAMP(cycles_start);
  xvMul(ImageIn0, ImageIn1, ImageOut, VECTOR_LENGTH);
  TIME_STAMP(cycles_stop);
  USER_DEFINED_HOOKS_STOP();
  cyclesIVP = cycles_stop - cycles_start;

  MEMCMP_EQUAL(ImageOut, ImageOutRef, VECTOR_LENGTH * sizeof(int16_t));

//  CHECK_COMPARE_TEXT((float)cyclesIVP / ((float)VECTOR_LENGTH), <=,
//		  reference_vecotr * TOLERANCE,"Vector fail");
//  CHECK_COMPARE_TEXT((float)cyclesAV / ((float)VECTOR_LENGTH), <=,
//		  reference_vector_auto * TOLERANCE,"Auto Vector fail");
}

TEST(VectorizationTest, VectorizationFilt3TapTest) {
  USER_DEFINED_HOOKS_STOP();
  int ind1, ind2;
  int check;
  long cycles_start, cycles_stop, cyclesIVP, cyclesAV;
#ifdef FPGA_95P
  float reference_vector_auto = 0.349;
  float reference_vecotr =  0.347;
#else
  float reference_vector_auto = 0.226;
  float reference_vecotr =0.199;
#endif
  //-----------------------------------------------------------------------------------------------------------
  //										3
  // Tap Filtering
  //-----------------------------------------------------------------------------------------------------------
  Filt3Tap(&filtercoeff[0][0], ImageIn0, ImageOutRef, VECTOR_LENGTH);

  USER_DEFINED_HOOKS_START();
  TIME_STAMP(cycles_start);
  Filt3TapVectorAuto(&filtercoeff[0][0], ImageIn0, ImageOut, VECTOR_LENGTH);
  TIME_STAMP(cycles_stop);
  USER_DEFINED_HOOKS_STOP();
  cyclesAV = cycles_stop - cycles_start;

  MEMCMP_EQUAL(ImageOut, ImageOutRef, VECTOR_LENGTH * sizeof(int16_t));

  USER_DEFINED_HOOKS_START();
  TIME_STAMP(cycles_start);
  xvFilt3Tap(&filtercoeff[0][0], ImageIn0, ImageOut, VECTOR_LENGTH);
  TIME_STAMP(cycles_stop);
  USER_DEFINED_HOOKS_STOP();
  cyclesIVP = cycles_stop - cycles_start;

  MEMCMP_EQUAL(ImageOut, ImageOutRef, VECTOR_LENGTH * sizeof(int16_t));

//  CHECK_COMPARE_TEXT((float)cyclesIVP / ((float)VECTOR_LENGTH), <=,
//		  reference_vecotr * TOLERANCE,"Vector fail");
//  CHECK_COMPARE_TEXT((float)cyclesAV / ((float)VECTOR_LENGTH), <=,
//		  reference_vector_auto * TOLERANCE,"Auto Vector fail");
}

TEST(VectorizationTest, VectorizationFilt5TapTest) {
  USER_DEFINED_HOOKS_STOP();
  int ind1, ind2;
  int check;
  long cycles_start, cycles_stop, cyclesIVP, cyclesAV;
#ifdef FPGA_95P
  float reference_vector_auto = 0.632;
  float reference_vecotr =  0.567;
#else
  float reference_vector_auto = 0.344;
  float reference_vecotr =0.317;
#endif
  //-----------------------------------------------------------------------------------------------------------
  //										3/5
  // Tap Filtering
  //-----------------------------------------------------------------------------------------------------------
  Filt5Tap(&filtercoeff[0][0], &ImageIn0[256], ImageOutRef, VECTOR_LENGTH);

  USER_DEFINED_HOOKS_START();
  TIME_STAMP(cycles_start);
  Filt5TapVectorAuto(&filtercoeff[0][0], &ImageIn0[256], ImageOut,
                     VECTOR_LENGTH);
  TIME_STAMP(cycles_stop);
  USER_DEFINED_HOOKS_STOP();
  cyclesAV = cycles_stop - cycles_start;

  MEMCMP_EQUAL(ImageOut, ImageOutRef, VECTOR_LENGTH * sizeof(int16_t));

  USER_DEFINED_HOOKS_START();
  TIME_STAMP(cycles_start);
  xvFilt5Tap(&filtercoeff[0][0], &ImageIn0[256], ImageOut, VECTOR_LENGTH);
  TIME_STAMP(cycles_stop);
  USER_DEFINED_HOOKS_STOP();
  cyclesIVP = cycles_stop - cycles_start;

  MEMCMP_EQUAL(ImageOut, ImageOutRef, VECTOR_LENGTH * sizeof(int16_t));

//  CHECK_COMPARE_TEXT((float)cyclesIVP / ((float)VECTOR_LENGTH), <=,
//		  reference_vecotr * TOLERANCE,"Vector fail");
//  CHECK_COMPARE_TEXT((float)cyclesAV / ((float)VECTOR_LENGTH), <=,
//		  reference_vector_auto * TOLERANCE,"Auto Vector fail");
}

TEST(VectorizationTest, VectorizationFilt3X3TapTest) {
  USER_DEFINED_HOOKS_STOP();
  int ind1, ind2;
  int check;
  long cycles_start, cycles_stop, cyclesIVP, cyclesAV;
#ifdef FPGA_95P
  float reference_vector_auto = 0.434;
  float reference_vecotr =  0.457;
#else
  float reference_vector_auto = 0.407;
  float reference_vecotr =0.383;
#endif
  //-----------------------------------------------------------------------------------------------------------
  //										3
  // X 3 Filter
  //-----------------------------------------------------------------------------------------------------------
  Filter3x3(ImageIn0, ImageOutRef, VECTOR_LENGTH, WIDTH, 16);

  USER_DEFINED_HOOKS_START();
  TIME_STAMP(cycles_start);
  Filt3x3VectorAuto(ImageIn0, ImageOut, VECTOR_LENGTH, WIDTH, 16);
  TIME_STAMP(cycles_stop);
  USER_DEFINED_HOOKS_STOP();
  cyclesAV = cycles_stop - cycles_start;
  check = 1;
  for (ind2 = 0; ind2 < 16; ind2++) {
    for (ind1 = 0; ind1 < VECTOR_LENGTH; ind1++) {
      if (ImageOut[ind2 * VECTOR_LENGTH + ind1] !=
          ImageOutRef[ind2 * VECTOR_LENGTH + ind1]) {
        check = 0;
      }
    }
  }

  USER_DEFINED_HOOKS_START();
  TIME_STAMP(cycles_start);
  xvFilter3x3(ImageIn0, ImageOut, VECTOR_LENGTH, WIDTH, 16);
  TIME_STAMP(cycles_stop);
  USER_DEFINED_HOOKS_STOP();
  cyclesIVP = cycles_stop - cycles_start;
  for (ind2 = 0; ind2 < 16; ind2++) {
    for (ind1 = 0; ind1 < VECTOR_LENGTH; ind1++) {
      if (ImageOut[ind2 * VECTOR_LENGTH + ind1] !=
          ImageOutRef[ind2 * VECTOR_LENGTH + ind1]) {
        check = 0;
      }
    }
  }
  CHECK_EQUAL(1, check);
//  if (check == 1) {
//    printf("\nautoVectorizationExamples\txvFilt3x3(ManualVectorized)      "
//           "\t%f\tCPP\tPASS\n",
//           (float)cyclesIVP / ((float)(VECTOR_LENGTH * 16)));
//    printf("\nautoVectorizationExamples\tFilt3x3VectorAuto(AutoVectorized)\t%"
//           "f\tCPP\tPASS\n",
//           (float)cyclesAV / ((float)(VECTOR_LENGTH * 16)));
//  } else {
//    printf("\nautoVectorizationExamples\txvFilt3x3(ManualVectorized)      "
//           "\t%f\tCPP\tFAIL\n",
//           (float)cyclesIVP / ((float)(VECTOR_LENGTH * 16)));
//    printf("\nautoVectorizationExamples\tFilt3x3VectorAuto(AutoVectorized)\t%"
//           "f\tCPP\tFAIL\n",
//           (float)cyclesAV / ((float)(VECTOR_LENGTH * 16)));
//  }

//  CHECK_COMPARE_TEXT((float)cyclesIVP / ((float)VECTOR_LENGTH*16), <=,
//		  reference_vecotr * TOLERANCE,"Vector fail");
//  CHECK_COMPARE_TEXT((float)cyclesAV / ((float)VECTOR_LENGTH*16), <=,
//		  reference_vector_auto * TOLERANCE,"Auto Vector fail");
}
#endif
