#include "deriv_check.h"

#include "cmp.h"
#include "log.h"
#include "mem.h"
#include "problem.h"
#include "sparse/sparse_matrix.h"

struct SleqpDerivCheckData
{
  SleqpProblem* problem;
  SleqpParams* params;

  SleqpSparseVec* unit_direction;

  SleqpSparseVec* hessian_estimate;

  SleqpSparseVec* hessian_left;
  SleqpSparseVec* hessian_right;

  SleqpSparseVec* hessian_prod;

  SleqpSparseVec* hessian_prod_zero;
  SleqpSparseVec* hessian_prod_cache;

  SleqpSparseVec* cons_grad_iterate;
  SleqpSparseVec* cons_grad_check_iterate;

  SleqpSparseVec* transposed_jacobian_product;
  SleqpSparseVec* combined_cons_grad_iterate;
  SleqpSparseVec* combined_cons_grad_check_iterate;

  SleqpSparseVec* multipliers;
  SleqpSparseVec* multipliers_zero;

  SleqpIterate* iterate;
  SleqpIterate* check_iterate;
};

static SLEQP_RETCODE restore_iterate(SleqpDerivCheckData* data,
                                     SleqpIterate* iterate)
{
  SleqpProblem* problem = data->problem;

  bool reject = false;

  int func_grad_nnz = 0;
  int cons_val_nnz = 0;
  int cons_jac_nnz = 0;

  SLEQP_CALL(sleqp_problem_set_value(problem,
                                     sleqp_iterate_get_primal(iterate),
                                     SLEQP_VALUE_REASON_CHECKING_DERIV,
                                     &reject,
                                     &func_grad_nnz,
                                     &cons_val_nnz,
                                     &cons_jac_nnz));

  if(reject)
  {
    sleqp_log_error("Function rejected restoration after derivative check");
  }

  return SLEQP_OKAY;
}

static SLEQP_RETCODE set_check_iterate(SleqpDerivCheckData* data,
                                       bool* reject)
{
  SleqpIterate* check_iterate = data->check_iterate;
  SleqpProblem* problem = data->problem;

  int func_grad_nnz = 0;
  int cons_val_nnz = 0;
  int cons_jac_nnz = 0;

  SLEQP_CALL(sleqp_problem_set_value(problem,
                                     sleqp_iterate_get_primal(check_iterate),
                                     SLEQP_VALUE_REASON_CHECKING_DERIV,
                                     reject,
                                     &func_grad_nnz,
                                     &cons_val_nnz,
                                     &cons_jac_nnz));

  SLEQP_CALL(sleqp_sparse_vector_reserve(sleqp_iterate_get_func_grad(check_iterate),
                                         func_grad_nnz));

  SLEQP_CALL(sleqp_sparse_vector_reserve(sleqp_iterate_get_cons_val(check_iterate),
                                         cons_val_nnz));

  SLEQP_CALL(sleqp_sparse_matrix_reserve(sleqp_iterate_get_cons_jac(check_iterate),
                                         cons_jac_nnz));

  return SLEQP_OKAY;
}

static double get_perturbation(const SleqpDerivCheckData* data,
                               const SleqpIterate* iterate,
                               int j)
{
  SleqpParams* params = data->params;

  SleqpSparseVec* primal = sleqp_iterate_get_primal(iterate);

  double base_perturbation = sleqp_params_get(params,
                                              SLEQP_PARAM_DERIV_PERTURBATION);

  double value = sleqp_sparse_vector_value_at(primal, j);

  value = SLEQP_ABS(value);

  value = SLEQP_MAX(value, 1.);

  return value * base_perturbation;
}

static SLEQP_RETCODE create_perturbed_unit_direction(SleqpDerivCheckData* data,
                                                     const SleqpIterate* iterate,
                                                     const int j,
                                                     double* perturbation)
{
  SleqpSparseVec* unit_direction = data->unit_direction;

  *perturbation = get_perturbation(data, iterate, j);

  SLEQP_CALL(sleqp_sparse_vector_clear(unit_direction));

  SLEQP_CALL(sleqp_sparse_vector_push(unit_direction, j, *perturbation));

  return SLEQP_OKAY;
}

static SLEQP_RETCODE create_check_iterate(SleqpDerivCheckData* data,
                                          const SleqpIterate* iterate,
                                          const int j,
                                          double* perturbation)
{
  SleqpIterate* check_iterate = data->check_iterate;
  SleqpSparseVec* unit_direction = data->unit_direction;

  const double zero_eps = sleqp_params_get(data->params,
                                           SLEQP_PARAM_ZERO_EPS);

  SLEQP_CALL(create_perturbed_unit_direction(data,
                                             iterate,
                                             j,
                                             perturbation));

  SLEQP_CALL(sleqp_sparse_vector_add(sleqp_iterate_get_primal(iterate),
                                     unit_direction,
                                     zero_eps,
                                     sleqp_iterate_get_primal(check_iterate)));

  return SLEQP_OKAY;
}

SLEQP_RETCODE sleqp_deriv_checker_create(SleqpDerivCheckData** star,
                                         SleqpProblem* problem,
                                         SleqpParams* params)
{
  SLEQP_CALL(sleqp_malloc(star));

  SleqpDerivCheckData* data = *star;

  const int num_constraints = sleqp_problem_num_constraints(problem);
  const int num_variables = sleqp_problem_num_variables(problem);

  data->problem = problem;
  SLEQP_CALL(sleqp_problem_capture(data->problem));

  SLEQP_CALL(sleqp_params_capture(params));
  data->params = params;

  data->iterate = NULL;

  // this will be a unit vector
  SLEQP_CALL(sleqp_sparse_vector_create(&data->unit_direction,
                                        num_variables,
                                        1));

  SLEQP_CALL(sleqp_sparse_vector_create_empty(&data->hessian_estimate,
                                              num_variables));

  SLEQP_CALL(sleqp_sparse_vector_create(&data->hessian_left,
                                        num_variables,
                                        1));

  SLEQP_CALL(sleqp_sparse_vector_create(&data->hessian_right,
                                        num_variables,
                                        1));

  SLEQP_CALL(sleqp_sparse_vector_create_empty(&data->hessian_prod,
                                              num_variables));

  SLEQP_CALL(sleqp_sparse_vector_create_empty(&data->hessian_prod_zero,
                                              num_variables));

  SLEQP_CALL(sleqp_sparse_vector_create_empty(&data->hessian_prod_cache,
                                              num_variables));

  SLEQP_CALL(sleqp_sparse_vector_create_empty(&data->cons_grad_iterate,
                                              num_variables));

  SLEQP_CALL(sleqp_sparse_vector_create_empty(&data->cons_grad_check_iterate,
                                              num_variables));

  SLEQP_CALL(sleqp_sparse_vector_create_empty(&data->transposed_jacobian_product,
                                              num_variables));

  SLEQP_CALL(sleqp_sparse_vector_create_empty(&data->combined_cons_grad_iterate,
                                              num_variables));

  SLEQP_CALL(sleqp_sparse_vector_create_empty(&data->combined_cons_grad_check_iterate,
                                              num_variables));

  SLEQP_CALL(sleqp_sparse_vector_create(&data->multipliers,
                                        num_constraints,
                                        1));

  SLEQP_CALL(sleqp_sparse_vector_create(&data->multipliers_zero,
                                        num_constraints,
                                        1));

  SLEQP_CALL(sleqp_iterate_create(&data->check_iterate,
                                  problem,
                                  sleqp_problem_var_lb(problem)));


  return SLEQP_OKAY;
}

static SLEQP_RETCODE check_func_first_order_at(SleqpDerivCheckData* data,
                                               SleqpIterate* iterate,
                                               int j,
                                               bool* valid,
                                               bool* reject)
{
  SleqpIterate* check_iterate = data->check_iterate;
  SleqpProblem* problem = data->problem;

  const double tolerance = sleqp_params_get(data->params, SLEQP_PARAM_DERIV_TOL);

  double perturbation;

  *valid = true;

  SLEQP_CALL(create_check_iterate(data, iterate, j, &perturbation));

  // set check iterate...
  SLEQP_CALL(set_check_iterate(data, reject));

  if(*reject)
  {
    return SLEQP_OKAY;
  }

  double check_val;

  SLEQP_CALL(sleqp_problem_eval(problem,
                                NULL,
                                &check_val,
                                NULL,
                                NULL,
                                NULL));

  SLEQP_CALL(sleqp_iterate_set_func_val(check_iterate, check_val));

  const double func_diff = sleqp_iterate_get_func_val(check_iterate) - sleqp_iterate_get_func_val(iterate);

  const double actual_value = func_diff / perturbation;

  const double expected_value = sleqp_sparse_vector_value_at(sleqp_iterate_get_func_grad(iterate), j);

  if(!sleqp_is_eq(expected_value, actual_value, tolerance))
  {
    sleqp_log_error("Derivative check failed for objective function gradient at %d: "
                    "grad = %.10e != %.10e = findiff",
                    j,
                    expected_value,
                    actual_value);

    *valid = false;
  }

  // restore original iterate...
  SLEQP_CALL(restore_iterate(data, iterate));

  return SLEQP_OKAY;
}

static SLEQP_RETCODE check_cons_first_order_at(SleqpDerivCheckData* data,
                                               SleqpIterate* iterate,
                                               int j,
                                               bool* valid,
                                               bool* reject)
{
  SleqpIterate* check_iterate = data->check_iterate;
  SleqpProblem* problem = data->problem;
  SleqpSparseMatrix* cons_jac = sleqp_iterate_get_cons_jac(iterate);

  const int num_constraints = sleqp_problem_num_constraints(problem);

  const double tolerance = sleqp_params_get(data->params,
                                            SLEQP_PARAM_DERIV_TOL);

  double perturbation;

  SLEQP_CALL(create_check_iterate(data, iterate, j, &perturbation));

  // set check iterate...
  SLEQP_CALL(set_check_iterate(data, reject));

  if(*reject)
  {
    return SLEQP_OKAY;
  }

  SLEQP_CALL(sleqp_problem_eval(problem,
                                NULL,
                                NULL,
                                NULL,
                                sleqp_iterate_get_cons_val(check_iterate),
                                NULL));

  for(int i = 0; i < num_constraints; ++i)
  {
    const double expected_value = sleqp_sparse_matrix_value_at(cons_jac, i, j);

    const double lower_value = sleqp_sparse_vector_value_at(sleqp_iterate_get_cons_val(iterate),
                                                            i);

    const double upper_value = sleqp_sparse_vector_value_at(sleqp_iterate_get_cons_val(check_iterate),
                                                            i);

    const double unit_direction = upper_value - lower_value;

    const double actual_value = unit_direction / perturbation;

    if(!sleqp_is_eq(expected_value, actual_value, tolerance))
    {
      sleqp_log_error("Derivative check failed for %d-th constraint gradient at index %d: "
                      "jac = %.10e != %.10e = findiff",
                      i,
                      j,
                      expected_value,
                      actual_value);

      *valid = false;
    }
  }

  // restore original iterate...
  SLEQP_CALL(restore_iterate(data, iterate));

  return SLEQP_OKAY;
}

SLEQP_RETCODE sleqp_deriv_check_first_order(SleqpDerivCheckData* data,
                                            SleqpIterate* iterate)
{
  SleqpProblem* problem = data->problem;

  const int num_variables = sleqp_problem_num_variables(problem);

  bool valid = true;
  bool reject = false;

  for(int j = 0; j < num_variables && !reject; ++j)
  {
    SLEQP_CALL(check_func_first_order_at(data,
                                         iterate,
                                         j,
                                         &valid,
                                         &reject));

    if(reject)
    {
      break;
    }

    SLEQP_CALL(check_cons_first_order_at(data,
                                         iterate,
                                         j,
                                         &valid,
                                         &reject));
  }

  if(reject)
  {
    sleqp_log_warn("Function rejected derivative check");

    SLEQP_CALL(restore_iterate(data, iterate));
  }

  if(!valid)
  {
    return SLEQP_INVALID_DERIV;
  }

  return SLEQP_OKAY;
}

static SLEQP_RETCODE check_func_second_order_at(SleqpDerivCheckData* data,
                                                SleqpIterate* iterate,
                                                int j,
                                                bool* valid,
                                                bool* reject)
{
  SleqpIterate* check_iterate = data->check_iterate;
  SleqpProblem* problem = data->problem;

  const int num_variables = sleqp_problem_num_variables(problem);

  *reject = false;

  const double tolerance = sleqp_params_get(data->params,
                                            SLEQP_PARAM_DERIV_TOL);

  double perturbation;

  SLEQP_CALL(create_check_iterate(data, iterate, j, &perturbation));

  SLEQP_CALL(set_check_iterate(data, reject));

  SLEQP_CALL(sleqp_problem_eval(problem,
                                NULL,
                                NULL,
                                sleqp_iterate_get_func_grad(check_iterate),
                                NULL,
                                NULL));

  SLEQP_CALL(sleqp_sparse_vector_add_scaled(sleqp_iterate_get_func_grad(iterate),
                                            sleqp_iterate_get_func_grad(check_iterate),
                                            -1.,
                                            1.,
                                            0,
                                            data->hessian_estimate));

  SLEQP_CALL(sleqp_sparse_vector_scale(data->hessian_estimate, 1./perturbation));

  SLEQP_CALL(sleqp_sparse_vector_clear(data->multipliers));

  SLEQP_CALL(sleqp_sparse_vector_clear(data->hessian_right));

  SLEQP_CALL(sleqp_sparse_vector_push(data->hessian_right, j, 1.));

  const double one = 1.;

  for(int k = 0; k < num_variables; ++k)
  {
    SLEQP_CALL(restore_iterate(data, iterate));

    SLEQP_CALL(sleqp_problem_hess_prod(problem,
                                       &one,
                                       data->hessian_right,
                                       data->multipliers,
                                       data->hessian_prod));

    SLEQP_CALL(sleqp_sparse_vector_clear(data->hessian_left));

    SLEQP_CALL(sleqp_sparse_vector_push(data->hessian_left, k, 1.));

    double hessian_coeff;

    SLEQP_CALL(sleqp_sparse_vector_dot(data->hessian_left,
                                       data->hessian_prod,
                                       &hessian_coeff));

    double* vec_ptr = sleqp_sparse_vector_at(data->hessian_estimate, k);

    const double expected_value = hessian_coeff;

    const double actual_value = vec_ptr ? (*vec_ptr) : 0.;

    if(!sleqp_is_eq(expected_value, actual_value, tolerance))
    {
      sleqp_log_error("Derivative check failed for objective function hessian at (%d, %d): "
                      "hess = %.10e != %.10e = findiff",
                      k,
                      j,
                      expected_value,
                      actual_value);

      *valid = false;
    }

  }

  return SLEQP_OKAY;
}

static SLEQP_RETCODE check_cons_second_order_at(SleqpDerivCheckData* data,
                                                SleqpIterate* iterate,
                                                int i,
                                                int j,
                                                bool* valid,
                                                bool* reject)
{
  SleqpIterate* check_iterate = data->check_iterate;
  SleqpProblem* problem = data->problem;

  const int num_variables = sleqp_problem_num_variables(problem);

  *reject = false;

  double one = 1.;

  const double tolerance = sleqp_params_get(data->params, SLEQP_PARAM_DERIV_TOL);

  double perturbation;

  SLEQP_CALL(create_check_iterate(data, iterate, j, &perturbation));

  SLEQP_CALL(set_check_iterate(data, reject));

  SleqpSparseMatrix* check_jac = sleqp_iterate_get_cons_jac(check_iterate);

  SLEQP_CALL(sleqp_problem_eval(problem,
                                NULL,
                                NULL,
                                NULL,
                                NULL,
                                check_jac));

  SLEQP_CALL(sleqp_sparse_vector_clear(data->multipliers));

  SLEQP_CALL(sleqp_sparse_vector_push(data->multipliers, i, 1));

  SLEQP_CALL(sleqp_sparse_matrix_trans_vector_product(sleqp_iterate_get_cons_jac(iterate),
                                                      data->multipliers,
                                                      0.,
                                                      data->cons_grad_iterate));

  SLEQP_CALL(sleqp_sparse_matrix_trans_vector_product(check_jac,
                                                      data->multipliers,
                                                      0.,
                                                      data->cons_grad_check_iterate));

  SLEQP_CALL(sleqp_sparse_vector_add_scaled(data->cons_grad_iterate,
                                            data->cons_grad_check_iterate,
                                            -1.,
                                            1.,
                                            0,
                                            data->hessian_estimate));

  SLEQP_CALL(sleqp_sparse_vector_scale(data->hessian_estimate, 1./perturbation));

  SLEQP_CALL(sleqp_sparse_vector_clear(data->hessian_right));

  SLEQP_CALL(sleqp_sparse_vector_push(data->hessian_right, j, 1.));

  SLEQP_CALL(sleqp_problem_hess_prod(problem,
                                     &one,
                                     data->hessian_right,
                                     data->multipliers_zero,
                                     data->hessian_prod_zero));

  SLEQP_CALL(restore_iterate(data, iterate));

  for(int k = 0; k < num_variables; ++k)
  {
    SLEQP_CALL(sleqp_problem_hess_prod(problem,
                                       &one,
                                       data->hessian_right,
                                       data->multipliers,
                                       data->hessian_prod_cache));

    SLEQP_CALL(sleqp_sparse_vector_add_scaled(data->hessian_prod_cache,
                                              data->hessian_prod_zero,
                                              1.,
                                              -1.,
                                              0,
                                              data->hessian_prod));

    SLEQP_CALL(sleqp_sparse_vector_clear(data->hessian_left));

    SLEQP_CALL(sleqp_sparse_vector_push(data->hessian_left, k, 1.));

    double hessian_coeff;

    SLEQP_CALL(sleqp_sparse_vector_dot(data->hessian_left,
                                       data->hessian_prod,
                                       &hessian_coeff));

    double* vec_ptr = sleqp_sparse_vector_at(data->hessian_estimate, k);

    const double expected_value = hessian_coeff;

    const double actual_value = vec_ptr ? (*vec_ptr) : 0.;

    if(!sleqp_is_eq(expected_value, actual_value, tolerance))
    {
      sleqp_log_error("Derivative check failed for %d-th constraint hessian at (%d, %d): "
                      "hess = %.10e != %.10e = findiff",
                      i,
                      k,
                      j,
                      expected_value,
                      actual_value);

      *valid = false;
    }

  }

  return SLEQP_OKAY;
}

SLEQP_RETCODE sleqp_deriv_check_second_order_exhaustive(SleqpDerivCheckData* data,
                                                        SleqpIterate* iterate)
{
  SleqpProblem* problem = data->problem;

  const int num_variables = sleqp_problem_num_variables(problem);
  const int num_constraints = sleqp_problem_num_constraints(problem);

  bool valid = true;
  bool reject = false;

  for(int j = 0; j < num_variables && !reject; ++j)
  {
    SLEQP_CALL(check_func_second_order_at(data,
                                          iterate,
                                          j,
                                          &valid,
                                          &reject));
  }

  if(reject)
  {
    sleqp_log_warn("Function rejected derivative check");

    SLEQP_CALL(restore_iterate(data, iterate));
  }

  for(int i = 0; i < num_constraints; ++i)
  {
    for(int j = 0; j < num_variables && !reject; ++j)
    {
      SLEQP_CALL(check_cons_second_order_at(data,
                                            iterate,
                                            i,
                                            j,
                                            &valid,
                                            &reject));
    }
  }

  if(reject)
  {
    sleqp_log_warn("Function rejected derivative check");
  }

  // restore original iterate...
  SLEQP_CALL(restore_iterate(data, iterate));

  if(!valid)
  {
    return SLEQP_INVALID_DERIV;
  }

  return SLEQP_OKAY;
}

static SLEQP_RETCODE compute_combined_cons_grad(SleqpDerivCheckData* data,
                                                SleqpSparseVec* multipliers,
                                                SleqpIterate* iterate,
                                                SleqpSparseVec* result)
{
  SleqpParams* params = data->params;

  const double zero_eps = sleqp_params_get(params, SLEQP_PARAM_ZERO_EPS);

  SLEQP_CALL(sleqp_sparse_matrix_trans_vector_product(sleqp_iterate_get_cons_jac(iterate),
                                                      multipliers,
                                                      zero_eps,
                                                      data->transposed_jacobian_product));

  SLEQP_CALL(sleqp_sparse_vector_add(sleqp_iterate_get_func_grad(iterate),
                                     data->transposed_jacobian_product,
                                     zero_eps,
                                     result));



  return SLEQP_OKAY;
}

static SLEQP_RETCODE check_second_order_at(SleqpDerivCheckData* data,
                                           SleqpIterate* iterate,
                                           int j,
                                           bool* valid,
                                           bool* reject)
{
  SleqpIterate* check_iterate = data->check_iterate;
  SleqpProblem* problem = data->problem;
  SleqpSparseVec* multipliers = sleqp_iterate_get_cons_dual(iterate);
  SleqpSparseVec* unit_direction = data->unit_direction;

  const int num_variables = sleqp_problem_num_variables(problem);

  int func_grad_nnz = 0;
  int cons_val_nnz = 0;
  int cons_jac_nnz = 0;

  const double one = 1.;

  const double tolerance = sleqp_params_get(data->params,
                                            SLEQP_PARAM_DERIV_TOL);

  double perturbation;

  // Compute direction, i.e., j-th unit vector
  {
    SLEQP_CALL(sleqp_sparse_vector_clear(unit_direction));

    SLEQP_CALL(sleqp_sparse_vector_push(unit_direction, j, 1.));
  }

  SLEQP_CALL(sleqp_problem_hess_prod(problem,
                                     &one,
                                     unit_direction,
                                     multipliers,
                                     data->hessian_prod));

  // Compute Hessian product estimate
  {
    SLEQP_CALL(compute_combined_cons_grad(data,
                                          multipliers,
                                          iterate,
                                          data->combined_cons_grad_iterate));

    SLEQP_CALL(create_check_iterate(data, iterate, j, &perturbation));

    SLEQP_CALL(set_check_iterate(data, reject));

    if(*reject)
    {
      return SLEQP_OKAY;
    }

    SLEQP_CALL(sleqp_problem_eval(problem,
                                  NULL,
                                  NULL,
                                  sleqp_iterate_get_func_grad(check_iterate),
                                  NULL,
                                  sleqp_iterate_get_cons_jac(check_iterate)));

    SLEQP_CALL(compute_combined_cons_grad(data,
                                          multipliers,
                                          check_iterate,
                                          data->combined_cons_grad_check_iterate));

    SLEQP_CALL(sleqp_sparse_vector_add_scaled(data->combined_cons_grad_iterate,
                                              data->combined_cons_grad_check_iterate,
                                              -1.,
                                              1.,
                                              0,
                                              data->hessian_estimate));

    SLEQP_CALL(sleqp_sparse_vector_scale(data->hessian_estimate, 1./perturbation));
  }


  // Validation
  {
    for(int k = 0; k < num_variables; ++k)
    {
      double* vec_ptr = NULL;

      vec_ptr = sleqp_sparse_vector_at(data->hessian_estimate, k);
      const double actual_value = vec_ptr ? (*vec_ptr) : 0.;

      vec_ptr = sleqp_sparse_vector_at(data->hessian_prod, k);
      const double expected_value = vec_ptr ? (*vec_ptr) : 0.;

      if(!sleqp_is_eq(expected_value, actual_value, tolerance))
      {
        sleqp_log_error("Derivative check failed for combined function hessian at (%d, %d): "
                        "hess = %.10e != %.10e = findiff",
                        k,
                        j,
                        expected_value,
                        actual_value);

        *valid = false;
      }
    }
  }

  // restore iterate...
  SLEQP_CALL(restore_iterate(data, iterate));

  return SLEQP_OKAY;
}

SLEQP_RETCODE sleqp_deriv_check_second_order_simple(SleqpDerivCheckData* data,
                                                    SleqpIterate* iterate)
{
  SleqpProblem* problem = data->problem;

  const int num_variables = sleqp_problem_num_variables(problem);

  bool valid = true;
  bool reject = false;

  for(int j = 0; j < num_variables &&!reject; ++j)
  {
    SLEQP_CALL(check_second_order_at(data,
                                     iterate,
                                     j,
                                     &valid,
                                     &reject));
  }

  if(reject)
  {
    sleqp_log_warn("Function rejected derivative check");

    SLEQP_CALL(restore_iterate(data, iterate));
  }

  if(!valid)
  {
    return SLEQP_INVALID_DERIV;
  }

  return SLEQP_OKAY;
}

SLEQP_RETCODE sleqp_deriv_checker_free(SleqpDerivCheckData** star)
{
  SleqpDerivCheckData* data = *star;

  if(!data)
  {
    return SLEQP_OKAY;
  }

  SLEQP_CALL(sleqp_iterate_release(&data->check_iterate));

  SLEQP_CALL(sleqp_sparse_vector_free(&data->multipliers_zero));

  SLEQP_CALL(sleqp_sparse_vector_free(&data->multipliers));

  SLEQP_CALL(sleqp_sparse_vector_free(&data->combined_cons_grad_check_iterate));

  SLEQP_CALL(sleqp_sparse_vector_free(&data->combined_cons_grad_iterate));

  SLEQP_CALL(sleqp_sparse_vector_free(&data->transposed_jacobian_product));

  SLEQP_CALL(sleqp_sparse_vector_free(&data->cons_grad_check_iterate));

  SLEQP_CALL(sleqp_sparse_vector_free(&data->cons_grad_iterate));

  SLEQP_CALL(sleqp_sparse_vector_free(&data->hessian_prod_cache));

  SLEQP_CALL(sleqp_sparse_vector_free(&data->hessian_prod_zero));

  SLEQP_CALL(sleqp_sparse_vector_free(&data->hessian_prod));

  SLEQP_CALL(sleqp_sparse_vector_free(&data->hessian_right));

  SLEQP_CALL(sleqp_sparse_vector_free(&data->hessian_left));

  SLEQP_CALL(sleqp_sparse_vector_free(&data->hessian_estimate));

  SLEQP_CALL(sleqp_sparse_vector_free(&data->unit_direction));

  SLEQP_CALL(sleqp_params_release(&data->params));

  SLEQP_CALL(sleqp_problem_release(&data->problem));

  sleqp_free(star);

  return SLEQP_OKAY;
}
