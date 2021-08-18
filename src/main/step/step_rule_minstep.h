#ifndef STEP_RULE_MINSTEP_H
#define STEP_RULE_MINSTEP_H

#include "step_rule.h"

#ifdef __cplusplus
extern "C" {
#endif

  /**
   * A non-monotone step rule, based on the maximum of the current
   * and a historic reduction ratio based on a reference value adjusted
   * according to the minimal merit value encountered during all iterations.
   *
   * See "Trust-region methods", pp. 355
   **/
  SLEQP_RETCODE sleqp_step_rule_minstep_create(SleqpStepRule** star,
                                               SleqpProblem* problem,
                                               SleqpParams* params,
                                               int step_count);

#ifdef __cplusplus
}
#endif

#endif /* STEP_RULE_MINSTEP_H */
