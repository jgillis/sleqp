#ifndef TR_UTIL_H
#define TR_UTIL_H

#include "params.h"
#include "sparse/sparse_vec.h"

#ifdef __cplusplus
extern "C" {
#endif

  SLEQP_RETCODE sleqp_tr_compute_bdry_sol(const SleqpSparseVec* previous,
                                          const SleqpSparseVec* direction,
                                          SleqpParams* params,
                                          double radius,
                                          SleqpSparseVec* result);

#ifdef __cplusplus
}
#endif

#endif /* TR_UTIL_H */