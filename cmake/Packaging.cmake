set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "${PROJECT_DESCRIPTION}")
set(CPACK_PACKAGE_DESCRIPTION "${PROJECT_LONG_DESCRIPTION}")
set(CPACK_RESOURCE_FILE_LICENSE "${PROJECT_SOURCE_DIR}/LICENSE")

set(CPACK_PACKAGE_CONTACT "${PROJECT_MAINTAINER_NAME} <${PROJECT_MAINTAINER_EMAIL}>")

set(CPACK_DEBIAN_PACKAGE_DEPENDS "libumfpack5 (>= 5.2), trlib (>= 0.2)")
set(CPACK_DEBIAN_PACKAGE_SECTION "science")
# set(CPACK_DEBIAN_PACKAGE_DESCRIPTION "${CPACK_PACKAGE_DESCRIPTION}")

set(CPACK_DEB_PACKAGE_COMPONENT On)
set(CPACK_DEB_COMPONENT_INSTALL On)

if(SLEQP_LPS_DEPS_DEBIAN)
  set(CPACK_DEBIAN_PACKAGE_DEPENDS "${CPACK_DEBIAN_PACKAGE_DEPENDS}, ${SLEQP_LPS_DEPS_DEBIAN}")
endif()

set(LIB_PKG_NAME "${PROJECT_NAME}${PROJECT_VERSION}")

set(CPACK_DEBIAN_LIBRARIES_PACKAGE_NAME "${LIB_PKG_NAME}")
set(CPACK_DEBIAN_LIBRARIES_PACKAGE_SHLIBDEPS On)


set(CPACK_DEBIAN_HEADERS_PACKAGE_DEPENDS "${CPACK_DEBIAN_PACKAGE_DEPENDS}")
set(CPACK_DEBIAN_HEADERS_PACKAGE_NAME "${PROJECT_NAME}-dev")
set(CPACK_DEBIAN_HEADERS_PACKAGE_DEPENDS "${LIB_PKG_NAME}")


set(CPACK_DEBIAN_PYTHON_PACKAGE_NAME "python3-${PROJECT_NAME}")
set(CPACK_DEBIAN_PYTHON_PACKAGE_DEPENDS "${LIB_PKG_NAME}, python3")
set(CPACK_DEBIAN_PYTHON_DESCRIPTION "${PROJECT_DESCRIPTION} (Python 3)")


set(CPACK_GENERATOR "DEB;TBZ2")

get_cmake_property(CPACK_COMPONENTS_ALL COMPONENTS)
list(REMOVE_ITEM CPACK_COMPONENTS_ALL "Unspecified")

include(CPack)
