#ifndef STEP_RULE_DIRECT_H
#define STEP_RULE_DIRECT_H

#include "step_rule.h"

#ifdef __cplusplus
extern "C" {
#endif

  /**
   * The default step rule, accepting iterates based on the reduction
   * ratio of the exact and model reduction.
   **/
  SLEQP_RETCODE sleqp_step_rule_direct_create(SleqpStepRule** star,
                                              SleqpProblem* problem,
                                              SleqpParams* params);

#ifdef __cplusplus
}
#endif

#endif /* STEP_RULE_DIRECT_H */