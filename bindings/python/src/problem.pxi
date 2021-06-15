#cython: language_level=3


cdef csleqp.SLEQP_RETCODE create_problem(csleqp.SleqpProblem** problem,
                                         csleqp.SleqpFunc* cfunc,
                                         csleqp.SleqpParams* cparams,
                                         np.ndarray var_lb,
                                         np.ndarray var_ub,
                                         np.ndarray general_lb,
                                         np.ndarray general_ub,
                                         object linear_coeffs = None,
                                         np.ndarray linear_lb = None,
                                         np.ndarray linear_ub = None):

  cdef csleqp.SleqpSparseVec* var_lb_vec
  cdef csleqp.SleqpSparseVec* var_ub_vec

  cdef csleqp.SleqpSparseVec* general_lb_vec
  cdef csleqp.SleqpSparseVec* general_ub_vec

  cdef csleqp.SleqpSparseVec* linear_lb_vec
  cdef csleqp.SleqpSparseVec* linear_ub_vec

  cdef csleqp.SleqpSparseMatrix* linear_coeffs_mat

  cdef int num_variables = var_lb.shape[0]
  cdef int num_constraints = general_lb.shape[0]

  assert cfunc != NULL

  cdef int num_linear_constraints = 0

  if linear_coeffs is not None:
    num_linear_constraints = linear_coeffs.shape[0]

  try:

    csleqp_call(csleqp.sleqp_sparse_vector_create_empty(&var_lb_vec,
                                                        num_variables))

    csleqp_call(csleqp.sleqp_sparse_vector_create_empty(&var_ub_vec,
                                                        num_variables))

    csleqp_call(csleqp.sleqp_sparse_vector_create_empty(&general_lb_vec,
                                                        num_constraints))

    csleqp_call(csleqp.sleqp_sparse_vector_create_empty(&general_ub_vec,
                                                        num_constraints))

    csleqp_call(csleqp.sleqp_sparse_matrix_create(&linear_coeffs_mat,
                                                  num_linear_constraints,
                                                  num_variables,
                                                  0))

    csleqp_call(csleqp.sleqp_sparse_vector_create_empty(&linear_lb_vec,
                                                        num_linear_constraints))

    csleqp_call(csleqp.sleqp_sparse_vector_create_empty(&linear_ub_vec,
                                                        num_linear_constraints))

    array_to_sleqp_sparse_vec(var_lb, var_lb_vec)
    array_to_sleqp_sparse_vec(var_ub, var_ub_vec)
    array_to_sleqp_sparse_vec(general_lb, general_lb_vec)
    array_to_sleqp_sparse_vec(general_ub, general_ub_vec)

    if linear_lb is not None:
      array_to_sleqp_sparse_vec(linear_lb, linear_lb_vec)

    if linear_ub is not None:
      array_to_sleqp_sparse_vec(linear_ub, linear_ub_vec)

    if linear_coeffs is not None:
      csleqp_call(matrix_to_sleqp_sparse_matrix(linear_coeffs,
                                                linear_coeffs_mat))

    csleqp_call(csleqp.sleqp_problem_create(problem,
                                            cfunc,
                                            cparams,
                                            var_lb_vec,
                                            var_ub_vec,
                                            general_lb_vec,
                                            general_ub_vec,
                                            linear_coeffs_mat,
                                            linear_lb_vec,
                                            linear_ub_vec))

    return csleqp.SLEQP_OKAY

  except SLEQPError as error:
    return error.code

  except BaseException as exception:
    return csleqp.SLEQP_INTERNAL_ERROR

  finally:
    csleqp_call(csleqp.sleqp_sparse_vector_free(&linear_ub_vec))
    csleqp_call(csleqp.sleqp_sparse_vector_free(&linear_lb_vec))

    csleqp_call(csleqp.sleqp_sparse_matrix_release(&linear_coeffs_mat))

    csleqp_call(csleqp.sleqp_sparse_vector_free(&general_ub_vec))
    csleqp_call(csleqp.sleqp_sparse_vector_free(&general_lb_vec))

    csleqp_call(csleqp.sleqp_sparse_vector_free(&var_ub_vec))
    csleqp_call(csleqp.sleqp_sparse_vector_free(&var_lb_vec))


cdef class _Problem:
  cdef csleqp.SleqpProblem* cproblem
  cdef _Func funcref

  def __cinit__(self):
    self.cproblem = NULL

  @staticmethod
  cdef _Problem create(csleqp.SleqpFunc* cfunc,
                       csleqp.SleqpParams* cparams,
                       np.ndarray var_lb,
                       np.ndarray var_ub,
                       np.ndarray cons_lb,
                       np.ndarray cons_ub,
                       object linear_coeffs = None,
                       np.ndarray linear_lb = None,
                       np.ndarray linear_ub = None):

    cdef _Problem _problem = _Problem()

    cdef int num_variables = var_lb.shape[0]
    cdef int num_constraints = cons_lb.shape[0]

    csleqp_call(create_problem(&_problem.cproblem,
                               cfunc,
                               cparams,
                               var_lb,
                               var_ub,
                               cons_lb,
                               cons_ub,
                               linear_coeffs,
                               linear_lb,
                               linear_ub))

    _problem.funcref = _Func()
    _problem.funcref.set_func(cfunc)

    return _problem

  @property
  def num_variables(self) -> int:
    return csleqp.sleqp_problem_num_variables(self.cproblem)

  @property
  def num_constraints(self) -> int:
    return csleqp.sleqp_problem_num_constraints(self.cproblem)

  @property
  def var_lb(self) -> np.array:
    return sleqp_sparse_vec_to_array(csleqp.sleqp_problem_var_lb(self.cproblem))

  @property
  def var_ub(self) -> np.array:
    return sleqp_sparse_vec_to_array(csleqp.sleqp_problem_var_ub(self.cproblem))

  @property
  def cons_lb(self) -> np.array:
    return sleqp_sparse_vec_to_array(csleqp.sleqp_problem_cons_lb(self.cproblem))

  @property
  def cons_ub(self) -> np.array:
    return sleqp_sparse_vec_to_array(csleqp.sleqp_problem_cons_ub(self.cproblem))

  @property
  def hess_struct(self) -> HessianStruct:
    return HessianStruct(self.funcref)

  def __dealloc__(self):
    csleqp_call(csleqp.sleqp_problem_release(&self.cproblem))


cdef class Problem:
  cdef dict __dict__

  cdef _Problem problem
  cdef _Func funcref

  cdef object _func

  def __cinit__(self,
                object func,
                Params params,
                np.ndarray var_lb,
                np.ndarray var_ub,
                np.ndarray cons_lb,
                np.ndarray cons_ub,
                **properties):

    cdef int num_variables = var_lb.shape[0]
    cdef int num_constraints = cons_lb.shape[0]
    cdef csleqp.SleqpFunc* cfunc

    self.funcref = _Func()

    csleqp_call(create_func(&cfunc,
                            func,
                            num_variables,
                            num_constraints))

    assert cfunc != NULL

    self._func = func

    try:
      self.problem = _Problem.create(cfunc,
                                     params.params,
                                     var_lb,
                                     var_ub,
                                     cons_lb,
                                     cons_ub,
                                     properties.get('linear_coeffs', None),
                                     properties.get('linear_lb', None),
                                     properties.get('linear_ub', None))

      self.funcref.set_func(cfunc)
      funcs.add(self.funcref)

    finally:
      csleqp_call(csleqp.sleqp_func_release(&cfunc))

    try:
      func.set_hessian_struct(self.hess_struct)
    except AttributeError:
      pass

  @property
  def func(self):
      return self._func

  @property
  def num_variables(self) -> int:
    return self.problem.num_variables

  @property
  def num_constraints(self) -> int:
    return self.problem.num_constraints

  @property
  def var_lb(self) -> np.array:
    return self.problem.var_lb

  @property
  def var_ub(self) -> np.array:
    return self.problem.var_ub

  @property
  def cons_lb(self) -> np.array:
    return self.problem.cons_lb

  @property
  def cons_ub(self) -> np.array:
    return self.problem.cons_ub

  @property
  def hess_struct(self) -> HessianStruct:
    return self.problem.hess_struct

  def _get_problem(self):
    return self.problem

cdef class LSQProblem:
  cdef dict __dict__

  cdef _Problem problem
  cdef _Func funcref

  cdef object _func

  def __cinit__(self,
                object func,
                Params params,
                np.ndarray var_lb,
                np.ndarray var_ub,
                np.ndarray cons_lb,
                np.ndarray cons_ub,
                num_residuals,
                **properties):

    cdef int num_variables = var_lb.shape[0]
    cdef int num_constraints = cons_lb.shape[0]
    cdef csleqp.SleqpFunc* cfunc

    self.funcref = _Func()

    csleqp_call(create_lsq_func(&cfunc,
                                func,
                                num_variables,
                                num_constraints,
                                num_residuals,
                                properties.get('regularization', 0.),
                                params.params))

    assert cfunc != NULL

    self._func = func

    try:
      self.problem = _Problem.create(cfunc,
                                     params.params,
                                     var_lb,
                                     var_ub,
                                     cons_lb,
                                     cons_ub)

      self.funcref.set_func(cfunc)
      lsq_funcs.add(self.funcref)

    finally:
      csleqp_call(csleqp.sleqp_func_release(&cfunc))

    try:
      func.set_hessian_struct(self.hess_struct)
    except AttributeError:
      pass

  @property
  def func(self):
      return self._func

  @property
  def num_variables(self) -> int:
    return self.problem.num_variables

  @property
  def num_constraints(self) -> int:
    return self.problem.num_constraints

  @property
  def var_lb(self) -> np.array:
    return self.problem.var_lb

  @property
  def var_ub(self) -> np.array:
    return self.problem.var_ub

  @property
  def cons_lb(self) -> np.array:
    return self.problem.cons_lb

  @property
  def cons_ub(self) -> np.array:
    return self.problem.cons_ub

  @property
  def hess_struct(self) -> HessianStruct:
    return self.problem.hess_struct

  def _get_problem(self):
    return self.problem
