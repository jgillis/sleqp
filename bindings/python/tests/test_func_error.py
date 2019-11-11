#!/usr/bin/env python

import numpy as np
import unittest

import sleqp

num_variables = 2
num_constraints = 1

class ErrorFunc(sleqp.Func):
  def set_value(self, v):
    raise Exception("Error in set_value")

class TypeErrorFunc(sleqp.Func):
  def set_value(self, v):
    pass

  def func_val(self):
    return 0

  def func_grad(self):
    return "wrong"

  def hess_prod(self, func_dual, direction, cons_dual):
    return "wrong"

class MatrixErrorFunc(sleqp.Func):
  def set_value(self, v):
    pass

  def set_matrix_value(self, m):
    self.m = m

  def func_val(self):
    return 0

  def func_grad_nnz(self):
    return 1

  def cons_jac_nnz(self):
    return 1

  def func_grad(self):
    return np.array([0])

  def hess_prod(self, func_dual, direction, cons_dual):
    return np.array([0])

  def cons_jac(self):
    return self.m



class FuncErrorTest(unittest.TestCase):

  def setUp(self):
    inf = sleqp.inf()

    self.var_lb = np.array([-inf, -inf])
    self.var_ub = np.array([inf, inf])

    self.cons_lb = np.array([])
    self.cons_ub = np.array([])

    self.x = np.array([0., 0.])

    self.params = sleqp.Params()
    self.options = sleqp.Options()

  def test_error_func(self):
    self.func = ErrorFunc(num_variables, num_constraints)

    self.problem = sleqp.Problem(self.func,
                                 self.params,
                                 self.var_lb,
                                 self.var_ub,
                                 self.cons_lb,
                                 self.cons_ub)

    self.solver = sleqp.Solver(self.problem,
                               self.params,
                               self.options,
                               self.x)

    with self.assertRaises(sleqp.SLEQPError) as context:
      self.solver.solve(100, 3600)

  def test_type_error_func(self):
    func = TypeErrorFunc(num_variables, num_constraints)

    problem = sleqp.Problem(func,
                            self.params,
                            self.var_lb,
                            self.var_ub,
                            self.cons_lb,
                            self.cons_ub)

    solver = sleqp.Solver(problem,
                          self.params,
                          self.options,
                          self.x)

    with self.assertRaises(sleqp.SLEQPError) as context:
      solver.solve(100, 3600)

  def test_matrix_error_func(self):
    func = MatrixErrorFunc(num_variables, num_constraints)

    problem = sleqp.Problem(func,
                            self.params,
                            self.var_lb,
                            self.var_ub,
                            self.cons_lb,
                            self.cons_ub)

    solver = sleqp.Solver(problem,
                          self.params,
                          self.options,
                          self.x)

    with self.assertRaises(sleqp.SLEQPError) as context:
      solver.solve(100, 3600)