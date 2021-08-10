#include "solver.h"

#include "feas.h"
#include "log.h"

#define HEADER_FORMAT "%10s |%14s |%14s |%14s |%14s |%14s |%14s |%14s |%14s |%14s |%14s |%14s |%14s |%14s | %18s"

#define LINE_FORMAT SLEQP_FORMAT_BOLD "%10d " SLEQP_FORMAT_RESET "|%14e |%14e |%14e |%14e |%14e |%14e |%14s |%14e |%14e |%14e |%14s |%14e |%14e | %18s"

#define INITIAL_LINE_FORMAT SLEQP_FORMAT_BOLD "%10d " SLEQP_FORMAT_RESET "|%14e |%14e |%14e |%14s |%14s |%14e |%14s |%14s |%14s |%14s |%14s |%14s |%14s | %18s"

SLEQP_RETCODE sleqp_solver_print_header(SleqpSolver* solver)
{
  sleqp_log_info(HEADER_FORMAT,
                 "Iteration",
                 "Func val",
                 "Merit val",
                 "Feas res",
                 "Slack res",
                 "Stat res",
                 "Penalty",
                 "Working set",
                 "LP tr",
                 "EQP tr",
                 "LP cond",
                 "Jac cond",
                 "Primal step",
                 "Dual step",
                 "Step type");

  return SLEQP_OKAY;
}

SLEQP_RETCODE sleqp_solver_print_initial_line(SleqpSolver* solver)
{
  sleqp_log_info(INITIAL_LINE_FORMAT,
                 solver->iteration,
                 sleqp_iterate_get_func_val(solver->iterate),
                 solver->current_merit_value,
                 solver->feasibility_residuum,
                 "",
                 "",
                 solver->penalty_parameter,
                 "",
                 "",
                 "",
                 "",
                 "",
                 "",
                 "",
                 "");

  return SLEQP_OKAY;
}

SLEQP_RETCODE sleqp_solver_print_line(SleqpSolver* solver)
{
  bool exact = false;
  double basis_condition, aug_jac_condition;

  SLEQP_CALL(sleqp_lpi_get_basis_condition(solver->lp_interface,
                                           &exact,
                                           &basis_condition));

  SLEQP_CALL(sleqp_aug_jacobian_get_condition_estimate(solver->aug_jacobian,
                                                       &aug_jac_condition));

  const char* steptype_descriptions[] = {
    [SLEQP_STEPTYPE_NONE] = "",
    [SLEQP_STEPTYPE_ACCEPTED] = "Accepted",
    [SLEQP_STEPTYPE_ACCEPTED_FULL] = "Accepted (full)",
    [SLEQP_STEPTYPE_ACCEPTED_SOC] = "Accepted SOC",
    [SLEQP_STEPTYPE_REJECTED] = "Rejected"
  };

  char jac_condition_buf[1024];

  if(aug_jac_condition != SLEQP_NONE)
  {
    snprintf(jac_condition_buf,
             1024,
             "%14e",
             aug_jac_condition);
  }
  else
  {
    snprintf(jac_condition_buf,
             1024,
             "-");
  }

  char working_set_buf[1024];

  SleqpWorkingSet* working_set = sleqp_iterate_get_working_set(solver->iterate);

  SleqpWorkingSet* trial_working_set = sleqp_iterate_get_working_set(solver->trial_iterate);

  if(sleqp_working_set_eq(working_set, trial_working_set))
  {
    snprintf(working_set_buf,
             1024,
             "--");
  }
  else
  {
    snprintf(working_set_buf,
             1024,
             "%dv/%dc",
             sleqp_working_set_num_active_vars(working_set),
             sleqp_working_set_num_active_cons(working_set));
  }

  sleqp_log_info(LINE_FORMAT,
                 solver->iteration,
                 sleqp_iterate_get_func_val(solver->iterate),
                 solver->current_merit_value,
                 solver->feasibility_residuum,
                 solver->slackness_residuum,
                 solver->stationarity_residuum,
                 solver->penalty_parameter,
                 working_set_buf,
                 solver->lp_trust_radius,
                 solver->trust_radius,
                 basis_condition,
                 jac_condition_buf,
                 solver->primal_diff_norm,
                 solver->dual_diff_norm,
                 steptype_descriptions[solver->last_step_type]);

  return SLEQP_OKAY;
}

static SLEQP_RETCODE solver_print_timer(SleqpTimer* timer,
                                        const char* message,
                                        double total_elapsed)
{
  const int buf_size = 4096;
  char buffer[buf_size];

  const int num_runs = sleqp_timer_get_num_runs(timer);
  const double avg_time = sleqp_timer_get_avg(timer);
  const double total_time = sleqp_timer_get_ttl(timer);
  const double percent = (total_time / total_elapsed) * 100.;

  snprintf(buffer,
           buf_size,
           "%30s: %5d (%.6fs avg, %8.2fs total = %5.2f%%)",
           message,
           num_runs,
           avg_time,
           total_time,
           percent);

  sleqp_log_info(buffer);

  return SLEQP_OKAY;
}

SLEQP_RETCODE sleqp_solver_print_stats(SleqpSolver* solver,
                                       double violation)
{
  const char* descriptions[] = {
    [SLEQP_FEASIBLE]   = SLEQP_FORMAT_BOLD SLEQP_FORMAT_YELLOW "feasible"   SLEQP_FORMAT_RESET,
    [SLEQP_OPTIMAL]    = SLEQP_FORMAT_BOLD SLEQP_FORMAT_GREEN  "optimal"    SLEQP_FORMAT_RESET,
    [SLEQP_INFEASIBLE] = SLEQP_FORMAT_BOLD SLEQP_FORMAT_RED    "infeasible" SLEQP_FORMAT_RESET,
    [SLEQP_INVALID]    = SLEQP_FORMAT_BOLD SLEQP_FORMAT_RED    "Invalid"    SLEQP_FORMAT_RESET
  };

  SleqpFunc* original_func = sleqp_problem_func(solver->original_problem);
  SleqpFunc* func = sleqp_problem_func(solver->problem);

  const bool with_hessian = !(solver->sr1_data || solver->bfgs_data);

  sleqp_log_info(SLEQP_FORMAT_BOLD "%30s: %s" SLEQP_FORMAT_RESET,
                 "Solution status",
                 descriptions[solver->status]);

  if(solver->scaling_data)
  {
    double unscaled_violation;

    SLEQP_CALL(sleqp_iterate_feasibility_residuum(solver->original_problem,
                                                  solver->original_iterate,
                                                  &unscaled_violation));

    sleqp_log_info(SLEQP_FORMAT_BOLD "%30s:     %5.10e" SLEQP_FORMAT_RESET,
                   "Scaled objective value",
                   sleqp_iterate_get_func_val(solver->iterate));

    sleqp_log_info(SLEQP_FORMAT_BOLD "%30s:     %5.10e" SLEQP_FORMAT_RESET,
                   "Scaled violation",
                   violation);

    sleqp_log_info("%30s:     %5.10e",
                   "Original objective value",
                   sleqp_iterate_get_func_val(solver->original_iterate));

    sleqp_log_info("%30s:     %5.10e",
                   "Original violation",
                   unscaled_violation);

  }
  else
  {
    sleqp_log_info(SLEQP_FORMAT_BOLD "%30s:     %5.10e" SLEQP_FORMAT_RESET,
                   "Objective value",
                   sleqp_iterate_get_func_val(solver->iterate));

    sleqp_log_info(SLEQP_FORMAT_BOLD "%30s:     %5.10e" SLEQP_FORMAT_RESET,
                   "Violation",
                   violation);
  }

  sleqp_log_info("%30s: %5d",
                 "Iterations",
                 solver->iteration);

  SLEQP_CALL(solver_print_timer(sleqp_func_get_set_timer(original_func),
                                "Setting function values",
                                solver->elapsed_seconds));

  SLEQP_CALL(solver_print_timer(sleqp_func_get_val_timer(original_func),
                                "Function evaluations",
                                solver->elapsed_seconds));

  SLEQP_CALL(solver_print_timer(sleqp_func_get_grad_timer(original_func),
                                "Gradient evaluations",
                                solver->elapsed_seconds));

  SLEQP_CALL(solver_print_timer(sleqp_func_get_cons_val_timer(original_func),
                                "Constraint evaluations",
                                solver->elapsed_seconds));

  SLEQP_CALL(solver_print_timer(sleqp_func_get_cons_jac_timer(original_func),
                                "Jacobian evaluations",
                                solver->elapsed_seconds));

  if(with_hessian)
  {
    SLEQP_CALL(solver_print_timer(sleqp_func_get_hess_timer(original_func),
                                  "Hessian products",
                                  solver->elapsed_seconds));
  }

  if(solver->bfgs_data)
  {
    SLEQP_CALL(solver_print_timer(sleqp_func_get_hess_timer(func),
                                  "BFGS products",
                                  solver->elapsed_seconds));

    SLEQP_CALL(solver_print_timer(sleqp_bfgs_update_timer(solver->bfgs_data),
                                  "BFGS updates",
                                  solver->elapsed_seconds));
  }

  if(solver->sr1_data)
  {
    SLEQP_CALL(solver_print_timer(sleqp_func_get_hess_timer(func),
                                  "SR1 products",
                                  solver->elapsed_seconds));

    SLEQP_CALL(solver_print_timer(sleqp_sr1_update_timer(solver->sr1_data),
                                  "SR1 updates",
                                  solver->elapsed_seconds));
  }

  SLEQP_CALL(solver_print_timer(sleqp_aug_jacobian_get_factorization_timer(solver->aug_jacobian),
                                "Factorizations",
                                solver->elapsed_seconds));

  SLEQP_CALL(solver_print_timer(sleqp_aug_jacobian_get_substitution_timer(solver->aug_jacobian),
                                "Substitutions",
                                solver->elapsed_seconds));

  SLEQP_CALL(solver_print_timer(sleqp_lpi_get_solve_timer(solver->lp_interface),
                                "Solved LPs",
                                solver->elapsed_seconds));

  SLEQP_CALL(solver_print_timer(sleqp_newton_get_timer(solver->newton_data),
                                "Solved EQPs",
                                solver->elapsed_seconds));

  SLEQP_CALL(solver_print_timer(sleqp_linesearch_get_timer(solver->linesearch),
                                "Line searches",
                                solver->elapsed_seconds));

  sleqp_log_info("%30s: %8.2fs", "Solving time", solver->elapsed_seconds);

  if(solver->status == SLEQP_INFEASIBLE)
  {
    SLEQP_CALL(sleqp_solver_restore_original_iterate(solver));

    SleqpSparseVec* original_cons_val = sleqp_iterate_get_cons_val(solver->original_iterate);

    SLEQP_CALL(sleqp_violation_values(solver->original_problem,
                                      original_cons_val,
                                      solver->original_violation));

    sleqp_log_info("Violations with respect to original problem: ");

    for(int index = 0; index < solver->original_violation->nnz; ++index)
    {
      sleqp_log_info("(%d) = %e",
                     solver->original_violation->indices[index],
                     solver->original_violation->data[index]);
    }
  }

  return SLEQP_OKAY;
}