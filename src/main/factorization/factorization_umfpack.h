#ifndef SLEQP_FACTORIZATION_UMFPACK_H
#define SLEQP_FACTORIZATION_UMFPACK_H

/**
 * @file factorization_umfpack.h
 * @brief Defintion of UMFPACK sparse factorization method.
 **/

#include "factorization.h"
#include "types.h"

#include "sparse/sparse_matrix.h"

SLEQP_NODISCARD
SLEQP_RETCODE
sleqp_factorization_umfpack_create(SleqpFactorization** star,
                                   SleqpParams* params);

#endif /* SLEQP_FACTORIZATION_UMFPACK_H */
