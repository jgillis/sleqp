#ifndef SLEQP_CUTEST_TYPES_H
#define SLEQP_CUTEST_TYPES_H

#include <cutest.h>

extern const logical cutest_true;
extern const logical cutest_false;

extern integer cutest_io_buffer;
extern integer cutest_iout;

#define SLEQP_CUTEST_CHECK_STATUS(status)                                      \
  do                                                                           \
  {                                                                            \
    if (status)                                                                \
    {                                                                          \
      sleqp_log_error("Error in CUTest call");                                 \
      return SLEQP_INTERNAL_ERROR;                                             \
    }                                                                          \
  } while (0)

#endif /* SLEQP_CUTEST_TYPES_H */
