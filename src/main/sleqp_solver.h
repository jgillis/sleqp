#ifndef SLEQP_SOLVER_H
#define SLEQP_SOLVER_H

#include "sleqp.h"

#ifdef __cplusplus
extern "C" {
#endif

  typedef struct SleqpSolver SleqpSolver;

  SLEQP_RETCODE sleqp_solver_create(SleqpSolver** star,
                                    SleqpProblem* problem,
                                    SleqpParams* params,
                                    SleqpSparseVec* x);

  SLEQP_RETCODE sleqp_solver_solve(SleqpSolver* solver,
                                   int max_num_iterations);

  SLEQP_RETCODE sleqp_solver_get_solution(SleqpSolver* solver,
                                          SleqpIterate** iterate);

  SLEQP_RETCODE sleqp_solver_free(SleqpSolver** star);

#ifdef __cplusplus
}
#endif

#endif /* SLEQP_SOLVER_H */