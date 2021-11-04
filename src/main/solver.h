#ifndef SLEQP_SOLVER_H
#define SLEQP_SOLVER_H

#include "pub_solver.h"

#include "callback_handler.h"
#include "deriv_check.h"
#include "merit.h"
#include "polish.h"
#include "problem_scaling.h"
#include "trial_point.h"

#include "problem_solver.h"

#include "quasi_newton/quasi_newton.h"
#include "step/step_rule.h"

#include "preprocessor/preprocessor.h"

#ifdef __cplusplus
extern "C" {
#endif

  struct SleqpSolver
  {
    int refcount;

    SleqpParams* params;

    SleqpOptions* options;

    SleqpProblem* original_problem;

    SleqpScaling* scaling_data;

    SleqpSparseVec* scaled_primal;
    SleqpSparseVec* primal;

    SleqpProblem* scaled_problem;

    SleqpPreprocessor* preprocessor;

    SleqpProblemScaling* problem_scaling;

    bool restore_original_iterate;
    SleqpIterate* original_iterate;

    SleqpIterate* scaled_iterate;

    SleqpProblem* problem;

    SleqpProblemSolver* problem_solver;

    SleqpProblem* restoration_problem;

    SleqpProblemSolver* restoration_problem_solver;

    SLEQP_SOLVER_PHASE solver_phase;

    SleqpSparseVec* restoration_primal;

    SleqpTimer* elapsed_timer;

    SLEQP_STATUS status;

    SleqpPolishing* polishing;

    SleqpCallbackHandler* callback_handlers[SLEQP_SOLVER_NUM_EVENTS];

    SleqpQuasiNewton* quasi_newton;

    double time_limit;

    int iterations;

    bool abort_next;
  };

  SLEQP_RETCODE sleqp_solver_print_stats(SleqpSolver* solver,
                                         double violation);

  SLEQP_RETCODE sleqp_solver_toggle_phase(SleqpSolver* solver);

  SLEQP_RETCODE sleqp_solver_restore_original_iterate(SleqpSolver* solver);


#ifdef __cplusplus
}
#endif

#endif /* SLEQP_SOLVER_H */
