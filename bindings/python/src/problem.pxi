#cython: language_level=3

cdef class Problem:
  cdef csleqp.SleqpProblem* problem

  cdef csleqp.SleqpSparseVec* var_lb
  cdef csleqp.SleqpSparseVec* var_ub

  cdef csleqp.SleqpSparseVec* cons_lb
  cdef csleqp.SleqpSparseVec* cons_ub

  cdef Func func
  cdef Params params

  def __cinit__(self,
                Func func,
                Params params,
                np.ndarray var_lb,
                np.ndarray var_ub,
                np.ndarray cons_lb,
                np.ndarray cons_ub):
    num_constraints = cons_lb.shape[0]
    num_variables = var_lb.shape[0]

    csleqp_call(csleqp.sleqp_sparse_vector_create(&self.var_lb,
                                                  num_variables,
                                                  0))

    csleqp_call(csleqp.sleqp_sparse_vector_create(&self.var_ub,
                                                  num_variables,
                                                  0))

    csleqp_call(csleqp.sleqp_sparse_vector_create(&self.cons_lb,
                                                  num_constraints,
                                                  0))

    csleqp_call(csleqp.sleqp_sparse_vector_create(&self.cons_ub,
                                                  num_constraints,
                                                  0))

    array_to_sleqp_sparse_vec(var_lb, self.var_lb)
    array_to_sleqp_sparse_vec(var_ub, self.var_ub)
    array_to_sleqp_sparse_vec(cons_lb, self.cons_lb)
    array_to_sleqp_sparse_vec(cons_ub, self.cons_ub)

    self.func = func
    self.params = params

    csleqp_call(csleqp.sleqp_problem_create(&self.problem,
                                            func.func,
                                            params.params,
                                            self.var_lb,
                                            self.var_ub,
                                            self.cons_lb,
                                            self.cons_ub))

  @property
  def num_variables(self) -> int:
    return self.problem.num_variables

  @property
  def num_constraints(self) -> int:
    return self.problem.num_constraints

  def __dealloc__(self):
    csleqp_call(csleqp.sleqp_problem_free(&self.problem))

    csleqp_call(csleqp.sleqp_sparse_vector_free(&self.cons_ub))
    csleqp_call(csleqp.sleqp_sparse_vector_free(&self.cons_lb))

    csleqp_call(csleqp.sleqp_sparse_vector_free(&self.var_ub))
    csleqp_call(csleqp.sleqp_sparse_vector_free(&self.var_lb))
