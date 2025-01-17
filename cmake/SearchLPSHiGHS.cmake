# - Try to find HiGHS
# See http://www.highs.dev for more information on HiGHS
#
# Once done, this will define
#
#  HIGHS_INCLUDE_DIRS   - where to find gurobi_c.h, etc.
#  HIGHS_LIBRARIES      - List of libraries when using gurobi.
#  HIGHS_FOUND          - True if Gurobi found.
#
# A maintainer may set HIGHS_ROOT to a highs installation root to tell
# this module where to look.

if(PKG_CONFIG_FOUND)
  pkg_check_modules(HIGHS highs)
  if(HIGHS_FOUND)
    set(HIGHS_LIBRARIES ${HIGHS_LINK_LIBRARIES})
  endif()
endif()

if(NOT HIGHS_FOUND)
  find_path(HIGHS_INCLUDE_DIRS
    NAMES Highs.h
    PATHS ${HIGHS_ROOT}
    PATH_SUFFIXES include highs)

  find_library(HIGHS_LIBRARIES
    NAMES highs
    PATHS ${HIGHS_ROOT}
    PATH_SUFFIXES lib)
endif()

if(NOT HIGHS_INCLUDE_DIRS)
  find_path(HIGHS_INCLUDE_DIRS
    NAMES Highs.h
    PATHS ${HIGHS_ROOT}
    PATH_SUFFIXES include highs)
endif()

if(HIGHS_INCLUDE_DIRS)
  find_file(HIGHS_CONFIG_HEADER
    NAMES "HConfig.h"
    PATHS ${HIGHS_INCLUDE_DIR})

  if(HIGHS_CONFIG_HEADER)
    file(STRINGS "${HIGHS_CONFIG_HEADER}" HIGHS_MAJOR REGEX "^#define HIGHS_VERSION_MAJOR +[0-9]+")
    string(REGEX REPLACE "^#define HIGHS_VERSION_MAJOR +([0-9]+)" "\\1" MAJOR ${HIGHS_MAJOR})

    file(STRINGS "${HIGHS_CONFIG_HEADER}" HIGHS_MINOR REGEX "^#define HIGHS_VERSION_MINOR +[0-9]+")
    string(REGEX REPLACE "^#define HIGHS_VERSION_MINOR +([0-9]+)" "\\1" MINOR ${HIGHS_MINOR})

    file(STRINGS "${HIGHS_CONFIG_HEADER}" HIGHS_PATCH REGEX "^#define HIGHS_VERSION_PATCH +[0-9]+")
    string(REGEX REPLACE "^#define HIGHS_VERSION_PATCH +([0-9]+)" "\\1" PATCH ${HIGHS_PATCH})

    file(STRINGS "${HIGHS_CONFIG_HEADER}" HIGHS_GITHASH REGEX "^#define HIGHS_GITHASH \"+[a-z0-9\-]+\"")

    if(HIGHS_GITHASH)
      string(REGEX REPLACE "^#define HIGHS_GITHASH \"+([a-z0-9\-]+)\"" "\\1" GITHASH ${HIGHS_GITHASH})
      set(HIGHS_VERSION "${MAJOR}.${MINOR}.${PATCH} [${GITHASH}]")
    else()
      set(HIGHS_VERSION "${MAJOR}.${MINOR}.${PATCH}")
    endif()

    mark_as_advanced(HIGHS_CONFIG_HEADER)
  endif()
endif()

if(NOT HIGHS_FOUND)
  find_package_handle_standard_args(HiGHS
    FOUND_VAR HIGHS_FOUND
    REQUIRED_VARS HIGHS_INCLUDE_DIRS HIGHS_LIBRARIES
    VERSION_VAR HIGHS_VERSION)
endif()

mark_as_advanced(HIGHS_INCLUDE_DIRS HIGHS_LIBRARIES)
