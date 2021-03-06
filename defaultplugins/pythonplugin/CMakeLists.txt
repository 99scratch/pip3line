####
# Released as open source by Gabriel Caudrelier
#
# Developed by Gabriel Caudrelier, gabriel dot caudrelier at gmail dot com
#
# https://github.com/metrodango/pip3line
#
# Released under AGPL see LICENSE for more information
####

message ( STATUS "Configuring the Python 3 plugin --" )
project( python3plugin )

set ( CMAKE_BUILD_TYPE Release )
add_definitions ( -Wall -DPYTHONPLUGIN_LIBRARY -DQT_NO_KEYWORDS -DBUILD_PYTHON_3)

set(CMAKE_AUTOMOC ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

find_package(Qt5Widgets)
find_package(Qt5Core)
find_package(Qt5Svg)

# hardcoded paths, in case none are supplied or found

if (WIN32)
  if("${CMAKE_SIZEOF_VOID_P}" STREQUAL "4")
    set (PYTHON_HARDCODED_INCLUDE "C:\\Python36\\include")
    set (PYTHON_HARDCODED_LIBS "C:\\Python36\\libs\\python36.lib")
  else ()
    set (PYTHON_HARDCODED_INCLUDE "C:\\Python36-x64\\include")
    set (PYTHON_HARDCODED_LIBS "C:\\Python36-x64\\libs\\python36.lib")
  endif() 

elseif(APPLE)
  list(APPEND CMAKE_PREFIX_PATH "/usr/local")
  set (PYTHON_HARDCODED_INCLUDE "/usr/local/Frameworks/Python.framework/Versions/3.5/include/python3.5m")
  set (PYTHON_HARDCODED_LIBS    "/usr/local/Frameworks/Python.framework/Versions/3.5/lib/libpython3.5.dylib")
else()
  set (PYTHON_HARDCODED_INCLUDE 
	  "/usr/include/python3.4"
	  "/usr/include/python3.4m")
  set (PYTHON_HARDCODED_LIBS "/usr/lib/libpython3.4.so")
endif()
	
if (NOT PYTHON3_INCLUDE_DIRS OR NOT PYTHON3_LIBRARIES)
    find_package(PythonLibs 3 )
    if (NOT PYTHON_INCLUDE_DIRS OR NOT PYTHON_LIBRARIES)
	message ( WARNING "Python 3 paths haven't been found or supplied, trying hardcoded values" )
	if (EXISTS ${PYTHON_HARDCODED_INCLUDE})
	  set (PYTHON_INCLUDE_DIRS ${PYTHON_HARDCODED_INCLUDE})
	else()
	  message ( FATAL_ERROR "No Python 3 headers found" )
	endif()
	if (EXISTS ${PYTHON_HARDCODED_LIBS})
	  set (PYTHON_LIBRARIES ${PYTHON_HARDCODED_LIBS})
	else()
	  message ( FATAL_ERROR "No Python 3 libraries found" )
	endif()

endif()
else()
    set (PYTHON_INCLUDE_DIRS  ${PYTHON3_INCLUDE_DIRS})
    set (PYTHON_LIBRARIES ${PYTHON3_LIBRARIES})
endif()



#if(PYTHONLIBS_FOUND)
include_directories (
        ${PYTHON_INCLUDE_DIRS}
        ../../libtransform/)

link_directories (
    ${PYTHON_LIBRARIES}
    ${PROJECT_SOURCE_DIR}/../../lib
    )

set ( CMAKE_LIBRARY_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/../../bin/plugins )

set ( pythonplugin_HDRS
    pythonplugin_global.h
    pythontransform.h
    pythonmodules.h
    pythonplugin.h
    )

set ( pythonplugin_SRCS
    pythonmodules.cpp
    pythontransform.cpp
    pythonplugin.cpp
    )
set ( EXTERN_LIB transform )

add_library ( python3plugin MODULE ${pythonplugin_SRCS} )

target_link_libraries ( python3plugin Qt5::Widgets Qt5::Svg ${EXTERN_LIB} ${PYTHON_LIBRARIES} )

IF(UNIX)
    install (TARGETS python3plugin DESTINATION lib/pip3line/
        PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE
        GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)
ENDIF(UNIX)

#endif(PYTHONLIBS_FOUND)
