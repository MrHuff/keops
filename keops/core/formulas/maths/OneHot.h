#pragma once

#include <sstream>
#include <assert.h>

#include "core/formulas/constants/Zero.h"
#include "core/autodiff/UnaryOp.h"
#include "core/pre_headers.h"

namespace keops {

//////////////////////////////////////////////////////////////
////       ONE-HOT REPRESENTATION : OneHot<F,DIM>         ////
//////////////////////////////////////////////////////////////

template< class F, int DIM_ >
struct OneHot : UnaryOp< OneHot, F, DIM_ > {
  static const int DIM = DIM_;

  static_assert(F::DIM == 1, "One-hot representation is only supported for scalar formulas.");
  static_assert(DIM_ >= 1, "A one-hot vector should have length >= 1.");

  static void PrintIdString(::std::stringstream &str) {
    str << "OneHot";
  }

  // N.B.: This may not be the most efficient implementation,
  //       with unnecessary casts, etc.
  static HOST_DEVICE INLINE
  void Operation(__TYPE__ *out, __TYPE__ *outF) {
#if USE_HALF && GPU_ON
#pragma unroll
    for (int k = 0; k < DIM; k++)
      out[k] = __heq2(h2rint(outF[0]),__float2half2_rn(k));
#elif USE_HALF
/*
#pragma unroll
    for (int k = 0; k < DIM; k++)
      out[k] = (round((float)outF[0]) == k) ? 1 : 0 ;
*/
#else
#pragma unroll
    for (int k = 0; k < DIM; k++)
      out[k] = 0.0;
    out[(int)(outF[0]+.5)] = 1.0;
#endif
  }

  // There is no gradient to accumulate on V, whatever V.
  template < class V, class GRADIN >
  using DiffT = Zero<V::DIM>;
};

#define OneHot(f,n) KeopsNS<OneHot<decltype(InvKeopsNS(f)),n>>()

}
