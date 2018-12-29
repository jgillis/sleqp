#include "sleqp_lpi_gurobi.h"

#include <assert.h>
#include <gurobi_c.h>

#include "sleqp_cmp.h"
#include "sleqp_mem.h"

#define GRB_BASIC 0
#define GRB_BASE_NONBASIC -1

typedef struct SleqpLpiGRB
{
  GRBenv* env;
  GRBmodel* model;

  int num_cols;
  int num_rows;

  int num_lp_cols;

  int* row_basis;
  int* col_basis;

} SleqpLpiGRB;

#define SLEQP_GRB_CALL(x, env)                          \
  do                                                    \
  {                                                     \
    int grb_ret_status = (x);                           \
                                                        \
    if(grb_ret_status)                                  \
    {                                                   \
      const char* error_string = GRBgeterrormsg(env);   \
                                                        \
      sleqp_log_error("Caught Gurobi error <%d> (%s)",  \
                      grb_ret_status,                   \
                      error_string);                    \
                                                        \
      switch(grb_ret_status)                            \
      {                                                 \
      case GRB_ERROR_OUT_OF_MEMORY:                     \
        return SLEQP_NOMEM;                             \
      default:                                          \
        return SLEQP_INTERNAL_ERROR;                    \
      }                                                 \
    }                                                   \
  } while(0)

static SLEQP_RETCODE gurobi_create_problem(void** star,
                                           int num_cols,
                                           int num_rows,
                                           SleqpParams* params)
{
  SleqpLpiGRB* lp_interface = NULL;

  SLEQP_CALL(sleqp_malloc(&lp_interface));

  *star = lp_interface;

  lp_interface->env = NULL;
  lp_interface->model = NULL;

  lp_interface->num_cols = num_cols;
  lp_interface->num_rows = num_rows;
  lp_interface->num_lp_cols = num_rows + num_cols;

  lp_interface->col_basis = NULL;
  lp_interface->row_basis = NULL;

  SLEQP_CALL(sleqp_calloc(&lp_interface->col_basis, num_cols));
  SLEQP_CALL(sleqp_calloc(&lp_interface->row_basis, num_rows));

  int err = GRBloadenv(&lp_interface->env, NULL);

  GRBenv* env = lp_interface->env;

  if(err || env == NULL)
  {
    sleqp_log_error("Failed to create Gurobi environment");
    return SLEQP_INTERNAL_ERROR;
  }

  SLEQP_GRB_CALL(GRBnewmodel(env,
                             &lp_interface->model,
                             "",
                             lp_interface->num_lp_cols,
                             NULL,
                             NULL,
                             NULL,
                             NULL,
                             NULL), env);

  GRBmodel* model = lp_interface->model;

  SLEQP_GRB_CALL(GRBsetintattr(model,
                               GRB_INT_ATTR_MODELSENSE,
                               GRB_MINIMIZE), env);

  for(int i = 0; i < num_rows; ++i)
  {

    SLEQP_GRB_CALL(GRBaddconstr(model,
                                0,
                                NULL,
                                NULL,
                                GRB_EQUAL,
                                0.,
                                NULL), env);
  }

  double neg_one = -1.;

  for(int i = 0; i < num_rows; ++i)
  {
    int c = num_cols + i;

    SLEQP_GRB_CALL(GRBchgcoeffs(model,
                                1,
                                &i,
                                &c,
                                &neg_one), env);
  }

  return SLEQP_OKAY;
}

static SLEQP_RETCODE gurobi_solve(void* lp_data,
                                  int num_cols,
                                  int num_rows)
{
  SleqpLpiGRB* lp_interface = lp_data;
  int sol_stat;

  GRBenv* env = lp_interface->env;
  GRBmodel* model = lp_interface->model;

  SLEQP_GRB_CALL(GRBoptimize(model), env);

  SLEQP_GRB_CALL(GRBgetintattr(model, GRB_INT_ATTR_STATUS, &sol_stat), env);

  assert(sol_stat == GRB_OPTIMAL);

  return SLEQP_OKAY;
}

static SLEQP_RETCODE gurobi_set_bounds(void* lp_data,
                                       int num_cols,
                                       int num_rows,
                                       double* cons_lb,
                                       double* cons_ub,
                                       double* vars_lb,
                                       double* vars_ub)
{
  SleqpLpiGRB* lp_interface = lp_data;

  GRBenv* env = lp_interface->env;
  GRBmodel* model = lp_interface->model;

  SLEQP_GRB_CALL(GRBsetdblattrarray(model,
                                    GRB_DBL_ATTR_LB,
                                    0,
                                    num_cols,
                                    vars_lb), env);

  SLEQP_GRB_CALL(GRBsetdblattrarray(model,
                                    GRB_DBL_ATTR_UB,
                                    0,
                                    num_cols,
                                    vars_ub), env);

  SLEQP_GRB_CALL(GRBsetdblattrarray(model,
                                    GRB_DBL_ATTR_LB,
                                    num_cols,
                                    num_rows,
                                    cons_lb), env);

  SLEQP_GRB_CALL(GRBsetdblattrarray(model,
                                    GRB_DBL_ATTR_UB,
                                    num_cols,
                                    num_rows,
                                    cons_ub), env);

  return SLEQP_OKAY;
}

static SLEQP_RETCODE gurobi_set_coefficients(void* lp_data,
                                             int num_cols,
                                             int num_rows,
                                             SleqpSparseMatrix* coeff_matrix)
{
  SleqpLpiGRB* lp_interface = lp_data;

  GRBenv* env = lp_interface->env;
  GRBmodel* model = lp_interface->model;

  for(int col = 0; col < coeff_matrix->num_cols; ++col)
  {
    for(int k = coeff_matrix->cols[col]; k < coeff_matrix->cols[col +1]; ++k)
    {
      int row = coeff_matrix->rows[k];
      double entry = coeff_matrix->data[k];

      SLEQP_GRB_CALL(GRBchgcoeffs(model,
                                  1,
                                  &row,
                                  &col,
                                  &entry), env);
    }
  }

  return SLEQP_OKAY;
}


static SLEQP_RETCODE gurobi_set_objective(void* lp_data,
                                          int num_cols,
                                          int num_rows,
                                          double* objective)
{
  SleqpLpiGRB* lp_interface = lp_data;

  GRBenv* env = lp_interface->env;
  GRBmodel* model = lp_interface->model;

  SLEQP_GRB_CALL(GRBsetdblattrarray(model,
                                    GRB_DBL_ATTR_OBJ,
                                    0,
                                    num_cols,
                                    objective), env);

  return SLEQP_OKAY;
}

static SLEQP_RETCODE gurobi_get_solution(void* lp_data,
                                         int num_cols,
                                         int num_rows,
                                         double* objective_value,
                                         double* solution_values)
{
  SleqpLpiGRB* lp_interface = lp_data;

  GRBenv* env = lp_interface->env;
  GRBmodel* model = lp_interface->model;

  if(objective_value)
  {
    SLEQP_GRB_CALL(GRBgetdblattr(model,
                                 GRB_DBL_ATTR_OBJVAL,
                                 objective_value), env);
  }

  if(solution_values)
  {
    SLEQP_GRB_CALL(GRBgetdblattrarray(model,
                                      GRB_DBL_ATTR_X,
                                      0,
                                      num_cols,
                                      solution_values), env);
  }

  return SLEQP_OKAY;
}

static SLEQP_RETCODE gurobi_get_varstats(void* lp_data,
                                         int num_cols,
                                         int num_rows,
                                         SLEQP_BASESTAT* variable_stats)
{
  SleqpLpiGRB* lp_interface = lp_data;

  GRBenv* env = lp_interface->env;
  GRBmodel* model = lp_interface->model;

  SLEQP_GRB_CALL(GRBgetintattrarray(model,
                                    GRB_INT_ATTR_VBASIS,
                                    0,
                                    num_cols,
                                    lp_interface->col_basis), env);

  for(int j = 0; j < num_cols; ++j)
  {
    switch(lp_interface->col_basis[j])
    {
    case GRB_BASIC:
      variable_stats[j] = SLEQP_BASESTAT_BASIC;
      break;
    case GRB_NONBASIC_LOWER:
      variable_stats[j] = SLEQP_BASESTAT_LOWER;
      break;
    case GRB_NONBASIC_UPPER:
      variable_stats[j] = SLEQP_BASESTAT_UPPER;
      break;
    case GRB_SUPERBASIC:
      sleqp_log_error("Encountered a super-basic variable");
    default:
      assert(false);
    }
  }

  return SLEQP_OKAY;
}

static SLEQP_RETCODE gurobi_get_consstats(void* lp_data,
                                          int num_cols,
                                          int num_rows,
                                          SLEQP_BASESTAT* constraint_stats)
{
  SleqpLpiGRB* lp_interface = lp_data;

  GRBenv* env = lp_interface->env;
  GRBmodel* model = lp_interface->model;

  SLEQP_GRB_CALL(GRBgetintattrarray(model,
                                    GRB_INT_ATTR_VBASIS,
                                    num_cols,
                                    num_rows,
                                    lp_interface->row_basis), env);

  for(int i = 0; i < num_rows; ++i)
  {
    switch(lp_interface->row_basis[i])
    {
    case GRB_BASIC:
      constraint_stats[i] = SLEQP_BASESTAT_BASIC;
      break;
    case GRB_NONBASIC_LOWER:
      constraint_stats[i] = SLEQP_BASESTAT_LOWER;
      break;
    case GRB_NONBASIC_UPPER:
      constraint_stats[i] = SLEQP_BASESTAT_UPPER;
      break;
    case GRB_SUPERBASIC:
      sleqp_log_error("Encountered a super-basic constraint");
    default:
      assert(false);
    }
  }

  return SLEQP_OKAY;
}

static SLEQP_RETCODE gurobi_free(void** star)
{
  SleqpLpiGRB* lp_interface = *star;

  if(!lp_interface)
  {
    return SLEQP_OKAY;
  }

  if(lp_interface->model)
  {
    GRBfreemodel(lp_interface->model);
  }

  /* Free environment */
  if(lp_interface->env)
  {
    GRBfreeenv(lp_interface->env);
  }

  sleqp_free(star);

  return SLEQP_OKAY;
}

SLEQP_RETCODE sleqp_lpi_gurobi_create_interface(SleqpLPi** lp_star,
                                                int num_cols,
                                                int num_rows,
                                                SleqpParams* params)
{
  return sleqp_lpi_create_interface(lp_star,
                                    num_cols,
                                    num_rows,
                                    params,
                                    gurobi_create_problem,
                                    gurobi_solve,
                                    gurobi_set_bounds,
                                    gurobi_set_coefficients,
                                    gurobi_set_objective,
                                    gurobi_get_solution,
                                    gurobi_get_varstats,
                                    gurobi_get_consstats,
                                    gurobi_free);
}