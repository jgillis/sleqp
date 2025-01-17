#include "working_step.h"

#include <math.h>

#include "cmp.h"
#include "fail.h"
#include "feas.h"
#include "mem.h"
#include "util.h"
#include "working_set.h"

static const double norm_ratio = .8;

struct SleqpWorkingStep
{
  int refcount;

  SleqpProblem* problem;
  SleqpSettings* settings;

  SleqpIterate* iterate;

  SleqpVec* lower_diff;
  SleqpVec* upper_diff;

  SleqpVec* initial_rhs;
  SleqpVec* initial_direction;

  SleqpDirection* step_direction;

  SleqpVec* initial_cons_val;

  SleqpVec* violated_constraint_multipliers;

  SleqpVec* sparse_cache;
  double* dense_cache;

  double reduced_trust_radius;
  bool initial_step_in_working_set;
};

SLEQP_RETCODE
sleqp_working_step_create(SleqpWorkingStep** star,
                          SleqpProblem* problem,
                          SleqpSettings* settings)
{
  SLEQP_CALL(sleqp_malloc(star));

  SleqpWorkingStep* step = *star;

  *step = (SleqpWorkingStep){0};

  step->refcount = 1;

  step->problem = problem;
  SLEQP_CALL(sleqp_problem_capture(step->problem));

  step->settings = settings;
  SLEQP_CALL(sleqp_settings_capture(step->settings));

  const int num_variables   = sleqp_problem_num_vars(problem);
  const int num_constraints = sleqp_problem_num_cons(problem);

  SLEQP_CALL(sleqp_vec_create_empty(&step->lower_diff, num_variables));

  SLEQP_CALL(sleqp_vec_create_empty(&step->upper_diff, num_variables));

  SLEQP_CALL(sleqp_vec_create_empty(&step->initial_rhs, num_constraints));

  SLEQP_CALL(sleqp_vec_create_empty(&step->initial_direction, num_variables));

  SLEQP_CALL(sleqp_direction_create(&step->step_direction, problem, settings));

  SLEQP_CALL(sleqp_vec_create_empty(&step->initial_cons_val, num_constraints));

  SLEQP_CALL(sleqp_vec_create_empty(&step->violated_constraint_multipliers,
                                    num_constraints));

  SLEQP_CALL(sleqp_vec_create_empty(&step->sparse_cache, num_variables));

  SLEQP_CALL(sleqp_alloc_array(&step->dense_cache,
                               SLEQP_MAX(num_variables, num_constraints)));

  return SLEQP_OKAY;
}

double
sleqp_working_step_newton_obj_offset(SleqpWorkingStep* step,
                                     double penalty_parameter)
{
  SleqpProblem* problem = step->problem;
  double offset         = 0.;

  // current objective value
  {
    offset += sleqp_iterate_obj_val(step->iterate);
  }

  // violation at initial step
  {
    SleqpVec* initial_cons_val = step->initial_cons_val;

    double violation;

    SLEQP_CALL(sleqp_total_violation(problem, initial_cons_val, &violation));

    offset += penalty_parameter * violation;
  }

  return offset;
}

static SLEQP_RETCODE
compute_initial_rhs(SleqpWorkingStep* step,
                    SleqpIterate* iterate,
                    SleqpAugJac* jacobian)
{
  SleqpProblem* problem = step->problem;

  SleqpVec* initial_rhs              = step->initial_rhs;
  const SleqpWorkingSet* working_set = sleqp_iterate_working_set(iterate);

  SleqpVec* lower_diff = step->lower_diff;
  SleqpVec* upper_diff = step->upper_diff;

  const double eps = sleqp_settings_real_value(step->settings, SLEQP_SETTINGS_REAL_EPS);

  SLEQP_NUM_ASSERT_PARAM(eps);

  const double zero_eps
    = sleqp_settings_real_value(step->settings, SLEQP_SETTINGS_REAL_ZERO_EPS);

  const int working_set_size = sleqp_working_set_size(working_set);

  SLEQP_CALL(sleqp_vec_resize(initial_rhs, working_set_size));

  {
    SLEQP_CALL(sleqp_vec_clear(initial_rhs));

    SLEQP_CALL(sleqp_vec_reserve(initial_rhs, working_set_size));

    SLEQP_CALL(sleqp_vec_resize(initial_rhs, working_set_size));
  }

  // variables
  {
    const SleqpVec* values = sleqp_iterate_primal(iterate);
    const SleqpVec* var_lb = sleqp_problem_vars_lb(problem);
    const SleqpVec* var_ub = sleqp_problem_vars_ub(problem);

    SLEQP_CALL(
      sleqp_vec_add_scaled(values, var_ub, -1., 1., zero_eps, upper_diff));

    SLEQP_CALL(
      sleqp_vec_add_scaled(values, var_lb, -1., 1., zero_eps, lower_diff));

    int k_lower = 0, k_upper = 0;

    while (k_lower < lower_diff->nnz || k_upper < upper_diff->nnz)
    {
      bool valid_lower = (k_lower < lower_diff->nnz);
      bool valid_upper = (k_upper < upper_diff->nnz);

      const int i_lower
        = valid_lower ? lower_diff->indices[k_lower] : lower_diff->dim + 1;
      const int i_upper
        = valid_upper ? upper_diff->indices[k_upper] : upper_diff->dim + 1;

      const int i_combined = SLEQP_MIN(i_lower, i_upper);

      valid_lower = valid_lower && (i_lower == i_combined);
      valid_upper = valid_upper && (i_upper == i_combined);

      const double lower_value = valid_lower ? lower_diff->data[k_lower] : 0.;
      const double upper_value = valid_upper ? upper_diff->data[k_upper] : 0.;

      const int i_set = sleqp_working_set_var_index(working_set, i_combined);

      const SLEQP_ACTIVE_STATE var_state
        = sleqp_working_set_var_state(working_set, i_combined);

      assert(var_state == SLEQP_INACTIVE || i_set != SLEQP_NONE);

      if (var_state == SLEQP_ACTIVE_UPPER)
      {
        SLEQP_CALL(sleqp_vec_push(initial_rhs, i_set, upper_value));
      }
      else if (var_state == SLEQP_ACTIVE_LOWER)
      {
        SLEQP_CALL(sleqp_vec_push(initial_rhs, i_set, lower_value));
      }
      else if (var_state == SLEQP_ACTIVE_BOTH)
      {
        sleqp_assert_is_eq(lower_value, upper_value, eps);

        SLEQP_CALL(sleqp_vec_push(initial_rhs, i_set, lower_value));
      }

      if (i_lower == i_combined)
      {
        ++k_lower;
      }

      if (i_upper == i_combined)
      {
        ++k_upper;
      }
    }
  }

  // constraints
  {
    SleqpVec* values  = sleqp_iterate_cons_val(iterate);
    SleqpVec* cons_lb = sleqp_problem_cons_lb(problem);
    SleqpVec* cons_ub = sleqp_problem_cons_ub(problem);

    SLEQP_CALL(
      sleqp_vec_add_scaled(values, cons_ub, -1., 1., zero_eps, upper_diff));

    SLEQP_CALL(
      sleqp_vec_add_scaled(values, cons_lb, -1., 1., zero_eps, lower_diff));

    int k_lower = 0, k_upper = 0;

    while (k_lower < lower_diff->nnz || k_upper < upper_diff->nnz)
    {
      bool valid_lower = (k_lower < lower_diff->nnz);
      bool valid_upper = (k_upper < upper_diff->nnz);

      const int i_lower
        = valid_lower ? lower_diff->indices[k_lower] : lower_diff->dim + 1;
      const int i_upper
        = valid_upper ? upper_diff->indices[k_upper] : upper_diff->dim + 1;

      const int i_combined = SLEQP_MIN(i_lower, i_upper);

      valid_lower = valid_lower && (i_lower == i_combined);
      valid_upper = valid_upper && (i_upper == i_combined);

      const double lower_value = valid_lower ? lower_diff->data[k_lower] : 0.;
      const double upper_value = valid_upper ? upper_diff->data[k_upper] : 0.;

      const int i_set = sleqp_working_set_cons_index(working_set, i_combined);

      const SLEQP_ACTIVE_STATE cons_state
        = sleqp_working_set_cons_state(working_set, i_combined);

      assert(cons_state == SLEQP_INACTIVE || i_set != SLEQP_NONE);

      if (cons_state == SLEQP_ACTIVE_UPPER)
      {
        SLEQP_CALL(sleqp_vec_push(initial_rhs, i_set, upper_value));
      }
      else if (cons_state == SLEQP_ACTIVE_LOWER)
      {
        SLEQP_CALL(sleqp_vec_push(initial_rhs, i_set, lower_value));
      }
      else if (cons_state == SLEQP_ACTIVE_BOTH)
      {
        sleqp_assert_is_eq(lower_value, upper_value, eps);

        SLEQP_CALL(sleqp_vec_push(initial_rhs, i_set, lower_value));
      }

      if (valid_lower)
      {
        ++k_lower;
      }

      if (valid_upper)
      {
        ++k_upper;
      }
    }
  }

  return SLEQP_OKAY;
}

static SLEQP_RETCODE
compute_initial_direction(SleqpWorkingStep* step,
                          SleqpIterate* iterate,
                          SleqpAugJac* jacobian)
{
  SLEQP_CALL(compute_initial_rhs(step, iterate, jacobian));

  SLEQP_CALL(sleqp_aug_jac_solve_min_norm(jacobian,
                                          step->initial_rhs,
                                          step->initial_direction));

#if SLEQP_DEBUG

  // Initial direction must be in working set
  {
    bool in_working_set   = false;
    SleqpProblem* problem = step->problem;

    const double eps = sleqp_settings_real_value(step->settings, SLEQP_SETTINGS_REAL_EPS);

    SLEQP_NUM_ASSERT_PARAM(eps);

    SLEQP_CALL(sleqp_direction_in_working_set(problem,
                                              iterate,
                                              step->initial_direction,
                                              step->dense_cache,
                                              eps,
                                              &in_working_set));

    sleqp_num_assert(in_working_set);
  }

#endif

  return SLEQP_OKAY;
}

// fills all but the Hessian product
static SLEQP_RETCODE
fill_initial_step(SleqpWorkingStep* step, SleqpIterate* iterate)
{
  SleqpProblem* problem     = step->problem;
  SleqpDirection* direction = step->step_direction;
  SleqpVec* primal          = sleqp_direction_primal(direction);

  // gradient
  {
    SleqpVec* obj_grad         = sleqp_iterate_obj_grad(iterate);
    double* direction_obj_grad = sleqp_direction_obj_grad(direction);
    SLEQP_CALL(sleqp_vec_dot(primal, obj_grad, direction_obj_grad));
  }

  // constraint Jacobian
  {
    const SleqpMat* cons_jac = sleqp_iterate_cons_jac(iterate);
    const int num_cons       = sleqp_problem_num_cons(problem);
    SleqpVec* direction_cons = sleqp_direction_cons_jac(direction);

    const double zero_eps
      = sleqp_settings_real_value(step->settings, SLEQP_SETTINGS_REAL_ZERO_EPS);

    SLEQP_CALL(sleqp_mat_mult_vec(cons_jac, primal, step->dense_cache));

    SLEQP_CALL(sleqp_vec_set_from_raw(direction_cons,
                                      step->dense_cache,
                                      num_cons,
                                      zero_eps));
  }

  return SLEQP_OKAY;
}

static SLEQP_RETCODE
compute_initial_step(SleqpWorkingStep* step,
                     SleqpIterate* iterate,
                     double trust_radius)
{
  SleqpDirection* direction = step->step_direction;
  SleqpVec* initial_step    = sleqp_direction_primal(direction);

  SLEQP_CALL(sleqp_vec_copy(step->initial_direction, initial_step));

  const double eps = sleqp_settings_real_value(step->settings, SLEQP_SETTINGS_REAL_EPS);

  const double initial_norm = sleqp_vec_norm(step->initial_direction);

  double alpha = 1.;

  step->initial_step_in_working_set = true;

  if (initial_norm != 0.)
  {
    assert(initial_norm > 0.);

    alpha = (norm_ratio * trust_radius) / (initial_norm);

    alpha = SLEQP_MIN(alpha, 1.);

    if (sleqp_is_eq(alpha, 1., eps))
    {
      // no scaling required...

      const double initial_norm_sq = initial_norm * initial_norm;

      const double trust_radius_sq = trust_radius * trust_radius;

      // sleqp_assert_is_lt(initial_norm_sq, trust_radius_sq, eps);

      step->reduced_trust_radius = sqrt(trust_radius_sq - initial_norm_sq);
    }
    else
    {
      step->initial_step_in_working_set = false;

      SLEQP_CALL(sleqp_vec_scale(initial_step, alpha));

      // we know that the scaled initial solution
      // has norm equal to norm_ratio * trust_radius
      step->reduced_trust_radius
        = trust_radius * sqrt(1. - (norm_ratio * norm_ratio));
    }
  }
  else
  {
    step->reduced_trust_radius = trust_radius;
  }

  SLEQP_CALL(fill_initial_step(step, iterate));

  return SLEQP_OKAY;
}

static SLEQP_RETCODE
compute_initial_cons_val(SleqpWorkingStep* step, SleqpIterate* iterate)
{
  SleqpDirection* direction = step->step_direction;

  const double zero_eps
    = sleqp_settings_real_value(step->settings, SLEQP_SETTINGS_REAL_ZERO_EPS);

  // Compute linearized constraint values at initial direction
  SLEQP_CALL(sleqp_vec_add(sleqp_iterate_cons_val(iterate),
                           sleqp_direction_cons_jac(direction),
                           zero_eps,
                           step->initial_cons_val));

  return SLEQP_OKAY;
}

static SLEQP_RETCODE
compute_violated_multipliers(SleqpWorkingStep* step, SleqpIterate* iterate)
{
  SleqpWorkingSet* working_set = sleqp_iterate_working_set(iterate);

  SleqpProblem* problem = step->problem;

  // Compute violated multipliers
  {
    SLEQP_CALL(
      sleqp_violated_cons_multipliers(problem,
                                      step->initial_cons_val,
                                      step->violated_constraint_multipliers,
                                      working_set));

    sleqp_log_debug("Violated constraints at initial Newton step: %d",
                    step->violated_constraint_multipliers->nnz);
  }

  return SLEQP_OKAY;
}

SLEQP_RETCODE
sleqp_working_step_set_iterate(SleqpWorkingStep* step,
                               SleqpIterate* iterate,
                               SleqpAugJac* jacobian,
                               double trust_radius)
{
  SLEQP_CALL(compute_initial_direction(step, iterate, jacobian));

  SLEQP_CALL(compute_initial_step(step, iterate, trust_radius));

  SLEQP_CALL(compute_initial_cons_val(step, iterate));

  SLEQP_CALL(compute_violated_multipliers(step, iterate));

  SLEQP_CALL(sleqp_iterate_release(&step->iterate));

  SLEQP_CALL(sleqp_iterate_capture(iterate));
  step->iterate = iterate;

  return SLEQP_OKAY;
}

SleqpVec*
sleqp_working_step_get_step(SleqpWorkingStep* step)
{
  return sleqp_direction_primal(step->step_direction);
}

SleqpDirection*
sleqp_working_step_direction(SleqpWorkingStep* step)
{
  return step->step_direction;
}

double
sleqp_working_step_reduced_trust_radius(SleqpWorkingStep* step)
{
  return step->reduced_trust_radius;
}

bool
sleqp_working_step_in_working_set(SleqpWorkingStep* step)
{
  return step->initial_step_in_working_set;
}

SleqpVec*
sleqp_working_step_violated_cons_multipliers(SleqpWorkingStep* step)
{
  return step->violated_constraint_multipliers;
}

SLEQP_NODISCARD
SLEQP_RETCODE
sleqp_working_step_set_multipliers(SleqpWorkingStep* step,
                                   const SleqpVec* multipliers)
{
  SleqpProblem* problem = step->problem;
  SleqpVec* primal      = sleqp_direction_primal(step->step_direction);

  SleqpVec* direction_hess = sleqp_direction_hess(step->step_direction);

  SLEQP_CALL(sleqp_problem_hess_prod(problem,
                                     primal,
                                     multipliers,
                                     direction_hess));

  return SLEQP_OKAY;
}

static SLEQP_RETCODE
working_step_free(SleqpWorkingStep** star)
{
  SleqpWorkingStep* step = *star;

  if (!step)
  {
    return SLEQP_OKAY;
  }

  sleqp_free(&step->dense_cache);

  SLEQP_CALL(sleqp_vec_free(&step->sparse_cache));

  SLEQP_CALL(sleqp_vec_free(&step->violated_constraint_multipliers));

  SLEQP_CALL(sleqp_vec_free(&step->initial_cons_val));

  SLEQP_CALL(sleqp_direction_release(&step->step_direction));

  SLEQP_CALL(sleqp_vec_free(&step->initial_direction));
  SLEQP_CALL(sleqp_vec_free(&step->initial_rhs));

  SLEQP_CALL(sleqp_vec_free(&step->upper_diff));
  SLEQP_CALL(sleqp_vec_free(&step->lower_diff));

  SLEQP_CALL(sleqp_iterate_release(&step->iterate));

  SLEQP_CALL(sleqp_settings_release(&step->settings));

  SLEQP_CALL(sleqp_problem_release(&step->problem));

  sleqp_free(star);

  return SLEQP_OKAY;
}

SLEQP_RETCODE
sleqp_working_step_capture(SleqpWorkingStep* step)
{
  ++step->refcount;

  return SLEQP_OKAY;
}

SLEQP_RETCODE
sleqp_working_step_release(SleqpWorkingStep** star)
{
  SleqpWorkingStep* step = *star;

  if (!step)
  {
    return SLEQP_OKAY;
  }

  if (--step->refcount == 0)
  {
    SLEQP_CALL(working_step_free(star));
  }

  *star = NULL;

  return SLEQP_OKAY;
}
