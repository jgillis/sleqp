#ifndef SLEQP_MATH_ERROR_H
#define SLEQP_MATH_ERROR_H

#include <fenv.h>
#include <math.h>

#include "error.h"
#include "types.h"

#if defined(math_errhandling) && defined(MATH_ERREXCEPT) && !defined(SLEQP_MATH_ERREXCEPT)
#define SLEQP_MATH_ERREXCEPT math_errhandling & MATH_ERREXCEPT
#else
#define SLEQP_MATH_ERREXCEPT 1
#endif

#define SLEQP_INIT_MATH_CHECK                                                  \
  fenv_t fenv_current;                                                         \
  do                                                                           \
  {                                                                            \
    if (SLEQP_MATH_ERREXCEPT)                                     \
    {                                                                          \
      fegetenv(&fenv_current);                                                 \
      fesetenv(FE_DFL_ENV);                                                    \
    }                                                                          \
  } while (false)

#define SLEQP_MATH_CHECK_ERRORS(error_flags)                                   \
  do                                                                           \
  {                                                                            \
    if (SLEQP_MATH_ERREXCEPT)                                     \
    {                                                                          \
      bool has_errors = fetestexcept(error_flags);                             \
                                                                               \
      if (has_errors)                                                          \
      {                                                                        \
        sleqp_raise(SLEQP_MATH_ERROR,                                          \
                    "Encountered floating point errors (%s, %s, %s, %s, %s)",  \
                    fetestexcept(FE_DIVBYZERO) ? "FE_DIVBYZERO" : "",          \
                    fetestexcept(FE_INEXACT) ? "FE_INEXACT" : "",              \
                    fetestexcept(FE_INVALID) ? "FE_INVALID" : "",              \
                    fetestexcept(FE_OVERFLOW) ? "FE_OVERFLOW" : "",            \
                    fetestexcept(FE_UNDERFLOW) ? "FE_UNDERFLOW" : "");         \
      }                                                                        \
    }                                                                          \
  } while (false)

#define SLEQP_MATH_CHECK_WARNINGS(warn_flags)                                  \
  do                                                                           \
  {                                                                            \
    if (SLEQP_MATH_ERREXCEPT)                                     \
    {                                                                          \
      const bool has_warning = fetestexcept(warn_flags);                       \
                                                                               \
      if (has_warning)                                                         \
      {                                                                        \
        sleqp_log_warn(                                                        \
          "Encountered floating point errors (%s, %s, %s, %s, %s)",            \
          fetestexcept(FE_DIVBYZERO) ? "FE_DIVBYZERO" : "",                    \
          fetestexcept(FE_INEXACT) ? "FE_INEXACT" : "",                        \
          fetestexcept(FE_INVALID) ? "FE_INVALID" : "",                        \
          fetestexcept(FE_OVERFLOW) ? "FE_OVERFLOW" : "",                      \
          fetestexcept(FE_UNDERFLOW) ? "FE_UNDERFLOW" : "");                   \
      }                                                                        \
    }                                                                          \
  } while (false)

#define SLEQP_MATH_CHECK(error_flags, warn_flags)                              \
  do                                                                           \
  {                                                                            \
    if (SLEQP_MATH_ERREXCEPT)                                     \
    {                                                                          \
      SLEQP_MATH_CHECK_WARNINGS(warn_flags);                                   \
      SLEQP_MATH_CHECK_ERRORS(error_flags);                                    \
    }                                                                          \
  } while (false)

#endif /* SLEQP_MATH_ERROR_H */
