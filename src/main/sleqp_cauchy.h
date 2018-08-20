#ifndef SLEQP_CAUCHY_H
#define SLEQP_CAUCHY_H

#include "sleqp.h"

#include "sleqp_active_set.h"
#include "sleqp_iterate.h"

#include "lp/sleqp_lpi.h"

#ifdef __cplusplus
extern "C" {
#endif

  typedef struct SleqpCauchyData SleqpCauchyData;

  SLEQP_RETCODE sleqp_cauchy_data_create(SleqpCauchyData** star,
                                         SleqpProblem* problem,
                                         SleqpLPi* lp_interface);

  SLEQP_RETCODE sleqp_cauchy_data_free(SleqpCauchyData** star);

  SLEQP_RETCODE sleqp_cauchy_compute_direction(SleqpProblem* problem,
                                               SleqpIterate* iterate,
                                               SleqpCauchyData* cauchy_data,
                                               double penalty,
                                               double trust_radius);

  SLEQP_RETCODE sleqp_cauchy_get_active_set(SleqpProblem* problem,
                                            SleqpIterate* iterate,
                                            SleqpCauchyData* cauchy_data,
                                            SleqpActiveSet* active_set,
                                            double trust_radius);

  SLEQP_RETCODE sleqp_cauchy_get_direction(SleqpProblem* problem,
                                           SleqpIterate* iterate,
                                           SleqpCauchyData* cauchy_data,
                                           SleqpSparseVec* direction);

#ifdef __cplusplus
}
#endif

#endif /* SLEQP_CAUCHY_H */
