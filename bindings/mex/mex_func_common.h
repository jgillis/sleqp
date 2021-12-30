#ifndef SLEQP_MEX_FUNC_COMMON_H
#define SLEQP_MEX_FUNC_COMMON_H

#include <assert.h>
#include <mex.h>
#include <threads.h>

#include "sleqp.h"

#define MEX_MSG_BUF_SIZE 512

#define MATLAB_CALL_SIMPLE(x)                                                  \
  do                                                                           \
  {                                                                            \
    mxArray* exception = (x);                                                  \
                                                                               \
    if (exception)                                                             \
    {                                                                          \
      sleqp_log_error("Exception in Matlab call");                             \
      return SLEQP_INTERNAL_ERROR;                                             \
    }                                                                          \
                                                                               \
  } while (0)

#define MATLAB_CALL(x)                                                         \
  do                                                                           \
  {                                                                            \
    mxArray* exception = (x);                                                  \
                                                                               \
    if (exception)                                                             \
    {                                                                          \
      char msg_buf[MEX_MSG_BUF_SIZE];                                          \
      mxArray* lhs;                                                            \
      MATLAB_CALL_SIMPLE(                                                      \
        mexCallMATLABWithTrap(1, &lhs, 1, &exception, MATLAB_FUNC_DISP));      \
      assert(mxIsChar(lhs));                                                   \
      mxGetString(lhs, msg_buf, MEX_MSG_BUF_SIZE);                             \
                                                                               \
      sleqp_log_error("Exception '%s' in Matlab call", msg_buf);               \
      return SLEQP_INTERNAL_ERROR;                                             \
    }                                                                          \
  } while (0)

SLEQP_RETCODE
mex_callback_from_struct(const mxArray* mex_callbacks,
                         const char* name,
                         mxArray** star);

SLEQP_RETCODE
mex_eval_into_sparse_vec(int nrhs,
                         mxArray** rhs,
                         SleqpParams* params,
                         SleqpSparseVec* vec);

SLEQP_RETCODE
mex_eval_into_sparse_matrix(int nrhs,
                            mxArray** rhs,
                            SleqpParams* params,
                            SleqpSparseMatrix* matrix);

#endif /* SLEQP_MEX_FUNC_COMMON_H */
