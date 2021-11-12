#ifndef SLEQP_TYPES_H
#define SLEQP_TYPES_H

#include "pub_types.h"

typedef enum {
        SLEQP_SOLVER_PHASE_OPTIMIZATION = 0,
        SLEQP_SOLVER_PHASE_RESTORATION,
        SLEQP_SOLVER_NUM_PHASES
} SLEQP_SOLVER_PHASE;

#endif /* SLEQP_TYPES_H */
