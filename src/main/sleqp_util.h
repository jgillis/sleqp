#ifndef SLEQP_UTIL_H
#define SLEQP_UTIL_H

/**
 * @file sleqp_util.h
 * @brief Definition of utility functions.
 **/

#include "sleqp_func.h"
#include "sleqp_iterate.h"
#include "sleqp_problem.h"
#include "sleqp_types.h"

#include "sparse/sleqp_sparse_matrix.h"
#include "sparse/sleqp_sparse_vec.h"

#ifdef __cplusplus
extern "C" {
#endif

  SLEQP_RETCODE sleqp_set_and_evaluate(SleqpProblem* problem,
                                       SleqpIterate* iterate);

  SLEQP_RETCODE sleqp_get_violated_multipliers(SleqpProblem* problem,
                                               SleqpSparseVec* x,
                                               SleqpSparseVec* cons_vals,
                                               double penalty_parameter,
                                               SleqpSparseVec* multipliers,
                                               SleqpWorkingSet* working_set,
                                               double eps);

  SLEQP_RETCODE sleqp_max_step_length(SleqpSparseVec* x,
                                      SleqpSparseVec* direction,
                                      SleqpSparseVec* var_lb,
                                      SleqpSparseVec* var_ub,
                                      double* max_step_length);

  SLEQP_RETCODE sleqp_get_violation(SleqpProblem* problem,
                                    SleqpIterate* iterate,
                                    double eps,
                                    SleqpSparseVec* violation);

#ifdef __cplusplus
}
#endif

#endif /* SLEQP_UTIL_H */
