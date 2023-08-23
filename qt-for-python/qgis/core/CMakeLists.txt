
set(pyqgis_core_OBJECT_CLASSES
  QgsEditorWidgetSetup
  QgsFieldModel
  QgsFieldProxyModel
  QgsFeedback
  QgsRenderContext
  QgsFeatureSource
  QgsMapLayerRenderer
  QgsTemporalRangeObject
  QgsExpressionContextGenerator
  QgsExpressionContextScopeGenerator
  QgsDataProvider
  QgsProjectTranslator
  QgsFeatureSink
  QgsMessageLog
  QgsRasterInterface
  QgsRasterBlock
)

set(pyqgis_core_VALUE_CLASSES
    QgsAttributes
    QgsFeature
    QgsExpressionContext
    QgsFeatureIterator
    QgsFeatureRequest
    QgsCoordinateTransformContext
    QgsReadWriteContext
    QgsAction
    QgsRectangle
    QgsDefaultValue
    QgsRenderChecker
    QgsPointXY
)

# Auto-Generate files - every class will have a cpp and h file
set(pyqgis_core_SRC
    # global module wrapper
    ${CMAKE_CURRENT_BINARY_DIR}/core/core_module_wrapper.cpp
    ${CMAKE_CURRENT_BINARY_DIR}/core/core_python.h

    # ${CMAKE_CURRENT_BINARY_DIR}/core/qgswkbtypes_wrapper.h
    # ${CMAKE_CURRENT_BINARY_DIR}/core/qgswkbtypes_wrapper.cpp

    # ${CMAKE_CURRENT_BINARY_DIR}/core/qgsgeometry_wrapper.h
    # ${CMAKE_CURRENT_BINARY_DIR}/core/qgsgeometry_wrapper.cpp

    # ${CMAKE_CURRENT_BINARY_DIR}/core/qgsunittypes_wrapper.h
    # ${CMAKE_CURRENT_BINARY_DIR}/core/qgsunittypes_wrapper.cpp

    # ${CMAKE_CURRENT_BINARY_DIR}/core/qgsexpressioncontextscope_wrapper.h
    # ${CMAKE_CURRENT_BINARY_DIR}/core/qgsexpressioncontextscope_wrapper.cpp

    # ${CMAKE_CURRENT_BINARY_DIR}/core/qgsmaplayer_wrapper.h
    # ${CMAKE_CURRENT_BINARY_DIR}/core/qgsmaplayer_wrapper.cpp

    # ${CMAKE_CURRENT_BINARY_DIR}/core/qgsproject_wrapper.h
    # ${CMAKE_CURRENT_BINARY_DIR}/core/qgsproject_wrapper.cpp

    ${CMAKE_CURRENT_BINARY_DIR}/core/qgsfield_wrapper.h
    ${CMAKE_CURRENT_BINARY_DIR}/core/qgsfield_wrapper.cpp

    # ${CMAKE_CURRENT_BINARY_DIR}/core/qgsfields_wrapper.h
    # ${CMAKE_CURRENT_BINARY_DIR}/core/qgsfields_wrapper.cpp

    # ${CMAKE_CURRENT_BINARY_DIR}/core/qgsfieldconstraints_wrapper.h
    # ${CMAKE_CURRENT_BINARY_DIR}/core/qgsfieldconstraints_wrapper.cpp

    # ${CMAKE_CURRENT_BINARY_DIR}/core/qgscoordinatereferencesystem_wrapper.h
    # ${CMAKE_CURRENT_BINARY_DIR}/core/qgscoordinatereferencesystem_wrapper.cpp

    # ${CMAKE_CURRENT_BINARY_DIR}/core/qgsabstractfeaturesource_wrapper.h
    # ${CMAKE_CURRENT_BINARY_DIR}/core/qgsabstractfeaturesource_wrapper.cpp

    # ${CMAKE_CURRENT_BINARY_DIR}/core/qgsapplication_wrapper.h
    # ${CMAKE_CURRENT_BINARY_DIR}/core/qgsapplication_wrapper.cpp

    # ${CMAKE_CURRENT_BINARY_DIR}/core/qgsvectorlayer_wrapper.h
    # ${CMAKE_CURRENT_BINARY_DIR}/core/qgsvectorlayer_wrapper.cpp

    # ${CMAKE_CURRENT_BINARY_DIR}/core/qgsvectordataprovider_wrapper.h
    # ${CMAKE_CURRENT_BINARY_DIR}/core/qgsvectordataprovider_wrapper.cpp

    # ${CMAKE_CURRENT_BINARY_DIR}/core/qgsrasterlayer_wrapper.h
    # ${CMAKE_CURRENT_BINARY_DIR}/core/qgsrasterlayer_wrapper.cpp

    # ${CMAKE_CURRENT_BINARY_DIR}/core/qgsrasterdataprovider_wrapper.h
    # ${CMAKE_CURRENT_BINARY_DIR}/core/qgsrasterdataprovider_wrapper.cpp

    # ${CMAKE_CURRENT_BINARY_DIR}/core/qgis_wrapper.h
    # ${CMAKE_CURRENT_BINARY_DIR}/core/qgis_wrapper.cpp

    # ${CMAKE_CURRENT_BINARY_DIR}/core/qgsrasterblockfeedback_wrapper.h
    # ${CMAKE_CURRENT_BINARY_DIR}/core/qgsrasterblockfeedback_wrapper.cpp
)

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
#    ${CMAKE_SOURCE_DIR}/src/core/geometry/qgswkbtypes.h
)

# set(TYPESYSTEM_CONTENT "")
# set(GLOBAL_HEADER_CONTENT "")
# foreach(class ${pyqgis_core_OBJECT_CLASSES})
#     string(TOLOWER ${class} class_lower)
# #    set(pyqgis_core_DEPENDS "${pyqgis_core_DEPENDS} ${CMAKE_SOURCE_DIR}/src/core/${class_lower}.h")
#     list(APPEND pyqgis_core_SRC "${CMAKE_CURRENT_BINARY_DIR}/core/${class_lower}_wrapper.h" "${CMAKE_CURRENT_BINARY_DIR}/core/${class_lower}_wrapper.cpp")
#     set(GLOBAL_HEADER_CONTENT "${GLOBAL_HEADER_CONTENT}\n#include <${class_lower}.h>")
#     set(TYPESYSTEM_CONTENT "${TYPESYSTEM_CONTENT}\n    <object-type name=\"${class}\"></object-type>")
# endforeach()
# set(TYPESYSTEM_CONTENT "${TYPESYSTEM_CONTENT}\n\n")
# foreach(class ${pyqgis_core_VALUE_CLASSES})
#     string(TOLOWER ${class} class_lower)
# #    set(pyqgis_core_DEPENDS "${pyqgis_core_DEPENDS} ${CMAKE_SOURCE_DIR}/src/core/${class_lower}.h")
#     list(APPEND pyqgis_core_SRC "${CMAKE_CURRENT_BINARY_DIR}/core/${class_lower}_wrapper.h" "${CMAKE_CURRENT_BINARY_DIR}/core/${class_lower}_wrapper.cpp")
#     set(GLOBAL_HEADER_CONTENT "${GLOBAL_HEADER_CONTENT}\n#include <${class_lower}.h>")
#     set(TYPESYSTEM_CONTENT "${TYPESYSTEM_CONTENT}\n    <value-type name=\"${class}\"></value-type>")
# endforeach()

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
