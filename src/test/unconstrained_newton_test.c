#include <stdlib.h>
#include <check.h>

#include "sleqp.h"
#include "sleqp_aug_jacobian.h"
#include "sleqp_cauchy.h"
#include "sleqp_cmp.h"
#include "sleqp_dual_estimation.h"
#include "sleqp_mem.h"
#include "sleqp_newton.h"

#include "lp/sleqp_lpi_soplex.h"

#include "test_common.h"

#include "quadfunc_fixture.h"

SleqpProblem* problem;
SleqpIterate* iterate;
SleqpLPi* lp_interface;
SleqpCauchyData* cauchy_data;

void newton_setup()
{
  quadfunc_setup();

  ASSERT_CALL(sleqp_problem_create(&problem,
                                   quadfunc,
                                   quadfunc_var_lb,
                                   quadfunc_var_ub,
                                   quadfunc_cons_lb,
                                   quadfunc_cons_ub));

  int num_variables = problem->num_variables;
  int num_constraints = problem->num_constraints;

  ASSERT_CALL(sleqp_iterate_create(&iterate,
                                   problem,
                                   quadfunc_x));

  ASSERT_CALL(sleqp_active_set_reset(iterate->active_set));

  int num_lp_variables = problem->num_variables + 2*problem->num_constraints;
  int num_lp_constraints = problem->num_constraints;

  ASSERT_CALL(sleqp_lpi_soplex_create_interface(&lp_interface,
                                                num_lp_variables,
                                                num_lp_constraints));

  ASSERT_CALL(sleqp_set_and_evaluate(problem, iterate));

  ASSERT_CALL(sleqp_cauchy_data_create(&cauchy_data,
                                       problem,
                                       lp_interface));
}

void newton_teardown()
{
  ASSERT_CALL(sleqp_cauchy_data_free(&cauchy_data));

  ASSERT_CALL(sleqp_lpi_free(&lp_interface));

  ASSERT_CALL(sleqp_iterate_free(&iterate));

  ASSERT_CALL(sleqp_problem_free(&problem));

  quadfunc_teardown();
}

START_TEST(newton_wide_step)
{
  SleqpNewtonData* newton_data;

  SleqpSparseVec* expected_step;
  SleqpSparseVec* actual_step;

  SleqpAugJacobian* jacobian;


  int num_variables = problem->num_variables;
  int num_constraints = problem->num_constraints;

  ASSERT_CALL(sleqp_sparse_vector_create(&expected_step, num_variables, 2));

  ASSERT_CALL(sleqp_sparse_vector_push(expected_step, 0, -1.));
  ASSERT_CALL(sleqp_sparse_vector_push(expected_step, 1, -2.));

  ASSERT_CALL(sleqp_sparse_vector_create(&actual_step, num_variables, 0));

  double penalty_parameter = 1.;
  double trust_radius = 10.;

  // create with empty active set
  ASSERT_CALL(sleqp_aug_jacobian_create(&jacobian,
                                        problem));

  ASSERT_CALL(sleqp_aug_jacobian_set_iterate(jacobian, iterate));

  ASSERT_CALL(sleqp_newton_data_create(&newton_data, problem));

  // we use the default (empty) active set for the Newton step,
  // trust region size should be large to ensure that
  // the solution is that of the unrestricted step
  ASSERT_CALL(sleqp_newton_compute_step(newton_data,
                                        iterate,
                                        jacobian,
                                        trust_radius,
                                        penalty_parameter,
                                        actual_step));

  ck_assert(sleqp_sparse_vector_eq(expected_step, actual_step));

  ASSERT_CALL(sleqp_newton_data_free(&newton_data));

  ASSERT_CALL(sleqp_aug_jacobian_free(&jacobian));

  ASSERT_CALL(sleqp_sparse_vector_free(&actual_step));

  ASSERT_CALL(sleqp_sparse_vector_free(&expected_step));
}
END_TEST

START_TEST(newton_small_step)
{
  SleqpNewtonData* newton_data;

  SleqpSparseVec* expected_step;
  SleqpSparseVec* actual_step;

  SleqpAugJacobian* jacobian;


  int num_variables = problem->num_variables;
  int num_constraints = problem->num_constraints;

  ASSERT_CALL(sleqp_sparse_vector_create(&expected_step, num_variables, 2));

  ASSERT_CALL(sleqp_sparse_vector_push(expected_step, 0, -0.44721359549995793));
  ASSERT_CALL(sleqp_sparse_vector_push(expected_step, 1, -0.89442719099991586));

  ASSERT_CALL(sleqp_sparse_vector_create(&actual_step, num_variables, 0));

  double penalty_parameter = 1.;
  double trust_radius = 1.;

  // create with empty active set
  ASSERT_CALL(sleqp_aug_jacobian_create(&jacobian,
                                        problem));

  ASSERT_CALL(sleqp_aug_jacobian_set_iterate(jacobian, iterate));

  ASSERT_CALL(sleqp_newton_data_create(&newton_data, problem));

  // we use the default (empty) active set for the Newton step,
  // trust region size should be large to ensure that
  // the solution is that of the unrestricted step
  ASSERT_CALL(sleqp_newton_compute_step(newton_data,
                                        iterate,
                                        jacobian,
                                        trust_radius,
                                        penalty_parameter,
                                        actual_step));

  ck_assert(sleqp_sparse_vector_eq(expected_step, actual_step));

  ASSERT_CALL(sleqp_newton_data_free(&newton_data));

  ASSERT_CALL(sleqp_aug_jacobian_free(&jacobian));

  ASSERT_CALL(sleqp_sparse_vector_free(&actual_step));

  ASSERT_CALL(sleqp_sparse_vector_free(&expected_step));
}
END_TEST

Suite* newton_test_suite()
{
  Suite *suite;
  TCase *tc_cons;

  suite = suite_create("Unconstrained newton step tests");

  tc_cons = tcase_create("Newton step");

  tcase_add_checked_fixture(tc_cons,
                            newton_setup,
                            newton_teardown);

  tcase_add_test(tc_cons, newton_wide_step);

  tcase_add_test(tc_cons, newton_small_step);

  suite_add_tcase(suite, tc_cons);

  return suite;
}

int main()
{
  int num_fails;
  Suite* suite;
  SRunner* srunner;

  suite = newton_test_suite();
  srunner = srunner_create(suite);

  srunner_set_fork_status(srunner, CK_NOFORK);
  srunner_run_all(srunner, CK_NORMAL);

  num_fails = srunner_ntests_failed(srunner);

  srunner_free(srunner);

  return (num_fails > 0) ? EXIT_FAILURE : EXIT_SUCCESS;
}
