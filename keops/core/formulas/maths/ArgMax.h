#pragma once

#include <assert.h>

#include "core/formulas/constants/Zero.h"
#include "core/autodiff/UnaryOp.h"

#include "core/pre_headers.h"
namespace keops {

//////////////////////////////////////////////////////////////
////                 ARGMAX : ArgMax< F >                 ////
//////////////////////////////////////////////////////////////

template<class F>
struct ArgMax : UnaryOp<ArgMax, F> {

  static_assert(F::DIM >= 1, "[KeOps] ArgMax operation is only possible when dimension is non zero.");

  static const int DIM = 1;

  static void PrintIdString(::std::stringstream &str) {
    str << "ArgMax";
  }

  static DEVICE INLINE
  void Operation(__TYPE__ *out, __TYPE__ *outF) {
#if USE_HALF && GPU_ON
    *out = __float2half2_rn(0.0f);
    __TYPE__ tmp = outF[0];
#pragma unroll
    for (int k = 1; k < F::DIM; k++) {
      // we have to work element-wise...
      __half2 cond = __hlt2(tmp,outF[k]);                  // cond = (tmp < outF[k]) (element-wise)
      __half2 negcond = __float2half2_rn(1.0f)-cond;       // negcond = 1-cond
      *out = cond * __float2half2_rn(k) + negcond * *out;  // out  = cond * k + (1-cond) * out 
      tmp = cond * outF[k] + negcond * tmp;                // tmp  = cond * outF[k] + (1-cond) * tmp
    }
#elif USE_HALF
// this should never be used...
#else
    *out = 0.0;
    __TYPE__ tmp = outF[0];
#pragma unroll
    for (int k = 1; k < F::DIM; k++)
      if (outF[k] > tmp) {
      	tmp = outF[k];
	*out = k;
      }
#endif
  }

  template<class V, class GRADIN>
  using DiffT = Zero<V::DIM>;

};

#define ArgMax(p) KeopsNS<ArgMax<decltype(InvKeopsNS(p))>>()

}
