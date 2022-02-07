
set(_warnings "")
string(REPLACE "-Werror" " " CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS})
message(${CMAKE_CXX_FLAGS})



set(PYTHON_OUTPUT_DIRECTORY ${QGIS_OUTPUT_DIRECTORY}/qt-for-python)
set (QGIS_PYTHON_OUTPUT_DIRECTORY ${PYTHON_OUTPUT_DIRECTORY}/qgis)

file (MAKE_DIRECTORY ${QGIS_PYTHON_OUTPUT_DIRECTORY})
set (CMAKE_RUNTIME_OUTPUT_DIRECTORY ${QGIS_PYTHON_OUTPUT_DIRECTORY})
set (CMAKE_LIBRARY_OUTPUT_DIRECTORY ${QGIS_PYTHON_OUTPUT_DIRECTORY})


# Macro to byte-compile a target's staged Python resource(s)
macro(PY_COMPILE TARGET_NAME RESOURCE_PATHS)
  if(WITH_PY_COMPILE)
    add_custom_command(TARGET ${TARGET_NAME}
      POST_BUILD
      COMMAND ${Python_EXECUTABLE} -m compileall -q ${RESOURCE_PATHS}
      WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
      COMMENT "Byte-compiling staged resource..."
    )
  endif()
endmacro(PY_COMPILE)

add_custom_target(pyutils ALL)
install(FILES ${PY_FILES} DESTINATION "${QGIS_PYTHON_DIR}")

# stage to output to make available when QGIS is run from build directory
foreach(pyfile ${PY_FILES})
  add_custom_command(TARGET pyutils
    POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "${QGIS_PYTHON_OUTPUT_DIRECTORY}"
    COMMAND ${CMAKE_COMMAND} -E copy ${pyfile} "${QGIS_PYTHON_OUTPUT_DIRECTORY}"
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    DEPENDS ${pyfile}
  )
  PY_COMPILE(pyutils "${QGIS_PYTHON_OUTPUT_DIRECTORY}/${pyfile}")
endforeach(pyfile)





if (WITH_QT6)
    find_package(Shiboken6 REQUIRED)
    find_package(PySide6 ${Qt5Widgets_VERSION} EXACT REQUIRED)
    include(PySide6ModuleBuild)
    set(PYSIDE_MAJOR_VERSION "6")
else()
    find_package(Shiboken2 REQUIRED)
    find_package(PySide2 ${Qt5Widgets_VERSION} EXACT REQUIRED)
    include(PySide2ModuleBuild)
    set(PYSIDE_MAJOR_VERSION "2")
endif()

add_subdirectory(qgis)
