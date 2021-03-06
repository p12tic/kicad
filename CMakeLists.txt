project( kicad )

cmake_minimum_required( VERSION 2.8.4 FATAL_ERROR )
# because of http://public.kitware.com/Bug/view.php?id=10395

# Path to local CMake modules.
set( CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/CMakeModules )

#
# KiCad build options should be added below.
#
# If you add a new build option, please add it's state to the CopyVersionInfoToClipboard()
# function in common/basicframe.cpp so that build option settings can be included in bug
# reports.
#

#for those who bored with uppercase
option( KICAD_KEEPCASE "turn-off automatic component name conversion to uppercase if selected" )

option( USE_WX_GRAPHICS_CONTEXT
    "Use wxGraphicsContext for rendering ( default OFF). Warning, this is experimental" )

option( USE_WX_OVERLAY
    "Use wxOverlay: Always ON for MAC ( default OFF). Warning, this is experimental" )

#One of these 2 option *must* be set to ON:
option( KICAD_STABLE_VERSION
    "set this option to ON to build the stable version of KICAD. mainly used to set version ID (default OFF)"
    )

option( KICAD_SCRIPTING
    "set this option ON to build the scripting support inside kicad binaries"
    )

option( KICAD_SCRIPTING_MODULES
    "set this option ON to build kicad modules that can be used from scripting languages"
    )

option( KICAD_SCRIPTING_WXPYTHON
    "set this option ON to build wxpython implementation for wx interface building in python and py.shell"
    )

# when option KICAD_SCRIPTING OR KICAD_SCRIPTING_MODULES is enabled:
# PYTHON_EXECUTABLE can be defined when invoking cmake
# ( use -DPYTHON_EXECUTABLE=<python path>/python.exe or python2 )
# when not defined by user, the default is python.exe under Windows and python2 for others
# python binary file should be is exec path.


option( USE_FP_LIB_TABLE "Use the new footprint library table implementation. ( default OFF)" )

option( BUILD_GITHUB_PLUGIN "Build the GITHUB_PLUGIN for pcbnew." OFF )


# Set version option (stable or testing)
if( KICAD_STABLE_VERSION)
    add_definitions( -DKICAD_STABLE_VERSION )
    message( STATUS "Building stable version of KiCad" )
endif()

# All CMake downloads go here.  Suggested is up in the source tree, not in the build dir where they
# would have to be downloaded over and over again.  The default is to choose a directory that is
# hidden on linux (starts with a '.') because there is a way to exclude this directory when grepping
# the source tree.
set( DOWNLOAD_DIR ${PROJECT_SOURCE_DIR}/.downloads-by-cmake
    CACHE PATH "Location of KiCad downloads, suggested is a dir common to all builds, i.e. global." )

#Add option to add user directories for linker, if any
LINK_DIRECTORIES( ${LINK_DIRECTORIES_PATH} )

if( UNIX )
    set( KICAD_USER_CONFIG_DIR $ENV{HOME} CACHE PATH "Location of user specifig KiCad config files" )
elseif( MINGW )
    set( KICAD_USER_CONFIG_DIR $ENV{%APPDATA%} CACHE PATH "Location of user specifig KiCad config files" )
endif()
mark_as_advanced( KICAD_USER_CONFIG_DIR )


#================================================
# Set flags for GCC.
#================================================

if( CMAKE_COMPILER_IS_GNUCXX )

    execute_process( COMMAND ${CMAKE_C_COMPILER} -dumpversion
        OUTPUT_VARIABLE GCC_VERSION
        OUTPUT_STRIP_TRAILING_WHITESPACE )

    # Establish -Wall early, so specialized relaxations of this may come
    # subsequently on the command line, such as in pcbnew/github/CMakeLists.txt
    set( CMAKE_C_FLAGS   "-Wall ${CMAKE_C_FLAGS}" )
    set( CMAKE_CXX_FLAGS "-Wall ${CMAKE_CXX_FLAGS}" )

    # The optimization level is -O1 instead of the usual -O2 level because
    # boost::polygon has a function (inflate polygon) broken by the -O2 level
    # with GCC 4.7.0 to 4.7.2 (works fine with  with GCC 4.6 and 4.7.3).
    # This lower optimization level does not have a significant change on the speed.
    # See also:
    #   https://bugs.launchpad.net/kicad/+bug/1056926
    #   https://svn.boost.org/trac/boost/ticket/7983
    if( GCC_VERSION VERSION_EQUAL 4.7.0 OR ( GCC_VERSION VERSION_GREATER 4.7.0 AND GCC_VERSION VERSION_LESS 4.7.3 ) )
        set( CMAKE_C_FLAGS_RELEASE   "-O1" )
        set( CMAKE_CXX_FLAGS_RELEASE "-O1" )
    else()
        set( CMAKE_C_FLAGS_RELEASE   "-O2" )
        set( CMAKE_CXX_FLAGS_RELEASE "-O2" )
    endif()

    set( CMAKE_C_FLAGS_DEBUG   "-g3 -ggdb3 -DDEBUG" )
    set( CMAKE_CXX_FLAGS_DEBUG "-g3 -ggdb3 -DDEBUG" )

    set( CMAKE_C_FLAGS_RELEASE   "${CMAKE_C_FLAGS_RELEASE} -DNDEBUG" )
    set( CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -DNDEBUG" )

    if( MINGW )
        set( CMAKE_EXE_LINKER_FLAGS_RELEASE "-s" )

        # _UNICODE definition seems needed under mingw/gcc 4.8
        # (Kicad uses unicode, and on Windows, wxWidgets >= 2.9.4 is mandatory
        # and uses unicode)
        if( GCC_VERSION VERSION_EQUAL 4.8.0 OR GCC_VERSION VERSION_GREATER 4.8.0 )
            add_definitions(-D_UNICODE)
        endif()

        # Since version 2.8.5, Cmake uses a response file (.rsp) to
        # pass the list of include paths to gcc
        # unfortunately, under mingw32+msys, at least with gcc 4.8 and previous,
        # this file is not expanded and include paths defined in this file are not taken in account
        # this is the case of wxWidgets includes
        # Disable this response file for includes ( See Windows-GNU.cmake module )
        if( WIN32 AND MSYS AND NOT CMAKE_CROSSCOMPILING )
            # fixme: it is needed only with MSYS+MINGW32? or always under MINGW
            if (${CMAKE_SIZEOF_VOID_P} MATCHES 4)
                set(CMAKE_CXX_USE_RESPONSE_FILE_FOR_INCLUDES 0)
            endif()
        endif()

    else()
        # We build DLL/DSOs from static libraries, so create position independent code
        # for all cases, since we do not have DLL/DSO specific static libraries.
        # Subdirectories via add_subdirectores() reference this variable, and it is either set or empty,
        # empty for Windows.
        set( PIC_FLAG -fPIC )

        set( CMAKE_C_FLAGS   "${CMAKE_C_FLAGS} ${PIC_FLAG}" )
        set( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${PIC_FLAG}" )

        # Thou shalt not link vaporware and tell us it's a valid DSO:
        set( CMAKE_SHARED_LINKER_FLAGS "-Wl,--no-undefined" )
        set( CMAKE_MODULE_LINKER_FLAGS "-Wl,--no-undefined" ) # needed by SWIG macros on linux

        # Set default flags for Release build.
        set( CMAKE_EXE_LINKER_FLAGS_RELEASE "-s" )

    endif()

    # quiet GCC 4.8.1 while in boost
    if( GCC_VERSION VERSION_GREATER 4.8 OR GCC_VERSION VERSION_EQUAL 4.8 )
        set( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-unused-local-typedefs" )
    endif()

endif( CMAKE_COMPILER_IS_GNUCXX )

if( KICAD_KEEPCASE )
    add_definitions( -DKICAD_KEEPCASE )
endif()

if( USE_WX_OVERLAY OR APPLE )
    add_definitions( -DUSE_WX_OVERLAY )
endif()

if( KICAD_SCRIPTING )
    add_definitions( -DKICAD_SCRIPTING )
endif()

if( KICAD_SCRIPTING_MODULES )
    add_definitions( -DKICAD_SCRIPTING_MODULES )
endif()

if( KICAD_SCRIPTING_WXPYTHON )
    add_definitions( -DKICAD_SCRIPTING_WXPYTHON )
endif()

if( USE_WX_GRAPHICS_CONTEXT )
    add_definitions( -DUSE_WX_GRAPHICS_CONTEXT )
endif()

# Allow user to override the default settings for adding images to menu items.  By default
# images in menu items are enabled on all platforms except OSX.  This can be over ridden by
# defining -DUSE_IMAGES_IN_MENUS=ON/OFF to force the preferred behavior.
if( NOT DEFINED USE_IMAGES_IN_MENUS )
    if( NOT APPLE )
        set( USE_IMAGES_IN_MENUS ON )
    endif()
else()
    if( USE_IMAGES_IN_MENUS )
        set( USE_IMAGES_IN_MENUS ON )
    endif()
endif()

#================================================
# Locations for install targets.
set( KICAD_BIN bin
    CACHE PATH "Location of KiCad binaries." )

set( KICAD_FP_LIB_INSTALL_PATH "${CMAKE_INSTALL_PREFIX}"
     CACHE PATH "Default path where footprint libraries are installed." )

if( UNIX )
    # Everything without leading / is relative to CMAKE_INSTALL_PREFIX.
    set( KICAD_PLUGINS lib/kicad/plugins
        CACHE PATH "Location of KiCad plugins." )
    set( KICAD_DATA share/kicad
        CACHE PATH "Location of KiCad data files." )
    set( KICAD_DOCS share/doc/kicad
        CACHE PATH "Location of KiCad documentation files." )
    set( KICAD_FP_LIB_INSTALL_PATH "${KICAD_FP_LIB_INSTALL_PATH}/share/kicad/modules" )
endif()

if( MINGW )
    # Everything without leading / is relative to CMAKE_INSTALL_PREFIX.
    set( KICAD_PLUGINS ${KICAD_BIN}/plugins
        CACHE PATH "Location of KiCad plugins." )
    set( KICAD_DATA share
        CACHE PATH "Location of KiCad data files." )
    set( KICAD_DOCS doc
        CACHE PATH "Location of KiCad documentation files." )
    set( KICAD_FP_LIB_INSTALL_PATH "${KICAD_FP_LIB_INSTALL_PATH}/modules" )
endif()

set( KICAD_DEMOS ${KICAD_DATA}/demos
    CACHE PATH "Location of KiCad demo files." )
set( KICAD_TEMPLATE ${KICAD_DATA}/template
    CACHE PATH "Location of KiCad template files." )

mark_as_advanced( KICAD_BIN
    KICAD_PLUGINS
    KICAD_DATA
    KICAD_DOCS
    KICAD_DEMOS
    KICAD_TEMPLATE )

include( Functions )

include( ExternalProject )

#================================================
# Find libraries that are needed to build KiCad.
#================================================
include( CheckFindPackageResult )

# Turn on wxWidgets compatibility mode for some classes
add_definitions(-DWX_COMPATIBILITY)

#######################
# Find OpenGL library #
#######################
find_package( OpenGL QUIET )
check_find_package_result( OPENGL_FOUND "OpenGL" )

#####################
# Find GLEW library #
#####################
find_package(GLEW)
check_find_package_result(GLEW_FOUND "GLEW")

######################
# Find Cairo library #
######################
find_package(Cairo 1.8.1 QUIET)
check_find_package_result(CAIRO_FOUND "Cairo")

# Download boost and possibly build parts of it
#################################################
find_package(Boost 1.54 COMPONENTS
    date_time
    filesystem
    iostreams
    locale
    program_options
    regex
    system
    thread
    REQUIRED)

##########################
# Find wxWidgets library #
##########################
# Here you can define what libraries of wxWidgets you need for your
# application. You can figure out what libraries you need here;
# http://www.wxwidgets.org/manuals/2.8/wx_librarieslist.html

# On Apple only wxwidgets 2.9 or higher doesn't need to find aui part of base
if( APPLE )
    find_package( wxWidgets COMPONENTS gl adv html core net base xml QUIET )
else()
    find_package( wxWidgets COMPONENTS gl aui adv html core net base xml QUIET )
endif()
check_find_package_result( wxWidgets_FOUND "wxWidgets" )


# Include wxWidgets macros.
include( ${wxWidgets_USE_FILE} )

# Include MinGW resource compiler.
include( MinGWResourceCompiler )

# Generate build system specific header file.
include( PerformFeatureChecks )
perform_feature_checks()

# Find GDI+ on windows if wxGraphicsContext is available.
if( MINGW AND USE_WX_GRAPHICS_CONTEXT )
    find_package( GdiPlus )
    check_find_package_result( GDI_PLUS_FOUND "GDI+" )
endif()

# Assist with header file searching optimization:
# INC_BEFORE and INC_AFTER are two lists which go at the front and back of the
# header file search lists, respectively.
# INC_BEFORE is for use with "include_directories( BEFORE ...)", which _reverses_
# the order during insertion.  (So put first wanted last, which is
# ${CMAKE_SOURCE_DIR/include.)  Use '.' for current source dir since
# we don't want expansion here and now, which would happen if using ${CMAKE_CURRENT_SOURCE_DIR}.
# Instead we use '.' which is applicable to any source directory below here as a result of
# this lack of expansion.
set( INC_BEFORE
    .
    ${CMAKE_SOURCE_DIR}/include
    )

set( INC_AFTER
    ${Boost_INCLUDE_DIR}
    #include <config.h>
    ${CMAKE_BINARY_DIR}
    )


# Find Python and other scripting resources
if( KICAD_SCRIPTING OR KICAD_SCRIPTING_MODULES )
    set( PythonInterp_FIND_VERSION )
    find_package( PythonInterp )
    check_find_package_result( PYTHONINTERP_FOUND "Python Interpreter" )

    # Get the correct Python site package install path from the Python interpreter found by
    # FindPythonInterp unless the user specifically defined a custom path.
    if( NOT PYTHON_SITE_PACKAGE_PATH )
        execute_process( COMMAND ${PYTHON_EXECUTABLE} -c "import distutils.sysconfig;print\"%s\"%distutils.sysconfig.get_python_lib()"
            OUTPUT_VARIABLE PYTHON_SITE_PACKAGE_PATH
            OUTPUT_STRIP_TRAILING_WHITESPACE
            )

        if( NOT PYTHON_SITE_PACKAGE_PATH )
            message( FATAL_ERROR "Error occurred while attemping to find the Python site library path." )
        endif()
    endif()

    set( PYTHON_DEST "${PYTHON_SITE_PACKAGE_PATH}" CACHE PATH "Python module install path." )
    mark_as_advanced( PYTHON_DEST )
    message( STATUS "Python module install path: ${PYTHON_DEST}" )

    find_package( PythonLibs )

    #message( STATUS "PYTHON_INCLUDE_DIRS:${PYTHON_INCLUDE_DIRS}" )

    # Infrequently needed headers go at end of search paths, append to INC_AFTER which
    # although is used for all components, should be a harmless hit for something like eeschema
    # so long as unused search paths are at the end like this.
    set( INC_AFTER ${INC_AFTER} ${PYTHON_INCLUDE_DIRS} ${CMAKE_CURRENT_SOURCE_DIR}/scripting )

    #message( STATUS "/ INC_AFTER:${INC_AFTER}" )

endif()


# Automagically create version header file.
include( CreateBzrVersionHeader )
create_bzr_version_header()

if( EXISTS ${CMAKE_SOURCE_DIR}/include/config.h )
    # This file may exist ( created by an alternate process to the svn test above),
    # e.g. could be created by a checkout script that produces a source tar file.
    # This directive means the about box will have the svn date & revision in it,
    # but the hard coded release date ( program version) will be preserved.
    add_definitions( -DHAVE_SVN_REVISION )
endif()


#================================================
# Let CMake look in these directories for nested
# 'CMakeLists.txt' files to process
#================================================

############################
# Binaries ( CMake targets ) #
############################

add_subdirectory( bitmaps_png )
add_subdirectory( common )
add_subdirectory( 3d-viewer )
add_subdirectory( cvpcb )
add_subdirectory( eeschema )
add_subdirectory( gerbview )
add_subdirectory( kicad )
add_subdirectory( lib_dxf )
add_subdirectory( pcbnew )
add_subdirectory( polygon )
add_subdirectory( pagelayout_editor )
add_subdirectory( potrace )
add_subdirectory( bitmap2component )
add_subdirectory( pcb_calculator )
add_subdirectory( tools )
#add_subdirectory( new )


# Make all libs and executables depend on ExternalProject_Add( boost ),
# except perhaps bitmap lib.  This allows a multi-threaded build to succeed.
add_dependencies( pcbnew boost )
add_dependencies( eeschema boost )
add_dependencies( cvpcb boost )
add_dependencies( gal boost )
add_dependencies( common boost )
add_dependencies( pcbcommon boost )
add_dependencies( 3d-viewer boost )
add_dependencies( pcad2kicadpcb boost )
add_dependencies( polygon boost )
add_dependencies( pl_editor boost )
add_dependencies( pnsrouter boost )


#############
# Resources #
#############
add_subdirectory( demos )
add_subdirectory( template )

#================================================
# Doxygen Output
#================================================
find_package( Doxygen )
if( DOXYGEN_FOUND )
    add_custom_target( doxygen-docs
        ${CMAKE_COMMAND} -E remove_directory Documentation/doxygen
        COMMAND ${DOXYGEN_EXECUTABLE}
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        DEPENDS Doxyfile
        COMMENT "building doxygen docs into directory Documentation/doxygen/html"
        )
else()
    message( STATUS "WARNING: Doxygen not found - doxygen-docs (Source Docs) target not created" )
endif()


#================================================
# "make uninstall" rules
#================================================
configure_file(
    "${CMAKE_MODULE_PATH}/cmake_uninstall.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake"
    IMMEDIATE @ONLY )

add_custom_target( uninstall
    "${CMAKE_COMMAND}" -P "${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake" )


#================================================
# Installation
#================================================

add_custom_target( install_user_configuration_files
    "${CMAKE_COMMAND}" -E copy "${PROJECT_SOURCE_DIR}/template/fp-lib-table" ${KICAD_USER_CONFIG_DIR}/
    COMMENT "Install template fp-lib-table into your home directory."
    )

install( FILES INSTALL.txt
    DESTINATION ${KICAD_DOCS}
    COMPONENT resources )

install( FILES resources/freeroute.jnlp
    DESTINATION ${KICAD_BIN}
    COMPONENT resources )

###
# Install scripts
###
if( UNIX )
    install( DIRECTORY scripts
        DESTINATION ${KICAD_DOCS}
        COMPONENT resources
        )
endif()

###
# FreeDesktop .desktop and MIME resources
###
if( UNIX )

    # Set paths
    set( UNIX_MIME_DIR resources/linux/mime )
    set( UNIX_MIMELNK_FILES ${UNIX_MIME_DIR}/mimelnk )
    set( UNIX_MIME_FILES ${UNIX_MIME_DIR}/mime )
    set( UNIX_ICONS_FILES ${UNIX_MIME_DIR}/icons )
    set( UNIX_APPLICATIONS_FILES ${UNIX_MIME_DIR}/applications )

    # Install Mimelnk directory
    install( DIRECTORY ${UNIX_MIMELNK_FILES}
        DESTINATION ${CMAKE_INSTALL_PREFIX}/share
        COMPONENT resources
        )

    # Install Mime directory
    install( DIRECTORY ${UNIX_ICONS_FILES}
        DESTINATION ${CMAKE_INSTALL_PREFIX}/share
        COMPONENT resources
        )

    # Install Icons
    install( DIRECTORY ${UNIX_MIME_FILES}
        DESTINATION ${CMAKE_INSTALL_PREFIX}/share
        COMPONENT resources
        )

    # Install Applications directory (.desktop files)
    install( DIRECTORY ${UNIX_APPLICATIONS_FILES}
        DESTINATION ${CMAKE_INSTALL_PREFIX}/share
        COMPONENT resources
        )
endif()

#include( CTest )


if( UNIX AND NOT APPLE )

    # Create a *.deb file:
    set( CPACK_GENERATOR "DEB" )
    set( CPACK_DEBIAN_PACKAGE_MAINTAINER "http://launchpad.net/kicad" )

    set( CPACK_PACKAGE_VERSION_MAJOR 1 )
    set( CPACK_PACKAGE_VERSION_MINOR 0 )
    set( CPACK_PACKAGE_VERSION_PATCH 0 )
    #set( CPACK_PACKAGE_CONTACT Firstname Lastname <email@company.com> )
    set( CPACK_PACKAGE_DESCRIPTION_SUMMARY "KiCad built by CMake build system." )

    # Tell debian CPack about all files which are configuration files
    add_conffiles()         # clear file
    add_conffiles( ${KICAD_USER_CONFIG_DIR}/fp-lib-table )  # append to it

    include( CPack )

endif()
