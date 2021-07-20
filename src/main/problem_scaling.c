#include "problem_scaling.h"

#include "cmp.h"
#include "func.h"
#include "log.h"
#include "math_error.h"
#include "mem.h"
#include "sparse/sparse_matrix.h"

struct SleqpProblemScaling
{
  int refcount;

  SleqpScaling* scaling;
  SleqpProblem* problem;
  SleqpParams* params;
  SleqpOptions* options;
  SleqpFunc* func;

  SleqpFunc* scaled_func;
  SleqpProblem* scaled_problem;

  SleqpSparseVec* unscaled_value;

  SleqpSparseVec* scaled_direction;
  SleqpSparseVec* scaled_cons_duals;
};

static SLEQP_RETCODE
scaled_func_set_value(SleqpFunc* func,
                      SleqpSparseVec* scaled_value,
                      SLEQP_VALUE_REASON reason,
                      int* func_grad_nnz,
                      int* cons_val_nnz,
                      int* cons_jac_nnz,
                      void* func_data)
{
  SleqpProblemScaling* problem_scaling = (SleqpProblemScaling*) func_data;
  SleqpScaling* scaling = problem_scaling->scaling;

  {
    const int error_flags = sleqp_options_get_int(problem_scaling->options,
                                                  SLEQP_OPTION_INT_FLOAT_ERROR_FLAGS);

    const int warn_flags = sleqp_options_get_int(problem_scaling->options,
                                                 SLEQP_OPTION_INT_FLOAT_WARNING_FLAGS);

    SLEQP_INIT_MATH_CHECK;

    SLEQP_CALL(sleqp_sparse_vector_copy(scaled_value,
                                        problem_scaling->unscaled_value));

    SLEQP_CALL(sleqp_unscale_point(scaling,
                                   problem_scaling->unscaled_value));

    SLEQP_MATH_CHECK(error_flags, warn_flags);
  }

  SLEQP_CALL(sleqp_func_set_value(problem_scaling->func,
                                  problem_scaling->unscaled_value,
                                  reason,
                                  func_grad_nnz,
                                  cons_val_nnz,
                                  cons_jac_nnz));

  return SLEQP_OKAY;
}

SLEQP_RETCODE scaled_func_val(SleqpFunc* func,
                              double* func_val,
                              void* func_data)
{
  SleqpProblemScaling* problem_scaling = (SleqpProblemScaling*) func_data;
  SleqpScaling* scaling = problem_scaling->scaling;

  SLEQP_CALL(sleqp_func_val(problem_scaling->func, func_val));

  (*func_val) = sleqp_scale_func_val(scaling, (*func_val));

  return SLEQP_OKAY;
}

SLEQP_RETCODE scaled_func_grad(SleqpFunc* func,
                               SleqpSparseVec* func_grad,
                               void* func_data)
{
  SleqpProblemScaling* problem_scaling = (SleqpProblemScaling*) func_data;
  SleqpScaling* scaling = problem_scaling->scaling;

  SLEQP_CALL(sleqp_func_grad(problem_scaling->func, func_grad));

  SLEQP_CALL(sleqp_scale_func_grad(scaling, func_grad));

  return SLEQP_OKAY;
}

SLEQP_RETCODE scaled_func_cons_val(SleqpFunc* func,
                                   const SleqpSparseVec* cons_indices,
                                   SleqpSparseVec* cons_val,
                                   void* func_data)
{
  SleqpProblemScaling* problem_scaling = (SleqpProblemScaling*) func_data;
  SleqpScaling* scaling = problem_scaling->scaling;

  SLEQP_CALL(sleqp_func_cons_val(problem_scaling->func, cons_indices, cons_val));

  SLEQP_CALL(sleqp_scale_cons_val(scaling, cons_val));

  return SLEQP_OKAY;
}

SLEQP_RETCODE scaled_func_cons_jac(SleqpFunc* func,
                                   const SleqpSparseVec* cons_indices,
                                   SleqpSparseMatrix* cons_jac,
                                   void* func_data)
{
  SleqpProblemScaling* problem_scaling = (SleqpProblemScaling*) func_data;
  SleqpScaling* scaling = problem_scaling->scaling;

  SLEQP_CALL(sleqp_func_cons_jac(problem_scaling->func, cons_indices, cons_jac));

  SLEQP_CALL(sleqp_scale_cons_jac(scaling, cons_jac));

  return SLEQP_OKAY;
}

static SLEQP_RETCODE
scaled_func_hess_prod(SleqpFunc* func,
                      const double* func_dual,
                      const SleqpSparseVec* direction,
                      const SleqpSparseVec* cons_duals,
                      SleqpSparseVec* product,
                      void* func_data)
{
  SleqpProblemScaling* problem_scaling = (SleqpProblemScaling*) func_data;
  SleqpScaling* scaling = problem_scaling->scaling;

  const int error_flags = sleqp_options_get_int(problem_scaling->options,
                                                SLEQP_OPTION_INT_FLOAT_ERROR_FLAGS);

  const int warn_flags = sleqp_options_get_int(problem_scaling->options,
                                               SLEQP_OPTION_INT_FLOAT_WARNING_FLAGS);

  SLEQP_CALL(sleqp_sparse_vector_copy(direction,
                                      problem_scaling->scaled_direction));

  SLEQP_CALL(sleqp_sparse_vector_copy(cons_duals,
                                      problem_scaling->scaled_cons_duals));

  {
    SLEQP_INIT_MATH_CHECK;

    SLEQP_CALL(sleqp_unscale_hessian_direction(scaling,
                                               problem_scaling->scaled_direction,
                                               problem_scaling->scaled_cons_duals));

    SLEQP_MATH_CHECK(error_flags, warn_flags);
  }

  SLEQP_CALL(sleqp_func_hess_prod(problem_scaling->func,
                                  func_dual,
                                  problem_scaling->scaled_direction,
                                  problem_scaling->scaled_cons_duals,
                                  product));

  {
    SLEQP_INIT_MATH_CHECK;

    SLEQP_CALL(sleqp_scale_hessian_product(scaling,
                                           product));

    SLEQP_MATH_CHECK(error_flags, warn_flags);
  }

  return SLEQP_OKAY;
}


SLEQP_RETCODE sleqp_problem_scaling_create(SleqpProblemScaling** star,
                                           SleqpScaling* scaling,
                                           SleqpProblem* problem,
                                           SleqpParams* params,
                                           SleqpOptions* options)
{
  SLEQP_CALL(sleqp_malloc(star));

  SleqpProblemScaling* problem_scaling = *star;

  *problem_scaling = (SleqpProblemScaling) {0};

  const int num_variables = sleqp_problem_num_variables(problem);
  const int num_constraints = sleqp_problem_num_constraints(problem);

  if(num_variables != sleqp_scaling_get_num_variables(scaling))
  {
    sleqp_log_error("Invalid number of variables provided to scaled problem");
    return SLEQP_ILLEGAL_ARGUMENT;
  }

  if(num_constraints != sleqp_scaling_get_num_constraints(scaling))
  {
    sleqp_log_error("Invalid number of constraints provided to scaled problem");
    return SLEQP_ILLEGAL_ARGUMENT;
  }

  problem_scaling->refcount = 1;

  problem_scaling->problem = problem;
  SLEQP_CALL(sleqp_problem_capture(problem_scaling->problem));

  problem_scaling->func = sleqp_problem_func(problem);

  SLEQP_CALL(sleqp_params_capture(params));
  problem_scaling->params = params;

  SLEQP_CALL(sleqp_options_capture(options));
  problem_scaling->options = options;

  problem_scaling->scaling = scaling;

  SLEQP_CALL(sleqp_scaling_capture(problem_scaling->scaling));

  SleqpFuncCallbacks callbacks = {
    .set_value = scaled_func_set_value,
    .func_val = scaled_func_val,
    .func_grad = scaled_func_grad,
    .cons_val = scaled_func_cons_val,
    .cons_jac = scaled_func_cons_jac,
    .hess_prod = scaled_func_hess_prod,
    .func_free = NULL
  };

  SLEQP_CALL(sleqp_func_create(&(problem_scaling->scaled_func),
                               &callbacks,
                               num_variables,
                               num_constraints,
                               problem_scaling));

  SLEQP_CALL(sleqp_hessian_struct_copy(sleqp_func_get_hess_struct(problem_scaling->func),
                                       sleqp_func_get_hess_struct(problem_scaling->scaled_func)));

  SLEQP_CALL(sleqp_problem_create(&(problem_scaling->scaled_problem),
                                  problem_scaling->scaled_func,
                                  problem_scaling->params,
                                  sleqp_problem_var_lb(problem),
                                  sleqp_problem_var_ub(problem),
                                  sleqp_problem_general_lb(problem),
                                  sleqp_problem_general_ub(problem),
                                  sleqp_problem_linear_coeffs(problem),
                                  sleqp_problem_linear_lb(problem),
                                  sleqp_problem_linear_ub(problem)));

  SLEQP_CALL(sleqp_sparse_vector_create_empty(&(problem_scaling->unscaled_value),
                                              num_variables));

  SLEQP_CALL(sleqp_sparse_vector_create_empty(&(problem_scaling->scaled_direction),
                                              num_variables));

  SLEQP_CALL(sleqp_sparse_vector_create_empty(&(problem_scaling->scaled_cons_duals),
                                              num_constraints));

  return SLEQP_OKAY;
}

SleqpProblem* sleqp_problem_scaling_get_problem(SleqpProblemScaling* problem_scaling)
{
  return problem_scaling->scaled_problem;
}

SLEQP_RETCODE sleqp_problem_scaling_flush(SleqpProblemScaling* problem_scaling)
{
  SleqpProblem* problem = problem_scaling->problem;
  SleqpScaling* scaling = problem_scaling->scaling;
  SleqpProblem* scaled_problem = problem_scaling->scaled_problem;

  const int error_flags = sleqp_options_get_int(problem_scaling->options,
                                                SLEQP_OPTION_INT_FLOAT_ERROR_FLAGS);

  const int warn_flags = sleqp_options_get_int(problem_scaling->options,
                                               SLEQP_OPTION_INT_FLOAT_WARNING_FLAGS);

  SLEQP_INIT_MATH_CHECK;

  SLEQP_CALL(sleqp_sparse_vector_copy(sleqp_problem_var_lb(problem),
                                      sleqp_problem_var_lb(scaled_problem)));

  SLEQP_CALL(sleqp_scale_point(scaling,
                               sleqp_problem_var_lb(scaled_problem)));

  SLEQP_CALL(sleqp_sparse_vector_copy(sleqp_problem_var_ub(problem),
                                      sleqp_problem_var_ub(scaled_problem)));

  SLEQP_CALL(sleqp_scale_point(scaling,
                               sleqp_problem_var_ub(scaled_problem)));

  SLEQP_CALL(sleqp_sparse_vector_copy(sleqp_problem_cons_lb(problem),
                                      sleqp_problem_cons_lb(scaled_problem)));

  SLEQP_CALL(sleqp_scale_cons_val(scaling,
                                  sleqp_problem_cons_lb(scaled_problem)));

  SLEQP_CALL(sleqp_sparse_vector_copy(sleqp_problem_cons_ub(problem),
                                      sleqp_problem_cons_ub(scaled_problem)));

  SLEQP_CALL(sleqp_scale_cons_val(scaling,
                                  sleqp_problem_cons_ub(scaled_problem)));

  SLEQP_CALL(sleqp_sparse_vector_copy(sleqp_problem_general_lb(problem),
                                      sleqp_problem_general_lb(scaled_problem)));

  SLEQP_CALL(sleqp_scale_cons_general(scaling,
                                     sleqp_problem_general_lb(scaled_problem)));

  SLEQP_CALL(sleqp_sparse_vector_copy(sleqp_problem_general_ub(problem),
                                      sleqp_problem_general_ub(scaled_problem)));

  SLEQP_CALL(sleqp_scale_cons_general(scaling,
                                     sleqp_problem_general_ub(scaled_problem)));

  SLEQP_CALL(sleqp_sparse_vector_copy(sleqp_problem_linear_lb(problem),
                                      sleqp_problem_linear_lb(scaled_problem)));

  SLEQP_CALL(sleqp_scale_cons_linear(scaling,
                                     sleqp_problem_linear_lb(scaled_problem)));

  SLEQP_CALL(sleqp_sparse_vector_copy(sleqp_problem_linear_ub(problem),
                                      sleqp_problem_linear_ub(scaled_problem)));

  SLEQP_CALL(sleqp_scale_cons_linear(scaling,
                                     sleqp_problem_linear_ub(scaled_problem)));

  SLEQP_CALL(sleqp_sparse_matrix_copy(sleqp_problem_linear_coeffs(problem),
                                      sleqp_problem_linear_coeffs(scaled_problem)));
  
  SLEQP_CALL(sleqp_scale_linear_coeffs(scaling,
                                       sleqp_problem_linear_coeffs(scaled_problem)));

  
  

  SLEQP_MATH_CHECK(error_flags, warn_flags);

  return SLEQP_OKAY;
}

static SLEQP_RETCODE problem_scaling_free(SleqpProblemScaling** star)
{
  SleqpProblemScaling* problem_scaling = *star;

  if(!problem_scaling)
  {
    return SLEQP_OKAY;
  }

  SLEQP_CALL(sleqp_sparse_vector_free(&(problem_scaling->scaled_cons_duals)));

  SLEQP_CALL(sleqp_sparse_vector_free(&(problem_scaling->scaled_direction)));

  SLEQP_CALL(sleqp_sparse_vector_free(&(problem_scaling->unscaled_value)));

  SLEQP_CALL(sleqp_problem_release(&(problem_scaling->scaled_problem)));

  SLEQP_CALL(sleqp_func_release(&(problem_scaling->scaled_func)));

  SLEQP_CALL(sleqp_scaling_release(&problem_scaling->scaling));

  SLEQP_CALL(sleqp_options_release(&problem_scaling->options));

  SLEQP_CALL(sleqp_params_release(&problem_scaling->params));

  SLEQP_CALL(sleqp_problem_release(&problem_scaling->problem));

  sleqp_free(star);

  *star = NULL;

  return SLEQP_OKAY;
}

SLEQP_RETCODE sleqp_problem_scaling_capture(SleqpProblemScaling* scaling)
{
  ++scaling->refcount;

  return SLEQP_OKAY;
}

SLEQP_RETCODE sleqp_problem_scaling_release(SleqpProblemScaling** star)
{
  SleqpProblemScaling* problem_scaling = *star;

  if(!problem_scaling)
  {
    return SLEQP_OKAY;
  }

  if(--problem_scaling->refcount == 0)
  {
    SLEQP_CALL(problem_scaling_free(star));
  }

  *star = NULL;

  return SLEQP_OKAY;
}