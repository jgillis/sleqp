#include "trial_point.h"

#include "direction.h"
#include "dyn.h"
#include "error.h"
#include "fail.h"
#include "gauss_newton.h"
#include "mem.h"
#include "newton.h"
#include "soc.h"

#include "aug_jac/box_constrained_aug_jac.h"
#include "aug_jac/standard_aug_jac.h"
#include "aug_jac/unconstrained_aug_jac.h"

#include "cauchy/box_constrained_cauchy.h"
#include "cauchy/standard_cauchy.h"
#include "cauchy/unconstrained_cauchy.h"

#include "dual_estimation/dual_estimation_lp.h"
#include "dual_estimation/dual_estimation_lsq.h"
#include "dual_estimation/dual_estimation_mixed.h"

static SLEQP_RETCODE
create_dual_estimation(SleqpTrialPointSolver* solver)
{
  SleqpOptions* options = solver->options;

  SLEQP_DUAL_ESTIMATION_TYPE estimation_type
    = sleqp_options_enum_value(options, SLEQP_OPTION_ENUM_DUAL_ESTIMATION_TYPE);

  if (estimation_type == SLEQP_DUAL_ESTIMATION_TYPE_LP)
  {
    SLEQP_CALL(sleqp_dual_estimation_lp_create(&solver->estimation_data,
                                               solver->cauchy_data));
  }
  else if (estimation_type == SLEQP_DUAL_ESTIMATION_TYPE_LSQ)
  {
    SLEQP_CALL(sleqp_dual_estimation_lsq_create(&solver->estimation_data,
                                                solver->problem,
                                                solver->aug_jac));
  }
  else
  {
    assert(estimation_type == SLEQP_DUAL_ESTIMATION_TYPE_MIXED);

    SLEQP_CALL(sleqp_dual_estimation_mixed_create(&solver->estimation_data,
                                                  solver->problem,
                                                  solver->cauchy_data,
                                                  solver->aug_jac));
  }

  return SLEQP_OKAY;
}

static SLEQP_RETCODE
create_aug_jac(SleqpTrialPointSolver* solver)
{
  SleqpProblem* problem = solver->problem;
  SleqpParams* params   = solver->params;

  const int num_constraints = sleqp_problem_num_cons(problem);

  if (sleqp_problem_is_unconstrained(problem))
  {
    SLEQP_CALL(sleqp_unconstrained_aug_jac_create(&solver->aug_jac, problem));
  }
  else if (num_constraints == 0)
  {
    SLEQP_CALL(sleqp_box_constrained_aug_jac_create(&solver->aug_jac, problem));
  }
  else
  {
    // create sparse factorization

    SLEQP_CALL(
      sleqp_factorization_create_default(&solver->factorization, params));

    SLEQP_CALL(sleqp_standard_aug_jac_create(&solver->aug_jac,
                                             problem,
                                             params,
                                             solver->factorization));
  }

  return SLEQP_OKAY;
}

static SLEQP_RETCODE
create_cauchy_solver(SleqpTrialPointSolver* solver)
{
  SleqpProblem* problem = solver->problem;
  SleqpParams* params   = solver->params;
  SleqpOptions* options = solver->options;

  const int num_constraints = sleqp_problem_num_cons(problem);

  if (sleqp_problem_is_unconstrained(problem))
  {
    SLEQP_CALL(
      sleqp_unconstrained_cauchy_create(&solver->cauchy_data, problem, params));
  }
  else if (num_constraints == 0)
  {
    SLEQP_CALL(sleqp_box_constrained_cauchy_create(&solver->cauchy_data,
                                                   problem,
                                                   params));
  }
  else
  {
    SLEQP_CALL(sleqp_standard_cauchy_create(&solver->cauchy_data,
                                            problem,
                                            params,
                                            options));
  }

  return SLEQP_OKAY;
}

static SLEQP_RETCODE
create_eqp_solver(SleqpTrialPointSolver* solver)
{
  SleqpProblem* problem = solver->problem;
  SleqpParams* params   = solver->params;
  SleqpOptions* options = solver->options;

  SLEQP_TR_SOLVER tr_solver
    = sleqp_options_enum_value(options, SLEQP_OPTION_ENUM_TR_SOLVER);

  if (tr_solver == SLEQP_TR_SOLVER_LSQR)
  {
    SleqpFunc* func = sleqp_problem_func(problem);

    if (sleqp_func_get_type(func) != SLEQP_FUNC_TYPE_LSQ)
    {
      sleqp_raise(SLEQP_ILLEGAL_ARGUMENT,
                  "LSQR solver is only available for LSQ problems");
    }

    SLEQP_CALL(sleqp_gauss_newton_solver_create(&solver->eqp_solver,
                                                solver->problem,
                                                solver->params,
                                                solver->working_step));
  }
  else
  {
    SLEQP_CALL(sleqp_newton_solver_create(&solver->eqp_solver,
                                          solver->problem,
                                          params,
                                          options,
                                          solver->working_step));
  }

  return SLEQP_OKAY;
}

static SLEQP_RETCODE
create_parametric_solver(SleqpTrialPointSolver* solver)
{
  SLEQP_PARAMETRIC_CAUCHY parametric_cauchy
    = sleqp_options_enum_value(solver->options,
                               SLEQP_OPTION_ENUM_PARAMETRIC_CAUCHY);

  if (parametric_cauchy == SLEQP_PARAMETRIC_CAUCHY_DISABLED)
  {
    return SLEQP_OKAY;
  }

  SLEQP_CALL(sleqp_parametric_solver_create(&solver->parametric_solver,
                                            solver->problem,
                                            solver->params,
                                            solver->options,
                                            solver->merit,
                                            solver->linesearch));

  SLEQP_CALL(sleqp_working_set_create(&solver->parametric_original_working_set,
                                      solver->problem));

  return SLEQP_OKAY;
}

SLEQP_RETCODE
sleqp_trial_point_solver_create(SleqpTrialPointSolver** star,
                                SleqpProblem* problem,
                                SleqpParams* params,
                                SleqpOptions* options)
{
  SLEQP_CALL(sleqp_malloc(star));

  SleqpTrialPointSolver* solver = *star;

  *solver = (SleqpTrialPointSolver){0};

  solver->refcount = 1;

  SLEQP_CALL(sleqp_problem_capture(problem));
  solver->problem = problem;

  SLEQP_CALL(sleqp_params_capture(params));
  solver->params = params;

  SLEQP_CALL(sleqp_options_capture(options));
  solver->options = options;

  const int num_variables   = sleqp_problem_num_vars(solver->problem);
  const int num_constraints = sleqp_problem_num_cons(solver->problem);

  SLEQP_CALL(sleqp_vec_create_empty(&solver->lp_step, num_variables));

  SLEQP_CALL(
    sleqp_direction_create(&solver->cauchy_direction, problem, params));

  SLEQP_CALL(
    sleqp_vec_create_empty(&solver->estimation_residuals, num_variables));

  SLEQP_CALL(
    sleqp_direction_create(&solver->newton_direction, problem, params));

  SLEQP_CALL(sleqp_direction_create(&solver->soc_direction, problem, params));

  SLEQP_CALL(sleqp_direction_create(&solver->trial_direction, problem, params));

  SLEQP_CALL(sleqp_vec_create_empty(&solver->multipliers, num_constraints));

  SLEQP_CALL(
    sleqp_vec_create_empty(&solver->initial_trial_point, num_variables));

  SLEQP_CALL(sleqp_merit_create(&solver->merit, problem, params));

  SLEQP_CALL(create_cauchy_solver(solver));

  SLEQP_CALL(create_aug_jac(solver));

  SLEQP_CALL(create_dual_estimation(solver));

  SLEQP_CALL(sleqp_linesearch_create(&solver->linesearch,
                                     solver->problem,
                                     params,
                                     solver->merit));

  SLEQP_CALL(
    sleqp_working_step_create(&solver->working_step, solver->problem, params));

  SLEQP_CALL(create_eqp_solver(solver));

  SLEQP_CALL(sleqp_soc_data_create(&solver->soc_data, solver->problem, params));

  SLEQP_CALL(create_parametric_solver(solver));

  SLEQP_CALL(sleqp_alloc_array(&solver->dense_cache,
                               SLEQP_MAX(num_variables, num_constraints)));

  SLEQP_CALL(sleqp_timer_create(&solver->elapsed_timer));

  solver->time_limit = SLEQP_NONE;

  solver->penalty_parameter = SLEQP_NONE;
  solver->trust_radius      = SLEQP_NONE;
  solver->lp_trust_radius   = SLEQP_NONE;

  return SLEQP_OKAY;
}

SLEQP_RETCODE
sleqp_trial_point_solver_set_iterate(SleqpTrialPointSolver* solver,
                                     SleqpIterate* iterate)
{
  SLEQP_CALL(sleqp_iterate_release(&solver->iterate));

  SLEQP_CALL(sleqp_iterate_capture(iterate));
  solver->iterate = iterate;

  SLEQP_CALL(sleqp_merit_func(solver->merit,
                              iterate,
                              solver->penalty_parameter,
                              &solver->current_merit_value));

  return SLEQP_OKAY;
}

SLEQP_RETCODE
sleqp_trial_point_solver_set_penalty_info(SleqpTrialPointSolver* solver,
                                          double feas_res,
                                          bool allow_global_reset)
{
  solver->feasibility_residuum   = feas_res;
  solver->allow_global_reset     = allow_global_reset;
  solver->performed_global_reset = false;

  return SLEQP_OKAY;
}

SLEQP_RETCODE
sleqp_trial_point_solver_set_trust_radius(SleqpTrialPointSolver* solver,
                                          double trust_radius)
{
  assert(trust_radius > 0.);

  solver->trust_radius = trust_radius;

  return SLEQP_OKAY;
}

SLEQP_RETCODE
sleqp_trial_point_solver_set_lp_trust_radius(SleqpTrialPointSolver* solver,
                                             double lp_trust_radius)
{
  assert(lp_trust_radius > 0.);

  solver->lp_trust_radius = lp_trust_radius;

  return SLEQP_OKAY;
}

SLEQP_RETCODE
sleqp_trial_point_solver_set_penalty(SleqpTrialPointSolver* solver,
                                     double penalty_parameter)
{
  assert(penalty_parameter > 0.);

  solver->penalty_parameter = penalty_parameter;

  SleqpIterate* iterate = solver->iterate;

  SLEQP_CALL(sleqp_merit_func(solver->merit,
                              iterate,
                              solver->penalty_parameter,
                              &solver->current_merit_value));

  return SLEQP_OKAY;
}

SLEQP_RETCODE
sleqp_trial_point_solver_penalty(SleqpTrialPointSolver* solver,
                                 double* penalty_parameter)
{
  *penalty_parameter = solver->penalty_parameter;

  return SLEQP_OKAY;
}

bool
sleqp_trial_point_solver_locally_infeasible(SleqpTrialPointSolver* solver)
{
  return solver->locally_infeasible;
}

SLEQP_RETCODE
sleqp_trial_point_solver_penalty_info(SleqpTrialPointSolver* solver,
                                      bool* performed_global_reset)
{
  *performed_global_reset = solver->performed_global_reset;
  return SLEQP_OKAY;
}

SleqpVec*
sleqp_trial_point_solver_multipliers(SleqpTrialPointSolver* solver)
{
  return solver->multipliers;
}

SleqpVec*
sleqp_trial_point_solver_cauchy_step(SleqpTrialPointSolver* solver)
{
  return sleqp_direction_primal(solver->cauchy_direction);
}

SleqpVec*
sleqp_trial_point_solver_trial_step(SleqpTrialPointSolver* solver)
{
  return sleqp_direction_primal(solver->trial_direction);
}

SleqpVec*
sleqp_trial_point_solver_soc_step(SleqpTrialPointSolver* solver)
{
  return sleqp_direction_primal(solver->soc_direction);
}

SLEQP_RETCODE
sleqp_trial_point_solver_rayleigh(SleqpTrialPointSolver* solver,
                                  double* min_rayleigh,
                                  double* max_rayleigh)
{
  SLEQP_CALL(sleqp_eqp_solver_current_rayleigh(solver->eqp_solver,
                                               min_rayleigh,
                                               max_rayleigh));

  return SLEQP_OKAY;
}

static SLEQP_RETCODE
compute_trial_iterate_from_step(SleqpTrialPointSolver* solver,
                                const SleqpVec* step,
                                SleqpIterate* trial_iterate)
{
  SleqpProblem* problem = solver->problem;

  SleqpIterate* iterate = solver->iterate;

  const double zero_eps
    = sleqp_params_value(solver->params, SLEQP_PARAM_ZERO_EPS);

  SLEQP_CALL(sleqp_vec_add(sleqp_iterate_primal(iterate),
                           step,
                           zero_eps,
                           solver->initial_trial_point));

  SLEQP_CALL(sleqp_vec_clip(solver->initial_trial_point,
                            sleqp_problem_vars_lb(problem),
                            sleqp_problem_vars_ub(problem),
                            zero_eps,
                            sleqp_iterate_primal(trial_iterate)));

  return SLEQP_OKAY;
}

static SLEQP_RETCODE
compute_trial_point_simple(SleqpTrialPointSolver* solver,
                           SleqpIterate* trial_iterate,
                           double* cauchy_merit_value,
                           bool quadratic_model,
                           bool* full_step)
{
  SleqpIterate* iterate = solver->iterate;

  const double eps = sleqp_params_value(solver->params, SLEQP_PARAM_EPS);

  SLEQP_NUM_ASSERT_PARAM(eps);

  SLEQP_CALL(sleqp_trial_point_solver_compute_cauchy_step(solver,
                                                          cauchy_merit_value,
                                                          quadratic_model,
                                                          full_step));

  SleqpDirection* trial_direction = solver->cauchy_direction;

  // Compute merit value
  {
    SLEQP_CALL(sleqp_merit_linear(solver->merit,
                                  solver->iterate,
                                  trial_direction,
                                  solver->penalty_parameter,
                                  cauchy_merit_value));
  }

  if (quadratic_model)
  {
    double hessian_prod;

    SLEQP_CALL(sleqp_vec_dot(sleqp_direction_primal(trial_direction),
                             sleqp_direction_hess(trial_direction),
                             &hessian_prod));

    (*cauchy_merit_value) += .5 * hessian_prod;

#if !defined(NDEBUG)

    {
      double actual_quadratic_merit_value;

      SLEQP_CALL(sleqp_merit_quadratic(solver->merit,
                                       iterate,
                                       solver->cauchy_direction,
                                       solver->penalty_parameter,
                                       &actual_quadratic_merit_value));

      sleqp_assert_is_eq(*cauchy_merit_value,
                         actual_quadratic_merit_value,
                         eps);
    }

#endif
  }

  SLEQP_CALL(
    sleqp_direction_copy(solver->cauchy_direction, solver->trial_direction));

  SLEQP_CALL(compute_trial_iterate_from_step(
    solver,
    sleqp_direction_primal(solver->trial_direction),
    trial_iterate));

  return SLEQP_OKAY;
}

static SLEQP_RETCODE
compute_trial_point_newton(SleqpTrialPointSolver* solver,
                           SleqpIterate* trial_iterate,
                           double* trial_merit_value,
                           bool* failed_eqp_step,
                           bool* full_step)
{
  SleqpIterate* iterate = solver->iterate;

  const double eps = sleqp_params_value(solver->params, SLEQP_PARAM_EPS);

  SleqpTimer* timer = solver->elapsed_timer;
  double time_limit = solver->time_limit;

  double remaining_time = sleqp_timer_remaining_time(timer, time_limit);

  SLEQP_NUM_ASSERT_PARAM(eps);

  double cauchy_merit_value;

  SLEQP_CALL(sleqp_trial_point_solver_compute_cauchy_step(solver,
                                                          &cauchy_merit_value,
                                                          true,
                                                          full_step));

  SLEQP_CALL(
    sleqp_eqp_solver_set_time_limit(solver->eqp_solver, remaining_time));

  SLEQP_CALL(sleqp_eqp_solver_compute_direction(solver->eqp_solver,
                                                solver->multipliers,
                                                solver->newton_direction));

#ifndef NDEBUG

  {
    bool direction_valid;

    const double zero_eps
      = sleqp_params_value(solver->params, SLEQP_PARAM_ZERO_EPS);

    SLEQP_CALL(sleqp_direction_check(solver->cauchy_direction,
                                     solver->problem,
                                     iterate,
                                     solver->multipliers,
                                     solver->dense_cache,
                                     zero_eps,
                                     &direction_valid));

    sleqp_num_assert(direction_valid);
  }

  {
    bool direction_valid;

    const double zero_eps
      = sleqp_params_value(solver->params, SLEQP_PARAM_ZERO_EPS);

    SLEQP_CALL(sleqp_direction_check(solver->newton_direction,
                                     solver->problem,
                                     iterate,
                                     solver->multipliers,
                                     solver->dense_cache,
                                     zero_eps,
                                     &direction_valid));

    sleqp_num_assert(direction_valid);
  }

#endif

  {
    SLEQP_LINESEARCH lineserach
      = sleqp_options_enum_value(solver->options, SLEQP_OPTION_ENUM_LINESEARCH);

    double step_length;

    if (lineserach == SLEQP_LINESEARCH_EXACT)
    {
      SLEQP_CALL(sleqp_linesearch_trial_step_exact(solver->linesearch,
                                                   solver->cauchy_direction,
                                                   cauchy_merit_value,
                                                   solver->newton_direction,
                                                   solver->multipliers,
                                                   solver->trial_direction,
                                                   &step_length,
                                                   trial_merit_value));
    }
    else
    {
      assert(lineserach == SLEQP_LINESEARCH_APPROX);

      SLEQP_CALL(sleqp_linesearch_trial_step(solver->linesearch,
                                             solver->cauchy_direction,
                                             cauchy_merit_value,
                                             solver->newton_direction,
                                             solver->multipliers,
                                             solver->trial_direction,
                                             &step_length,
                                             trial_merit_value));
    }

    *(failed_eqp_step) = (step_length == 0.);
  }

#if !defined(NDEBUG)

  {
    bool direction_valid;

    const double zero_eps
      = sleqp_params_value(solver->params, SLEQP_PARAM_ZERO_EPS);

    SLEQP_CALL(sleqp_direction_check(solver->trial_direction,
                                     solver->problem,
                                     iterate,
                                     solver->multipliers,
                                     solver->dense_cache,
                                     zero_eps,
                                     &direction_valid));

    sleqp_num_assert(direction_valid);
  }

  {
    double actual_quadratic_merit_value;

    SLEQP_CALL(sleqp_merit_quadratic(solver->merit,
                                     iterate,
                                     solver->trial_direction,
                                     solver->penalty_parameter,
                                     &actual_quadratic_merit_value));

    sleqp_assert_is_eq(*trial_merit_value, actual_quadratic_merit_value, eps);
  }

#endif

  SLEQP_CALL(compute_trial_iterate_from_step(
    solver,
    sleqp_direction_primal(solver->trial_direction),
    trial_iterate));

  return SLEQP_OKAY;
}

static SLEQP_RETCODE
compute_trial_point_deterministic(SleqpTrialPointSolver* solver,
                                  SleqpIterate* trial_iterate,
                                  double* trial_merit_value,
                                  bool* failed_eqp_step,
                                  bool* full_step)
{
  const SleqpOptions* options = solver->options;

  const bool quadratic_model
    = sleqp_options_bool_value(options, SLEQP_OPTION_BOOL_USE_QUADRATIC_MODEL);

  const bool perform_newton_step
    = quadratic_model
      && sleqp_options_bool_value(options,
                                  SLEQP_OPTION_BOOL_PERFORM_NEWTON_STEP);

  if (perform_newton_step)
  {
    SLEQP_CALL(compute_trial_point_newton(solver,
                                          trial_iterate,
                                          trial_merit_value,
                                          failed_eqp_step,
                                          full_step));
  }
  else
  {
    SLEQP_CALL(compute_trial_point_simple(solver,
                                          trial_iterate,
                                          trial_merit_value,
                                          quadratic_model,
                                          full_step));
  }

  return SLEQP_OKAY;
}

static double
compute_required_accuracy(SleqpTrialPointSolver* solver, double model_reduction)
{
  const double accepted_reduction
    = sleqp_params_value(solver->params, SLEQP_PARAM_ACCEPTED_REDUCTION);

  // TODO: Make this adjustable
  // must be > 0, < .5 *accepted_reduction
  const double required_accuracy_factor = .4 * accepted_reduction;

  return required_accuracy_factor * model_reduction;
}

static SLEQP_RETCODE
evaluate_iterate(SleqpTrialPointSolver* solver,
                 SleqpProblem* problem,
                 SleqpIterate* iterate)
{
  double obj_val;

  SleqpVec* obj_grad          = sleqp_iterate_obj_grad(iterate);
  SleqpSparseMatrix* cons_jac = sleqp_iterate_cons_jac(iterate);
  SleqpVec* cons_val          = sleqp_iterate_cons_val(iterate);

  SLEQP_CALL(
    sleqp_problem_eval(problem, &obj_val, obj_grad, cons_val, cons_jac));

  SLEQP_CALL(sleqp_iterate_set_obj_val(iterate, obj_val));

  return SLEQP_OKAY;
}

static SLEQP_RETCODE
solver_refine_step(SleqpTrialPointSolver* solver,
                   SleqpIterate* trial_iterate,
                   double* model_trial_value,
                   bool* full_step)
{
  SleqpProblem* problem = solver->problem;
  SleqpFunc* func       = sleqp_problem_func(problem);

  SleqpOptions* options = solver->options;

  SleqpIterate* iterate = solver->iterate;

  assert(sleqp_func_get_type(func) == SLEQP_FUNC_TYPE_DYNAMIC);

  const bool quadratic_model
    = sleqp_options_bool_value(options, SLEQP_OPTION_BOOL_USE_QUADRATIC_MODEL);

  const bool perform_newton_step
    = quadratic_model
      && sleqp_options_bool_value(options,
                                  SLEQP_OPTION_BOOL_PERFORM_NEWTON_STEP);

  while (true)
  {
    double current_accuracy;

    SLEQP_CALL(sleqp_dyn_func_get_accuracy(func, &current_accuracy));

    const double model_reduction
      = solver->current_merit_value - (*model_trial_value);

    const double required_accuracy
      = compute_required_accuracy(solver, model_reduction);

    if (current_accuracy <= required_accuracy)
    {
      break;
    }

    sleqp_log_debug("Current accuracy of %e is insufficient, reducing to %e",
                    current_accuracy,
                    required_accuracy);

    SLEQP_CALL(sleqp_dyn_func_set_accuracy(func, required_accuracy));

    SLEQP_CALL(evaluate_iterate(solver, problem, iterate));

    SLEQP_CALL(sleqp_merit_func(solver->merit,
                                iterate,
                                solver->penalty_parameter,
                                &solver->current_merit_value));

    if (perform_newton_step)
    {
      bool failed_eqp_step;
      SLEQP_CALL(compute_trial_point_newton(solver,
                                            trial_iterate,
                                            model_trial_value,
                                            &failed_eqp_step,
                                            full_step));
    }
    else
    {
      SLEQP_CALL(compute_trial_point_simple(solver,
                                            trial_iterate,
                                            model_trial_value,
                                            quadratic_model,
                                            full_step));
    }
  }

  return SLEQP_OKAY;
}

static SLEQP_RETCODE
compute_trial_point_dynamic(SleqpTrialPointSolver* solver,
                            SleqpIterate* trial_iterate,
                            double* trial_merit_value,
                            bool* failed_eqp_step,
                            bool* full_step)
{
  SLEQP_CALL(compute_trial_point_deterministic(solver,
                                               trial_iterate,
                                               trial_merit_value,
                                               failed_eqp_step,
                                               full_step));

  SLEQP_CALL(
    solver_refine_step(solver, trial_iterate, trial_merit_value, full_step));

  SleqpVec* trial_step = sleqp_direction_primal(solver->trial_direction);

  SLEQP_CALL(
    compute_trial_iterate_from_step(solver, trial_step, trial_iterate));

  return SLEQP_OKAY;
}

SLEQP_RETCODE
sleqp_trial_point_solver_compute_trial_point(SleqpTrialPointSolver* solver,
                                             SleqpIterate* trial_iterate,
                                             double* trial_merit_value,
                                             bool* failed_eqp_step,
                                             bool* full_step,
                                             bool* reject)
{
  assert(solver->trust_radius != SLEQP_NONE);
  assert(solver->lp_trust_radius != SLEQP_NONE);
  assert(solver->penalty_parameter != SLEQP_NONE);

  SleqpProblem* problem = solver->problem;

  SleqpFunc* func = sleqp_problem_func(problem);

  *failed_eqp_step = false;
  // TODO: enable manual rejects
  *reject = false;

  SLEQP_CALL(sleqp_timer_start(solver->elapsed_timer));

  if (sleqp_func_get_type(func) == SLEQP_FUNC_TYPE_DYNAMIC)
  {
    SLEQP_CALL(compute_trial_point_dynamic(solver,
                                           trial_iterate,
                                           trial_merit_value,
                                           failed_eqp_step,
                                           full_step));

    return SLEQP_OKAY;
  }
  else
  {
    SLEQP_CALL(compute_trial_point_deterministic(solver,
                                                 trial_iterate,
                                                 trial_merit_value,
                                                 failed_eqp_step,
                                                 full_step));
  }

  SLEQP_CALL(sleqp_timer_stop(solver->elapsed_timer));

  return SLEQP_OKAY;
}

static SLEQP_RETCODE
compute_trial_point_soc_deterministic(SleqpTrialPointSolver* solver,
                                      SleqpIterate* trial_iterate,
                                      bool* reject)
{
  SleqpIterate* iterate = solver->iterate;

  *reject = false;

  SleqpVec* soc_step = sleqp_direction_primal(solver->soc_direction);

  SLEQP_CALL(
    sleqp_soc_compute_step(solver->soc_data,
                           solver->aug_jac,
                           iterate,
                           sleqp_direction_primal(solver->trial_direction),
                           trial_iterate,
                           soc_step));

  SLEQP_CALL(compute_trial_iterate_from_step(solver, soc_step, trial_iterate));

  return SLEQP_OKAY;
}

static SLEQP_RETCODE
compute_trial_point_soc_dynamic(SleqpTrialPointSolver* solver,
                                SleqpIterate* trial_iterate,
                                bool* reject)
{
  SleqpProblem* problem = solver->problem;
  SleqpFunc* func       = sleqp_problem_func(problem);

  assert(sleqp_func_get_type(func) == SLEQP_FUNC_TYPE_DYNAMIC);

  SleqpIterate* iterate = solver->iterate;

  SleqpOptions* options = solver->options;

  SleqpVec* soc_step = sleqp_direction_primal(solver->soc_direction);

  const double zero_eps
    = sleqp_params_value(solver->params, SLEQP_PARAM_ZERO_EPS);

  const bool quadratic_model
    = sleqp_options_bool_value(options, SLEQP_OPTION_BOOL_USE_QUADRATIC_MODEL);

  SLEQP_CALL(
    sleqp_soc_compute_step(solver->soc_data,
                           solver->aug_jac,
                           iterate,
                           sleqp_direction_primal(solver->trial_direction),
                           trial_iterate,
                           soc_step));

  SLEQP_CALL(sleqp_direction_reset(solver->soc_direction,
                                   problem,
                                   iterate,
                                   solver->multipliers,
                                   solver->dense_cache,
                                   zero_eps));

  double soc_model_merit = SLEQP_NONE;

  if (quadratic_model)
  {
    SLEQP_CALL(sleqp_merit_quadratic(solver->merit,
                                     iterate,
                                     solver->soc_direction,
                                     solver->penalty_parameter,
                                     &soc_model_merit));
  }
  else
  {
    SLEQP_CALL(sleqp_merit_linear(solver->merit,
                                  iterate,
                                  solver->soc_direction,
                                  solver->penalty_parameter,
                                  &soc_model_merit));
  }

  const double model_reduction = solver->current_merit_value - soc_model_merit;

  double current_accuracy;

  SLEQP_CALL(sleqp_dyn_func_get_accuracy(func, &current_accuracy));

  const double required_accuracy
    = compute_required_accuracy(solver, model_reduction);

  *reject = (current_accuracy > required_accuracy);

  return SLEQP_OKAY;
}

SLEQP_RETCODE
sleqp_trial_point_solver_compute_trial_point_soc(SleqpTrialPointSolver* solver,
                                                 SleqpIterate* trial_iterate,
                                                 bool* reject)
{
  SleqpProblem* problem = solver->problem;
  SleqpFunc* func       = sleqp_problem_func(problem);

  SLEQP_CALL(sleqp_timer_start(solver->elapsed_timer));

  if (sleqp_func_get_type(func) == SLEQP_FUNC_TYPE_DYNAMIC)
  {
    SLEQP_CALL(compute_trial_point_soc_dynamic(solver, trial_iterate, reject));
  }
  else
  {
    SLEQP_CALL(
      compute_trial_point_soc_deterministic(solver, trial_iterate, reject));
  }

  SLEQP_CALL(sleqp_timer_stop(solver->elapsed_timer));

  return SLEQP_OKAY;
}

static SLEQP_RETCODE
trial_point_solver_free(SleqpTrialPointSolver** star)
{
  SleqpTrialPointSolver* solver = *star;

  SLEQP_CALL(sleqp_timer_free(&solver->elapsed_timer));

  sleqp_free(&solver->dense_cache);

  SLEQP_CALL(sleqp_parametric_solver_release(&solver->parametric_solver));

  SLEQP_CALL(
    sleqp_working_set_release(&solver->parametric_original_working_set));

  SLEQP_CALL(sleqp_soc_data_release(&solver->soc_data));

  SLEQP_CALL(sleqp_eqp_solver_release(&solver->eqp_solver));

  SLEQP_CALL(sleqp_working_step_release(&solver->working_step));

  SLEQP_CALL(sleqp_linesearch_release(&solver->linesearch));

  SLEQP_CALL(sleqp_aug_jac_release(&solver->aug_jac));

  SLEQP_CALL(sleqp_factorization_release(&solver->factorization));

  SLEQP_CALL(sleqp_dual_estimation_release(&solver->estimation_data));

  SLEQP_CALL(sleqp_cauchy_release(&solver->cauchy_data));

  SLEQP_CALL(sleqp_iterate_release(&solver->iterate));

  SLEQP_CALL(sleqp_merit_release(&solver->merit));

  SLEQP_CALL(sleqp_vec_free(&solver->initial_trial_point));
  SLEQP_CALL(sleqp_vec_free(&solver->multipliers));

  SLEQP_CALL(sleqp_direction_release(&solver->trial_direction));
  SLEQP_CALL(sleqp_direction_release(&solver->soc_direction));

  SLEQP_CALL(sleqp_direction_release(&solver->newton_direction));
  SLEQP_CALL(sleqp_vec_free(&solver->estimation_residuals));
  SLEQP_CALL(sleqp_direction_release(&solver->cauchy_direction));
  SLEQP_CALL(sleqp_vec_free(&solver->lp_step));

  SLEQP_CALL(sleqp_options_release(&solver->options));

  SLEQP_CALL(sleqp_params_release(&solver->params));

  SLEQP_CALL(sleqp_problem_release(&solver->problem));

  sleqp_free(&solver);

  return SLEQP_OKAY;
}

SLEQP_RETCODE
sleqp_trial_point_solver_capture(SleqpTrialPointSolver* solver)
{
  ++solver->refcount;

  return SLEQP_OKAY;
}

SLEQP_RETCODE
sleqp_trial_point_solver_release(SleqpTrialPointSolver** star)
{
  SleqpTrialPointSolver* trial_point_solver = *star;

  if (!trial_point_solver)
  {
    return SLEQP_OKAY;
  }

  if (--trial_point_solver->refcount == 0)
  {
    SLEQP_CALL(trial_point_solver_free(star));
  }

  *star = NULL;

  return SLEQP_OKAY;
}
