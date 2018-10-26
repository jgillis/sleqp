#include "sleqp_solver.h"

#include <assert.h>
#include <math.h>

#include "sleqp_cmp.h"
#include "sleqp_mem.h"

#include "sleqp_defs.h"

#include "sleqp_aug_jacobian.h"

#include "sleqp_cauchy.h"
#include "sleqp_dual_estimation.h"

#include "sleqp_newton.h"

#include "sleqp_soc.h"

#include "sleqp_iterate.h"
#include "sleqp_merit.h"

#include "lp/sleqp_lpi.h"
#include "lp/sleqp_lpi_soplex.h"

struct SleqpSolver
{
  SleqpProblem* problem;

  SleqpIterate* iterate;

  SleqpLPi* lp_interface;

  SleqpCauchyData* cauchy_data;

  SleqpSparseVec* cauchy_direction;

  SleqpSparseVec* cauchy_hessian_direction;

  SleqpSparseVec* cauchy_step;
  double cauchy_step_length;

  SleqpNewtonData* newton_data;

  SleqpSparseVec* newton_step;

  SleqpSparseVec* cauchy_newton_direction;

  SleqpSparseVec* cauchy_newton_hessian_direction;

  SleqpSparseVec* trial_direction;

  SleqpIterate* trial_iterate;

  SleqpAugJacobian* aug_jacobian;

  SleqpDualEstimationData* estimation_data;
  SleqpSparseVec* estimation_residuum;

  SleqpMeritData* merit_data;

  SleqpSparseVec* linear_merit_gradient;

  SleqpSOCData* soc_data;

  SleqpSparseVec* soc_direction;

  SleqpSparseVec* soc_corrected_direction;

  SleqpSparseVec* cons_diff;

  // parameters, adjusted throughout...

  double trust_radius;

  double lp_trust_radius;

  double penalty_parameter;
};

const double accepted_reduction = 1e-8;

SLEQP_RETCODE sleqp_solver_create(SleqpSolver** star,
                                  SleqpProblem* problem,
                                  SleqpSparseVec* x)
{
  SLEQP_CALL(sleqp_malloc(star));

  SleqpSolver* solver = *star;
  int num_constraints = problem->num_constraints;
  int num_variables = problem->num_variables;

  solver->problem = problem;

  SleqpSparseVec* xclip;

  assert(sleqp_sparse_vector_valid(x));

  SLEQP_CALL(sleqp_sparse_vector_clip(x,
                                      solver->problem->var_lb,
                                      solver->problem->var_ub,
                                      &xclip));

  SLEQP_CALL(sleqp_iterate_create(&solver->iterate,
                                  solver->problem,
                                  xclip));

  // TODO: make this generic at a later point...

  int num_lp_variables = num_variables + 2*num_constraints;
  int num_lp_constraints = num_constraints;

  SLEQP_CALL(sleqp_lpi_soplex_create_interface(&solver->lp_interface,
                                               num_lp_variables,
                                               num_lp_constraints));

  SLEQP_CALL(sleqp_cauchy_data_create(&solver->cauchy_data,
                                      problem,
                                      solver->lp_interface));

  SLEQP_CALL(sleqp_sparse_vector_create(&solver->cauchy_direction,
                                        num_variables,
                                        0));

  SLEQP_CALL(sleqp_sparse_vector_create(&solver->cauchy_hessian_direction,
                                        num_variables,
                                        0));

  SLEQP_CALL(sleqp_sparse_vector_create(&solver->cauchy_step,
                                        num_variables,
                                        0));

  SLEQP_CALL(sleqp_newton_data_create(&solver->newton_data,
                                      problem));

  SLEQP_CALL(sleqp_sparse_vector_create(&solver->newton_step,
                                        num_variables,
                                        0));

  SLEQP_CALL(sleqp_sparse_vector_create(&solver->cauchy_newton_direction,
                                        num_variables,
                                        0));

  SLEQP_CALL(sleqp_sparse_vector_create(&solver->cauchy_newton_hessian_direction,
                                        num_variables,
                                        0));

  SLEQP_CALL(sleqp_sparse_vector_create(&solver->trial_direction,
                                        num_variables,
                                        0));

  SLEQP_CALL(sleqp_iterate_create(&solver->trial_iterate,
                                  solver->problem,
                                  solver->iterate->x));

  SLEQP_CALL(sleqp_aug_jacobian_create(&solver->aug_jacobian, problem));

  SLEQP_CALL(sleqp_dual_estimation_data_create(&solver->estimation_data,
                                               problem));

  SLEQP_CALL(sleqp_sparse_vector_create(&solver->estimation_residuum,
                                        num_variables,
                                        0));

  SLEQP_CALL(sleqp_merit_data_create(&solver->merit_data,
                                     problem,
                                     problem->func));

  SLEQP_CALL(sleqp_sparse_vector_create(&solver->linear_merit_gradient,
                                        num_variables,
                                        0));

  SLEQP_CALL(sleqp_soc_data_create(&solver->soc_data,
                                   problem));

  SLEQP_CALL(sleqp_sparse_vector_create(&solver->soc_direction,
                                        num_variables,
                                        0));

  SLEQP_CALL(sleqp_sparse_vector_create(&solver->soc_corrected_direction,
                                        num_variables,
                                        0));

  SLEQP_CALL(sleqp_sparse_vector_create(&solver->cons_diff,
                                        num_constraints,
                                        0));

  // TODO: Set this to something different?!
  solver->trust_radius = 1.;
  solver->lp_trust_radius = 1.;

  // suggested by authors
  solver->penalty_parameter = 10.;

  return SLEQP_OKAY;
}

static SLEQP_RETCODE update_penalty_parameter(SleqpSolver* solver)
{
  SleqpProblem* problem = solver->problem;
  SleqpIterate* iterate = solver->iterate;

  int num_constraints = problem->num_constraints;

  const double violation_tolerance = 1e-8;
  const double min_decrease = .1;
  const int max_increases = 100;

  const double penalty_increase = 10.;

  if(num_constraints == 0)
  {
    return SLEQP_OKAY;
  }

  SleqpCauchyData* cauchy_data = solver->cauchy_data;

  double current_violation;

  sleqp_cauchy_get_violation(cauchy_data, &current_violation);

  current_violation /= num_constraints;

  if(current_violation <= violation_tolerance)
  {
    return SLEQP_OKAY;
  }

  sleqp_log_debug("Updating penalty parameter");

  SLEQP_CALL(sleqp_cauchy_solve(cauchy_data,
                                NULL,
                                solver->penalty_parameter));

  double inf_violation;

  sleqp_cauchy_get_violation(cauchy_data, &inf_violation);

  inf_violation /= num_constraints;

  if(inf_violation <= violation_tolerance)
  {
    for(int i = 0; i < max_increases; ++i)
    {
      solver->penalty_parameter *= penalty_increase;

      SLEQP_CALL(sleqp_cauchy_solve(cauchy_data,
                                    iterate->func_grad,
                                    solver->penalty_parameter));

      double next_violation;

      sleqp_cauchy_get_violation(cauchy_data, &next_violation);

      next_violation /= num_constraints;

      if(next_violation <= violation_tolerance)
      {
        return SLEQP_OKAY;
      }
    }
  }
  else
  {
    for(int i = 0; i < max_increases; ++i)
    {
      solver->penalty_parameter *= penalty_increase;

      SLEQP_CALL(sleqp_cauchy_solve(cauchy_data,
                                    iterate->func_grad,
                                    solver->penalty_parameter));

      double next_violation;

      sleqp_cauchy_get_violation(cauchy_data, &next_violation);

      next_violation /= num_constraints;

      if((current_violation - next_violation) >= min_decrease*(current_violation - inf_violation))
      {
        return SLEQP_OKAY;
      }
    }
  }


  return SLEQP_OKAY;
}

static SLEQP_RETCODE update_lp_trust_radius(bool trial_step_accepted,
                                            double trial_direction_norm,
                                            double cauchy_step_norm,
                                            double cauchy_step_length,
                                            double* lp_trust_radius)
{
  if(trial_step_accepted)
  {
    double norm_increase_factor = 1.2;

    trial_direction_norm *= norm_increase_factor;
    cauchy_step_norm *= norm_increase_factor;

    double scaled_trust_radius = .1 * (*lp_trust_radius);

    double update_lhs = SLEQP_MAX(trial_direction_norm,
                                  cauchy_step_norm);

    update_lhs = SLEQP_MAX(update_lhs, scaled_trust_radius);

    if(sleqp_eq(cauchy_step_length, 1.))
    {
      (*lp_trust_radius) *= 7.;
    }

    *lp_trust_radius = SLEQP_MIN(update_lhs, *lp_trust_radius);

  }
  else
  {
    double half_norm = .5 * trial_direction_norm;
    double small_radius = .1 * (*lp_trust_radius);

    double reduced_radius = SLEQP_MAX(half_norm, small_radius);

    *lp_trust_radius = SLEQP_MIN(reduced_radius, *lp_trust_radius);
  }

  return SLEQP_OKAY;
}

static SLEQP_RETCODE update_trust_radius(double reduction_ratio,
                                         bool trial_step_accepted,
                                         double direction_norm,
                                         double* trust_radius)
{
  if(reduction_ratio >= 0.9)
  {
    *trust_radius = SLEQP_MAX(*trust_radius, 7*direction_norm);
  }
  else if(reduction_ratio >= 0.3)
  {
    *trust_radius = SLEQP_MAX(*trust_radius, 2*direction_norm);
  }
  else if(trial_step_accepted)
  {
    // stays the same
  }
  else
  {
    *trust_radius = SLEQP_MIN(0.5*(*trust_radius),
                              0.5*direction_norm);
  }

  return SLEQP_OKAY;
}

static SLEQP_RETCODE compute_trial_direction(SleqpSolver* solver,
                                             double* quadratic_reduction)
{
  SLEQP_CALL(sleqp_merit_linear_gradient(solver->merit_data,
                                         solver->iterate,
                                         solver->cauchy_step,
                                         solver->penalty_parameter,
                                         solver->linear_merit_gradient));

  double prod_chc;
  double prod_che;
  double prod_ehe;

  SLEQP_CALL(sleqp_sparse_vector_dot(solver->cauchy_step,
                                     solver->cauchy_hessian_direction,
                                     &prod_chc));

  SLEQP_CALL(sleqp_sparse_vector_dot(solver->cauchy_step,
                                     solver->cauchy_newton_hessian_direction,
                                     &prod_che));

  SLEQP_CALL(sleqp_sparse_vector_dot(solver->cauchy_newton_direction,
                                     solver->cauchy_newton_hessian_direction,
                                     &prod_ehe));

  // holds the product of the gradient of the quadratic model
  // at alpha = 0 with the step direction
  double gradient_product = 0.;

  {
    double temp;

    SLEQP_CALL(sleqp_sparse_vector_dot(solver->cauchy_newton_direction,
                                       solver->linear_merit_gradient,
                                       &temp));

    gradient_product = temp + prod_chc;
  }

  // holds the value of the quadratic model at alpha = 0
  double zero_func_val = 0.;

  {
    SLEQP_CALL(sleqp_merit_linear(solver->merit_data,
                                  solver->iterate,
                                  solver->cauchy_step,
                                  solver->penalty_parameter,
                                  &zero_func_val));

    zero_func_val += .5 * prod_chc;
  }

  int max_it = 100;

  double alpha = 1.;

  // compute the maximum trial step length
  {
    SleqpProblem* problem = solver->problem;

    SLEQP_CALL(sleqp_sparse_vector_add(solver->iterate->x,
                                       solver->cauchy_step,
                                       1., 1.,
                                       solver->trial_direction));

    SLEQP_CALL(sleqp_max_step_length(solver->trial_direction,
                                     solver->cauchy_newton_direction,
                                     problem->var_lb,
                                     problem->var_ub,
                                     &alpha));
  }

  // TODO: Make these adjustable

  double eta = 1e-3;
  double tau = 0.9;

  for(int i = 0; i < max_it; ++i)
  {
    // holds the value of the quadratic model at alpha
    double alpha_func_val = 0.;

    SLEQP_CALL(sleqp_sparse_vector_add(solver->cauchy_step,
                                       solver->cauchy_newton_direction,
                                       1., alpha,
                                       solver->trial_direction));

    {
      double temp;

      SLEQP_CALL(sleqp_merit_linear(solver->merit_data,
                                    solver->iterate,
                                    solver->trial_direction,
                                    solver->penalty_parameter,
                                    &temp));

      alpha_func_val += temp;

      alpha_func_val += 0.5 * prod_chc;

      alpha_func_val += alpha * prod_che;

      alpha_func_val += 0.5*(alpha*alpha) * prod_ehe;
    }

    (*quadratic_reduction) = zero_func_val - alpha_func_val;

    if(alpha_func_val <= zero_func_val + eta*alpha*gradient_product)
    {
      break;
    }

    alpha *= tau;
  }


  return SLEQP_OKAY;
}

static bool should_terminate(SleqpSolver* solver)
{
  SleqpProblem* problem = solver->problem;
  SleqpIterate* iterate = solver->iterate;

  const double optimality_residuum = sleqp_sparse_vector_norminf(solver->estimation_residuum);

  double multiplier_norm = 0.;

  {
    multiplier_norm += sleqp_sparse_vector_normsq(iterate->cons_dual);
    multiplier_norm += sleqp_sparse_vector_normsq(iterate->vars_dual);

    multiplier_norm = sqrt(multiplier_norm);
  }

  double slackness_residuum = 0.;

  {
    SleqpSparseVec* cons_val = iterate->cons_val;
    SleqpSparseVec* cons_dual = iterate->cons_dual;

    int k_v = 0, k_d = 0;

    while(k_v < cons_val->nnz || k_d < cons_dual->nnz)
    {
      bool valid_val = (k_v < cons_val->nnz);
      bool valid_dual = (k_d < cons_dual->nnz);

      int i_first = valid_val ? cons_val->indices[k_v] : cons_val->dim + 1;
      int i_second = valid_dual ? cons_dual->indices[k_d] : cons_dual->dim + 1;

      int i_combined = SLEQP_MIN(i_first, i_second);

      valid_val = valid_val && i_first == i_combined;
      valid_dual = valid_dual && i_second == i_combined;

      double value = valid_val ? cons_val->data[k_v] : 0.;
      double dual = valid_dual ? cons_dual->data[k_d] : 0.;

      double current_residuum = value * dual;

      current_residuum = SLEQP_ABS(current_residuum);

      slackness_residuum = SLEQP_MAX(current_residuum, slackness_residuum);

      if(valid_val)
      {
        ++k_v;
      }

      if(valid_dual)
      {
        ++k_d;
      }
    }
  }

  const double optimality_tolerance = 1e-6;

  {
    double residuum = SLEQP_MAX(optimality_residuum, slackness_residuum);

    sleqp_log_info("Residuum: %f", residuum);

    if(residuum >= optimality_tolerance * (1 + multiplier_norm))
    {
      return false;
    }
  }

  double cons_residuum = 0.;

  {
    SleqpSparseVec* cons_diff = solver->cons_diff;

    SLEQP_CALL(sleqp_sparse_vector_add(problem->cons_ub,
                                       iterate->cons_val,
                                       -1.,
                                       1.,
                                       cons_diff));

    for(int k = 0; k < cons_diff->nnz; ++k)
    {
      double current_residuum = SLEQP_MAX(cons_diff->data[k], 0.);

      cons_residuum = SLEQP_MAX(cons_residuum, current_residuum);
    }

    SLEQP_CALL(sleqp_sparse_vector_add(problem->cons_lb,
                                       iterate->cons_val,
                                       1.,
                                       -1.,
                                       cons_diff));

    for(int k = 0; k < cons_diff->nnz; ++k)
    {
      double current_residuum = SLEQP_MAX(cons_diff->data[k], 0.);

      cons_residuum = SLEQP_MAX(cons_residuum, current_residuum);
    }
  }

  return (cons_residuum < optimality_tolerance * (1 + multiplier_norm));
}

static SLEQP_RETCODE compute_trial_point(SleqpSolver* solver,
                                         double* quadratic_reduction)
{
  SleqpProblem* problem = solver->problem;
  SleqpIterate* iterate = solver->iterate;

  double one = 1.;

  // compute Cauchy direction / step and dual estimation
  {

    SLEQP_CALL(sleqp_cauchy_set_iterate(solver->cauchy_data,
                                        iterate,
                                        solver->lp_trust_radius));

    SLEQP_CALL(sleqp_cauchy_solve(solver->cauchy_data,
                                  iterate->func_grad,
                                  solver->penalty_parameter));

    SLEQP_CALL(sleqp_cauchy_get_active_set(solver->cauchy_data,
                                           iterate,
                                           solver->lp_trust_radius));

    SLEQP_CALL(sleqp_aug_jacobian_set_iterate(solver->aug_jacobian,
                                              iterate));

    SLEQP_CALL(sleqp_cauchy_get_direction(solver->cauchy_data,
                                          iterate,
                                          solver->cauchy_direction));

    SLEQP_CALL(sleqp_dual_estimation_compute(solver->estimation_data,
                                             iterate,
                                             solver->estimation_residuum,
                                             solver->aug_jacobian));

    SLEQP_CALL(sleqp_func_hess_product(problem->func,
                                       &one,
                                       solver->cauchy_direction,
                                       iterate->cons_dual,
                                       solver->cauchy_hessian_direction));

    SLEQP_CALL(sleqp_sparse_vector_copy(solver->cauchy_direction,
                                        solver->cauchy_step));

    SLEQP_CALL(sleqp_cauchy_compute_step(solver->cauchy_data,
                                         iterate,
                                         solver->penalty_parameter,
                                         solver->cauchy_hessian_direction,
                                         solver->cauchy_step,
                                         &solver->cauchy_step_length));
  }

  // compute Newton step
  {
    SLEQP_CALL(sleqp_newton_compute_step(solver->newton_data,
                                         solver->iterate,
                                         solver->aug_jacobian,
                                         solver->trust_radius,
                                         solver->penalty_parameter,
                                         solver->newton_step));

  }

  // compute Cauchy Newton direction (from Cauchy point to Newton point)
  {
    SLEQP_CALL(sleqp_sparse_vector_add(solver->newton_step,
                                       solver->cauchy_step,
                                       1.,
                                       -1.,
                                       solver->cauchy_newton_direction));

    SLEQP_CALL(sleqp_func_hess_product(problem->func,
                                       &one,
                                       solver->cauchy_newton_direction,
                                       iterate->cons_dual,
                                       solver->cauchy_newton_hessian_direction));
  }

  SLEQP_CALL(compute_trial_direction(solver, quadratic_reduction));

  SLEQP_CALL(sleqp_sparse_vector_add(iterate->x,
                                     solver->trial_direction,
                                     1., 1.,
                                     solver->trial_iterate->x));

  return SLEQP_OKAY;
}

static SLEQP_RETCODE perform_soc(SleqpSolver* solver)
{
  SleqpProblem* problem = solver->problem;
  SleqpIterate* iterate = solver->iterate;
  SleqpIterate* trial_iterate = solver->trial_iterate;
  SleqpSparseVec* trial_point = trial_iterate->x;

  // TODO: evaluate only the required (= active) constraint values,

  SLEQP_CALL(sleqp_func_eval(problem->func,
                             NULL,
                             NULL,
                             NULL,
                             trial_iterate->cons_val,
                             NULL));

  SLEQP_CALL(sleqp_soc_compute(solver->soc_data,
                               solver->aug_jacobian,
                               iterate,
                               trial_iterate,
                               solver->soc_direction));

  double max_step_length = 1.;

  SLEQP_CALL(sleqp_max_step_length(trial_point,
                                   solver->soc_direction,
                                   problem->var_lb,
                                   problem->var_ub,
                                   &max_step_length));

  SLEQP_CALL(sleqp_sparse_vector_add(solver->trial_direction,
                                     solver->soc_direction,
                                     1.,
                                     max_step_length,
                                     solver->soc_corrected_direction));

  SLEQP_CALL(sleqp_sparse_vector_add(iterate->x,
                                     solver->soc_corrected_direction,
                                     1.,
                                     1.,
                                     trial_point));


  return SLEQP_OKAY;
}

static SLEQP_RETCODE set_func_value(SleqpSolver* solver,
                                    SleqpIterate* iterate)
{
  SleqpProblem* problem = solver->problem;

  int func_grad_nnz = 0;
  int cons_val_nnz = 0;
  int cons_jac_nnz = 0;

  SLEQP_CALL(sleqp_func_set_value(problem->func,
                                  iterate->x,
                                  &func_grad_nnz,
                                  &cons_val_nnz,
                                  &cons_jac_nnz));

  SLEQP_CALL(sleqp_sparse_vector_reserve(iterate->func_grad, func_grad_nnz));

  SLEQP_CALL(sleqp_sparse_vector_reserve(iterate->cons_val, cons_val_nnz));

  SLEQP_CALL(sleqp_sparse_matrix_reserve(iterate->cons_jac, cons_jac_nnz));

  return SLEQP_OKAY;
}

static SLEQP_RETCODE sleqp_perform_iteration(SleqpSolver* solver)
{
  SleqpProblem* problem = solver->problem;
  SleqpIterate* iterate = solver->iterate;
  SleqpIterate* trial_iterate = solver->trial_iterate;

  double quadratic_reduction = 0.;

  SLEQP_CALL(compute_trial_point(solver, &quadratic_reduction));

  assert(!sleqp_neg(quadratic_reduction));

  set_func_value(solver, trial_iterate);

  SLEQP_CALL(sleqp_func_eval(problem->func,
                             NULL,
                             &trial_iterate->func_val,
                             NULL,
                             NULL,
                             NULL));

  double actual_reduction = iterate->func_val - trial_iterate->func_val;

  double reduction_ratio = actual_reduction / quadratic_reduction;

  sleqp_log_debug("Current function value: %f, trial function value: %f",
                  iterate->func_val,
                  trial_iterate->func_val);

  sleqp_log_debug("Reduction ratio: %f, actual: %f, quadratic: %f",
                  reduction_ratio,
                  actual_reduction,
                  quadratic_reduction);

  double direction_norm = sleqp_sparse_vector_normsq(solver->trial_direction);

  direction_norm = sqrt(direction_norm);

  bool step_accepted = true;

  if(reduction_ratio >= accepted_reduction)
  {
    sleqp_log_debug("Trial step accepted");
  }
  else
  {
    sleqp_log_debug("Step rejected");

    step_accepted = false;

    /*
    sleqp_log_debug("Trial step rejected, performing SOC");

    SLEQP_CALL(perform_soc(solver));

    SLEQP_CALL(sleqp_func_set_value(problem->func,
                                    trial_iterate->x,
                                    &func_grad_nnz,
                                    &cons_val_nnz,
                                    &cons_jac_nnz));

    SLEQP_CALL(sleqp_sparse_vector_reserve(trial_iterate->func_grad, func_grad_nnz));

    SLEQP_CALL(sleqp_sparse_vector_reserve(trial_iterate->cons_val, cons_val_nnz));

    SLEQP_CALL(sleqp_sparse_matrix_reserve(trial_iterate->cons_jac, cons_jac_nnz));

    SLEQP_CALL(sleqp_func_eval(problem->func,
                               NULL,
                               &trial_iterate->func_val,
                               NULL,
                               NULL,
                               NULL));

    double soc_reduction = iterate->func_val - trial_iterate->func_val;

    double soc_reduction_ratio = actual_reduction / quadratic_reduction;

    if(soc_reduction_ratio >= accepted_reduction)
    {
      sleqp_log_debug("SOC-corrected step accepted");

      step_accepted = true;
    }
    else
    {
      sleqp_log_debug("Step rejected");
      step_accepted = false;
    }
    */
  }

  // update trust radii, penalty parameter
  {
    SLEQP_CALL(update_trust_radius(reduction_ratio,
                                   step_accepted,
                                   direction_norm,
                                   &(solver->trust_radius)));

    double trial_direction_norm = sleqp_sparse_vector_norminf(solver->trial_direction);
    double cauchy_step_norm = sleqp_sparse_vector_norminf(solver->cauchy_step);

    SLEQP_CALL(update_lp_trust_radius(step_accepted,
                                      trial_direction_norm,
                                      cauchy_step_norm,
                                      solver->cauchy_step_length,
                                      &(solver->lp_trust_radius)));

    SLEQP_CALL(update_penalty_parameter(solver));
  }

  // update current iterate

  if(step_accepted)
  {
    // get the remaining data to fill the iterate

    SLEQP_CALL(sleqp_func_eval(problem->func,
                               NULL,
                               NULL,
                               trial_iterate->func_grad,
                               trial_iterate->cons_val,
                               trial_iterate->cons_jac));

    solver->trial_iterate = iterate;
    solver->iterate = trial_iterate;
  }
  else
  {
    set_func_value(solver, iterate);
  }

  return SLEQP_OKAY;
}

SLEQP_RETCODE sleqp_solve(SleqpSolver* solver,
                          int max_num_iterations)
{
  SleqpProblem* problem = solver->problem;
  SleqpIterate* iterate = solver->iterate;

  sleqp_log_info("Solving a problem with %d variables, %d constraints",
                 problem->num_variables,
                 problem->num_constraints);

  SLEQP_CALL(sleqp_set_and_evaluate(problem, iterate));

  sleqp_log_info("Initial function value: %f",
                 solver->iterate->func_val);

  for(int i = 0; i < max_num_iterations; ++i)
  {
    SLEQP_CALL(sleqp_perform_iteration(solver));

    sleqp_log_info("Iteration %d, function value: %f, D_LP: %f, D_EQP: %f",
                   (i + 1),
                   solver->iterate->func_val,
                   solver->lp_trust_radius,
                   solver->trust_radius);

    if(should_terminate(solver))
    {
      break;
    }
  }

  sleqp_log_info("Final solution: ");

  SLEQP_CALL(sleqp_sparse_vector_fprintf(solver->iterate->x, stdout));

  return SLEQP_OKAY;
}

SLEQP_RETCODE sleqp_solver_free(SleqpSolver** star)
{
  SleqpSolver* solver = *star;
  SLEQP_CALL(sleqp_sparse_vector_free(&solver->cons_diff));

  SLEQP_CALL(sleqp_sparse_vector_free(&solver->soc_corrected_direction));

  SLEQP_CALL(sleqp_sparse_vector_free(&solver->soc_direction));

  SLEQP_CALL(sleqp_soc_data_free(&solver->soc_data));

  SLEQP_CALL(sleqp_sparse_vector_free(&solver->linear_merit_gradient));

  SLEQP_CALL(sleqp_merit_data_free(&solver->merit_data));

  SLEQP_CALL(sleqp_sparse_vector_free(&solver->estimation_residuum));

  SLEQP_CALL(sleqp_dual_estimation_data_free(&solver->estimation_data));

  SLEQP_CALL(sleqp_aug_jacobian_free(&solver->aug_jacobian));

  SLEQP_CALL(sleqp_iterate_free(&solver->trial_iterate));

  SLEQP_CALL(sleqp_sparse_vector_free(&solver->trial_direction));

  SLEQP_CALL(sleqp_sparse_vector_free(&solver->cauchy_newton_hessian_direction));

  SLEQP_CALL(sleqp_sparse_vector_free(&solver->cauchy_newton_direction));

  SLEQP_CALL(sleqp_sparse_vector_free(&solver->newton_step));

  SLEQP_CALL(sleqp_newton_data_free(&solver->newton_data));

  SLEQP_CALL(sleqp_sparse_vector_free(&solver->cauchy_step));

  SLEQP_CALL(sleqp_sparse_vector_free(&solver->cauchy_hessian_direction));

  SLEQP_CALL(sleqp_sparse_vector_free(&solver->cauchy_direction));

  SLEQP_CALL(sleqp_cauchy_data_free(&solver->cauchy_data));

  SLEQP_CALL(sleqp_lpi_free(&solver->lp_interface));

  SLEQP_CALL(sleqp_iterate_free(&solver->iterate));

  sleqp_free(star);

  return SLEQP_OKAY;
}
