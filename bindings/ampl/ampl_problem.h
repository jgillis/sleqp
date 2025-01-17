#ifndef SLEQP_AMPL_PROBLEM_H
#define SLEQP_AMPL_PROBLEM_H

#include "sleqp.h"

#include "ampl_data.h"

SLEQP_NODISCARD
SLEQP_RETCODE
sleqp_ampl_problem_create(SleqpProblem** star,
                          SleqpAmplData* data,
                          SleqpSettings* settings,
                          bool halt_on_error);

#endif /* SLEQP_AMPL_PROBLEM_H */
