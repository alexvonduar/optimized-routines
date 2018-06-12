/*
 * Double-precision 2^x function.
 *
 * Copyright (c) 2018, Arm Limited.
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <math.h>
#include <stdint.h>
#include "math_config.h"

#define N (1 << EXP_TABLE_BITS)
#define Shift __exp_data.exp2_shift
#define T __exp_data.tab
#define C1 __exp_data.exp2_poly[0]
#define C2 __exp_data.exp2_poly[1]
#define C3 __exp_data.exp2_poly[2]
#define C4 __exp_data.exp2_poly[3]
#define C5 __exp_data.exp2_poly[4]
#define C6 __exp_data.exp2_poly[5]

static inline double
specialcase (double_t tmp, uint64_t sbits, uint64_t ki)
{
  double_t scale, y;

  if ((ki & 0x80000000) == 0)
    {
      /* k > 0, the exponent of scale might have overflowed by <= 85.  */
      sbits -= 97ull << 52;
      scale = asdouble (sbits);
      y = 0x1p97 * (scale + scale * tmp);
      return check_oflow (y);
    }
  /* k < 0, need special care in the subnormal range.  */
  sbits += 1022ull << 52;
  scale = asdouble (sbits);
  y = scale + scale * tmp;
  if (y < 1.0)
    {
      /* Round y to the right precision before scaling it into the subnormal
	 range to avoid double rounding that can cause 0.5+E/2 ulp error where
	 E is the worst-case ulp error outside the subnormal range.  So this
	 is only useful if the goal is better than 1 ulp worst-case error.  */
      double_t hi, lo;
      lo = scale - y + scale * tmp;
      hi = 1.0 + y;
      lo = 1.0 - hi + y + lo;
      y = eval_as_double (hi + lo) - 1.0;
      /* Avoid -0.0 with downward rounding.  */
      if (WANT_ROUNDING && y == 0.0)
	y = 0.0;
      /* The underflow exception needs to be signaled explicitly.  */
      force_eval_double (0x1p-1022 * 0x1p-1022);
    }
  y = 0x1p-1022 * y;
  return check_uflow (y);
}

static inline uint32_t
top16 (double x)
{
  return asuint64 (x) >> 48;
}

double
exp2 (double x)
{
  uint32_t abstop;
  uint64_t ki, idx, top, sbits;
  /* double_t for better performance on targets with FLT_EVAL_METHOD==2.  */
  double_t kd, r, r2, scale, tail, tmp;

  abstop = top16 (x) & 0x7fff;
  if (unlikely (abstop - top16 (0x1p-54) >= top16 (992.0) - top16 (0x1p-54)))
    {
      if (abstop - top16 (0x1p-54) >= 0x80000000)
	/* Avoid spurious underflow for tiny x.  */
	/* Note: 0 is common input.  */
	return WANT_ROUNDING ? 1.0 + x : 1.0;
      if (abstop >= top16 (1088.0))
	{
	  if (asuint64 (x) == asuint64 (-INFINITY))
	    return 0.0;
	  if (abstop >= top16 (INFINITY))
	    return 1.0 + x;
	  if (asuint64 (x) >> 63)
	    return __math_uflow (0);
	  else
	    return __math_oflow (0);
	}
      /* Large x is special cased below.  */
      abstop = 0;
    }

  /* exp2(x) = 2^(k/N) * 2^r, with 2^r in [2^(-1/2N),2^(1/2N)].  */
  /* x = k/N + r, with int k and r in [-1/2N, 1/2N].  */
  kd = eval_as_double (x + Shift);
  ki = asuint64 (kd); /* k.  */
  kd -= Shift; /* k/N for int k.  */
  r = x - kd;
  /* 2^(k/N) ~= scale * (1 + tail).  */
  idx = 2*(ki % N);
  top = ki << (52 - EXP_TABLE_BITS);
  tail = asdouble (T[idx]);
  /* This is only a valid scale when -1023*N < k < 1024*N.  */
  sbits = T[idx + 1] + top;
  /* exp2(x) = 2^(k/N) * 2^r ~= scale + scale * (tail + 2^r - 1).  */
  /* Evaluation is optimized assuming superscalar pipelined execution.  */
  r2 = r*r;
  /* Without fma the worst case error is 0.5/N ulp larger.  */
  /* Worst case error is less than 0.5+0.86/N+(abs poly error * 2^53) ulp.  */
#if EXP2_POLY_ORDER == 4
  tmp = tail + r*C1 + r2*C2 + r*r2*(C3 + r*C4);
#elif EXP2_POLY_ORDER == 5
  tmp = tail + r*C1 + r2*(C2 + r*C3) + r2*r2*(C4 + r*C5);
#elif EXP2_POLY_ORDER == 6
  tmp = tail + r*C1 + r2*(0.5 + r*C3) + r2*r2*(C4 + r*C5 + r2*C6);
#endif
  if (unlikely (abstop == 0))
    return specialcase (tmp, sbits, ki);
  scale = asdouble (sbits);
  return scale + scale * tmp;
}
#if USE_GLIBC_ABI
strong_alias (exp2, __exp2_finite)
hidden_alias (exp2, __ieee754_exp2)
#endif
