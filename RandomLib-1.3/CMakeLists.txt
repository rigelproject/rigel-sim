# $Id: b07642a7d29eed25324ee651cc39933a513857d7 $

cmake_minimum_required (VERSION 2.6)
project (RandomLib)

set (RandomLib_VERSION_MAJOR 1)
set (RandomLib_VERSION_MINOR 3)
set (RandomLib_VERSION "${RandomLib_VERSION_MAJOR}.${RandomLib_VERSION_MINOR}")
set (LIBVERSION 1)
set (LIBVERSIONFULL 1.0.3)

# User-settable cache variables

# (1) Build as a shared library?  On Windows systems, this is typically
# more trouble than it's worth.
if (WIN32)
  option (RANDOM_SHARED_LIB "Build RandomLib as a shared library" OFF)
else (WIN32)
  option (RANDOM_SHARED_LIB "Build RandomLib as a shared library" ON)
endif (WIN32)

# (2) By default, cmake looks for hardware support of vector operations.
# This option allows you to disable this.
option (DISABLE_VECTOR_OPTIMIZATIONS "Do not look for SSE2 or AltiVec support"
  OFF)

# (3) By default, cmake looks for boost which is used by some of the
# examples. This option allows you to disable this.
option (DISABLE_BOOST "Do not look for boost libraries" OFF)

# Finding other packages.  (These are only need for the examples.  The
# library does not depend on them.)

# On Windows we use boost's static libraries.
if (NOT DISABLE_BOOST)
  if (WIN32)
    set (Boost_USE_STATIC_LIBS ON)
  endif (WIN32)
  find_package (Boost COMPONENTS serialization date_time)
endif (NOT DISABLE_BOOST)

# Optionally use OpenMP in RandomParallel.
find_package (OpenMP)

# The debug version of the library is called Random_d.
set (CMAKE_DEBUG_POSTFIX _d)

# Look for vector support SSE2 for Intel chips and AltiVec for PowerPC.
include (CheckIncludeFileCXX)
if (DISABLE_VECTOR_OPTIMIZATIONS)
  set (VECTOR_FLAGS "")
  set (HAVE_SSE2 OFF)
  set (HAVE_ALTIVEC OFF)
else (DISABLE_VECTOR_OPTIMIZATIONS)
  if (CMAKE_SYSTEM_PROCESSOR MATCHES "i686" OR
    CMAKE_SYSTEM_PROCESSOR MATCHES "amd64" OR
    CMAKE_SYSTEM_PROCESSOR MATCHES "x86")
    check_include_file_cxx ("emmintrin.h" HAVE_SSE2)
    if (HAVE_SSE2)
      if (WIN32)
        set (VECTOR_FLAGS "/arch:SSE2")
      else (WIN32)
        set (VECTOR_FLAGS "-msse2")
      endif (WIN32)
    else (HAVE_SSE2)
      set (VECTOR_FLAGS "")
    endif (HAVE_SSE2)
  elseif (CMAKE_SYSTEM_PROCESSOR MATCHES "powerpc")
    check_type_size ("vector unsigned" VECTOR_UNSIGNED BUILTIN_TYPES_ONLY)
    if (HAVE_VECTOR_UNSIGNED)
      set (VECTOR_FLAGS "-maltivec")
      set (HAVE_ALTIVEC ON)
    else (HAVE_VECTOR_UNSIGNED)
      set (VECTOR_FLAGS "")
      set (HAVE_ALTIVEC OFF)
    endif (HAVE_VECTOR_UNSIGNED)
  endif (CMAKE_SYSTEM_PROCESSOR MATCHES "i686" OR
    CMAKE_SYSTEM_PROCESSOR MATCHES "amd64" OR
    CMAKE_SYSTEM_PROCESSOR MATCHES "x86")
endif (DISABLE_VECTOR_OPTIMIZATIONS)

# Determine system properties
include (CheckTypeSize)
check_type_size ("long double" LONG_DOUBLE BUILTIN_TYPES_ONLY)

# Create a Config.h to expose system information to the compiler
configure_file (
    "${PROJECT_SOURCE_DIR}/include/RandomLib/Config.h.in"
    "${PROJECT_BINARY_DIR}/include/RandomLib/Config.h" )

# The documentation depends on doxygen.  Because this require tools that
# may not be available on an end-user's system, the creation of the
# final documentation is therefore only done in "MAINTAINER" mode.  The
# maintainer runs "make distrib-all" which installs the transformed
# documentation files into the source tree.
if (NOT WIN32)
  find_program (HAVE_DOXYGEN doxygen)
endif (NOT WIN32)
if (NOT WIN32 AND HAVE_DOXYGEN)
  set (MAINTAINER ON CACHE BOOL "Allow build of additional components")
else (NOT WIN32 AND HAVE_DOXYGEN)
  set (MAINTAINER OFF CACHE BOOL "Allow build of additional components")
endif (NOT WIN32 AND HAVE_DOXYGEN)
if (MAINTAINER)
  add_custom_target (distrib-all)
  add_dependencies (distrib-all distrib-doc)
endif (MAINTAINER)

# Set a default build type for single-configuration cmake generators if
# no build type is set.
if (NOT CMAKE_CONFIGURATION_TYPES AND NOT CMAKE_BUILD_TYPE)
   set (CMAKE_BUILD_TYPE Release)
endif (NOT CMAKE_CONFIGURATION_TYPES AND NOT CMAKE_BUILD_TYPE)

# On non-Windows machine, make the compiler more picky.  Also unrool
# loops in optimizing build types.
if (NOT WIN32)
  set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${VECTOR_FLAGS} -Wall -Wextra")
  set (CMAKE_CXX_FLAGS_RELWITHDEBINFO
    "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} -funroll-loops")
  set (CMAKE_CXX_FLAGS_RELEASE
    "${CMAKE_CXX_FLAGS_RELEASE} -funroll-loops")
endif (NOT WIN32)

# The list of examples to build.
set (EXAMPLES
  RandomExample RandomTime RandomThread RandomSave
  RandomExact RandomLambda RandomCoverage)

# Set the include directories.  Look in ${PROJECT_BINARY_DIR}/include
# first because that's where Config.h will be
include_directories ("${PROJECT_BINARY_DIR}/include")
include_directories (include)

# The list of subdirectories to process
add_subdirectory (src)
add_subdirectory (include/RandomLib)
add_subdirectory (examples)
add_subdirectory (doc)

# config file support for find_package(RandomLib).  This needs to deal
# with two environments: (1) finding the build tree and (2) finding the
# install tree.  randomlib-config.cmake detects which situation it is
# handing by looking at @RANDOMLIB_ROOT_DIR@.  If this is an absolute
# path, it's in the build tree; otherwise, it's in the install tree
# (incidentally, this allows the install tree to be relocated).

# randomlib-config.cmake for the build tree
set (RANDOMLIB_ROOT_DIR "${PROJECT_BINARY_DIR}")
set (RandomLib_INCLUDE_DIRS
  "${PROJECT_BINARY_DIR}/include" "${PROJECT_SOURCE_DIR}/include" )
configure_file (cmake/randomlib-config.cmake.in
  "${PROJECT_BINARY_DIR}/randomlib-config.cmake" @ONLY)
configure_file (cmake/randomlib-config-version.cmake.in
  "${PROJECT_BINARY_DIR}/randomlib-config-version.cmake" @ONLY)

# randomlib-config.cmake for the install tree.  It's installed in
# ${INSTALL_CMAKE_DIR} and @RANDOMLIB_ROOT_DIR@ is the relative path to
# the root from there.
if (WIN32)
  set(INSTALL_CMAKE_DIR "cmake")
  set (RANDOMLIB_ROOT_DIR "..")
else (WIN32)
  set(INSTALL_CMAKE_DIR
    "share/cmake/${CMAKE_PROJECT_NAME}-${RandomLib_VERSION}")
  set (RANDOMLIB_ROOT_DIR "../../..")
endif (WIN32)
# @RandomLib_INCLUDE_DIRS@ is not used in the install tree;
# reset it to prevent the source and build paths appearing in the
# installed config files
set (RandomLib_INCLUDE_DIRS IGNORE)
configure_file (cmake/randomlib-config.cmake.in
  "${PROJECT_BINARY_DIR}/InstallFiles/randomlib-config.cmake" @ONLY)
configure_file (cmake/randomlib-config-version.cmake.in
  "${PROJECT_BINARY_DIR}/InstallFiles/randomlib-config-version.cmake" @ONLY)
install (FILES
  "${PROJECT_BINARY_DIR}/InstallFiles/randomlib-config.cmake"
  "${PROJECT_BINARY_DIR}/InstallFiles/randomlib-config-version.cmake"
  DESTINATION "${INSTALL_CMAKE_DIR}")

# Make information about the cmake targets (the library)
# available.
export (TARGETS Random
  FILE "${PROJECT_BINARY_DIR}/RandomLibDepends.cmake")
install (EXPORT RandomLibDepends DESTINATION "${INSTALL_CMAKE_DIR}")

# Don't want cmake to look in build trees, so don't do
# export (PACKAGE RandomLib)

# Packaging support; we deal with 
# (1) a source distribution: cmake make a tar.gz file and the zip file
# is created from this.  Only the maintainer can do this, because of
# the need to generate additional documentation files.
# (2) a binary distribution: code is included for Linux, Apple, and
# Windows, but only the Windows distribution has been exercised.

# Need to ensure that system dlls get included in a binary distribution
include (InstallRequiredSystemLibraries)

# The configuration of CPack is via variable that need to be set before
# the include (CPack).
set (CPACK_PACKAGE_VERSION_MAJOR ${RandomLib_VERSION_MAJOR})
set (CPACK_PACKAGE_VERSION_MINOR ${RandomLib_VERSION_MINOR})
set (CPACK_PACKAGE_VERSION_PATCH 0)
set (CPACK_PACKAGE_CONTACT charles@karney.com)
set (CPACK_PACKAGE_VENDOR "RandomLib")
set (CPACK_PACKAGE_DESCRIPTION_SUMMARY
  "Random library and documentation")
# It's necessary to specify a rather long list of files to be excluded
# in the source distribution.
set (CPACK_SOURCE_IGNORE_FILES
  "#"
  "~\$"
  "${PROJECT_SOURCE_DIR}/(BUILD|WINDOWS|TAGS|randomlib-[0-9])"
  "${PROJECT_SOURCE_DIR}/(release-notes|distrib)/"
  "${PROJECT_SOURCE_DIR}/(cgi-bin|scratch|srcL)/"
  "${PROJECT_SOURCE_DIR}/(tests|testdata)/"
  "${PROJECT_SOURCE_DIR}/(makefile-admin|install_manifest.txt)"
  "Makefile\$"
  "Config.h\$"
  "TAGS\$"
  "/\\\\.(deps|libs|svn)/"
  "/\\\\.git"
  "/(UpgradeLog|_UpgradeReport)"
  "/(CMakeCache.txt|CPack)"
  "/(CMakeFiles|_CPack_Packages|BACKUP)/"
  "/config\\\\.(h|status)"
  "/Config-ac\\\\.h\$"
  "/(status|libtool)\$"
  "/stamp-"
  "/junk"
  "${PROJECT_SOURCE_DIR}/(|.*/)rand\\\\."
  "\\\\.(bak|BAK|patch|tmp|zip|gz|lo|o|ncb|suo|a|la|log|exe)\$"
  "\\\\.(mex|mexw32|time|eps)\$"
  "/(Release|Debug|Win32)"
  "\\\\.cache/"
  "proj.*\\\\.user\$"
  "lib.*\\\\.so"
  "examples/[A-Za-z]+\$"
  "windows/.*Upgrade"
  "/(fix-permissions|autogen|biblio)\\\\.sh"
  "${PROJECT_SOURCE_DIR}/[^/]*\\\\.html" )
set (CPACK_SOURCE_GENERATOR TGZ)

set (CPACK_RESOURCE_FILE_LICENSE ${PROJECT_SOURCE_DIR}/LICENSE.txt)
set (CPACK_PACKAGE_INSTALL_DIRECTORY
  "${CMAKE_PROJECT_NAME}-${RandomLib_VERSION}")
set (CPACK_SOURCE_PACKAGE_FILE_NAME "${CPACK_PACKAGE_INSTALL_DIRECTORY}")

if (WIN32)
  # The Windows binary packager is NSIS.  Set the necessary variables
  # for this.
  set (CPACK_NSIS_CONTACT "charles@karney.com")
  set (CPACK_NSIS_URL_INFO_ABOUT "http://randomlib.sf.net")
  set (CPACK_NSIS_HELP_LINK "mailto:charles@karney.com")
  set (CPACK_NSIS_PACKAGE_NAME
    "${CMAKE_PROJECT_NAME} ${RandomLib_VERSION}")
  set (CPACK_NSIS_DISPLAY_NAME ${CPACK_NSIS_PACKAGE_NAME})
  set (CPACK_PACKAGE_FILE_NAME "${CPACK_PACKAGE_INSTALL_DIRECTORY}-win32")
  set (CPACK_NSIS_EXTRA_INSTALL_COMMANDS "
  CreateShortCut \\\"$SMPROGRAMS\\\\$STARTMENU_FOLDER\\\\Library Documentation.lnk\\\" \\\"$INSTDIR\\\\share\\\\doc\\\\RandomLib\\\\html\\\\index.html\\\"
")
  set (CPACK_NSIS_EXTRA_UNINSTALL_COMMANDS "
  !insertmacro MUI_STARTMENU_GETFOLDER Application $MUI_TEMP
  Delete \\\"$SMPROGRAMS\\\\$MUI_TEMP\\\\Library Documentation.lnk\\\"
")
  set (CPACK_NSIS_MODIFY_PATH ON)
elseif (APPLE)
  # Not tested
  set (CPACK_GENERATOR DMG)
  set (CPACK_PACKAGE_FILE_NAME "${CPACK_PACKAGE_INSTALL_DIRECTORY}-darwin")
else (WIN32)
  # Not tested
  set (CPACK_GENERATOR TGZ)
endif (WIN32)

include (CPack)

# Another maintainer-specific target is building the source distribution
# via the target dist.  This calls package_source to make a tar.gz file.
# However this needs to be touched up to support the vanilla Makefiles
# provided with GeographicLib.  This entails
# (1) creating Makefile (which includes Makefile.mk);
# (2) creating a bare-bones Config.h (with just the version information);
# (3) making sure that make thinks the generated documentation files are
# up-to-date.
# Then a new tar.gz file and zip file are created.  To avoid potential
# problems with directory permissions, tar and zip are told only to
# archive the files.
if (MAINTAINER)
  add_custom_target (dist
    COMMAND ${CMAKE_MAKE_PROGRAM} package_source
    COMMAND
      cd _CPack_Packages/Linux-Source/TGZ/${CPACK_SOURCE_PACKAGE_FILE_NAME} &&
      echo include Makefile.mk > Makefile &&
      sed -e "s/Unconfigured/${RandomLib_VERSION}/"
      ${PROJECT_SOURCE_DIR}/include/RandomLib/Config.h.template
      > include/RandomLib/Config.h
    COMMAND
      cd _CPack_Packages/Linux-Source/TGZ/${CPACK_SOURCE_PACKAGE_FILE_NAME} &&
      touch doc/html/index.html
    COMMAND
      cd _CPack_Packages/Linux-Source/TGZ &&
      find ${CPACK_SOURCE_PACKAGE_FILE_NAME} -type f |
      tar cfzT ${CMAKE_BINARY_DIR}/${CPACK_SOURCE_PACKAGE_FILE_NAME}.tar.gz -
    COMMAND
      rm -f ${CPACK_SOURCE_PACKAGE_FILE_NAME}.zip &&
      cd _CPack_Packages/Linux-Source/TGZ &&
      find ${CPACK_SOURCE_PACKAGE_FILE_NAME} -type f |
      zip -q ${CMAKE_BINARY_DIR}/${CPACK_SOURCE_PACKAGE_FILE_NAME}.zip -@
  )
  add_dependencies (dist distrib-all)
endif (MAINTAINER)

# Add a test target; the tests are in tools.
enable_testing ()
