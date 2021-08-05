#ifndef WORKING_STEP_H
#define WORKING_STEP_H

/**
 * @file working_step.h
 * @brief Definition of functions used for the computation of the initial step towards the working set.
 **/

#include "aug_jacobian.h"
#include "options.h"
#include "params.h"
#include "problem.h"
#include "iterate.h"

#ifdef __cplusplus
extern "C" {
#endif

  typedef struct SleqpWorkingStep SleqpWorkingStep;

  SLEQP_NODISCARD
  SLEQP_RETCODE sleqp_working_step_create(SleqpWorkingStep** star,
                                          SleqpProblem* problem,
                                          SleqpParams* params);

  SLEQP_NODISCARD
  SLEQP_RETCODE sleqp_working_step_set_iterate(SleqpWorkingStep* step,
                                               SleqpIterate* iterate,
                                               SleqpAugJacobian* jacobian,
                                               double trust_radius);

  SleqpSparseVec* sleqp_working_step_get_direction(SleqpWorkingStep* step);

  SleqpSparseVec* sleqp_working_step_get_step(SleqpWorkingStep* step);

  double sleqp_working_step_get_reduced_trust_radius(SleqpWorkingStep* step);

  SleqpSparseVec* sleqp_working_step_get_violated_cons_multipliers(SleqpWorkingStep* step);

  SleqpSparseVec* sleqp_working_step_get_violated_vars_multipliers(SleqpWorkingStep* step);

  SLEQP_NODISCARD
  SLEQP_RETCODE sleqp_working_step_capture(SleqpWorkingStep* step);

  SLEQP_NODISCARD
  SLEQP_RETCODE sleqp_working_step_release(SleqpWorkingStep** star);

#ifdef __cplusplus
}
#endif

#endif /* WORKING_STEP_H */