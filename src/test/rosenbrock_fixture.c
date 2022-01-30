#include "rosenbrock_fixture.h"

#include <threads.h>

#include "cmp.h"
#include "mem.h"

const int rosenbrock_num_variables   = 2;
const int rosenbrock_num_constraints = 0;

typedef struct RosenbrockData
{
  double a, b;

  double* x;
} RosenbrockData;

static inline double
sq(double v)
{
  return v * v;
}

SleqpFunc* rosenbrock_func;

SleqpSparseVec* rosenbrock_var_lb;
SleqpSparseVec* rosenbrock_var_ub;
SleqpSparseVec* rosenbrock_cons_lb;
SleqpSparseVec* rosenbrock_cons_ub;
SleqpSparseVec* rosenbrock_initial;
SleqpSparseVec* rosenbrock_optimal;

SLEQP_RETCODE
rosenbrock_set(SleqpFunc* func,
               SleqpSparseVec* x,
               SLEQP_VALUE_REASON reason,
               bool* reject,
               int* obj_grad_nnz,
               int* cons_val_nnz,
               int* cons_jac_nnz,
               void* func_data)
{
  *obj_grad_nnz = 2;
  *cons_val_nnz = 0;
  *cons_jac_nnz = 0;

  RosenbrockData* data = (RosenbrockData*)func_data;

  data->x[0] = 0;
  data->x[1] = 0;

  int k_x = 0;

  while (k_x < x->nnz)
  {
    data->x[x->indices[k_x]] = x->data[k_x];

    ++k_x;
  }

  return SLEQP_OKAY;
}

SLEQP_RETCODE
rosenbrock_obj_val(SleqpFunc* func, double* obj_val, void* func_data)
{
  RosenbrockData* data = (RosenbrockData*)func_data;

  double x = data->x[0];
  double y = data->x[1];

  double a = data->a;
  double b = data->b;

  double xsq = sq(x);

  *obj_val = sq(a - x) + b * sq(y - xsq);

  return SLEQP_OKAY;
}

SLEQP_RETCODE
rosenbrock_obj_grad(SleqpFunc* func, SleqpSparseVec* obj_grad, void* func_data)
{
  RosenbrockData* data = (RosenbrockData*)func_data;

  assert(obj_grad->nnz == 0);
  assert(obj_grad->dim == 2);

  double x = data->x[0];
  double y = data->x[1];

  double a = data->a;
  double b = data->b;

  double xsq = sq(x);

  double gradx = (4. * b * x * (xsq - y)) + 2. * x - 2. * a;

  double grady = -2. * b * (xsq - y);

  SLEQP_CALL(sleqp_sparse_vector_push(obj_grad, 0, gradx));

  SLEQP_CALL(sleqp_sparse_vector_push(obj_grad, 1, grady));

  return SLEQP_OKAY;
}

SLEQP_RETCODE
rosenbrock_hess_prod(SleqpFunc* func,
                     const double* obj_dual,
                     const SleqpSparseVec* direction,
                     const SleqpSparseVec* cons_duals,
                     SleqpSparseVec* product,
                     void* func_data)
{
  RosenbrockData* data = (RosenbrockData*)func_data;

  double x = data->x[0];
  double y = data->x[1];

  double b = data->b;

  double xsq = sq(x);

  double d[2];

  SLEQP_CALL(sleqp_sparse_vector_to_raw(direction, d));

  if (obj_dual)
  {
    SLEQP_CALL(sleqp_sparse_vector_reserve(product, 2));

    SLEQP_CALL(sleqp_sparse_vector_push(
      product,
      0,
      (8. * b * xsq + 4. * b * (xsq - y) + 2.) * d[0] - (4. * b * x) * d[1]));

    SLEQP_CALL(
      sleqp_sparse_vector_push(product,
                               1,
                               (-4. * b * x) * d[0] + (2. * b) * d[1]));
  }

  return SLEQP_OKAY;
}

SLEQP_RETCODE
rosenbrock_free(void* data)
{
  RosenbrockData* func_data = (RosenbrockData*)data;

  sleqp_free(&func_data->x);

  sleqp_free(&func_data);

  return SLEQP_OKAY;
}

void
rosenbrock_create(SleqpFunc** fstar,
                  SleqpSparseVec** var_lbstar,
                  SleqpSparseVec** var_ubstar,
                  SleqpSparseVec** cons_lbstar,
                  SleqpSparseVec** cons_ubstar,
                  SleqpSparseVec** init_star,
                  SleqpSparseVec** opt_star)
{
  RosenbrockData* func_data;

  const double inf = sleqp_infinity();

  ASSERT_CALL(sleqp_malloc(&func_data));

  func_data->a = 1.;
  func_data->b = 100.;

  ASSERT_CALL(sleqp_alloc_array(&func_data->x, 2));

  SleqpFuncCallbacks callbacks = {.set_value = rosenbrock_set,
                                  .obj_val   = rosenbrock_obj_val,
                                  .obj_grad  = rosenbrock_obj_grad,
                                  .cons_val  = NULL,
                                  .cons_jac  = NULL,
                                  .hess_prod = rosenbrock_hess_prod,
                                  .func_free = rosenbrock_free};

  ASSERT_CALL(sleqp_func_create(fstar,
                                &callbacks,
                                rosenbrock_num_variables,
                                rosenbrock_num_constraints,
                                func_data));

  ASSERT_CALL(sleqp_sparse_vector_create(var_lbstar, 2, 2));

  ASSERT_CALL(sleqp_sparse_vector_push(*var_lbstar, 0, -inf));
  ASSERT_CALL(sleqp_sparse_vector_push(*var_lbstar, 1, -inf));

  ASSERT_CALL(sleqp_sparse_vector_create(var_ubstar, 2, 2));

  ASSERT_CALL(sleqp_sparse_vector_push(*var_ubstar, 0, inf));
  ASSERT_CALL(sleqp_sparse_vector_push(*var_ubstar, 1, inf));

  ASSERT_CALL(sleqp_sparse_vector_create_empty(cons_lbstar, 0));

  ASSERT_CALL(sleqp_sparse_vector_create_empty(cons_ubstar, 0));

  ASSERT_CALL(sleqp_sparse_vector_create_empty(init_star, 2));

  ASSERT_CALL(sleqp_sparse_vector_create_full(opt_star, 2));

  ASSERT_CALL(sleqp_sparse_vector_push(*opt_star, 0, 1.));
  ASSERT_CALL(sleqp_sparse_vector_push(*opt_star, 1, 1.));
}

void
rosenbrock_setup()
{
  rosenbrock_create(&rosenbrock_func,
                    &rosenbrock_var_lb,
                    &rosenbrock_var_ub,
                    &rosenbrock_cons_lb,
                    &rosenbrock_cons_ub,
                    &rosenbrock_initial,
                    &rosenbrock_optimal);
}

void
rosenbrock_teardown()
{
  ASSERT_CALL(sleqp_sparse_vector_free(&rosenbrock_optimal));

  ASSERT_CALL(sleqp_sparse_vector_free(&rosenbrock_initial));

  ASSERT_CALL(sleqp_sparse_vector_free(&rosenbrock_cons_ub));

  ASSERT_CALL(sleqp_sparse_vector_free(&rosenbrock_cons_lb));

  ASSERT_CALL(sleqp_sparse_vector_free(&rosenbrock_var_ub));

  ASSERT_CALL(sleqp_sparse_vector_free(&rosenbrock_var_lb));

  ASSERT_CALL(sleqp_func_release(&rosenbrock_func));
}
