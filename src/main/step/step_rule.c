#include "step_rule.h"

#include <float.h>

#include "cmp.h"
#include "fail.h"
#include "mem.h"

#include "step/step_rule_direct.h"
#include "step/step_rule_minstep.h"
#include "step/step_rule_window.h"

static const int window_size = 25;
static const int step_count  = 2;

struct SleqpStepRule
{
  int refcount;

  SleqpStepRuleCallbacks callbacks;
  SleqpProblem* problem;

  void* step_data;
};

SLEQP_RETCODE
sleqp_step_rule_create(SleqpStepRule** star,
                       SleqpProblem* problem,
                       SleqpSettings* settings,
                       SleqpStepRuleCallbacks* callbacks,
                       void* step_data)
{
  SLEQP_CALL(sleqp_malloc(star));

  SleqpStepRule* rule = *star;

  *rule = (SleqpStepRule){0};

  rule->refcount = 1;

  rule->callbacks = *callbacks;

  SLEQP_CALL(sleqp_problem_capture(problem));

  rule->problem   = problem;
  rule->step_data = step_data;

  return SLEQP_OKAY;
}

SLEQP_RETCODE
sleqp_step_rule_apply(SleqpStepRule* rule,
                      double iterate_merit,
                      double trial_exact_merit,
                      double trial_model_merit,
                      bool* accept_step,
                      double* redution_ratio)
{
  SLEQP_CALL(rule->callbacks.rule_apply(iterate_merit,
                                        trial_exact_merit,
                                        trial_model_merit,
                                        accept_step,
                                        redution_ratio,
                                        rule->step_data));

  return SLEQP_OKAY;
}

SLEQP_NODISCARD
SLEQP_RETCODE
sleqp_step_rule_reset(SleqpStepRule* rule)
{
  if (rule->callbacks.rule_reset)
  {
    SLEQP_CALL(rule->callbacks.rule_reset(rule->step_data));
  }

  return SLEQP_OKAY;
}

SLEQP_RETCODE
sleqp_step_rule_capture(SleqpStepRule* rule)
{
  ++rule->refcount;

  return SLEQP_OKAY;
}

static SLEQP_RETCODE
step_rule_free(SleqpStepRule** star)
{
  SleqpStepRule* rule = *star;

  if (!rule)
  {
    return SLEQP_OKAY;
  }

  if (rule->callbacks.rule_free)
  {
    SLEQP_CALL(rule->callbacks.rule_free(rule->step_data));
  }

  rule->step_data = NULL;

  SLEQP_CALL(sleqp_problem_release(&rule->problem));

  sleqp_free(star);

  return SLEQP_OKAY;
}

SLEQP_RETCODE
sleqp_step_rule_release(SleqpStepRule** star)
{
  SleqpStepRule* rule = *star;

  if (!rule)
  {
    return SLEQP_OKAY;
  }

  if (--rule->refcount == 0)
  {
    SLEQP_CALL(step_rule_free(star));
  }

  *star = NULL;

  return SLEQP_OKAY;
}

SLEQP_RETCODE
sleqp_step_rule_create_default(SleqpStepRule** star,
                               SleqpProblem* problem,
                               SleqpSettings* settings)
{
  SLEQP_STEP_RULE step_rule
    = sleqp_settings_enum_value(settings, SLEQP_SETTINGS_ENUM_STEP_RULE);

  if (step_rule == SLEQP_STEP_RULE_DIRECT)
  {
    SLEQP_CALL(sleqp_step_rule_direct_create(star, problem, settings));
  }
  else if (step_rule == SLEQP_STEP_RULE_WINDOW)
  {
    SLEQP_CALL(
      sleqp_step_rule_window_create(star, problem, settings, window_size));
  }
  else
  {
    assert(step_rule == SLEQP_STEP_RULE_MINSTEP);

    SLEQP_CALL(
      sleqp_step_rule_minstep_create(star, problem, settings, step_count));
  }

  return SLEQP_OKAY;
}
