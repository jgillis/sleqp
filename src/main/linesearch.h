#ifndef SLEQP_LINESEARCH_H
#define SLEQP_LINESEARCH_H

/**
 * @file linesearch.h
 * @brief Definition of linesearch functions.
 **/

#include "merit.h"
#include "timer.h"

typedef struct SleqpLineSearch SleqpLineSearch;

/**
 * Creates a new linesearch object.
 *
 * @param[in] problem     The underlying problem
 * @param[in] params      The problem parameters
 * @param[in] merit_data  A merit-function
 *
 **/
SLEQP_NODISCARD
SLEQP_RETCODE
sleqp_linesearch_create(SleqpLineSearch** star,
                        SleqpProblem* problem,
                        SleqpParams* params,
                        SleqpMerit* merit);

/**
 * Sets the iterate to be used for subsequent line searches as well
 * as suitable parameters.
 *
 * @param[in]      iterate               The current iterate
 * @param[in]      penalty_parameter     The penalty parameter \f$ v \f$
 * @param[in]      trust_radius          The current trust radius
 *
 * @note The trust radius is supposed to be the EQP trust radius, note the LP
 *trust radius
 *
 **/
SLEQP_NODISCARD
SLEQP_RETCODE
sleqp_linesearch_set_iterate(SleqpLineSearch* linesearch,
                             SleqpIterate* iterate,
                             double penalty_parameter,
                             double trust_radius);

/**
 * Computes the Cauchy step by (approximately) minimizing the
 * quadratic penalty along a direction. The direction will
 * be scaled and then returned.
 *
 * @param[in]      linesearch            Linesearch data
 * @param[in,out]  direction             The direction
 * @param[in,out]  hessian_direction     The product of the Hessian with the
 *initial direction
 * @param[out]     step_length           The computed step length
 * @param[out]     quadratic_merit_value The quadratic trial value for the
 *Cauchy step
 *
 **/
SLEQP_NODISCARD
SLEQP_RETCODE
sleqp_linesearch_cauchy_step(SleqpLineSearch* linesearch,
                             SleqpSparseVec* direction,
                             const SleqpSparseVec* multipliers,
                             SleqpSparseVec* hessian_direction,
                             bool* full_step,
                             double* quadratic_merit_value);

/**
 * Computes the trial direction by (approximately) minimizing the
 * quadratic penalty along the Cauchy-Newton direction using an Armijo-like
 *method.
 *
 * @param[in]      linesearch                   Linesearch data
 * @param[in]      cauchy_step                  The Cauchy step
 * @param[in]      cauchy_hessian_step          The product of the Hessian with
 *the Cauchy step
 * @param[in]      cauchy_quadratic_merit_value The quadratic merit value of the
 *Cauchy step
 * @param[in]      newton_step                  The Newton step
 * @param[in]      newton_hessian_direction     The product of the Hessian with
 *the Newton direction
 * @param[in]      multipliers                  The Hessian multipliers
 * @param[out]     trial_step                   The trial step
 * @param[out]     full_step                    Whether the full LP step was
 *accepted
 * @param[out]     quadratic_merit_value        The quadratic trial value for
 *the trial step
 *
 **/
SLEQP_NODISCARD
SLEQP_RETCODE
sleqp_linesearch_trial_step(SleqpLineSearch* linesearch,
                            const SleqpSparseVec* cauchy_step,
                            const SleqpSparseVec* cauchy_hessian_step,
                            const double cauchy_quadratic_merit_value,
                            const SleqpSparseVec* newton_step,
                            const SleqpSparseVec* newton_hessian_step,
                            const SleqpSparseVec* multipliers,
                            SleqpSparseVec* trial_step,
                            double* step_length,
                            double* trial_quadratic_merit_value);

SLEQP_NODISCARD
SLEQP_RETCODE
sleqp_linesearch_trial_step_exact(SleqpLineSearch* linesearch,
                                  const SleqpSparseVec* cauchy_step,
                                  const SleqpSparseVec* cauchy_hessian_step,
                                  const double cauchy_quadratic_merit_value,
                                  const SleqpSparseVec* newton_step,
                                  const SleqpSparseVec* newton_hessian_step,
                                  const SleqpSparseVec* multipliers,
                                  SleqpSparseVec* trial_step,
                                  double* step_length,
                                  double* trial_quadratic_merit_value);

SleqpTimer*
sleqp_linesearch_get_timer(SleqpLineSearch* linesearch);

SLEQP_NODISCARD
SLEQP_RETCODE
sleqp_linesearch_capture(SleqpLineSearch* solver);

SLEQP_NODISCARD
SLEQP_RETCODE
sleqp_linesearch_release(SleqpLineSearch** star);

#endif /* SLEQP_LINESEARCH_H */
