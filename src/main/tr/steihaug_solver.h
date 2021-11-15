#ifndef SLEQP_STEIHAUG_SOLVER_H
#define SLEQP_STEIHAUG_SOLVER_H

#include "tr_solver.h"

SLEQP_NODISCARD
SLEQP_RETCODE
sleqp_steihaug_solver_create(SleqpTRSolver** star,
                             SleqpProblem* problem,
                             SleqpParams* params,
                             SleqpOptions* options);

#endif /* SLEQP_STEIHAUG_SOLVER_H */
