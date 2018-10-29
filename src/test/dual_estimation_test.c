#include <stdlib.h>
#include <check.h>

#include "sleqp.h"
#include "sleqp_aug_jacobian.h"
#include "sleqp_cauchy.h"
#include "sleqp_cmp.h"
#include "sleqp_dual_estimation.h"
#include "sleqp_mem.h"

#include "lp/sleqp_lpi_soplex.h"

#include "test_common.h"

#include "quadfunc_fixture.h"

START_TEST(test_simply_constrained_dual_estimation)
{
  SleqpParams* params;
  SleqpProblem* problem;
  SleqpIterate* iterate;
  SleqpLPi* lp_interface;
  SleqpCauchyData* cauchy_data;
  SleqpActiveSet* active_set;
  SleqpAugJacobian* jacobian;

  SleqpDualEstimationData* estimation_data;

  double penalty_parameter = 1., trust_radius = 0.1;

  ASSERT_CALL(sleqp_params_create(&params));

  ASSERT_CALL(sleqp_problem_create(&problem,
                                   quadfunc,
                                   params,
                                   quadfunc_var_lb,
                                   quadfunc_var_ub,
                                   quadfunc_cons_lb,
                                   quadfunc_cons_ub));

  ASSERT_CALL(sleqp_iterate_create(&iterate,
                                   problem,
                                   quadfunc_x));

  int num_lp_variables = problem->num_variables + 2*problem->num_constraints;
  int num_lp_constraints = problem->num_constraints;

  ASSERT_CALL(sleqp_lpi_soplex_create_interface(&lp_interface,
                                                num_lp_variables,
                                                num_lp_constraints));

  ASSERT_CALL(sleqp_set_and_evaluate(problem, iterate));

  ASSERT_CALL(sleqp_cauchy_data_create(&cauchy_data,
                                       problem,
                                       params,
                                       lp_interface));

  ASSERT_CALL(sleqp_active_set_create(&active_set,
                                      problem));

  ASSERT_CALL(sleqp_aug_jacobian_create(&jacobian,
                                        problem,
                                        params));

  ASSERT_CALL(sleqp_dual_estimation_data_create(&estimation_data, problem));

  ASSERT_CALL(sleqp_cauchy_set_iterate(cauchy_data,
                                       iterate,
                                       trust_radius));

  ASSERT_CALL(sleqp_cauchy_solve(cauchy_data,
                                 iterate->func_grad,
                                 penalty_parameter));

  ASSERT_CALL(sleqp_cauchy_get_active_set(cauchy_data,
                                          iterate,
                                          trust_radius));

  ASSERT_CALL(sleqp_aug_jacobian_set_iterate(jacobian, iterate));

  ASSERT_CALL(sleqp_dual_estimation_compute(estimation_data,
                                            iterate,
                                            NULL,
                                            jacobian));

  SleqpSparseVec* vars_dual = iterate->vars_dual;

  ck_assert(sleqp_sparse_vector_at(vars_dual, 0));
  ck_assert(sleqp_sparse_vector_at(vars_dual, 1));

  double tolerance = 1e-8;

  ck_assert(sleqp_eq(*sleqp_sparse_vector_at(vars_dual, 0), -2., tolerance));
  ck_assert(sleqp_eq(*sleqp_sparse_vector_at(vars_dual, 1), -4., tolerance));

  ASSERT_CALL(sleqp_dual_estimation_data_free(&estimation_data));

  ASSERT_CALL(sleqp_aug_jacobian_free(&jacobian));

  ASSERT_CALL(sleqp_active_set_free(&active_set));

  ASSERT_CALL(sleqp_cauchy_data_free(&cauchy_data));

  ASSERT_CALL(sleqp_lpi_free(&lp_interface));

  ASSERT_CALL(sleqp_iterate_free(&iterate));

  ASSERT_CALL(sleqp_problem_free(&problem));

  ASSERT_CALL(sleqp_params_free(&params));
}
END_TEST

Suite* dual_estimation_test_suite()
{
  Suite *suite;
  TCase *tc_dual_estimation;

  suite = suite_create("Dual estimation tests");

  tc_dual_estimation = tcase_create("Simply constrained");

  tcase_add_checked_fixture(tc_dual_estimation,
                            quadfunc_setup,
                            quadfunc_teardown);

  tcase_add_test(tc_dual_estimation, test_simply_constrained_dual_estimation);
  suite_add_tcase(suite, tc_dual_estimation);

  return suite;
}

int main()
{
  int num_fails;
  Suite* suite;
  SRunner* srunner;

  suite = dual_estimation_test_suite();
  srunner = srunner_create(suite);

  srunner_set_fork_status(srunner, CK_NOFORK);
  srunner_run_all(srunner, CK_NORMAL);

  num_fails = srunner_ntests_failed(srunner);

  srunner_free(srunner);

  return (num_fails > 0) ? EXIT_FAILURE : EXIT_SUCCESS;
}