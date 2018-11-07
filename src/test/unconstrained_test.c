#include <stdlib.h>
#include <check.h>

#include "sleqp_cmp.h"
#include "sleqp_mem.h"
#include "sleqp_solver.h"

#include "test_common.h"

#include "rosenbrock_fixture.h"


START_TEST(test_unconstrained_solve)
{
  SleqpSparseVec* expected_solution;

  SleqpParams* params;
  SleqpProblem* problem;
  SleqpSolver* solver;


  ASSERT_CALL(sleqp_sparse_vector_create(&expected_solution, 2, 2));

  ASSERT_CALL(sleqp_sparse_vector_push(expected_solution, 0, 1.));
  ASSERT_CALL(sleqp_sparse_vector_push(expected_solution, 1, 1.));

  ASSERT_CALL(sleqp_params_create(&params));

  ASSERT_CALL(sleqp_problem_create(&problem,
                                   rosenbrock_func,
                                   params,
                                   rosenbrock_var_lb,
                                   rosenbrock_var_ub,
                                   rosenbrock_cons_lb,
                                   rosenbrock_cons_ub));

  ASSERT_CALL(sleqp_solver_create(&solver,
                                  problem,
                                  params,
                                  rosenbrock_x));

  /*
  ASSERT_CALL(sleqp_sparse_vector_clear(rosenbrock_x));

  ASSERT_CALL(sleqp_sparse_vector_reserve(rosenbrock_x, 2));

  ASSERT_CALL(sleqp_sparse_vector_push(rosenbrock_x, 0, 10.));
  ASSERT_CALL(sleqp_sparse_vector_push(rosenbrock_x, 1, 10.));
  */

  // 100 iterations should be plenty...
  ASSERT_CALL(sleqp_solver_solve(solver, 100));

  SleqpIterate* solution_iterate;

  ASSERT_CALL(sleqp_solver_get_solution(solver,
                                        &solution_iterate));

  ck_assert_int_eq(sleqp_solver_get_status(solver), SLEQP_OPTIMAL);

  SleqpSparseVec* actual_solution = solution_iterate->x;

  ck_assert(sleqp_sparse_vector_eq(actual_solution,
                                   expected_solution,
                                   1e-6));

  ASSERT_CALL(sleqp_solver_free(&solver));

  ASSERT_CALL(sleqp_problem_free(&problem));

  ASSERT_CALL(sleqp_params_free(&params));

  ASSERT_CALL(sleqp_sparse_vector_free(&expected_solution));
}
END_TEST

Suite* unconstrained_test_suite()
{
  Suite *suite;
  TCase *tc_uncons;

  suite = suite_create("Unconstrained tests");

  tc_uncons = tcase_create("Unconstrained solution test");

  tcase_add_checked_fixture(tc_uncons,
                            rosenbrock_setup,
                            rosenbrock_teardown);

  tcase_add_test(tc_uncons, test_unconstrained_solve);
  suite_add_tcase(suite, tc_uncons);

  return suite;
}


int main()
{
  int num_fails;
  Suite* suite;
  SRunner* srunner;

  suite = unconstrained_test_suite();
  srunner = srunner_create(suite);

  srunner_set_fork_status(srunner, CK_NOFORK);
  srunner_run_all(srunner, CK_NORMAL);

  num_fails = srunner_ntests_failed(srunner);

  srunner_free(srunner);

  return (num_fails > 0) ? EXIT_FAILURE : EXIT_SUCCESS;
}
