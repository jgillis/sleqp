#include "dyn_rosenbrock_fixture.h"

#include <stdlib.h>

#include "dyn.h"

#include "test_common.h"

SleqpFunc* dyn_rosenbrock_func;

SLEQP_RETCODE dyn_rosenbrock_func_val(SleqpFunc* func,
                                      double accuracy,
                                      double* func_val,
                                      void* func_data)
{
  double actual_func_val;

  SLEQP_CALL(rosenbrock_val(rosenbrock_func,
                            &actual_func_val,
                            func_data));

  int r = rand();

  double noise = ((double) r) / ((double) RAND_MAX);

  // uniform in [-1, -1]
  noise = 2.*noise - 1;

  *func_val = (actual_func_val + accuracy * noise);

  return SLEQP_OKAY;
}

void dyn_rosenbrock_setup()
{
  srand(42);

  rosenbrock_setup();

  SleqpDynFuncCallbacks callbacks = {
    .set_value = rosenbrock_set,
    .func_val  = dyn_rosenbrock_func_val,
    .func_grad = rosenbrock_grad,
    .cons_val  = NULL,
    .cons_jac  = NULL,
    .hess_prod = rosenbrock_hess_prod,
    .func_free = NULL
  };

  void* func_data = sleqp_func_get_data(rosenbrock_func);

  ASSERT_CALL(sleqp_dyn_func_create(&dyn_rosenbrock_func,
                                    &callbacks,
                                    rosenbrock_num_variables,
                                    rosenbrock_num_constraints,
                                    func_data));
}

void dyn_rosenbrock_teardown()
{
  ASSERT_CALL(sleqp_func_release(&dyn_rosenbrock_func));

  rosenbrock_teardown();
}
