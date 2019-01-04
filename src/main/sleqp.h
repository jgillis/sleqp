#ifndef SLEQP_H
#define SLEQP_H

#include "sleqp_active_set.h"
#include "sleqp_aug_jacobian.h"
#include "sleqp_cauchy.h"
#include "sleqp_cmp.h"
#include "sleqp_defs.h"
#include "sleqp_deriv_check.h"
#include "sleqp_dual_estimation.h"
#include "sleqp_func.h"
#include "sleqp_iterate.h"
#include "sleqp_log.h"
#include "sleqp_mem.h"
#include "sleqp_merit.h"
#include "sleqp_newton.h"
#include "sleqp_params.h"
#include "sleqp_problem.h"
#include "sleqp_soc.h"
#include "sleqp_solver.h"
#include "sleqp_types.h"
#include "sleqp_util.h"

#include "lp/sleqp_lpi.h"
#include "lp/sleqp_lpi_soplex.h"
#include "lp/sleqp_lpi_types.h"

#include "sparse/sleqp_sparse_factorization.h"
#include "sparse/sleqp_sparse_matrix.h"
#include "sparse/sleqp_sparse_vec.h"

#endif /* SLEQP_H */
