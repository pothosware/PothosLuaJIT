# Locate Lua library
# This module defines
#  LUAJIT_FOUND, if false, do not try to link to Lua 
#  LUAJIT_LIBRARIES
#  LUAJIT_INCLUDE_DIR, where to find lua.h 
#
# Note that the expected include convention is
#  #include "lua.h"
# and not
#  #include <lua/lua.h>
# This is because, the lua location is not standardized and may exist
# in locations other than lua/

#=============================================================================
# Copyright 2007-2009 Kitware, Inc.
#
# Distributed under the OSI-approved BSD License (the "License");
# see accompanying file Copyright.txt for details.
#
# This software is distributed WITHOUT ANY WARRANTY; without even the
# implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
# See the License for more information.
#=============================================================================
# (To distributed this file outside of CMake, substitute the full
#  License text for the above reference.)
#
# ################
# 2010 - modified for cronkite to find luajit instead of lua, as it was before.
# ################
# 2020 - modified to support LuaJIT 2.1
#

FIND_PATH(LUAJIT_INCLUDE_DIR lua.h
  HINTS
  $ENV{LUAJIT_DIR}
  PATH_SUFFIXES include/luajit-2.0 include/luajit2.0 include/luajit-2.1 include/luajit2.1 include/luajit include
  PATHS
  ~/Library/Frameworks
  /Library/Frameworks
  /usr/local
  /usr
  /sw # Fink
  /opt/local # DarwinPorts
  /opt/csw # Blastwave
  /opt
)

FIND_LIBRARY(LUAJIT_LIBRARY 
  NAMES luajit-51 luajit-5.1 luajit
  HINTS
  $ENV{LUAJIT_DIR}
  PATH_SUFFIXES lib64 lib
  PATHS
  ~/Library/Frameworks
  /Library/Frameworks
  /usr/local
  /usr
  /sw
  /opt/local
  /opt/csw
  /opt
)

IF(LUAJIT_LIBRARY)
  # include the math library for Unix
  IF(UNIX AND NOT APPLE)
    FIND_LIBRARY(LUAJIT_MATH_LIBRARY m)
    SET( LUAJIT_LIBRARIES "${LUAJIT_LIBRARY};${LUAJIT_MATH_LIBRARY}" CACHE STRING "Lua Libraries")
  # For Windows and Mac, don't need to explicitly include the math library
  ELSE(UNIX AND NOT APPLE)
    SET( LUAJIT_LIBRARIES "${LUAJIT_LIBRARY}" CACHE STRING "Lua Libraries")
  ENDIF(UNIX AND NOT APPLE)
ENDIF(LUAJIT_LIBRARY)

INCLUDE(FindPackageHandleStandardArgs)
# handle the QUIETLY and REQUIRED arguments and set LUAJIT_FOUND to TRUE if 
# all listed variables are TRUE
FIND_PACKAGE_HANDLE_STANDARD_ARGS(LuaJIT  DEFAULT_MSG  LUAJIT_LIBRARIES LUAJIT_INCLUDE_DIR)

MARK_AS_ADVANCED(LUAJIT_INCLUDE_DIR LUAJIT_LIBRARIES LUAJIT_LIBRARY LUAJIT_MATH_LIBRARY)
