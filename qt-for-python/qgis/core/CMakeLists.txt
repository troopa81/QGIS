
# call typesystem_generator to generate binding

# TODO Where does qgis_core_global.h comes from ? we need to generate it
set(TYPESYSTEM_FILE "${CMAKE_SOURCE_DIR}/qt-for-python/qgis/core/typesystem_generated.xml")
set(SNIPPET_FILE "${CMAKE_SOURCE_DIR}/qt-for-python/qgis/core/qgis_core.cpp")
set(CLASSBLOCKLIST_FILE "${CMAKE_SOURCE_DIR}/qt-for-python/qgis/core/classBlockList.txt")

set(TYPESYSTEM_GENERATOR_ARGS "")
get_target_property(INCLUDE_DIRS qgis_core INCLUDE_DIRECTORIES)
list(TRANSFORM INCLUDE_DIRS PREPEND "-I")

add_custom_target(update_pyside_bindings
    COMMAND $<TARGET_FILE:typesystem_generator> --output-file="${TYPESYSTEM_FILE}" --snippet-file=${SNIPPET_FILE} --class-block-list-file ${CLASSBLOCKLIST_FILE} -- ${INCLUDE_DIRS} ${CMAKE_CURRENT_BINARY_DIR}/qgis_core_global.h
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    DEPENDS typesystem_generator
  )

# TODO restart this code if typesystem has been modified
# Get src code from typesystem file
set(pyqgis_core_SRC

  # global module wrapper
  ${CMAKE_CURRENT_BINARY_DIR}/core/core_module_wrapper.cpp
  ${CMAKE_CURRENT_BINARY_DIR}/core/core_python.h
)

file(STRINGS ${TYPESYSTEM_FILE} TYPESYSTEM_CONTENT)

# Parse typesystem file to generate the sources files to build
list(FILTER TYPESYSTEM_CONTENT INCLUDE REGEX "namespace-type|value-type|object-type")
foreach(LINE ${TYPESYSTEM_CONTENT})

  string(REGEX MATCH "</namespace-type|</value-type|</object-type" END_ITEM ${LINE} )
  if (END_ITEM)
    list(POP_BACK TS_ITEMS _)
  else ()
    string(REGEX MATCH "name=\"([^\"]*)\"" _ ${LINE} )
    set(ITEM ${CMAKE_MATCH_1})
    string(TOLOWER ${ITEM} ITEM)

    list(APPEND TS_ITEMS ${ITEM})

    list(JOIN TS_ITEMS "_" ITEM_NAME)

    string(REGEX MATCH ".*/>" END_ITEM ${LINE} )
    if (END_ITEM)
      list(POP_BACK TS_ITEMS _)
    endif()

    list(APPEND pyqgis_core_SRC
      ${CMAKE_CURRENT_BINARY_DIR}/core/${ITEM_NAME}_wrapper.cpp
      ${CMAKE_CURRENT_BINARY_DIR}/core/${ITEM_NAME}_wrapper.h)
  endif ()
endforeach()

# The includes are needed to parse and build classes specified in our typesystem
set(pyqgis_core_include_paths
  $<JOIN:$<TARGET_PROPERTY:qgis_core,INTERFACE_INCLUDE_DIRECTORIES>,${PATH_SEP}>
  ${CMAKE_BINARY_DIR}
)

# Set list of paths where shiboken should look for typesystem
set(pyqgis_core_typesystem_paths
    # PySide path, this variable was exposed by FindPySide2.cmake
    ${PYSIDE_TYPESYSTEMS}
)

# Include flags/path that will be set in 'target_include_directories'
set(pyqgis_core_target_include_directories
    ${Python_INCLUDE_DIRS}
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_BINARY_DIR}
)


# Libraries necessary to link the target for the command 'target_link_libraries'
if (BUILD_WITH_QT6)
  set(pyqgis_core_target_link_libraries
    qgis_core
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Xml
    ${Python_LIBRARIES}
  )
else()
  set(pyqgis_core_target_link_libraries
    qgis_core
    Qt5::Core
    Qt5::Gui
    Qt5::Widgets
    Qt5::Xml
    ${Python_LIBRARIES}
  )
endif()

# Dependencies - changes on these files should trigger new bindings
set(pyqgis_core_DEPENDS
  ${TYPESYSTEM_FILE}
  ${SNIPPET_FILE}
)

if (CMAKE_CXX_COMPILER_ID MATCHES "GNU")
  set_source_files_properties(
    ${pyqgis_core_SRC}
    PROPERTIES COMPILE_FLAGS "-Wno-missing-field-initializers -Wno-cast-function-type -Wno-unused-parameter")
endif()

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/qgis_core_global.h ${CMAKE_CURRENT_BINARY_DIR}/qgis_core_global.h COPYONLY)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/typesystem_qgis_core.xml.in ${CMAKE_CURRENT_BINARY_DIR}/typesystem_qgis_core.xml)

CREATE_PYTHON_BINDINGS(
    "core"
    "${pyqgis_core_typesystem_paths}"
    "${pyqgis_core_include_paths}"
    "${pyqgis_core_SRC}"
    "${pyqgis_core_target_include_directories}"
    "${pyqgis_core_target_link_libraries}"
    ${CMAKE_CURRENT_BINARY_DIR}/qgis_core_global.h
    ${CMAKE_CURRENT_BINARY_DIR}/typesystem_qgis_core.xml
    "${pyqgis_core_DEPENDS}"
    ${QGIS_PYTHON_OUTPUT_DIRECTORY}/core
)

add_dependencies(pycore pyqgis)

set(PY_FILES __init__.py)
foreach(pyfile ${PY_FILES})
  add_custom_command(TARGET pycore
    POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "${QGIS_PYTHON_OUTPUT_DIRECTORY}/core"
    COMMAND ${CMAKE_COMMAND} -E copy ${pyfile} "${QGIS_PYTHON_OUTPUT_DIRECTORY}/core"
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    DEPENDS ${pyfile}
  )
  PY_COMPILE(pyutils "${QGIS_PYTHON_OUTPUT_DIRECTORY}/${pyfile}")
endforeach(pyfile)
