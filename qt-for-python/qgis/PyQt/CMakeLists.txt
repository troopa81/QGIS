set (QGIS_PYQT_DIR ${Python_SITEARCH}/qgis/PyQt)
set (QGIS_PYQTCOMPAT_OUTPUT_DIRECTORY ${QGIS_PYTHON_OUTPUT_DIRECTORY}/PyQt)

set(PYQT_COMPAT_FILES
  __init__.py
  QtWidgets.py
  QtCore.py
  QtGui.py
  QtPrintSupport.py
  QtWebKit.py
  QtWebKitWidgets.py
  QtNetwork.py
  QtXml.py
  QtSql.py
  QtTest.py
  QtSvg.py
  Qsci.py
  Qt.py
  sip.py
#  uic/__init__.py
#  uic/pyuic.py
#  uic/properties.py
)

add_custom_target(pyqtcompat ALL)
add_dependencies(pyqtcompat pyutils)

# this might be used to allow building with PySide6
set(PYSIDE_PREFIX PyQt5)

foreach(pyfile ${PYQT_COMPAT_FILES})
  get_filename_component(_dir ${pyfile} PATH)
  set(pyfile ${PYSIDE_PREFIX}/${pyfile})
  add_custom_command(TARGET pyqtcompat
    POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory ${QGIS_PYQTCOMPAT_OUTPUT_DIRECTORY}
    COMMAND ${CMAKE_COMMAND} -E copy ${pyfile} ${QGIS_PYQTCOMPAT_OUTPUT_DIRECTORY}
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    DEPENDS ${pyfile}
  )
  install(FILES ${pyfile} DESTINATION "${QGIS_PYQT_DIR}/${_dir}")
endforeach(pyfile)

PY_COMPILE(pyqtcompat "${PYTHON_OUTPUT_DIRECTORY}")
