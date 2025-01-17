#include "lpi_highs.h"

#include <math.h>

#include <interfaces/highs_c_api.h>

#include "cmp.h"
#include "defs.h"
#include "error.h"
#include "fail.h"
#include "log.h"
#include "mem.h"
#include "pub_types.h"

typedef struct SleqpLpiHIGHS
{
  void* highs;

  SLEQP_LP_STATUS status;

  int num_cols;
  int num_rows;

  int num_bases;
  int** row_bases;
  int** col_bases;

  int* col_basis;
  int* row_basis;

  double* costs;
  double* col_lb;
  double* col_ub;

  double* row_lb;
  double* row_ub;

  enum
  {
    NONE       = 0,
    COL_BOUNDS = (1 << 0),
    ROW_BOUNDS = (1 << 1),
    OBJECTIVE  = (1 << 2),
    COEFFS     = (1 << 3),
    ALL        = (COL_BOUNDS | ROW_BOUNDS | OBJECTIVE | COEFFS),
  } dirty;

  double* cols_primal_dummysol;
  double* rows_primal_dummysol;
  double* cols_dual_dummysol;
  double* rows_dual_dummysol;

} SleqpLpiHIGHS;

#define SLEQP_HIGHS_CALL(x, highs)                                             \
  do                                                                           \
  {                                                                            \
    int highs_ret_status = (x);                                                \
                                                                               \
    if (highs_ret_status == kHighsStatusError)                                 \
    {                                                                          \
      sleqp_raise(SLEQP_INTERNAL_ERROR,                                        \
                  "Caught HiGHS error <%d>",                                   \
                  highs_ret_status);                                           \
    }                                                                          \
  } while (0)

static SLEQP_RETCODE
highs_create_problem(void** star,
                     int num_cols,
                     int num_rows,
                     SleqpSettings* settings)
{
  SleqpLpiHIGHS* lp_interface = NULL;

  SLEQP_CALL(sleqp_malloc(&lp_interface));

  *lp_interface = (SleqpLpiHIGHS){0};

  *star = lp_interface;

  lp_interface->num_cols = num_cols;
  lp_interface->num_rows = num_rows;
  lp_interface->status   = SLEQP_LP_STATUS_UNKNOWN;

  SLEQP_CALL(sleqp_alloc_array(&lp_interface->col_basis, num_cols));
  SLEQP_CALL(sleqp_alloc_array(&lp_interface->row_basis, num_rows));

  SLEQP_CALL(sleqp_alloc_array(&lp_interface->cols_primal_dummysol,
                               lp_interface->num_cols));
  SLEQP_CALL(sleqp_alloc_array(&lp_interface->rows_primal_dummysol,
                               lp_interface->num_rows));
  SLEQP_CALL(sleqp_alloc_array(&lp_interface->cols_dual_dummysol,
                               lp_interface->num_cols));
  SLEQP_CALL(sleqp_alloc_array(&lp_interface->rows_dual_dummysol,
                               lp_interface->num_rows));

  lp_interface->highs = Highs_create();
  void* highs         = lp_interface->highs;

  if (sleqp_log_level() < SLEQP_LOG_DEBUG)
  {
    SLEQP_HIGHS_CALL(Highs_setBoolOptionValue(highs, "output_flag", false),
                     highs);
  }

  {
    const int num_threads
      = sleqp_settings_int_value(settings, SLEQP_SETTINGS_INT_NUM_THREADS);

    if (num_threads != SLEQP_NONE)
    {
      SLEQP_HIGHS_CALL(Highs_setIntOptionValue(highs, "threads", num_threads),
                       highs);
    }

    bool verbose = (sleqp_log_level() >= SLEQP_LOG_DEBUG);

    if (verbose)
    {
      SLEQP_HIGHS_CALL(Highs_setIntOptionValue(highs, "log_dev_level", 2),
                       highs);
    }
  }

  SLEQP_HIGHS_CALL(
    Highs_setDoubleOptionValue(highs, "infinite_cost", sleqp_infinity()),
    highs);

  SLEQP_HIGHS_CALL(
    Highs_setDoubleOptionValue(highs, "infinite_bound", sleqp_infinity()),
    highs);

  // setting these option causes HiGHS to fail for the
  // constrained test example without quadratic model while the defaults work

  // const double feas_eps = sleqp_settings_real(settings,
  //                                             SLEQP_SETTINGS_REAL_FEASIBILITY_TOL);

  // const double stat_eps = sleqp_settings_real(settings,
  //                                             SLEQP_SETTINGS_REAL_STATIONARITY_TOL);

  // SLEQP_HIGHS_CALL(Highs_setDoubleOptionValue(highs,
  //                                            "primal_feasibility_tolerance",
  //                                            feas_eps), highs);

  // SLEQP_HIGHS_CALL(Highs_setDoubleOptionValue(highs,
  //                                            "dual_feasibility_tolerance",
  //                                            stat_eps), highs);

  // allocate dummy vectors to pass an empty linear program of appropriate size

  SLEQP_CALL(sleqp_alloc_array(&lp_interface->costs, num_cols));

  SLEQP_CALL(sleqp_alloc_array(&lp_interface->col_lb, num_cols));
  SLEQP_CALL(sleqp_alloc_array(&lp_interface->col_ub, num_cols));

  SLEQP_CALL(sleqp_alloc_array(&lp_interface->row_lb, num_rows));
  SLEQP_CALL(sleqp_alloc_array(&lp_interface->row_ub, num_rows));

  for (int i = 0; i < num_cols; ++i)
  {
    lp_interface->costs[i] = 0.;
  }

  for (int i = 0; i < num_cols; ++i)
  {
    lp_interface->col_lb[i] = 0.;
  }

  for (int i = 0; i < num_cols; ++i)
  {
    lp_interface->col_ub[i] = 0.;
  }

  lp_interface->dirty = ALL;

  return SLEQP_OKAY;
}

static SLEQP_RETCODE
highs_write(void* lp_data, const char* filename)
{
  SleqpLpiHIGHS* lp_interface = (SleqpLpiHIGHS*)lp_data;
  void* highs                 = lp_interface->highs;

  SLEQP_HIGHS_CALL(Highs_writeModel(highs, filename), highs);

  return SLEQP_OKAY;
}

static SLEQP_RETCODE
prepare_problem(SleqpLpiHIGHS* lp_interface)
{
  // coeffs must be set
  assert(!(lp_interface->dirty & COEFFS));

  void* highs = lp_interface->highs;

  if (lp_interface->dirty & COL_BOUNDS)
  {

    for (int j = 0; j < lp_interface->num_cols; ++j)
    {
      SLEQP_HIGHS_CALL(Highs_changeColBounds(highs,
                                             j,
                                             lp_interface->col_lb[j],
                                             lp_interface->col_ub[j]),
                       highs);
    }

    lp_interface->dirty &= ~(COL_BOUNDS);
  }

  if (lp_interface->dirty & ROW_BOUNDS)
  {

    for (int i = 0; i < lp_interface->num_rows; ++i)
    {
      SLEQP_HIGHS_CALL(Highs_changeRowBounds(highs,
                                             i,
                                             lp_interface->row_lb[i],
                                             lp_interface->row_ub[i]),
                       highs);
    }

    lp_interface->dirty &= ~(ROW_BOUNDS);
  }

  if (lp_interface->dirty & OBJECTIVE)
  {

    for (int j = 0; j < lp_interface->num_cols; ++j)
    {
      SLEQP_HIGHS_CALL(Highs_changeColCost(highs, j, lp_interface->costs[j]),
                       highs);
    }

    lp_interface->dirty &= ~(OBJECTIVE);
  }

  return SLEQP_OKAY;
}

static SLEQP_RETCODE
highs_solve(void* lp_data, int num_cols, int num_rows, double time_limit)
{
  int model_status;

  SleqpLpiHIGHS* lp_interface = (SleqpLpiHIGHS*)lp_data;
  void* highs                 = lp_interface->highs;

  SLEQP_CALL(prepare_problem(lp_interface));

  assert(lp_interface->dirty == NONE);

  if (time_limit != SLEQP_NONE)
  {
    SLEQP_HIGHS_CALL(
      Highs_setDoubleOptionValue(highs, "time_limit", time_limit),
      highs);
  }

  SLEQP_HIGHS_CALL(Highs_run(highs), highs);

  model_status = Highs_getModelStatus(highs);

  if (model_status == kHighsModelStatusOptimal)
  {
    lp_interface->status = SLEQP_LP_STATUS_OPTIMAL;
  }
  else if (model_status == kHighsModelStatusInfeasible)
  {
    lp_interface->status = SLEQP_LP_STATUS_INF;
  }
  else if (model_status == kHighsModelStatusUnboundedOrInfeasible)
  {
    lp_interface->status = SLEQP_LP_STATUS_INF_OR_UNBOUNDED;
  }
  else if (model_status == kHighsModelStatusUnbounded)
  {
    lp_interface->status = SLEQP_LP_STATUS_UNBOUNDED;
  }
  else if (model_status == kHighsModelStatusTimeLimit)
  {
    lp_interface->status = SLEQP_LP_STATUS_UNKNOWN;
    return SLEQP_ABORT_TIME;
  }
  else
  {
    sleqp_raise(SLEQP_INTERNAL_ERROR, "Invalid HiGHS status: %d", model_status);
  }

  return SLEQP_OKAY;
}

static SLEQP_LP_STATUS
highs_status(void* lp_data)
{
  SleqpLpiHIGHS* lp_interface = (SleqpLpiHIGHS*)lp_data;

  return lp_interface->status;
}

static double
adjust_neg_inf(double value)
{
  return sleqp_is_infinite(-value) ? -INFINITY : value;
}

static double
adjust_pos_inf(double value)
{
  return sleqp_is_infinite(value) ? INFINITY : value;
}

static SLEQP_RETCODE
highs_set_bounds(void* lp_data,
                 int num_cols,
                 int num_rows,
                 double* cons_lb,
                 double* cons_ub,
                 double* vars_lb,
                 double* vars_ub)
{
  SleqpLpiHIGHS* lp_interface = (SleqpLpiHIGHS*)lp_data;

  for (int i = 0; i < num_cols; ++i)
  {
    lp_interface->col_lb[i] = adjust_neg_inf(vars_lb[i]);
    lp_interface->col_ub[i] = adjust_pos_inf(vars_ub[i]);

    lp_interface->dirty |= COL_BOUNDS;
  }

  for (int i = 0; i < num_rows; ++i)
  {
    lp_interface->row_lb[i] = adjust_neg_inf(cons_lb[i]);
    lp_interface->row_ub[i] = adjust_pos_inf(cons_ub[i]);

    lp_interface->dirty |= ROW_BOUNDS;
  }

  return SLEQP_OKAY;
}

static SLEQP_RETCODE
highs_set_coeffs(void* lp_data,
                 int num_cols,
                 int num_rows,
                 SleqpMat* coeff_matrix)
{
  SleqpLpiHIGHS* lp_interface = (SleqpLpiHIGHS*)lp_data;
  void* highs                 = lp_interface->highs;

  assert(sleqp_mat_num_rows(coeff_matrix) == num_rows);
  assert(sleqp_mat_num_cols(coeff_matrix) == num_cols);

  const int* coeff_matrix_cols = sleqp_mat_cols(coeff_matrix);
  const int* coeff_matrix_rows = sleqp_mat_rows(coeff_matrix);
  double* coeff_matrix_data    = sleqp_mat_data(coeff_matrix);
  const int coeff_nnz          = sleqp_mat_nnz(coeff_matrix);

  SLEQP_HIGHS_CALL(Highs_passLp(highs,
                                lp_interface->num_cols,    // num cols
                                lp_interface->num_rows,    // num rows
                                coeff_nnz,                 // num nnz
                                kHighsMatrixFormatColwise, // format
                                kHighsObjSenseMinimize,    // sense
                                0.,                        // objective offset
                                lp_interface->costs,       // costs
                                lp_interface->col_lb,      // var lb
                                lp_interface->col_ub,      // var ub
                                lp_interface->row_lb,      // cons lb
                                lp_interface->row_ub,      // cons ub
                                coeff_matrix_cols,         // coeff colptr
                                coeff_matrix_rows,         // coeff indices
                                coeff_matrix_data),        // coeff values
                   highs);

  lp_interface->dirty &= ~(COL_BOUNDS | ROW_BOUNDS | OBJECTIVE | COEFFS);

  return SLEQP_OKAY;
}

static SLEQP_RETCODE
highs_set_objective(void* lp_data,
                    int num_cols,
                    int num_rows,
                    double* objective)
{
  SleqpLpiHIGHS* lp_interface = (SleqpLpiHIGHS*)lp_data;

  for (int i = 0; i < num_cols; ++i)
  {
    lp_interface->costs[i] = objective[i];
  }

  lp_interface->dirty |= OBJECTIVE;

  return SLEQP_OKAY;
}

static SLEQP_RETCODE
highs_reserve_bases(SleqpLpiHIGHS* lp_interface, int size)
{
  if (size <= lp_interface->num_bases)
  {
    return SLEQP_OKAY;
  }

  SLEQP_CALL(sleqp_realloc(&lp_interface->row_bases, size));
  SLEQP_CALL(sleqp_realloc(&lp_interface->col_bases, size));

  for (int j = lp_interface->num_bases; j < size; ++j)
  {
    SLEQP_CALL(
      sleqp_alloc_array(&lp_interface->row_bases[j], lp_interface->num_rows));
    SLEQP_CALL(
      sleqp_alloc_array(&lp_interface->col_bases[j], lp_interface->num_cols));
  }

  lp_interface->num_bases = size;

  return SLEQP_OKAY;
}

static SLEQP_RETCODE
basestat_for(int status, SLEQP_BASESTAT* stat)
{
  if (status == kHighsBasisStatusBasic)
  {
    *stat = SLEQP_BASESTAT_BASIC;
    return SLEQP_OKAY;
  }
  else if (status == kHighsBasisStatusLower)
  {
    *stat = SLEQP_BASESTAT_LOWER;
    return SLEQP_OKAY;
  }
  else if (status == kHighsBasisStatusUpper)
  {
    *stat = SLEQP_BASESTAT_UPPER;
    return SLEQP_OKAY;
  }
  else if (status == kHighsBasisStatusZero)
  {
    *stat = SLEQP_BASESTAT_ZERO;
    return SLEQP_OKAY;
  }
  else if (status == kHighsBasisStatusNonbasic)
  {
    sleqp_raise(SLEQP_INTERNAL_ERROR,
                "Encountered an unspecific non-basic variable");
  }

  sleqp_raise(SLEQP_INTERNAL_ERROR, "Invalid basis status");
}

static int
basestat_from(SLEQP_BASESTAT status)
{
  switch (status)
  {
  case SLEQP_BASESTAT_BASIC:
    return kHighsBasisStatusBasic;
  case SLEQP_BASESTAT_LOWER:
    return kHighsBasisStatusLower;
  case SLEQP_BASESTAT_UPPER:
    return kHighsBasisStatusUpper;
  case SLEQP_BASESTAT_ZERO:
    return kHighsBasisStatusZero;
  default:
    break;
  }

  // Invalid basis status reported
  assert(false);

  return kHighsBasisStatusBasic;
}

static SLEQP_RETCODE
highs_set_basis(void* lp_data,
                int index,
                const SLEQP_BASESTAT* col_stats,
                const SLEQP_BASESTAT* row_stats)
{
  SleqpLpiHIGHS* lp_interface = (SleqpLpiHIGHS*)lp_data;

  assert(index >= 0);

  SLEQP_CALL(highs_reserve_bases(lp_interface, index + 1));

  int* row_base = lp_interface->row_bases[index];
  int* col_base = lp_interface->col_bases[index];

  for (int j = 0; j < lp_interface->num_cols; ++j)
  {
    col_base[j] = basestat_from(col_stats[j]);
  }

  for (int i = 0; i < lp_interface->num_rows; ++i)
  {
    row_base[i] = basestat_from(row_stats[i]);
  }

  return SLEQP_OKAY;
}

static SLEQP_RETCODE
highs_save_basis(void* lp_data, int index)
{
  SleqpLpiHIGHS* lp_interface = (SleqpLpiHIGHS*)lp_data;
  void* highs                 = lp_interface->highs;

  assert(index >= 0);

  SLEQP_CALL(highs_reserve_bases(lp_interface, index + 1));

  SLEQP_HIGHS_CALL(Highs_getBasis(highs,
                                  lp_interface->col_bases[index],
                                  lp_interface->row_bases[index]),
                   highs);

  return SLEQP_OKAY;
}

static SLEQP_RETCODE
highs_restore_basis(void* lp_data, int index)
{
  SleqpLpiHIGHS* lp_interface = (SleqpLpiHIGHS*)lp_data;
  void* highs                 = lp_interface->highs;

  assert(index >= 0);
  assert(index < lp_interface->num_bases);

  SLEQP_HIGHS_CALL(Highs_setBasis(highs,
                                  lp_interface->col_bases[index],
                                  lp_interface->row_bases[index]),
                   highs);

  return SLEQP_OKAY;
}

static SLEQP_RETCODE
highs_primal_sol(void* lp_data,
                 int num_cols,
                 int num_rows,
                 double* objective_value,
                 double* solution_values)
{
  SleqpLpiHIGHS* lp_interface = (SleqpLpiHIGHS*)lp_data;
  void* highs                 = lp_interface->highs;

  if (objective_value)
  {
    *objective_value = Highs_getObjectiveValue(highs);
  }

  if (solution_values)
  {
    SLEQP_HIGHS_CALL(Highs_getSolution(highs,
                                       solution_values,
                                       lp_interface->cols_dual_dummysol,
                                       lp_interface->rows_primal_dummysol,
                                       lp_interface->rows_dual_dummysol),
                     highs);
  }

  return SLEQP_OKAY;
}

static SLEQP_RETCODE
highs_dual_sol(void* lp_data,
               int num_cols,
               int num_rows,
               double* vars_dual,
               double* cons_dual)
{
  SleqpLpiHIGHS* lp_interface = (SleqpLpiHIGHS*)lp_data;
  void* highs                 = lp_interface->highs;

  assert(lp_interface->num_cols == num_cols);

  if (!vars_dual)
  {
    vars_dual = lp_interface->cols_dual_dummysol;
  }

  if (!cons_dual)
  {
    cons_dual = lp_interface->rows_dual_dummysol;
  }

  SLEQP_HIGHS_CALL(Highs_getSolution(highs,
                                     lp_interface->cols_primal_dummysol,
                                     vars_dual,
                                     cons_dual,
                                     cons_dual),
                   highs);

  return SLEQP_OKAY;
}

static SLEQP_RETCODE
highs_vars_stats(void* lp_data,
                 int num_cols,
                 int num_rows,
                 SLEQP_BASESTAT* variable_stats)
{
  SleqpLpiHIGHS* lp_interface = (SleqpLpiHIGHS*)lp_data;
  void* highs                 = lp_interface->highs;

  SLEQP_HIGHS_CALL(
    Highs_getBasis(highs, lp_interface->col_basis, lp_interface->row_basis),
    highs);

  for (int j = 0; j < num_cols; ++j)
  {
    SLEQP_CALL(basestat_for(lp_interface->col_basis[j], variable_stats + j));
  }

  return SLEQP_OKAY;
}

static SLEQP_RETCODE
highs_cons_stats(void* lp_data,
                 int num_cols,
                 int num_rows,
                 SLEQP_BASESTAT* constraint_stats)
{
  SleqpLpiHIGHS* lp_interface = (SleqpLpiHIGHS*)lp_data;
  void* highs                 = lp_interface->highs;

  SLEQP_HIGHS_CALL(
    Highs_getBasis(highs, lp_interface->col_basis, lp_interface->row_basis),
    highs);

  for (int j = 0; j < num_rows; ++j)
  {
    SLEQP_CALL(basestat_for(lp_interface->row_basis[j], constraint_stats + j));
  }

  return SLEQP_OKAY;
}

static SLEQP_RETCODE
highs_basis_cond(void* lp_data, bool* exact, double* condition)
{
  *exact     = false;
  *condition = SLEQP_NONE;

  return SLEQP_OKAY;
}

static SLEQP_RETCODE
highs_free(void** star)
{
  SleqpLpiHIGHS* lp_interface = *star;
  void* highs                 = lp_interface->highs;

  if (!lp_interface)
  {
    return SLEQP_OKAY;
  }

  sleqp_free(&lp_interface->rows_dual_dummysol);
  sleqp_free(&lp_interface->cols_dual_dummysol);
  sleqp_free(&lp_interface->rows_primal_dummysol);
  sleqp_free(&lp_interface->cols_primal_dummysol);

  sleqp_free(&lp_interface->row_ub);
  sleqp_free(&lp_interface->row_lb);

  sleqp_free(&lp_interface->col_ub);
  sleqp_free(&lp_interface->col_lb);

  sleqp_free(&lp_interface->costs);

  sleqp_free(&lp_interface->row_basis);
  sleqp_free(&lp_interface->col_basis);

  for (int i = 0; i < lp_interface->num_bases; ++i)
  {
    sleqp_free(&lp_interface->row_bases[i]);
    sleqp_free(&lp_interface->col_bases[i]);
  }

  sleqp_free(&lp_interface->row_bases);
  sleqp_free(&lp_interface->col_bases);

  if (lp_interface->highs)
  {
    Highs_destroy(highs);
  }

  sleqp_free(star);

  return SLEQP_OKAY;
}

SLEQP_RETCODE
sleqp_lpi_highs_create(SleqpLPi** lp_star,
                       int num_cols,
                       int num_rows,
                       SleqpSettings* settings)
{
  SleqpLPiCallbacks callbacks = {.create_problem = highs_create_problem,
                                 .solve          = highs_solve,
                                 .status         = highs_status,
                                 .set_bounds     = highs_set_bounds,
                                 .set_coeffs     = highs_set_coeffs,
                                 .set_obj        = highs_set_objective,
                                 .set_basis      = highs_set_basis,
                                 .save_basis     = highs_save_basis,
                                 .restore_basis  = highs_restore_basis,
                                 .primal_sol     = highs_primal_sol,
                                 .dual_sol       = highs_dual_sol,
                                 .vars_stats     = highs_vars_stats,
                                 .cons_stats     = highs_cons_stats,
                                 .basis_cond     = highs_basis_cond,
                                 .write          = highs_write,
                                 .free_problem   = highs_free};

  return sleqp_lpi_create(lp_star,
                          SLEQP_LP_SOLVER_HIGHS_NAME,
                          SLEQP_LP_SOLVER_HIGHS_VERSION,
                          num_cols,
                          num_rows,
                          settings,
                          &callbacks);
}

SLEQP_RETCODE
sleqp_lpi_create_default(SleqpLPi** lp_interface,
                         int num_variables,
                         int num_constraints,
                         SleqpSettings* settings)
{
  SLEQP_CALL(sleqp_lpi_highs_create(lp_interface,
                                    num_variables,
                                    num_constraints,
                                    settings));

  return SLEQP_OKAY;
}
