#include "sleqp_sparse.h"

#include <assert.h>

#include "sleqp_cmp.h"
#include "sleqp_mem.h"

SLEQP_RETCODE sleqp_sparse_vector_create(SleqpSparseVec** vstar,
                                         size_t dim,
                                         size_t nnz_max)
{
  assert(nnz_max <= dim);

  SLEQP_CALL(sleqp_malloc(vstar));

  SleqpSparseVec *vec = *vstar;

  vec->nnz = 0;
  vec->dim = dim;
  vec->nnz_max = nnz_max;

  SLEQP_CALL(sleqp_calloc(&vec->data, nnz_max));
  SLEQP_CALL(sleqp_calloc(&vec->indices, nnz_max));

  return SLEQP_OKAY;
}

SLEQP_RETCODE sleqp_sparse_vector_push(SleqpSparseVec* vec,
                                       size_t idx,
                                       double value)
{
  assert(idx < vec->nnz_max);

  if(vec->nnz > 0)
  {
    assert(idx > vec->indices[vec->nnz - 1]);
  }

  vec->data[vec->nnz] = value;
  vec->indices[vec->nnz] = idx;

  ++(vec->nnz);

  return SLEQP_OKAY;
}

SLEQP_RETCODE sleqp_sparse_vector_reserve(SleqpSparseVec* vec,
                                          size_t nnz)
{
  if(vec->nnz_max >= nnz)
  {
    return SLEQP_OKAY;
  }

  SLEQP_CALL(sleqp_realloc(&vec->data, nnz));
  SLEQP_CALL(sleqp_realloc(&vec->indices, nnz));

  vec->nnz_max = nnz;

  return SLEQP_OKAY;
}

SLEQP_RETCODE sleqp_sparse_vector_clip(SleqpSparseVec* x,
                                       SleqpSparseVec* lb,
                                       SleqpSparseVec* ub,
                                       SleqpSparseVec** xstar)
{
  const size_t dim = x->dim;

  assert(lb->dim == dim);
  assert(ub->dim == dim);

  sleqp_sparse_vector_create(xstar,
                             dim,
                             SLEQP_MIN(x->nnz + lb->nnz, dim));

  size_t k_x = 0, k_lb = 0, k_ub = 0;

  SleqpSparseVec* xclip = *xstar;

  while(k_x < x->nnz || k_lb < lb->nnz || k_ub < ub->nnz)
  {
    double x_val = 0;

    SLEQP_Bool valid_x = (k_x < x->nnz);
    SLEQP_Bool valid_lb = (k_lb < lb->nnz);
    SLEQP_Bool valid_ub = (k_ub < ub->nnz);

    size_t idx = valid_x ? x->indices[k_x] : dim + 1;
    idx = SLEQP_MIN(idx, valid_lb ? lb->indices[k_lb] : dim + 1);
    idx = SLEQP_MIN(idx, valid_ub ? ub->indices[k_ub] : dim + 1);

    if(valid_x && idx == x->indices[k_x])
    {
      x_val = x->data[k_x];
    }
    else
    {
      x_val = 0.;
    }

    if(valid_lb && idx == lb->indices[k_lb])
    {
      x_val = SLEQP_MAX(x_val, lb->data[k_lb]);
    }

    if(valid_ub && idx == ub->indices[k_ub])
    {
      x_val = SLEQP_MIN(x_val, ub->data[k_ub]);
    }

    if(!sleqp_zero(x_val))
    {
      SLEQP_CALL(sleqp_sparse_vector_push(xclip,
                                          idx,
                                          x_val));
    }

    if(valid_lb && idx == lb->indices[k_lb])
    {
      ++k_lb;
    }

    if(valid_ub && idx == ub->indices[k_ub])
    {
      ++k_ub;
    }

    if(valid_x && idx == x->indices[k_x])
    {
      ++k_x;
    }

  }

  return SLEQP_OKAY;
}

SLEQP_RETCODE sleqp_sparse_vector_free(SleqpSparseVec** vstar)
{
  SleqpSparseVec *vec = *vstar;

  sleqp_free(&vec->indices);
  sleqp_free(&vec->data);

  sleqp_free(&vec);

  *vstar = NULL;

  return SLEQP_OKAY;
}

SLEQP_RETCODE sleqp_sparse_matrix_create(SleqpSparseMatrix** mstar,
                                         size_t num_rows,
                                         size_t num_cols,
                                         size_t nnz_max)
{
  SLEQP_CALL(sleqp_malloc(mstar));

  SleqpSparseMatrix* matrix = *mstar;

  matrix->nnz = 0;
  matrix->nnz_max = nnz_max;

  matrix->num_cols = num_cols;
  matrix->num_rows = num_rows;

  SLEQP_CALL(sleqp_calloc(&matrix->data, nnz_max));
  SLEQP_CALL(sleqp_calloc(&matrix->cols, num_cols + 1));
  SLEQP_CALL(sleqp_calloc(&matrix->rows, nnz_max));

  return SLEQP_OKAY;
}

SLEQP_RETCODE sleqp_sparse_matrix_reserve(SleqpSparseMatrix* matrix,
                                          size_t nnz)
{
  if(matrix->nnz_max >= nnz)
  {
    return SLEQP_OKAY;
  }

  SLEQP_CALL(sleqp_realloc(&matrix->data, nnz));
  SLEQP_CALL(sleqp_realloc(&matrix->rows, nnz));

  matrix->nnz_max = nnz;

  return SLEQP_OKAY;
}

SLEQP_RETCODE sleqp_sparse_matrix_fprintf(SleqpSparseMatrix* matrix,
                                          FILE* output)
{
  fprintf(output,
          "Sparse matrix, dimension: %ld x %ld, entries: %ld\n",
          matrix->num_rows,
          matrix->num_cols,
          matrix->nnz);

  size_t col = 0;

  for(size_t k = 0; k < matrix->nnz; ++k)
  {
    while(col < matrix->cols[k])
    {
      ++col;
    }

    fprintf(output, "(%ld, %ld) = %f\n",
            matrix->rows[k],
            col,
            matrix->data[k]);
  }

  return SLEQP_OKAY;
}

SLEQP_RETCODE sleqp_sparse_matrix_free(SleqpSparseMatrix** mstar)
{
  SleqpSparseMatrix* matrix = *mstar;

  sleqp_free(&matrix->rows);
  sleqp_free(&matrix->cols);
  sleqp_free(&matrix->data);

  sleqp_free(&matrix);

  *mstar = NULL;

  return SLEQP_OKAY;
}
