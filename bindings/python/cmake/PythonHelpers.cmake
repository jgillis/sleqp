set(ENABLE_UNIT_TESTS On)

if(NOT TOX)
  message(WARNING "Could not find tox executable, python unit tests will be disabled")
  set(ENABLE_UNIT_TESTS Off)
endif()

function(add_python_project)

  cmake_parse_arguments(
    PARSE_ARGV 0 ARGS
    ""
    "PROJECT_NAME;PROJECT_COMPONENT"
    "CONFIG_FILES"
    )

  set(PROJECT_NAME "${ARGS_PROJECT_NAME}")

  set(TARGET_NAME "${PROJECT_NAME}_python")

  set(PROJECT_DIR "${CMAKE_CURRENT_SOURCE_DIR}")

  set(DOC_DIR "${PROJECT_DIR}/docs")

  set(DOC_TARGET "${TARGET_NAME}_doc")

  set(PYTHON_CFLAGS "")

  foreach(CONFIG_FILE ${ARGS_CONFIG_FILES})
    get_filename_component(CONFIG_FILE ${CONFIG_FILE} ABSOLUTE)
    get_filename_component(RESULT_FILE ${CONFIG_FILE} NAME_WLE)
    get_filename_component(RESULT_DIR ${CONFIG_FILE} DIRECTORY)
    configure_file(${CONFIG_FILE}
      "${RESULT_DIR}/${RESULT_FILE}"
      @ONLY)
  endforeach()

  if(CMAKE_C_FLAGS)
    set(PYTHON_CFLAGS "${CMAKE_C_FLAGS}")
  elseif(CMAKE_BUILD_TYPE)
    set(PYTHON_CFLAGS "${CMAKE_C_FLAGS_${CMAKE_BUILD_TYPE}}")
  endif()

  add_custom_target(${TARGET_NAME} ALL)

  add_custom_target(${DOC_TARGET})

  find_program(MAKE make)

  if(MAKE)
    add_custom_command(
      TARGET ${DOC_TARGET}
      COMMAND ${MAKE} html
      WORKING_DIRECTORY ${DOC_DIR})

  add_dependencies(${DOC_TARGET} ${TARGET_NAME})

  endif()

  add_custom_target("${TARGET_NAME}_sdist")

  add_custom_command(
    TARGET "${TARGET_NAME}_sdist"
    COMMAND ${CMAKE_COMMAND} -E env CFLAGS=${PYTHON_CFLAGS} ${PYTHON_EXECUTABLE} -m build -s
    WORKING_DIRECTORY ${PROJECT_DIR})

  add_custom_target("${TARGET_NAME}_bdist_wheel")

  add_custom_command(
    TARGET "${TARGET_NAME}_bdist_wheel"
    COMMAND ${CMAKE_COMMAND} -E env CFLAGS=${PYTHON_CFLAGS} ${PYTHON_EXECUTABLE} -m build -w
    WORKING_DIRECTORY ${PROJECT_DIR})

  if(ENABLE_UNIT_TESTS)
    add_test(NAME "${TARGET_NAME}_tests"
      COMMAND ${CMAKE_COMMAND} -E env CFLAGS=${PYTHON_CFLAGS} "${TOX}"
      WORKING_DIRECTORY "${PROJECT_DIR}")
  endif()

  set(INSTALL_SCRIPT_NAME "${CMAKE_CURRENT_BINARY_DIR}/python_install_${PROJECT_NAME}.cmake")

  configure_file("${CMAKE_CURRENT_SOURCE_DIR}/cmake/python_install.cmake.in"
    "${INSTALL_SCRIPT_NAME}"
    @ONLY)

  install(
    SCRIPT "${INSTALL_SCRIPT_NAME}"
    COMPONENT "${ARGS_PROJECT_COMPONENT}")

endfunction()
