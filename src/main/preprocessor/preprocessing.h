#ifndef SLEQP_PREPROCESSING_H
#define SLEQP_PREPROCESSING_H

#include "sparse/sparse_matrix.h"
#include "sparse/sparse_vec.h"

SLEQP_RETCODE
sleqp_preprocessing_merge_entries(const SleqpSparseVec* source,
                                  SleqpSparseVec* target,
                                  int num_entries,
                                  const int* entry_indices,
                                  double* entry_values);

SLEQP_RETCODE
sleqp_preprocessing_add_zero_entries(const SleqpSparseVec* source,
                                     SleqpSparseVec* target,
                                     int num_entries,
                                     const int* entry_indices);

#endif /* SLEQP_PREPROCESSING_H */
