#ifndef SLEQP_AMPL_DATA_H
#define SLEQP_AMPL_DATA_H

#include <asl.h>

#include "sleqp.h"

typedef struct
{
  ASL* asl;
  bool is_constrained;
  int num_variables;
  int num_constraints;
  int num_linear;

  double* var_lb;
  double* var_ub;

  double* cons_lb;
  double* cons_ub;

  double* x;

} SleqpAmplData;

SLEQP_NODISCARD
SLEQP_RETCODE
map_ampl_inf(double* values, int num_values);

SLEQP_NODISCARD
SLEQP_RETCODE
sleqp_ampl_data_create(SleqpAmplData** star, ASL* asl);

SLEQP_NODISCARD
SLEQP_RETCODE
sleqp_ampl_data_free(SleqpAmplData** star);

#endif /* SLEQP_AMPL_DATA_H */