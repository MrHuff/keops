#pragma once

#include <sstream>

#include "core/autodiff/UnaryOp.h"
#include "core/reductions/Reduction.h"
#include "core/reductions/Zero_Reduction.h"
#include "core/pre_headers.h"
#include "core/utils/Infinity.h"

namespace keops {
/////////////////////////////////////////////////////////////////////////
//          min+argmin reduction : base class                          //
/////////////////////////////////////////////////////////////////////////
template < class F, int tagI=0 >
struct Min_ArgMin_Reduction_Base : public Reduction<F,tagI> {

    // We work with a (values,indices) vector
	static const int DIMRED = 2*F::DIM;	// dimension of temporary variable for reduction
		
		template < typename TYPE >
		struct InitializeReduction {
			DEVICE INLINE void operator()(TYPE *tmp) {
				VectAssign<F::DIM>(tmp, cast_to<TYPE>(PLUS_INFINITY<TYPE>::value));
				VectAssign<F::DIM>(tmp + F::DIM, cast_to<TYPE>(0.0f));
			}
		};

template < typename TYPEACC, typename TYPE >
		struct ReducePairScalar {
			DEVICE INLINE void operator()(TYPEACC *tmp, TYPE xi, TYPE val) {
					if(xi<*tmp) {
						*tmp = xi;
						*(tmp+F::DIM) = val;
					}
				}
			};

#if USE_HALF && GPU_ON
template < typename TYPEACC >
	struct ReducePairScalar<TYPEACC, half2 > {
			DEVICE INLINE void operator()(TYPEACC *tmp, half2 xi, half2 val) {
					half2 cond = __hlt2(xi,*tmp);
					half2 negcond = (half2)cast_to<half2>(1.0f)-cond;
					*tmp = cond * xi + negcond * *tmp;
					*(tmp+F::DIM) = cond * val + negcond * *(tmp+F::DIM);
				}
			};
#endif

		// equivalent of the += operation
		template < typename TYPEACC, typename TYPE >
		struct ReducePairShort {
			DEVICE INLINE void operator()(TYPEACC *tmp, TYPE *xi, TYPE val) {
				VectApply<ReducePairScalar<TYPEACC,TYPE>,F::DIM>(tmp,xi,val);
			}
		};
        
		// equivalent of the += operation
		template < typename TYPEACC, typename TYPE >
		struct ReducePair {
			DEVICE INLINE void operator()(TYPEACC *tmp, TYPE *xi) {
#pragma unroll
				for(int k=0; k<F::DIM; k++) {
#if USE_HALF && GPU_ON
					__half2 cond = __hlt2(xi[k],tmp[k]);
					__half2 negcond = __float2half2_rn(1.0f)-cond;
					tmp[k] = cond * xi[k] + negcond * tmp[k];
					tmp[F::DIM+k] = cond * xi[F::DIM+k] + negcond * tmp[F::DIM+k];
#elif USE_HALF
#else
					if(xi[k]<tmp[k]) {
						tmp[k] = xi[k];
						tmp[F::DIM+k] = xi[F::DIM+k];
					}
#endif
				}
			}
		};
        
};

#define Min_ArgMin_Reduction(F,I) KeopsNS<Min_ArgMin_Reduction<decltype(InvKeopsNS(F)),I>>()

// Implements the min+argmin reduction operation : for each i or each j, find the minimal value of Fij and its index
// operation is vectorized: if Fij is vector-valued, min+argmin is computed for each dimension.

template < class F, int tagI=0 >
struct Min_ArgMin_Reduction : public Min_ArgMin_Reduction_Base<F,tagI>, UnaryOp<Min_ArgMin_Reduction,F,tagI> {

        static const int DIM = 2*F::DIM;		// DIM is dimension of output of convolution ; for a min-argmin reduction it is equal to 2 times the dimension of output of formula
		
    static void PrintIdString(::std::stringstream& str) {
        str << "Min_ArgMin_Reduction";
    }
        
    template < typename TYPEACC, typename TYPE >
    struct FinalizeOutput {
        DEVICE INLINE void operator()(TYPEACC *acc, TYPE *out, TYPE **px, int i) {
            for(int k=0; k<DIM; k++)
                out[k] = acc[k];
        }
    };

    // no gradient implemented here

};

// Implements the argmin reduction operation : for each i or each j, find the index of the
// minimal value of Fij
// operation is vectorized: if Fij is vector-valued, argmin is computed for each dimension.

template < class F, int tagI=0 >
struct ArgMin_Reduction : public Min_ArgMin_Reduction_Base<F,tagI>, UnaryOp<ArgMin_Reduction,F,tagI> {
        
        static const int DIM = F::DIM;		// DIM is dimension of output of convolution ; for a argmin reduction it is equal to the dimension of output of formula
		
    static void PrintIdString(::std::stringstream& str) {
        str << "ArgMin_Reduction";
    }

    template < typename TYPEACC, typename TYPE >
    struct FinalizeOutput {
        DEVICE INLINE void operator()(TYPEACC *acc, TYPE *out, TYPE **px, int i) {
#pragma unroll
            for(int k=0; k<F::DIM; k++)
                out[k] = acc[F::DIM+k];
        }
    };

    template < class V, class GRADIN >
    using DiffT = Zero_Reduction<V::DIM,(V::CAT)%2>;
    // remark : if V::CAT is 2 (parameter), we will get tagI=(V::CAT)%2=0, so we will do reduction wrt j.
    // In this case there is a summation left to be done by the user.

};

// Implements the min reduction operation : for each i or each j, find the
// minimal value of Fij
// operation is vectorized: if Fij is vector-valued, min is computed for each dimension.

template < class F, int tagI=0 >
struct Min_Reduction : public Min_ArgMin_Reduction_Base<F,tagI>, UnaryOp<Min_Reduction,F,tagI> {
        
        static const int DIM = F::DIM;		// DIM is dimension of output of convolution ; for a min reduction it is equal to the dimension of output of formula
		
    static void PrintIdString(::std::stringstream& str) {
        str << "Min_Reduction";
    }

    template < typename TYPEACC, typename TYPE >
    struct FinalizeOutput {
        DEVICE INLINE void operator()(TYPEACC *acc, TYPE *out, TYPE **px, int i) {
#pragma unroll
            for(int k=0; k<F::DIM; k++)
                out[k] = acc[k];
        }
    };

    // no gradient implemented here

};
#define ArgMin_Reduction(F,I) KeopsNS<ArgMin_Reduction<decltype(InvKeopsNS(F)),I>>()
#define Min_Reduction(F,I) KeopsNS<Min_Reduction<decltype(InvKeopsNS(F)),I>>()
#define Min_ArgMin_Reduction(F,I) KeopsNS<Min_ArgMin_Reduction<decltype(InvKeopsNS(F)),I>>()

}
