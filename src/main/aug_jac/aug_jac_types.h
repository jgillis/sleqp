#ifndef SLEQP_AUG_JAC_TYPES_H
#define SLEQP_AUG_JAC_TYPES_H

#include "iterate.h"

#ifdef __cplusplus
extern "C" {
#endif

  typedef SLEQP_RETCODE (*SLEQP_AUG_JAC_SET_ITERATE)(SleqpIterate* iterate,
                                                     void* aug_jac);

  typedef SLEQP_RETCODE (*SLEQP_AUG_JAC_MIN_NORM_SOLUTION)(SleqpSparseVec* rhs,
                                                           SleqpSparseVec* sol,
                                                           void* aug_jac);

  typedef SLEQP_RETCODE (*SLEQP_AUG_JAC_PROJECTION)(SleqpSparseVec* rhs,
                                                    SleqpSparseVec* primal_sol,
                                                    SleqpSparseVec* dual_sol,
                                                    void* aug_jac);

  typedef SLEQP_RETCODE (*SLEQP_AUG_JAC_CONDITION)(bool* exact,
                                                   double* condition,
                                                   void *aug_jac);

  typedef SLEQP_RETCODE (*SLEQP_AUG_JAC_FREE)(void* aug_jac);

  typedef struct {
    SLEQP_AUG_JAC_SET_ITERATE       set_iterate;
    SLEQP_AUG_JAC_MIN_NORM_SOLUTION min_norm_solution;
    SLEQP_AUG_JAC_PROJECTION        projection;
    SLEQP_AUG_JAC_CONDITION         condition;
    SLEQP_AUG_JAC_FREE              free;
  } SleqpAugJacCallbacks;


#ifdef __cplusplus
}
#endif

#endif /* SLEQP_AUG_JAC_TYPES_H */