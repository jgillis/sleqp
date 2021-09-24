#include "unconstrained_cauchy.h"

#include "mem.h"
#include "working_set.h"

typedef struct {
  SleqpProblem* problem;
  SleqpParams* params;

  SleqpIterate* iterate;
  double trust_radius;

  SleqpSparseVec* direction;
  double objective;

} CauchyData;

static SLEQP_RETCODE
unconstrained_cauchy_set_iterate(SleqpIterate* iterate,
                                 double trust_radius,
                                 void* data)
{
  CauchyData* cauchy_data = (CauchyData*) data;

  SLEQP_CALL(sleqp_iterate_release(&cauchy_data->iterate));

  SLEQP_CALL(sleqp_iterate_capture(iterate));

  cauchy_data->iterate = iterate;

  cauchy_data->trust_radius = trust_radius;

  return SLEQP_OKAY;
}

static SLEQP_RETCODE
unconstrained_cauchy_set_trust_radius(double trust_radius,
                                      void* data)
{
  CauchyData* cauchy_data = (CauchyData*) data;

  cauchy_data->trust_radius = trust_radius;

  return SLEQP_OKAY;
}

static SLEQP_RETCODE
unconstrained_cauchy_solve(SleqpSparseVec* gradient,
                           double penalty,
                           SLEQP_CAUCHY_OBJECTIVE_TYPE objective_type,
                           void* data)
{
  CauchyData* cauchy_data = (CauchyData*) data;

  SleqpProblem* problem = cauchy_data->problem;
  SleqpIterate* iterate = cauchy_data->iterate;

  SleqpSparseVec* direction = cauchy_data->direction;

  double* objective = &(cauchy_data->objective);

  (*objective) = sleqp_iterate_get_func_val(iterate);

  const SleqpSparseVec* grad = sleqp_iterate_get_func_grad(iterate);

  const int num_variables = sleqp_problem_num_variables(problem);
  const double trust_radius = cauchy_data->trust_radius;

  SLEQP_CALL(sleqp_sparse_vector_clear(direction));

  int k_g = 0;

  for(int j = 0; j < num_variables; ++j)
  {
    while(k_g < grad->nnz && grad->indices[k_g] < j)
    {
      ++k_g;
    }

    const double g_val = (k_g < grad->nnz && grad->indices[k_g] == j) ? grad->data[k_g] : 0.;
    const double ng_val = -g_val;

    if(ng_val >= 0.)
    {
      SLEQP_CALL(sleqp_sparse_vector_push(direction, j, trust_radius));

      (*objective) += trust_radius * g_val;
    }
    else
    {
      SLEQP_CALL(sleqp_sparse_vector_push(direction, j, -trust_radius));

      (*objective) += (-trust_radius) * g_val;
    }
  }

  return SLEQP_OKAY;
}

static SLEQP_RETCODE
unconstrained_cauchy_get_objective_value(double* objective_value,
                                         void* data)
{
  CauchyData* cauchy_data = (CauchyData*) data;

  *objective_value = cauchy_data->objective;

  return SLEQP_OKAY;
}

static SLEQP_RETCODE
unconstrained_cauchy_get_working_set(SleqpIterate* iterate,
                                     void* data)
{
  SleqpWorkingSet* working_set = sleqp_iterate_get_working_set(iterate);

  SLEQP_CALL(sleqp_working_set_reset(working_set));

  return SLEQP_OKAY;
}

static SLEQP_RETCODE
unconstrained_cauchy_get_direction(SleqpSparseVec* direction,
                                   void* data)
{
  CauchyData* cauchy_data = (CauchyData*) data;

  SLEQP_CALL(sleqp_sparse_vector_copy(cauchy_data->direction,
                                      direction));

  return SLEQP_OKAY;
}

static SLEQP_RETCODE
unconstrained_cauchy_locally_infeasible(bool* locally_infeasible,
                                        void* data)
{
  *locally_infeasible = false;

  return SLEQP_OKAY;
}

static SLEQP_RETCODE
unconstrained_cauchy_get_dual_estimation(SleqpIterate* iterate,
                                         void* data)
{
  SleqpSparseVec* vars_dual = sleqp_iterate_get_vars_dual(iterate);
  SleqpSparseVec* cons_dual = sleqp_iterate_get_cons_dual(iterate);

  SLEQP_CALL(sleqp_sparse_vector_clear(vars_dual));
  SLEQP_CALL(sleqp_sparse_vector_clear(cons_dual));

  return SLEQP_OKAY;
}

static SLEQP_RETCODE
unconstrained_cauchy_get_violation(double* violation,
                                   void* data)
{
  *violation = 0.;

  return SLEQP_OKAY;
}

static SLEQP_RETCODE
unconstrained_cauchy_get_basis_condition(bool* exact,
                                         double* condition,
                                         void* data)
{
  *condition = 1.;
  *exact = true;

  return SLEQP_OKAY;
}

static SLEQP_RETCODE
unconstrained_cauchy_free(void* data)
{
  CauchyData* cauchy_data = (CauchyData*) data;

  SLEQP_CALL(sleqp_sparse_vector_free(&cauchy_data->direction));

  SLEQP_CALL(sleqp_iterate_release(&cauchy_data->iterate));

  SLEQP_CALL(sleqp_params_release(&cauchy_data->params));

  SLEQP_CALL(sleqp_problem_release(&cauchy_data->problem));

  sleqp_free(&cauchy_data);

  return SLEQP_OKAY;
}

static SLEQP_RETCODE
cauchy_data_create(CauchyData** star,
                   SleqpProblem* problem,
                   SleqpParams* params)
{
  SLEQP_CALL(sleqp_malloc(star));

  CauchyData* cauchy_data = *star;

  *cauchy_data = (CauchyData) {0};

  SLEQP_CALL(sleqp_problem_capture(problem));

  cauchy_data->problem = problem;

  SLEQP_CALL(sleqp_params_capture(params));

  cauchy_data->params = params;

  const int num_variables = sleqp_problem_num_variables(problem);

  SLEQP_CALL(sleqp_sparse_vector_create_full(&cauchy_data->direction,
                                             num_variables));

  cauchy_data->trust_radius = SLEQP_NONE;

  return SLEQP_OKAY;
}

SLEQP_RETCODE sleqp_unconstrained_cauchy_create(SleqpCauchy** star,
                                                SleqpProblem* problem,
                                                SleqpParams* params)
{
  CauchyData* cauchy_data;

  SLEQP_CALL(cauchy_data_create(&cauchy_data, problem, params));

  SleqpCauchyCallbacks callbacks = {
    .set_iterate         = unconstrained_cauchy_set_iterate,
    .set_trust_radius    = unconstrained_cauchy_set_trust_radius,
    .solve               = unconstrained_cauchy_solve,
    .get_objective_value = unconstrained_cauchy_get_objective_value,
    .get_working_set     = unconstrained_cauchy_get_working_set,
    .get_direction       = unconstrained_cauchy_get_direction,
    .locally_infeasible  = unconstrained_cauchy_locally_infeasible,
    .get_dual_estimation = unconstrained_cauchy_get_dual_estimation,
    .get_violation       = unconstrained_cauchy_get_violation,
    .get_basis_condition = unconstrained_cauchy_get_basis_condition,
    .free                = unconstrained_cauchy_free
  };

  SLEQP_CALL(sleqp_cauchy_create(star,
                                 &callbacks,
                                 (void*) cauchy_data));

  return SLEQP_OKAY;
}
