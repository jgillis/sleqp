#ifndef SLEQP_PROBLEM_SCALING_H
#define SLEQP_PROBLEM_SCALING_H

#include "settings.h"
#include "scale.h"

typedef struct SleqpProblemScaling SleqpProblemScaling;

SLEQP_NODISCARD
SLEQP_RETCODE
sleqp_problem_scaling_create(SleqpProblemScaling** problem_scaling,
                             SleqpScaling* scaling_data,
                             SleqpProblem* problem,
                             SleqpSettings* settings);

SLEQP_NODISCARD
SLEQP_RETCODE
sleqp_problem_scaling_flush(SleqpProblemScaling* problem_scaling);

SLEQP_NODISCARD
SLEQP_RETCODE
sleqp_problem_scaling_capture(SleqpProblemScaling* scaling);

SLEQP_NODISCARD
SLEQP_RETCODE
sleqp_problem_scaling_release(SleqpProblemScaling** star);

SleqpProblem*
sleqp_problem_scaling_get_problem(SleqpProblemScaling* scaling);

#endif /* SLEQP_PROBLEM_SCALING_H */
