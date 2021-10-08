#ifndef SLEQP_QUASI_NEWTON_TYPES_H
#define SLEQP_QUASI_NEWTON_TYPES_H

#include "iterate.h"

#ifdef __cplusplus
extern "C" {
#endif

  typedef struct SleqpQuasiNewton SleqpQuasiNewton;

  typedef SLEQP_RETCODE (*SLEQP_QUASI_NEWTON_PUSH)(const SleqpIterate* old_iterate,
                                                   const SleqpIterate* new_iterate,
                                                   const SleqpSparseVec* multipliers,
                                                   void* quasi_newton_data);

  typedef SLEQP_RETCODE (*SLEQP_QUASI_NEWTON_RESET)(void* quasi_newton_data);

  typedef SLEQP_RETCODE (*SLEQP_QUASI_NEWTON_HESS_PROD)(const SleqpSparseVec* direction,
                                                        SleqpSparseVec* product,
                                                        void* quasi_newton_data);

  typedef SLEQP_RETCODE (*SLEQP_QUASI_NEWTON_FREE)(void* quasi_newton_data);

  typedef struct {
    SLEQP_QUASI_NEWTON_PUSH push;
    SLEQP_QUASI_NEWTON_RESET reset;
    SLEQP_QUASI_NEWTON_HESS_PROD hess_prod;
    SLEQP_QUASI_NEWTON_FREE free;
  } SleqpQuasiNewtonCallbacks;

#ifdef __cplusplus
}
#endif

#endif /* SLEQP_QUASI_NEWTON_TYPES_H */