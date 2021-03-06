#pragma once

#include <sstream>
#include <assert.h>

#include "core/autodiff/BinaryOp.h"
#include "core/formulas/maths/Add.h"
#include "core/formulas/maths/TensorProd.h"
#include "core/formulas/maths/VecMatMult.h"

#include "core/pre_headers.h"

namespace keops {


/////////////////////////////////////////////////////////////////////////
////      Matrix-vector product      A x b                           ////
/////////////////////////////////////////////////////////////////////////

template<class A, class B>
struct MatVecMult: BinaryOp<MatVecMult, A, B> {
  // A is vector of size n*p, interpreted as matrix, B is vector of size p, interpreted as column vector
  // output is vector of size n

  static_assert(A::DIM % B::DIM == 0, "Dimensions of A and B are not compatible for matrix-vector product");

  static const int DIM = A::DIM / B::DIM;

  static void PrintIdString(::std::stringstream &str) {
    str << "x";
  }
#if C_CONTIGUOUS //row major
  static DEVICE INLINE void Operation(__TYPE__ *out, __TYPE__ *inA, __TYPE__ *inB) {
        int q = 0;
#pragma unroll
        for (int i = 0; i < DIM; i++) {
#if USE_HALF
#if GPU_ON
            out[i] = __float2half2_rn(0.0f);
#pragma unroll
            for (int k = 0; k < B::DIM; k++, q++)
                out[i] =  __hfma2(inA[q], inB[k], out[i]);
#endif
#else
            out[i] = 0.0f;
#pragma unroll
            for (int k = 0; k < B::DIM; k++, q++)
                out[i] += inA[q] * inB[k];
#endif
        }
    }
#else // column major
  static DEVICE INLINE void Operation(__TYPE__ *out, __TYPE__ *inA, __TYPE__ *inB) {
#pragma unroll
    for (int i = 0; i < DIM; i++) {
#if USE_HALF && GPU_ON
      out[i] = __float2half2_rn(0.0f);
#elif USE_HALF
#else
      out[i] = 0.0f;
#endif
#pragma unroll
      for (int k = 0; k < B::DIM; k++)
#if USE_HALF
#if GPU_ON
        out[i] = __hfma2(inA[k * DIM + i], inB[k], out[i]);
#else
#endif
#else
        out[i] += inA[k * DIM + i] * inB[k];
#endif
    }
  }
#endif

  template<class V, class GRADIN>
  using DiffTA = typename A::template DiffT<V, GRADIN>;

  template<class V, class GRADIN>
  using DiffTB = typename B::template DiffT<V, GRADIN>;

  template<class V, class GRADIN>
  using DiffT = Add<DiffTA<V, TensorProd<GRADIN, B>>, DiffTB<V, VecMatMult<GRADIN, A>>>;

};

#define MatVecMult(f,g) KeopsNS<MatVecMult<decltype(InvKeopsNS(f)),decltype(InvKeopsNS(g))>>()

}
