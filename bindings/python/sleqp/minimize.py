import numpy as np

import sleqp

_base_pert = np.sqrt(np.finfo(float).eps)

class _MinFunc:

  def __init__(self, fun, grad, cons_fun, cons_jac, hessp, args, num_variables):
    self.x = np.zeros((num_variables,))
    self.fun = fun
    self._cons_fun = cons_fun
    self._cons_jac = cons_jac
    self.grad = grad
    self.hessp = hessp
    self.args = args
    self.num_variables = num_variables

    self._cons_vals = None
    self._func_val = None

  def set_value(self, v, reason):
    if (self.x == v).all():
      return

    self.x[:] = v
    self._cons_vals = None
    self._func_val = None

  def cons_vals(self):
    if self._cons_vals is None:
      self._cons_vals = self._cons_fun(self.x, *self.args)

    return self._cons_vals

  def cons_jac(self):
    self._cons_jac.f0 = self.cons_vals()
    jac = self._cons_jac(self.x, *self.args)
    return jac

  def func_val(self):
    if self._func_val is None:
      self._func_val = self.fun(self.x, *self.args)

    return self._func_val

  def func_grad(self):
    if self.grad is not None:
      return self.grad(self.x, *self.args)

    grad = np.empty_like(self.x)

    xd = np.copy(self.x)

    orig_func_val = self.func_val()

    for i in range(self.num_variables):

      pert = _base_pert * max(abs(self.x[i]), 1.)
      xd[i] += pert

      pert_func_val = self.fun(xd, *self.args)

      grad[i] = (pert_func_val - orig_func_val) / pert

      xd[i] = self.x[i]

    return grad

  def hess_prod(self, func_dual, direction, _):
    prod = self.hessp(self.x,
                      direction,
                      *self.args)

    return func_dual * prod


class OptimizeResult:
  def __init__(self, solver):
    self.x = solver.solution.primal
    self.status = solver.status
    self.success = (self.status == sleqp.Status.Optimal)
    self.nit = solver.iterations

  def __getitem__(self, key):
    return getattr(self, key)

def _create_variable_bounds(num_variables, bounds):
  inf = sleqp.inf()

  var_lb = np.full((num_variables,), -inf)
  var_ub = np.full((num_variables,), inf)

  if bounds is None:
    return (var_lb, var_ub)

  try:
    var_lb = np.array(bounds.lb)
    var_ub = np.array(bounds.ub)
    return (var_lb, var_ub)
  except AttributeError:
    pass

  for i, (lb, ub) in enumerate(bounds):
    if lb is not None:
      var_lb[i] = lb
    if ub is not None:
      var_ub[i] = ub

  return (var_lb, var_ub)


def _create_constraint_bounds(constraints):
  num_constraints = len(constraints)

  inf = sleqp.inf()
  cons_lb = np.full((num_constraints,), -inf)
  cons_ub = np.full((num_constraints,), inf)

  for i, constraint in enumerate(constraints):
    if constraint['type'] == 'ineq':
      cons_lb[i] = 0.
      cons_ub[i] = inf
    else:
      assert constraint['type'] == 'eq'
      cons_lb[i] = 0.
      cons_ub[i] = 0.

  return (cons_lb, cons_ub)


def _create_constraint_eval(constraints):
  num_constraints = len(constraints)

  cons_evals = []

  for constraint in constraints:
    cons_evals.append(constraint['fun'])

  def cons_eval(x, *args):
    return np.array([cons_eval(x, *args) for cons_eval in cons_evals])

  return cons_eval


def _create_constraint_jac(constraints, num_variables):

  num_constraints = len(constraints)

  from sleqp._derivative import (findiff_twopoint,
                                 findiff_threepoint,
                                 findiff_cs,
                                 derivative)

  cons_jacs = []

  for constraint in constraints:
    fun = constraint['fun']
    jac = constraint.get('jac')
    if jac is not None:

      if callable(jac):
        cons_jacs.append(derivative(jac))
      elif jac == '2-point':
        cons_jacs.append(findiff_twopoint(fun))
      elif jac == '3-point':
        cons_jacs.append(findiff_threepoint(fun))
      elif jac == 'cs':
        cons_jacs.append(findiff_cs(fun))
      else:
        assert False
    else:
      cons_jacs.append(findiff_twopoint(fun))


  class ConstraintJacobian:

    def __init__(self, cons_jacs, args=()):
      self.cons_jacs = cons_jacs

      assert len(cons_jacs) == num_constraints

      self.f0 = None

    def __call__(self, x0, args=()):
      jac = np.empty((num_constraints, num_variables))

      for (i, cons_jac) in enumerate(self.cons_jacs):
        jac[i,:] = cons_jac(x0, args, self.f0[i])

      return jac

  return ConstraintJacobian(cons_jacs)


def _add_solver_callback(solver, callback):

  def accepted_iterate(solver, iterate):
    abort = callback(iterate.primal)

    if abort is True:
      solver.abort()

  solver.add_callback(sleqp.SolverEvent.AcceptedIterate,
                      accepted_iterate)


def minimize(fun, x0, args=(), jac=None, hessp=None, bounds=None, constraints=None, callback=None):
  """
  A drop-in replacement for :func:`scipy.optimize.minimize`, minimizing a scalar function
  of one or more variables subjecting to constraints.

  :param fun:
      The objective function to be minimized.
      ``fun(x, *args) -> float``
      where ``x`` is an 1-D array with shape (n,) and ``args``
      is a tuple of the fixed parameters needed to completely
      specify the function.
  :type fun: callable
  :param x0:
        Initial guess. Array of real elements of size (n,),
        where 'n' is the number of independent variables.
  :type x0: :class:`numpy.ndarray`, shape (n,)
  :param args:
        Extra arguments passed to the objective function and its
        derivatives (`fun`, `jac` and `hess` functions).
  :type args: tuple, optional
  :param bounds:
        Bounds on variables. There are two ways to specify the bounds:
        1. Instance of :class:`scipy.optimize.Bounds` class.
        2. Sequence of ``(min, max)`` pairs for each element in `x`. ``None`` is used to specify no bound.
  :type bounds: sequence or :class:`scipy.optimize.Bounds`, optional
  :param callback:
        Called after each iteration. If callback returns True
        the algorithm execution is terminated. The signature is: ``callback(xk)``
        where ``xk`` is the current guess.
  :type callback: callable, optional
  :return: An improved guess
  :rtype: :class:`numpy.ndarray`, shape (n,)
  """
  if not isinstance(args, tuple):
    args = (args,)

  num_variables = len(x0)
  num_constraints = 0

  cons_lb = np.zeros((0,))
  cons_ub = np.zeros((0,))
  cons_eval = None
  cons_jac = None

  if constraints is not None:
    num_constraints = len(constraints)

    (cons_lb, cons_ub) = _create_constraint_bounds(constraints)
    cons_eval = _create_constraint_eval(constraints)
    cons_jac = _create_constraint_jac(constraints, num_variables)

  inf = sleqp.inf()

  initial_sol = np.array(x0)

  (var_lb, var_ub) = _create_variable_bounds(num_variables,
                                             bounds)

  min_func = _MinFunc(fun, jac, cons_eval, cons_jac, hessp, args, num_variables)

  problem = sleqp.Problem(min_func,
                          var_lb,
                          var_ub,
                          cons_lb,
                          cons_ub)

  params = sleqp.Params()

  options = sleqp.Options(deriv_check=sleqp.DerivCheck.First)

  if hessp is None:
    options.hessian_eval = sleqp.HessianEval.SR1

  solver = sleqp.Solver(problem,
                        params,
                        options,
                        initial_sol)

  if callback is not None:
    _add_solver_callback(solver, callback)

  solver.solve(100, 3600)

  return OptimizeResult(solver)