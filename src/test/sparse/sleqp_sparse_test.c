#include <stdlib.h>
#include <check.h>

#include "test_common.h"

#include "sparse/sleqp_sparse.h"

START_TEST(test_sparse_reserve)
{
  SleqpSparseMatrix* matrix;

  size_t size = 5;

  ASSERT_CALL(sleqp_sparse_matrix_create(&matrix,
                                         size,
                                         size,
                                         0));

  ASSERT_CALL(sleqp_sparse_matrix_reserve(matrix, 10));

  ck_assert(matrix->nnz_max >= 10);
  ck_assert_int_eq(matrix->nnz, 0);

  ASSERT_CALL(sleqp_sparse_matrix_free(&matrix));
}
END_TEST

START_TEST(test_sparse_increase_size)
{
  SleqpSparseMatrix* matrix;

  size_t num_nnz = 5;
  size_t initial_size = 3, size = 10;

  ASSERT_CALL(sleqp_sparse_matrix_create(&matrix,
                                         initial_size,
                                         initial_size,
                                         0));

  ASSERT_CALL(sleqp_sparse_matrix_reserve(matrix, num_nnz));

  ASSERT_CALL(sleqp_sparse_matrix_resize(matrix, size, size));

  ck_assert_int_eq(matrix->num_rows, size);
  ck_assert_int_eq(matrix->num_cols, size);

  for(int col = 0; col < size; ++col)
  {
    ck_assert_int_eq(matrix->cols[col], 0);
  }

  ck_assert_int_eq(matrix->nnz, 0);
  ck_assert_int_eq(matrix->nnz_max, num_nnz);

  ASSERT_CALL(sleqp_sparse_matrix_free(&matrix));

}
END_TEST

START_TEST(test_sparse_remove_column)
{
  SleqpSparseMatrix* matrix;

  size_t size = 5;

  ASSERT_CALL(sleqp_sparse_matrix_create(&matrix,
                                         size,
                                         size,
                                         size));

  for(size_t current = 0; current < size; ++current)
  {
    ASSERT_CALL(sleqp_sparse_matrix_add_column(matrix, current));

    ASSERT_CALL(sleqp_sparse_matrix_push(matrix,
                                         current,
                                         current,
                                         1.));
  }

  ck_assert_int_eq(matrix->nnz, size);

  int removed = 0;

  for(size_t column = size - 1; column >= 0; --column)
  {
    ASSERT_CALL(sleqp_sparse_matrix_remove_column(matrix, column));

    ck_assert_int_eq(matrix->cols[column + 1] - matrix->cols[column], 0);

    ck_assert_int_eq(matrix->nnz, size - removed);

    ++removed;
  }


  ASSERT_CALL(sleqp_sparse_matrix_free(&matrix));

}
END_TEST

START_TEST(test_sparse_construction)
{
  SleqpSparseMatrix* identity;

  size_t size = 5;

  ASSERT_CALL(sleqp_sparse_matrix_create(&identity,
                                         size,
                                         size,
                                         size));

  ck_assert_int_eq(identity->num_cols, size);
  ck_assert_int_eq(identity->num_rows, size);
  ck_assert_int_eq(identity->nnz_max, size);
  ck_assert_int_eq(identity->nnz, 0);

  for(size_t current = 0; current < size; ++current)
  {
    ASSERT_CALL(sleqp_sparse_matrix_add_column(identity, current));

    ASSERT_CALL(sleqp_sparse_matrix_push(identity,
                                         current,
                                         current,
                                         1.));

    ck_assert_int_eq(identity->nnz, current + 1);
  }

  for(size_t row = 0; row < identity->num_rows; ++row)
  {
    for(size_t col = 0; col < identity->num_cols; ++col)
    {
      double* value = sleqp_sparse_matrix_at(identity, row, col);

      if(row != col)
      {
        ck_assert(!value);
      }
      else
      {
        ck_assert(value);
        ck_assert(*value == 1.);
      }
    }
  }

  ASSERT_CALL(sleqp_sparse_matrix_free(&identity));
}
END_TEST

START_TEST(test_sparse_decrease_size)
{
  SleqpSparseMatrix* identity;

  size_t size = 5, reduced_size = 2;

  ASSERT_CALL(sleqp_sparse_matrix_create(&identity,
                                         size,
                                         size,
                                         size));

  for(size_t current = 0; current < size; ++current)
  {
    ASSERT_CALL(sleqp_sparse_matrix_add_column(identity, current));

    ASSERT_CALL(sleqp_sparse_matrix_push(identity,
                                         current,
                                         current,
                                         1.));
  }

  ASSERT_CALL(sleqp_sparse_matrix_resize(identity, 2, 2));

  for(int column = reduced_size; column < size; ++column)
  {
    ck_assert_int_eq(identity->cols[column + 1] - identity->cols[column], 0);
  }


  ASSERT_CALL(sleqp_sparse_matrix_free(&identity));
}
END_TEST

Suite* sparse_test_suite()
{
  Suite *suite;
  TCase *tc_sparse_construction;
  TCase *tc_sparse_modification;

  suite = suite_create("Sparse tests");

  tc_sparse_construction = tcase_create("Sparse matrix construction");

  tcase_add_test(tc_sparse_construction, test_sparse_construction);

  tc_sparse_modification = tcase_create("Sparse matrix modification");

  tcase_add_test(tc_sparse_modification, test_sparse_reserve);
  tcase_add_test(tc_sparse_modification, test_sparse_increase_size);
  tcase_add_test(tc_sparse_modification, test_sparse_decrease_size);
  tcase_add_test(tc_sparse_modification, test_sparse_remove_column);

  suite_add_tcase(suite, tc_sparse_construction);

  return suite;
}


int main()
{
  int num_fails;
  Suite* suite;
  SRunner* srunner;

  suite = sparse_test_suite();
  srunner = srunner_create(suite);

  srunner_run_all(srunner, CK_NORMAL);

  num_fails = srunner_ntests_failed(srunner);

  srunner_free(srunner);

  return (num_fails > 0) ? EXIT_FAILURE : EXIT_SUCCESS;
}
