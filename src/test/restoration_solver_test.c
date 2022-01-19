#include <check.h>
#include <stdlib.h>

#include "cmp.h"
#include "feas.h"
#include "mem.h"
#include "solver.h"
#include "util.h"

#include "test_common.h"

#include "wachbieg_fixture.h"

SleqpParams* params;
SleqpOptions* options;

SleqpProblem* problem;

void
restoration_solver_setup()
{
  wachbieg_setup();

  ASSERT_CALL(sleqp_params_create(&params));
  ASSERT_CALL(sleqp_options_create(&options));

  ASSERT_CALL(
    sleqp_options_set_bool_value(options,
                                 SLEQP_OPTION_BOOL_ENABLE_RESTORATION_PHASE,
                                 true));

  ASSERT_CALL(sleqp_problem_create_simple(&problem,
                                          wachbieg_func,
                                          params,
                                          wachbieg_var_lb,
                                          wachbieg_var_ub,
                                          wachbieg_cons_lb,
                                          wachbieg_cons_ub));
}

void
restoration_solver_teardown()
{
  ASSERT_CALL(sleqp_problem_release(&problem));

  ASSERT_CALL(sleqp_options_release(&options));
  ASSERT_CALL(sleqp_params_release(&params));

  wachbieg_teardown();
}

START_TEST(test_solve)
{
  SleqpSolver* solver;

  ASSERT_CALL(sleqp_solver_create(&solver,
                                  problem,
                                  params,
                                  options,
                                  wachbieg_initial,
                                  NULL));

  ASSERT_CALL(sleqp_solver_solve(solver, SLEQP_NONE, SLEQP_NONE));

  ck_assert_int_eq(sleqp_solver_status(solver), SLEQP_STATUS_OPTIMAL);

  SleqpIterate* solution_iterate;

  ASSERT_CALL(sleqp_solver_solution(solver, &solution_iterate));

  SleqpSparseVec* actual_solution = sleqp_iterate_primal(solution_iterate);

  ck_assert(sleqp_sparse_vector_eq(actual_solution, wachbieg_optimal, 1e-6));

  ASSERT_CALL(sleqp_solver_release(&solver));
}
END_TEST

Suite*
restoration_solver_test_suite()
{
  Suite* suite;
  TCase* tc_restoration;

  suite = suite_create("Restoration solver tests");

  tc_restoration = tcase_create("Restoration test");

  tcase_add_checked_fixture(tc_restoration,
                            restoration_solver_setup,
                            restoration_solver_teardown);

  tcase_add_test(tc_restoration, test_solve);

  suite_add_tcase(suite, tc_restoration);

  return suite;
}

TEST_MAIN(restoration_solver_test_suite)