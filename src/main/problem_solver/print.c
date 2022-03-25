#include "problem_solver.h"

#include "feas.h"
#include "log.h"

#define HEADER_FORMAT                                                          \
  "%10s |%14s |%14s |%14s |%14s |%14s |%14s |%14s |%14s |%14s |%14s |%14s | "  \
  "%18s"

#define LINE_FORMAT                                                            \
  SLEQP_FORMAT_BOLD                                                            \
  "%10s " SLEQP_FORMAT_RESET                                                   \
  "|%14e |%14e |%14e |%14e |%14e |%14e |%14s |%14e |%14e |%14e |%14e | %18s"

#define INITIAL_LINE_FORMAT                                                    \
  SLEQP_FORMAT_BOLD                                                            \
  "%10s " SLEQP_FORMAT_RESET                                                   \
  "|%14e |%14e |%14e |%14s |%14s |%14e |%14s |%14s |%14s |%14s |%14s | %18s"

#define DEFAULT_BUF_SIZE 1024

static SLEQP_RETCODE
print_iteration(SleqpProblemSolver* solver, char* buf, int buf_size)
{
  if (solver->solver_phase == SLEQP_SOLVER_PHASE_OPTIMIZATION)
  {
    snprintf(buf, buf_size, "%d", solver->iteration);
  }
  else
  {
    snprintf(buf, buf_size, "R %d", solver->iteration);
  }

  return SLEQP_OKAY;
}

SLEQP_RETCODE
sleqp_problem_solver_print_header(SleqpProblemSolver* solver)
{
  sleqp_log_info(HEADER_FORMAT,
                 "Iteration",
                 "Obj val",
                 "Merit val",
                 "Feas res",
                 "Slack res",
                 "Stat res",
                 "Penalty",
                 "Working set",
                 "LP tr",
                 "EQP tr",
                 /*
                 "LP cond",
                 "Jac cond",
                 */
                 "Primal step",
                 "Dual step",
                 "Step type");

  return SLEQP_OKAY;
}

SLEQP_RETCODE
sleqp_problem_solver_print_initial_line(SleqpProblemSolver* solver)
{
  char iteration_buf[DEFAULT_BUF_SIZE];

  SLEQP_CALL(print_iteration(solver, iteration_buf, DEFAULT_BUF_SIZE));

  sleqp_log_info(INITIAL_LINE_FORMAT,
                 iteration_buf,
                 sleqp_iterate_obj_val(solver->iterate),
                 solver->current_merit_value,
                 solver->feas_res,
                 "",
                 "",
                 solver->penalty_parameter,
                 "",
                 "",
                 "",
                 "",
                 "",
                 "");

  return SLEQP_OKAY;
}

/*
static SLEQP_RETCODE print_cond(char cond_buffer[DEFAULT_BUF_SIZE],
                                bool exact,
                                double condition)
{
  if(condition != SLEQP_NONE)
  {
    if(exact)
    {
      snprintf(cond_buffer,
               DEFAULT_BUF_SIZE,
               "%.4e",
               condition);
    }
    else
    {
      snprintf(cond_buffer,
               DEFAULT_BUF_SIZE,
               "~%.4e",
               condition);
    }
  }
  else
  {
    snprintf(cond_buffer,
             DEFAULT_BUF_SIZE,
             "n/a");
  }

  return SLEQP_OKAY;
}
*/

SLEQP_RETCODE
sleqp_problem_solver_print_line(SleqpProblemSolver* solver)
{
  /*
  bool exact = false;
  double basis_condition, aug_jac_condition;

  char jac_cond_buf[DEFAULT_BUF_SIZE];
  char basis_cond_buf[DEFAULT_BUF_SIZE];

  SLEQP_CALL(sleqp_aug_jac_condition(solver->aug_jac,
                                     &exact,
                                     &aug_jac_condition));

  SLEQP_CALL(print_cond(jac_cond_buf,
                        exact,
                        aug_jac_condition));

  SLEQP_CALL(sleqp_cauchy_get_basis_condition(solver->cauchy_data,
                                              &exact,
                                              &basis_condition));

  SLEQP_CALL(print_cond(basis_cond_buf,
                        exact,
                        basis_condition));
  */

  const char* steptype_descriptions[]
    = {[SLEQP_STEPTYPE_NONE]          = "",
       [SLEQP_STEPTYPE_ACCEPTED]      = "Accepted",
       [SLEQP_STEPTYPE_ACCEPTED_FULL] = "Accepted (full)",
       [SLEQP_STEPTYPE_ACCEPTED_SOC]  = "Accepted SOC",
       [SLEQP_STEPTYPE_REJECTED]      = "Rejected"};

  char working_set_buf[DEFAULT_BUF_SIZE];

  char iteration_buf[DEFAULT_BUF_SIZE];

  SLEQP_CALL(print_iteration(solver, iteration_buf, DEFAULT_BUF_SIZE));

  SleqpWorkingSet* working_set = sleqp_iterate_working_set(solver->iterate);

  SleqpWorkingSet* trial_working_set
    = sleqp_iterate_working_set(solver->trial_iterate);

  if (sleqp_working_set_eq(working_set, trial_working_set))
  {
    snprintf(working_set_buf, DEFAULT_BUF_SIZE, "--");
  }
  else
  {
    snprintf(working_set_buf,
             DEFAULT_BUF_SIZE,
             "%dv/%dc",
             sleqp_working_set_num_active_vars(working_set),
             sleqp_working_set_num_active_cons(working_set));
  }

  sleqp_log_info(LINE_FORMAT,
                 iteration_buf,
                 sleqp_iterate_obj_val(solver->iterate),
                 solver->current_merit_value,
                 solver->feas_res,
                 solver->slack_res,
                 solver->stat_res,
                 solver->penalty_parameter,
                 working_set_buf,
                 solver->lp_trust_radius,
                 solver->trust_radius,
                 /*
                 basis_cond_buf,
                 jac_cond_buf,
                 */
                 solver->primal_diff_norm,
                 solver->dual_diff_norm,
                 steptype_descriptions[solver->last_step_type]);

  return SLEQP_OKAY;
}

SLEQP_RETCODE
sleqp_problem_solver_print_stats(const SleqpProblemSolver* solver)
{
  SleqpTrialPointSolver* trial_point_solver = solver->trial_point_solver;

  sleqp_log_info("%30s: %5d", "Accepted steps", solver->num_accepted_steps);

  sleqp_log_info("%30s: %5d",
                 "SOC Accepted steps",
                 solver->num_soc_accepted_steps);

  sleqp_log_info("%30s: %5d", "Rejected steps", solver->num_rejected_steps);

  sleqp_log_info("%30s: %5d", "Failed EQP steps", solver->num_failed_eqp_steps);

  const double elapsed_seconds = sleqp_timer_get_ttl(solver->elapsed_timer);

  SLEQP_CALL(
    sleqp_trial_point_solver_print_stats(trial_point_solver, elapsed_seconds));

  return SLEQP_OKAY;
}
