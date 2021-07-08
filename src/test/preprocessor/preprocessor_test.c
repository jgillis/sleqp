#include <stdlib.h>
#include <check.h>

#include "options.h"
#include "problem.h"
#include "solver.h"
#include "working_set.h"

#include "preprocessor/preprocessor.h"

#include "test_common.h"
#include "rosenbrock_fixture.h"

static const int num_linear = 1;
static const int num_variables = 2;

SleqpParams* params;

SleqpSparseVec* linear_lb;
SleqpSparseVec* linear_ub;

double* cache;

void setup()
{
  rosenbrock_setup();

  ASSERT_CALL(sleqp_params_create(&params));

  ASSERT_CALL(sleqp_sparse_vector_create_full(&linear_lb, num_linear));
  ASSERT_CALL(sleqp_sparse_vector_create_full(&linear_ub, num_linear));

  ASSERT_CALL(sleqp_alloc_array(&cache, num_variables));
}

void teardown()
{
  sleqp_free(&cache);

  ASSERT_CALL(sleqp_params_release(&params));

  ASSERT_CALL(sleqp_sparse_vector_free(&linear_ub));
  ASSERT_CALL(sleqp_sparse_vector_free(&linear_lb));

  rosenbrock_teardown();
}


START_TEST(test_single_empty_row)
{
  SleqpSparseMatrix* linear_coeffs;
  SleqpPreprocessor* preprocessor;
  SleqpProblem* problem;

  ASSERT_CALL(sleqp_sparse_matrix_create(&linear_coeffs,
                                         num_linear,
                                         num_variables,
                                         0));

  ASSERT_CALL(sleqp_problem_create(&problem,
                                   rosenbrock_func,
                                   params,
                                   rosenbrock_var_lb,
                                   rosenbrock_var_ub,
                                   rosenbrock_cons_lb,
                                   rosenbrock_cons_ub,
                                   linear_coeffs,
                                   linear_lb,
                                   linear_ub));

  ASSERT_CALL(sleqp_preprocessor_create(&preprocessor,
                                        problem,
                                        params));

  ck_assert_int_eq(sleqp_preprocessor_result(preprocessor),
                   SLEQP_PREPROCESSING_RESULT_SUCCESS);

  SleqpProblem* transformed_problem = sleqp_preprocessor_transformed_problem(preprocessor);

  SleqpSparseMatrix* transformed_linear_coeffs = sleqp_problem_linear_coeffs(transformed_problem);

  ck_assert_int_eq(sleqp_sparse_matrix_get_num_rows(transformed_linear_coeffs),
                   0);

  ck_assert_int_eq(sleqp_problem_linear_lb(transformed_problem)->dim,
                   0);

  ck_assert_int_eq(sleqp_problem_linear_ub(transformed_problem)->dim,
                   0);

  ASSERT_CALL(sleqp_preprocessor_release(&preprocessor));

  ASSERT_CALL(sleqp_problem_release(&problem));

  ASSERT_CALL(sleqp_sparse_matrix_release(&linear_coeffs));
}
END_TEST

START_TEST(test_fixed_var_linear_trans)
{
  SleqpSparseMatrix* linear_coeffs;
  SleqpPreprocessor* preprocessor;
  SleqpProblem* problem;

  const double linear_lb_val = -5.;
  const double linear_ub_val = 4.;
  const double var_value = 2.;
  const double linear_coeff = 2.;

  const int linear_nnz = 2;

  const double eps = sleqp_params_get(params, SLEQP_PARAM_EPS);
  const double zero_eps = sleqp_params_get(params, SLEQP_PARAM_ZERO_EPS);

  const double inf = sleqp_infinity();

  double var_lb[] = {var_value, -inf};

  double var_ub[] = {var_value, inf};

  ASSERT_CALL(sleqp_sparse_vector_from_raw(rosenbrock_var_lb,
                                           var_lb,
                                           num_variables,
                                           zero_eps));

  ASSERT_CALL(sleqp_sparse_vector_from_raw(rosenbrock_var_ub,
                                           var_ub,
                                           num_variables,
                                           zero_eps));

  ASSERT_CALL(sleqp_sparse_matrix_create(&linear_coeffs,
                                         num_linear,
                                         num_variables,
                                         linear_nnz));

  ASSERT_CALL(sleqp_sparse_matrix_push_column(linear_coeffs, 0));

  ASSERT_CALL(sleqp_sparse_matrix_push(linear_coeffs, 0, 0, linear_coeff));

  ASSERT_CALL(sleqp_sparse_matrix_push_column(linear_coeffs, 1));

  ASSERT_CALL(sleqp_sparse_matrix_push(linear_coeffs, 0, 1, linear_coeff));

  ASSERT_CALL(sleqp_sparse_vector_push(linear_lb, 0, linear_lb_val));

  ASSERT_CALL(sleqp_sparse_vector_push(linear_ub, 0, linear_ub_val));

  ASSERT_CALL(sleqp_problem_create(&problem,
                                   rosenbrock_func,
                                   params,
                                   rosenbrock_var_lb,
                                   rosenbrock_var_ub,
                                   rosenbrock_cons_lb,
                                   rosenbrock_cons_ub,
                                   linear_coeffs,
                                   linear_lb,
                                   linear_ub));

  ASSERT_CALL(sleqp_preprocessor_create(&preprocessor,
                                        problem,
                                        params));

  ck_assert_int_eq(sleqp_preprocessor_result(preprocessor),
                   SLEQP_PREPROCESSING_RESULT_SUCCESS);

  SleqpProblem* transformed_problem = sleqp_preprocessor_transformed_problem(preprocessor);

  SleqpSparseMatrix* transformed_linear_coeffs = sleqp_problem_linear_coeffs(transformed_problem);

  ck_assert_int_eq(sleqp_problem_num_variables(transformed_problem),
                   1);

  ck_assert_int_eq(sleqp_sparse_matrix_get_nnz(transformed_linear_coeffs),
                   1);

  SleqpSparseVec* transformed_linear_lb = sleqp_problem_linear_lb(transformed_problem);
  SleqpSparseVec* transformed_linear_ub = sleqp_problem_linear_ub(transformed_problem);

  ck_assert(sleqp_is_eq(sleqp_sparse_vector_value_at(transformed_linear_lb, 0),
                        linear_lb_val - var_value*linear_coeff,
                        eps));

  ck_assert(sleqp_is_eq(sleqp_sparse_vector_value_at(transformed_linear_ub, 0),
                        linear_ub_val - var_value*linear_coeff,
                        eps));

  ASSERT_CALL(sleqp_preprocessor_release(&preprocessor));

  ASSERT_CALL(sleqp_problem_release(&problem));

  ASSERT_CALL(sleqp_sparse_matrix_release(&linear_coeffs));
}
END_TEST

START_TEST(test_positive_bound_row)
{
  SleqpSparseMatrix* linear_coeffs;
  SleqpPreprocessor* preprocessor;
  SleqpProblem* problem;

  const double eps = sleqp_params_get(params, SLEQP_PARAM_EPS);

  ASSERT_CALL(sleqp_sparse_vector_push(linear_lb, 0, 1.));

  ASSERT_CALL(sleqp_sparse_vector_push(linear_ub, 0, 4.));

  ASSERT_CALL(sleqp_sparse_matrix_create(&linear_coeffs,
                                         num_linear,
                                         num_variables,
                                         1));

  ASSERT_CALL(sleqp_sparse_matrix_push_column(linear_coeffs, 0));

  ASSERT_CALL(sleqp_sparse_matrix_push(linear_coeffs, 0, 0, 2.));

  ASSERT_CALL(sleqp_sparse_matrix_push_column(linear_coeffs, 1));

  ASSERT_CALL(sleqp_problem_create(&problem,
                                   rosenbrock_func,
                                   params,
                                   rosenbrock_var_lb,
                                   rosenbrock_var_ub,
                                   rosenbrock_cons_lb,
                                   rosenbrock_cons_ub,
                                   linear_coeffs,
                                   linear_lb,
                                   linear_ub));

  ASSERT_CALL(sleqp_preprocessor_create(&preprocessor,
                                        problem,
                                        params));

  ck_assert_int_eq(sleqp_preprocessor_result(preprocessor),
                   SLEQP_PREPROCESSING_RESULT_SUCCESS);

  SleqpProblem* transformed_problem = sleqp_preprocessor_transformed_problem(preprocessor);

  ck_assert_int_eq(sleqp_problem_num_linear_constraints(transformed_problem),
                   0);

  SleqpSparseVec* transformed_var_lb = sleqp_problem_var_lb(transformed_problem);
  SleqpSparseVec* transformed_var_ub = sleqp_problem_var_ub(transformed_problem);

  ck_assert(sleqp_is_eq(sleqp_sparse_vector_value_at(transformed_var_lb, 0),
                        1./2.,
                        eps));

  ck_assert(sleqp_is_eq(sleqp_sparse_vector_value_at(transformed_var_ub, 0),
                        2.,
                        eps));

  ASSERT_CALL(sleqp_preprocessor_release(&preprocessor));

  ASSERT_CALL(sleqp_problem_release(&problem));

  ASSERT_CALL(sleqp_sparse_matrix_release(&linear_coeffs));
}
END_TEST

START_TEST(test_negative_bound_row)
{
  SleqpSparseMatrix* linear_coeffs;
  SleqpPreprocessor* preprocessor;
  SleqpProblem* problem;

  const int num_linear = 1;

  const double eps = sleqp_params_get(params, SLEQP_PARAM_EPS);

  ASSERT_CALL(sleqp_sparse_vector_push(linear_lb, 0, 1.));

  ASSERT_CALL(sleqp_sparse_vector_push(linear_ub, 0, 4.));

  ASSERT_CALL(sleqp_sparse_matrix_create(&linear_coeffs,
                                         num_linear,
                                         num_variables,
                                         1));

  ASSERT_CALL(sleqp_sparse_matrix_push_column(linear_coeffs, 0));

  ASSERT_CALL(sleqp_sparse_matrix_push(linear_coeffs, 0, 0, -2.));

  ASSERT_CALL(sleqp_sparse_matrix_push_column(linear_coeffs, 1));

  ASSERT_CALL(sleqp_problem_create(&problem,
                                   rosenbrock_func,
                                   params,
                                   rosenbrock_var_lb,
                                   rosenbrock_var_ub,
                                   rosenbrock_cons_lb,
                                   rosenbrock_cons_ub,
                                   linear_coeffs,
                                   linear_lb,
                                   linear_ub));

  ASSERT_CALL(sleqp_preprocessor_create(&preprocessor,
                                        problem,
                                        params));

  ck_assert_int_eq(sleqp_preprocessor_result(preprocessor),
                   SLEQP_PREPROCESSING_RESULT_SUCCESS);

  SleqpProblem* transformed_problem = sleqp_preprocessor_transformed_problem(preprocessor);

  ck_assert_int_eq(sleqp_problem_num_linear_constraints(transformed_problem),
                   0);

  SleqpSparseVec* transformed_var_lb = sleqp_problem_var_lb(transformed_problem);
  SleqpSparseVec* transformed_var_ub = sleqp_problem_var_ub(transformed_problem);

  ck_assert(sleqp_is_eq(sleqp_sparse_vector_value_at(transformed_var_lb, 0),
                        -2.,
                        eps));

  ck_assert(sleqp_is_eq(sleqp_sparse_vector_value_at(transformed_var_ub, 0),
                        -1./2.,
                        eps));

  ASSERT_CALL(sleqp_preprocessor_release(&preprocessor));

  ASSERT_CALL(sleqp_problem_release(&problem));

  ASSERT_CALL(sleqp_sparse_matrix_release(&linear_coeffs));
}
END_TEST


/*
 * An example for a forcing constraint,
 * 1 <= x + y, y <= 0, x <= 1.
 * Should fix x = 1, y = 0.
 */

START_TEST(test_forcing_constraint)
{
  SleqpSparseMatrix* linear_coeffs;
  SleqpPreprocessor* preprocessor;
  SleqpProblem* problem;

  const int num_linear = 1;

  const double inf = sleqp_infinity();
  const double zero_eps = sleqp_params_get(params, SLEQP_PARAM_ZERO_EPS);

  double var_lb[] = {-inf, -inf};

  double var_ub[] = {1., 0.};

  ASSERT_CALL(sleqp_sparse_vector_from_raw(rosenbrock_var_lb,
                                           var_lb,
                                           num_variables,
                                           zero_eps));

  ASSERT_CALL(sleqp_sparse_vector_from_raw(rosenbrock_var_ub,
                                           var_ub,
                                           num_variables,
                                           zero_eps));

  ASSERT_CALL(sleqp_sparse_vector_push(linear_lb, 0, 1.));

  ASSERT_CALL(sleqp_sparse_vector_push(linear_ub, 0, inf));

  ASSERT_CALL(sleqp_sparse_matrix_create(&linear_coeffs,
                                         num_linear,
                                         num_variables,
                                         2));

  ASSERT_CALL(sleqp_sparse_matrix_push_column(linear_coeffs, 0));

  ASSERT_CALL(sleqp_sparse_matrix_push(linear_coeffs, 0, 0, 1.));

  ASSERT_CALL(sleqp_sparse_matrix_push_column(linear_coeffs, 1));

  ASSERT_CALL(sleqp_sparse_matrix_push(linear_coeffs, 0, 1, 1.));

  ASSERT_CALL(sleqp_problem_create(&problem,
                                   rosenbrock_func,
                                   params,
                                   rosenbrock_var_lb,
                                   rosenbrock_var_ub,
                                   rosenbrock_cons_lb,
                                   rosenbrock_cons_ub,
                                   linear_coeffs,
                                   linear_lb,
                                   linear_ub));

  ASSERT_CALL(sleqp_preprocessor_create(&preprocessor,
                                        problem,
                                        params));

  ck_assert_int_eq(sleqp_preprocessor_result(preprocessor),
                   SLEQP_PREPROCESSING_RESULT_SUCCESS);

  SleqpProblem* transformed_problem = sleqp_preprocessor_transformed_problem(preprocessor);

  ck_assert_int_eq(sleqp_problem_num_linear_constraints(transformed_problem),
                   0);

  ck_assert_int_eq(sleqp_problem_num_variables(transformed_problem),
                   0);

  ASSERT_CALL(sleqp_preprocessor_release(&preprocessor));

  ASSERT_CALL(sleqp_problem_release(&problem));

  ASSERT_CALL(sleqp_sparse_matrix_release(&linear_coeffs));
}
END_TEST

START_TEST(test_dominated_row)
{
  SleqpSparseMatrix* linear_coeffs;
  SleqpPreprocessor* preprocessor;
  SleqpProblem* problem;

  const double zero_eps = sleqp_params_get(params, SLEQP_PARAM_ZERO_EPS);

  const int num_linear = 1;

  ASSERT_CALL(sleqp_sparse_vector_push(linear_lb, 0, -1.));

  ASSERT_CALL(sleqp_sparse_vector_push(linear_ub, 0, 10.));

  ASSERT_CALL(sleqp_sparse_matrix_create(&linear_coeffs,
                                         num_linear,
                                         num_variables,
                                         2));

  ASSERT_CALL(sleqp_sparse_vector_clear(rosenbrock_var_lb));

  double var_ub[] = {1., 1,};
  ASSERT_CALL(sleqp_sparse_vector_from_raw(rosenbrock_var_ub,
                                           var_ub,
                                           2,
                                           zero_eps));

  ASSERT_CALL(sleqp_sparse_matrix_push_column(linear_coeffs, 0));

  ASSERT_CALL(sleqp_sparse_matrix_push(linear_coeffs, 0, 0, 1.));

  ASSERT_CALL(sleqp_sparse_matrix_push_column(linear_coeffs, 1));

  ASSERT_CALL(sleqp_sparse_matrix_push(linear_coeffs, 0, 1, 1.));

  ASSERT_CALL(sleqp_problem_create(&problem,
                                   rosenbrock_func,
                                   params,
                                   rosenbrock_var_lb,
                                   rosenbrock_var_ub,
                                   rosenbrock_cons_lb,
                                   rosenbrock_cons_ub,
                                   linear_coeffs,
                                   linear_lb,
                                   linear_ub));

  ASSERT_CALL(sleqp_preprocessor_create(&preprocessor,
                                        problem,
                                        params));

  ck_assert_int_eq(sleqp_preprocessor_result(preprocessor),
                   SLEQP_PREPROCESSING_RESULT_SUCCESS);

  SleqpProblem* transformed_problem = sleqp_preprocessor_transformed_problem(preprocessor);

  ck_assert_int_eq(sleqp_problem_num_linear_constraints(transformed_problem),
                   0);

  ASSERT_CALL(sleqp_preprocessor_release(&preprocessor));

  ASSERT_CALL(sleqp_problem_release(&problem));

  ASSERT_CALL(sleqp_sparse_matrix_release(&linear_coeffs));
}
END_TEST

START_TEST(test_failure)
{
  SleqpPreprocessor* preprocessor;
  SleqpProblem* problem;

  ASSERT_CALL(sleqp_problem_create_simple(&problem,
                                          rosenbrock_func,
                                          params,
                                          rosenbrock_var_lb,
                                          rosenbrock_var_ub,
                                          rosenbrock_cons_lb,
                                          rosenbrock_cons_ub));

  ASSERT_CALL(sleqp_preprocessor_create(&preprocessor,
                                        problem,
                                        params));

  ck_assert_int_eq(sleqp_preprocessor_result(preprocessor),
                   SLEQP_PREPROCESSING_RESULT_FAILURE);

  SleqpProblem* transformed_problem = sleqp_preprocessor_transformed_problem(preprocessor);

  ck_assert_int_eq(sleqp_problem_num_linear_constraints(transformed_problem),
                   0);

  ASSERT_CALL(sleqp_preprocessor_release(&preprocessor));

  ASSERT_CALL(sleqp_problem_release(&problem));
}
END_TEST

START_TEST(test_simple_infeasibility)
{
  SleqpSparseMatrix* linear_coeffs;
  SleqpPreprocessor* preprocessor;
  SleqpProblem* problem;

  const int num_linear = 1;

  ASSERT_CALL(sleqp_sparse_vector_push(linear_lb, 0, 1.));

  ASSERT_CALL(sleqp_sparse_vector_push(linear_ub, 0, 2.));

  ASSERT_CALL(sleqp_sparse_matrix_create(&linear_coeffs,
                                         num_linear,
                                         num_variables,
                                         0));

  ASSERT_CALL(sleqp_problem_create(&problem,
                                   rosenbrock_func,
                                   params,
                                   rosenbrock_var_lb,
                                   rosenbrock_var_ub,
                                   rosenbrock_cons_lb,
                                   rosenbrock_cons_ub,
                                   linear_coeffs,
                                   linear_lb,
                                   linear_ub));

  ASSERT_CALL(sleqp_preprocessor_create(&preprocessor,
                                        problem,
                                        params));

  ck_assert_int_eq(sleqp_preprocessor_result(preprocessor),
                   SLEQP_PREPROCESSING_RESULT_INFEASIBLE);

  ASSERT_CALL(sleqp_preprocessor_release(&preprocessor));

  ASSERT_CALL(sleqp_problem_release(&problem));

  ASSERT_CALL(sleqp_sparse_matrix_release(&linear_coeffs));
}
END_TEST

START_TEST(test_fixed_var)
{
  SleqpPreprocessor* preprocessor;
  SleqpProblem* problem;

  ASSERT_CALL(sleqp_sparse_vector_clear(rosenbrock_var_lb));
  ASSERT_CALL(sleqp_sparse_vector_clear(rosenbrock_var_ub));

  ASSERT_CALL(sleqp_sparse_vector_push(rosenbrock_var_lb, 0, 0.));
  ASSERT_CALL(sleqp_sparse_vector_push(rosenbrock_var_lb, 1, 1.));

  ASSERT_CALL(sleqp_sparse_vector_push(rosenbrock_var_ub, 0, 0.));
  ASSERT_CALL(sleqp_sparse_vector_push(rosenbrock_var_ub, 1, 2.));

  ASSERT_CALL(sleqp_problem_create_simple(&problem,
                                          rosenbrock_func,
                                          params,
                                          rosenbrock_var_lb,
                                          rosenbrock_var_ub,
                                          rosenbrock_cons_lb,
                                          rosenbrock_cons_ub));

  ASSERT_CALL(sleqp_preprocessor_create(&preprocessor,
                                        problem,
                                        params));

  SleqpProblem* transformed_problem = sleqp_preprocessor_transformed_problem(preprocessor);

  ck_assert_int_eq(sleqp_problem_num_variables(transformed_problem),
                   1);

  ASSERT_CALL(sleqp_preprocessor_release(&preprocessor));

  ASSERT_CALL(sleqp_problem_release(&problem));
}
END_TEST

START_TEST(test_solve)
{
  SleqpSparseMatrix* linear_coeffs;
  SleqpPreprocessor* preprocessor;
  SleqpProblem* problem;
  SleqpOptions* options;
  SleqpSolver* solver;

  SleqpIterate* transformed_solution_iterate;
  SleqpIterate* original_solution_iterate;

  const double eps = sleqp_params_get(params, SLEQP_PARAM_EPS);

  ASSERT_CALL(sleqp_sparse_matrix_create(&linear_coeffs,
                                         num_linear,
                                         num_variables,
                                         0));

  ASSERT_CALL(sleqp_problem_create(&problem,
                                   rosenbrock_func,
                                   params,
                                   rosenbrock_var_lb,
                                   rosenbrock_var_ub,
                                   rosenbrock_cons_lb,
                                   rosenbrock_cons_ub,
                                   linear_coeffs,
                                   linear_lb,
                                   linear_ub));

  ASSERT_CALL(sleqp_iterate_create(&original_solution_iterate,
                                   problem,
                                   rosenbrock_initial));

  ASSERT_CALL(sleqp_options_create(&options));

  ASSERT_CALL(sleqp_preprocessor_create(&preprocessor,
                                        problem,
                                        params));

  ck_assert_int_eq(sleqp_preprocessor_result(preprocessor),
                   SLEQP_PREPROCESSING_RESULT_SUCCESS);

  SleqpProblem* transformed_problem = sleqp_preprocessor_transformed_problem(preprocessor);

  ASSERT_CALL(sleqp_solver_create(&solver,
                                  transformed_problem,
                                  params,
                                  options,
                                  rosenbrock_initial,
                                  NULL));

  // 100 iterations should be plenty...
  ASSERT_CALL(sleqp_solver_solve(solver, 100, -1));

  ASSERT_CALL(sleqp_solver_get_solution(solver,
                                        &transformed_solution_iterate));

  ASSERT_CALL(sleqp_preprocessor_restore_iterate(preprocessor,
                                                 transformed_solution_iterate,
                                                 original_solution_iterate));

  // actual tests
  {
    SleqpSparseVec* cons_dual = sleqp_iterate_get_cons_dual(original_solution_iterate);

    ck_assert_int_eq(cons_dual->dim, num_linear);

    ck_assert(sleqp_is_eq(sleqp_sparse_vector_value_at(cons_dual, 0),
                          0.,
                          eps));

    SleqpWorkingSet* working_set = sleqp_iterate_get_working_set(original_solution_iterate);

    ck_assert_int_eq(sleqp_working_set_get_constraint_state(working_set, 0),
                     SLEQP_INACTIVE);
  }

  ASSERT_CALL(sleqp_solver_release(&solver));

  ASSERT_CALL(sleqp_preprocessor_release(&preprocessor));

  ASSERT_CALL(sleqp_options_release(&options));

  ASSERT_CALL(sleqp_iterate_release(&original_solution_iterate));

  ASSERT_CALL(sleqp_problem_release(&problem));

  ASSERT_CALL(sleqp_sparse_matrix_release(&linear_coeffs));
}
END_TEST

START_TEST(test_restore_positive_bound_row)
{
  SleqpSparseMatrix* linear_coeffs;
  SleqpPreprocessor* preprocessor;
  SleqpProblem* problem;

  SleqpIterate* original_iterate;
  SleqpIterate* transformed_iterate;

  const double eps = sleqp_params_get(params, SLEQP_PARAM_EPS);

  ASSERT_CALL(sleqp_sparse_vector_push(linear_lb, 0, 1.));

  ASSERT_CALL(sleqp_sparse_vector_push(linear_ub, 0, 4.));

  ASSERT_CALL(sleqp_sparse_matrix_create(&linear_coeffs,
                                         num_linear,
                                         num_variables,
                                         1));

  ASSERT_CALL(sleqp_sparse_matrix_push_column(linear_coeffs, 0));

  ASSERT_CALL(sleqp_sparse_matrix_push(linear_coeffs, 0, 0, 2.));

  ASSERT_CALL(sleqp_sparse_matrix_push_column(linear_coeffs, 1));

  ASSERT_CALL(sleqp_problem_create(&problem,
                                   rosenbrock_func,
                                   params,
                                   rosenbrock_var_lb,
                                   rosenbrock_var_ub,
                                   rosenbrock_cons_lb,
                                   rosenbrock_cons_ub,
                                   linear_coeffs,
                                   linear_lb,
                                   linear_ub));

  ASSERT_CALL(sleqp_iterate_create(&original_iterate,
                                   problem,
                                   rosenbrock_initial));

  ASSERT_CALL(sleqp_preprocessor_create(&preprocessor,
                                        problem,
                                        params));

  ck_assert_int_eq(sleqp_preprocessor_result(preprocessor),
                   SLEQP_PREPROCESSING_RESULT_SUCCESS);

  SleqpProblem* transformed_problem = sleqp_preprocessor_transformed_problem(preprocessor);

  ASSERT_CALL(sleqp_iterate_create(&transformed_iterate,
                                   transformed_problem,
                                   rosenbrock_initial));

  // Set working set
  {
    SleqpWorkingSet* working_set = sleqp_iterate_get_working_set(transformed_iterate);

    ASSERT_CALL(sleqp_working_set_add_variable(working_set, 0, SLEQP_ACTIVE_UPPER));

    SleqpSparseVec* vars_dual = sleqp_iterate_get_vars_dual(transformed_iterate);

    ASSERT_CALL(sleqp_sparse_vector_reserve(vars_dual, 2));

    ASSERT_CALL(sleqp_sparse_vector_push(vars_dual, 0, 3.));
  }

  ASSERT_CALL(sleqp_preprocessor_restore_iterate(preprocessor,
                                                 transformed_iterate,
                                                 original_iterate));

  // Actual tests
  {
    SleqpWorkingSet* working_set = sleqp_iterate_get_working_set(original_iterate);

    ck_assert_int_eq(sleqp_working_set_get_variable_state(working_set, 0),
                     SLEQP_INACTIVE);

    ck_assert_int_eq(sleqp_working_set_get_variable_state(working_set, 1),
                     SLEQP_INACTIVE);

    ck_assert_int_eq(sleqp_working_set_get_constraint_state(working_set, 0),
                     SLEQP_ACTIVE_UPPER);

    SleqpSparseVec* vars_dual = sleqp_iterate_get_vars_dual(original_iterate);

    ck_assert_int_eq(vars_dual->nnz, 0);

    SleqpSparseVec* cons_dual = sleqp_iterate_get_cons_dual(original_iterate);

    ck_assert_int_eq(cons_dual->nnz, 1);

    ck_assert(sleqp_is_eq(cons_dual->data[0], 3./2., eps));

  }

  ASSERT_CALL(sleqp_iterate_release(&transformed_iterate));

  ASSERT_CALL(sleqp_preprocessor_release(&preprocessor));

  ASSERT_CALL(sleqp_iterate_release(&original_iterate));

  ASSERT_CALL(sleqp_problem_release(&problem));

  ASSERT_CALL(sleqp_sparse_matrix_release(&linear_coeffs));
}
END_TEST

START_TEST(test_restore_negative_bound_row)
{
  SleqpSparseMatrix* linear_coeffs;
  SleqpPreprocessor* preprocessor;
  SleqpProblem* problem;

  SleqpIterate* original_iterate;
  SleqpIterate* transformed_iterate;

  const double eps = sleqp_params_get(params, SLEQP_PARAM_EPS);

  ASSERT_CALL(sleqp_sparse_vector_push(linear_lb, 0, 1.));

  ASSERT_CALL(sleqp_sparse_vector_push(linear_ub, 0, 4.));

  ASSERT_CALL(sleqp_sparse_matrix_create(&linear_coeffs,
                                         num_linear,
                                         num_variables,
                                         1));

  ASSERT_CALL(sleqp_sparse_matrix_push_column(linear_coeffs, 0));

  ASSERT_CALL(sleqp_sparse_matrix_push(linear_coeffs, 0, 0, -2.));

  ASSERT_CALL(sleqp_sparse_matrix_push_column(linear_coeffs, 1));

  ASSERT_CALL(sleqp_problem_create(&problem,
                                   rosenbrock_func,
                                   params,
                                   rosenbrock_var_lb,
                                   rosenbrock_var_ub,
                                   rosenbrock_cons_lb,
                                   rosenbrock_cons_ub,
                                   linear_coeffs,
                                   linear_lb,
                                   linear_ub));

  ASSERT_CALL(sleqp_iterate_create(&original_iterate,
                                   problem,
                                   rosenbrock_initial));

  ASSERT_CALL(sleqp_preprocessor_create(&preprocessor,
                                        problem,
                                        params));

  ck_assert_int_eq(sleqp_preprocessor_result(preprocessor),
                   SLEQP_PREPROCESSING_RESULT_SUCCESS);

  SleqpProblem* transformed_problem = sleqp_preprocessor_transformed_problem(preprocessor);

  ASSERT_CALL(sleqp_iterate_create(&transformed_iterate,
                                   transformed_problem,
                                   rosenbrock_initial));

  // Set working set
  {
    SleqpWorkingSet* working_set = sleqp_iterate_get_working_set(transformed_iterate);

    ASSERT_CALL(sleqp_working_set_add_variable(working_set, 0, SLEQP_ACTIVE_UPPER));

    SleqpSparseVec* vars_dual = sleqp_iterate_get_vars_dual(transformed_iterate);

    ASSERT_CALL(sleqp_sparse_vector_reserve(vars_dual, 2));

    ASSERT_CALL(sleqp_sparse_vector_push(vars_dual, 0, 3.));
  }

  ASSERT_CALL(sleqp_preprocessor_restore_iterate(preprocessor,
                                                 transformed_iterate,
                                                 original_iterate));

  // Actual tests
  {
    SleqpWorkingSet* working_set = sleqp_iterate_get_working_set(original_iterate);

    ck_assert_int_eq(sleqp_working_set_get_variable_state(working_set, 0),
                     SLEQP_INACTIVE);

    ck_assert_int_eq(sleqp_working_set_get_variable_state(working_set, 1),
                     SLEQP_INACTIVE);

    ck_assert_int_eq(sleqp_working_set_get_constraint_state(working_set, 0),
                     SLEQP_ACTIVE_LOWER);

    SleqpSparseVec* vars_dual = sleqp_iterate_get_vars_dual(original_iterate);

    ck_assert_int_eq(vars_dual->nnz, 0);

    SleqpSparseVec* cons_dual = sleqp_iterate_get_cons_dual(original_iterate);

    ck_assert_int_eq(cons_dual->nnz, 1);

    ck_assert(sleqp_is_eq(cons_dual->data[0], -3./2., eps));
  }

  ASSERT_CALL(sleqp_iterate_release(&transformed_iterate));

  ASSERT_CALL(sleqp_preprocessor_release(&preprocessor));

  ASSERT_CALL(sleqp_iterate_release(&original_iterate));

  ASSERT_CALL(sleqp_problem_release(&problem));

  ASSERT_CALL(sleqp_sparse_matrix_release(&linear_coeffs));
}
END_TEST

START_TEST(test_restore_forcing_constraint)
{
  SleqpSparseMatrix* linear_coeffs;
  SleqpPreprocessor* preprocessor;
  SleqpProblem* problem;
  SleqpIterate* iterate;

  const int num_linear = 1;

  const double inf = sleqp_infinity();

  const double eps = sleqp_params_get(params,
                                      SLEQP_PARAM_EPS);

  const double zero_eps = sleqp_params_get(params,
                                           SLEQP_PARAM_ZERO_EPS);

  double var_lb[] = {-inf, -inf};

  double var_ub[] = {1., 0.};

  ASSERT_CALL(sleqp_sparse_vector_from_raw(rosenbrock_var_lb,
                                           var_lb,
                                           num_variables,
                                           zero_eps));

  ASSERT_CALL(sleqp_sparse_vector_from_raw(rosenbrock_var_ub,
                                           var_ub,
                                           num_variables,
                                           zero_eps));

  ASSERT_CALL(sleqp_sparse_vector_push(linear_lb, 0, 1.));

  ASSERT_CALL(sleqp_sparse_vector_push(linear_ub, 0, inf));

  ASSERT_CALL(sleqp_sparse_matrix_create(&linear_coeffs,
                                         num_linear,
                                         num_variables,
                                         2));

  ASSERT_CALL(sleqp_sparse_matrix_push_column(linear_coeffs, 0));

  ASSERT_CALL(sleqp_sparse_matrix_push(linear_coeffs, 0, 0, 1.));

  ASSERT_CALL(sleqp_sparse_matrix_push_column(linear_coeffs, 1));

  ASSERT_CALL(sleqp_sparse_matrix_push(linear_coeffs, 0, 1, 1.));

  ASSERT_CALL(sleqp_problem_create(&problem,
                                   rosenbrock_func,
                                   params,
                                   rosenbrock_var_lb,
                                   rosenbrock_var_ub,
                                   rosenbrock_cons_lb,
                                   rosenbrock_cons_ub,
                                   linear_coeffs,
                                   linear_lb,
                                   linear_ub));

  ASSERT_CALL(sleqp_iterate_create(&iterate,
                                   problem,
                                   rosenbrock_initial));

  ASSERT_CALL(sleqp_preprocessor_create(&preprocessor,
                                        problem,
                                        params));

  ck_assert_int_eq(sleqp_preprocessor_result(preprocessor),
                   SLEQP_PREPROCESSING_RESULT_SUCCESS);

  SleqpProblem* transformed_problem = sleqp_preprocessor_transformed_problem(preprocessor);

  SleqpSparseVec* transformed_initial;
  SleqpIterate* transformed_iterate;

  ASSERT_CALL(sleqp_sparse_vector_create_empty(&transformed_initial, 0));

  ASSERT_CALL(sleqp_iterate_create(&transformed_iterate,
                                   transformed_problem,
                                   transformed_initial));

  ASSERT_CALL(sleqp_preprocessor_restore_iterate(preprocessor,
                                                 transformed_iterate,
                                                 iterate));

  {
    double stationarity_residuum;
    ASSERT_CALL(sleqp_iterate_stationarity_residuum(problem,
                                                    iterate,
                                                    cache,
                                                    &stationarity_residuum));

    ck_assert(sleqp_is_zero(stationarity_residuum, eps));
  }


  ASSERT_CALL(sleqp_iterate_release(&transformed_iterate));


  ASSERT_CALL(sleqp_sparse_vector_free(&transformed_initial));

  ASSERT_CALL(sleqp_preprocessor_release(&preprocessor));

  ASSERT_CALL(sleqp_iterate_release(&iterate));

  ASSERT_CALL(sleqp_problem_release(&problem));

  ASSERT_CALL(sleqp_sparse_matrix_release(&linear_coeffs));
}
END_TEST

START_TEST(test_restore_fixed_vars)
{
  SleqpPreprocessor* preprocessor;

  SleqpProblem* problem;
  SleqpIterate* iterate;

  SleqpSparseVec* transformed_initial;
  SleqpIterate* transformed_iterate;

  const double eps = sleqp_params_get(params,
                                      SLEQP_PARAM_EPS);

  ASSERT_CALL(sleqp_sparse_vector_clear(rosenbrock_var_lb));
  ASSERT_CALL(sleqp_sparse_vector_clear(rosenbrock_var_ub));

  ASSERT_CALL(sleqp_problem_create_simple(&problem,
                                          rosenbrock_func,
                                          params,
                                          rosenbrock_var_lb,
                                          rosenbrock_var_ub,
                                          rosenbrock_cons_lb,
                                          rosenbrock_cons_ub));

  ASSERT_CALL(sleqp_iterate_create(&iterate,
                                   problem,
                                   rosenbrock_initial));

  ASSERT_CALL(sleqp_preprocessor_create(&preprocessor,
                                        problem,
                                        params));

  ck_assert_int_eq(sleqp_preprocessor_result(preprocessor),
                   SLEQP_PREPROCESSING_RESULT_SUCCESS);

  SleqpProblem* transformed_problem = sleqp_preprocessor_transformed_problem(preprocessor);

  ck_assert_int_eq(sleqp_problem_num_variables(transformed_problem),
                   0);

  ASSERT_CALL(sleqp_sparse_vector_create_empty(&transformed_initial, 0));

  ASSERT_CALL(sleqp_iterate_create(&transformed_iterate,
                                   transformed_problem,
                                   transformed_initial));

  ASSERT_CALL(sleqp_preprocessor_restore_iterate(preprocessor,
                                                 transformed_iterate,
                                                 iterate));

  double stationarity_residuum;
  ASSERT_CALL(sleqp_iterate_stationarity_residuum(problem,
                                                  iterate,
                                                  cache,
                                                  &stationarity_residuum));

  ck_assert(sleqp_is_zero(stationarity_residuum, eps));

  ASSERT_CALL(sleqp_iterate_release(&transformed_iterate));

  ASSERT_CALL(sleqp_sparse_vector_free(&transformed_initial));

  ASSERT_CALL(sleqp_preprocessor_release(&preprocessor));

  ASSERT_CALL(sleqp_iterate_release(&iterate));

  ASSERT_CALL(sleqp_problem_release(&problem));
}
END_TEST


Suite* preprocessor_test_suite()
{
  Suite *suite;
  TCase *tc_prob;

  suite = suite_create("Preprocessor tests");

  tc_prob = tcase_create("Problem transformation tests");

  tcase_add_checked_fixture(tc_prob,
                            setup,
                            teardown);

  tcase_add_test(tc_prob, test_single_empty_row);

  tcase_add_test(tc_prob, test_fixed_var_linear_trans);

  tcase_add_test(tc_prob, test_positive_bound_row);

  tcase_add_test(tc_prob, test_negative_bound_row);

  tcase_add_test(tc_prob, test_forcing_constraint);

  tcase_add_test(tc_prob, test_dominated_row);

  tcase_add_test(tc_prob, test_failure);

  tcase_add_test(tc_prob, test_simple_infeasibility);

  tcase_add_test(tc_prob, test_fixed_var);

  tcase_add_test(tc_prob, test_solve);

  tcase_add_test(tc_prob, test_restore_positive_bound_row);

  tcase_add_test(tc_prob, test_restore_negative_bound_row);

  tcase_add_test(tc_prob, test_restore_forcing_constraint);

  tcase_add_test(tc_prob, test_restore_fixed_vars);

  suite_add_tcase(suite, tc_prob);

  return suite;
}

TEST_MAIN(preprocessor_test_suite)
