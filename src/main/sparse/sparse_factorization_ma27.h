#ifndef SLEQP_SPARSE_FACTORIZATION_MA27_H
#define SLEQP_SPARSE_FACTORIZATION_MA27_H

#include "types.h"
#include "sparse_matrix.h"
#include "sparse_factorization.h"

SLEQP_NODISCARD
SLEQP_RETCODE sleqp_sparse_factorization_ma27_create(SleqpSparseFactorization** star,
                                                     SleqpParams* params);

#endif /* SLEQP_SPARSE_FACTORIZATION_MA27_H */
