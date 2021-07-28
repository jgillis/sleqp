#ifndef SLEQP_SPARSE_FACTORIZATION_H
#define SLEQP_SPARSE_FACTORIZATION_H

#ifdef __cplusplus
extern "C" {
#endif

#include "params.h"
#include "timer.h"
#include "types.h"
#include "sparse_factorization_types.h"

  SLEQP_NODISCARD
  SLEQP_RETCODE sleqp_sparse_factorization_create(SleqpSparseFactorization** star,
                                                  const char* name,
                                                  const char* version,
                                                  SleqpParams* params,
                                                  SleqpSparseFactorizationCallbacks* callbacks,
                                                  void* factorization_data);

  const char* sleqp_sparse_factorization_get_name(SleqpSparseFactorization* sparse_factorization);

  const char* sleqp_sparse_factorization_get_version(SleqpSparseFactorization* sparse_factorization);

  SLEQP_NODISCARD
  SLEQP_RETCODE sleqp_sparse_factorization_create_default(SleqpSparseFactorization** star,
                                                          SleqpParams* params);

  SLEQP_NODISCARD
  SLEQP_RETCODE sleqp_sparse_factorization_capture(SleqpSparseFactorization* sparse_factorization);

  SLEQP_NODISCARD
  SLEQP_RETCODE sleqp_sparse_factorization_set_matrix(SleqpSparseFactorization* sparse_factorization,
                                                      SleqpSparseMatrix* matrix);

  SLEQP_NODISCARD
  SLEQP_RETCODE sleqp_sparse_factorization_solve(SleqpSparseFactorization* sparse_factorization,
                                                 SleqpSparseVec* rhs);

  SLEQP_NODISCARD
  SLEQP_RETCODE sleqp_sparse_factorization_get_sol(SleqpSparseFactorization* sparse_factorization,
                                                   SleqpSparseVec* sol,
                                                   int begin,
                                                   int end,
                                                   double zero_eps);

  SLEQP_NODISCARD
  SLEQP_RETCODE sleqp_sparse_factorization_get_condition_estimate(SleqpSparseFactorization* sparse_factorization,
                                                                  double* condition_estimate);

  SLEQP_NODISCARD
  SLEQP_RETCODE sleqp_sparse_factorization_capture(SleqpSparseFactorization* sparse_factorization);

  SLEQP_NODISCARD
  SLEQP_RETCODE sleqp_sparse_factorization_release(SleqpSparseFactorization** star);

#ifdef __cplusplus
}
#endif

#endif /* SLEQP_SPARSE_FACTORIZATION_H */
