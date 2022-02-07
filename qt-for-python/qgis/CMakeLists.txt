add_subdirectory(core)
add_subdirectory(PyQt)

add_custom_target(pyqgis ALL)

set(PY_FILES __init__.py)
foreach(pyfile ${PY_FILES})
  add_custom_command(TARGET pyqgis
    POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "${QGIS_PYTHON_OUTPUT_DIRECTORY}"
    COMMAND ${CMAKE_COMMAND} -E copy ${pyfile} "${QGIS_PYTHON_OUTPUT_DIRECTORY}"
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    DEPENDS ${pyfile}
  )
  PY_COMPILE(pyutils "${QGIS_PYTHON_OUTPUT_DIRECTORY}/${pyfile}")
endforeach(pyfile)

install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/__init__.py DESTINATION ${Python3_SITELIB}/pyqgis_core)
