
// @snippet QgsFeature-__setitem__
QVariant cppValue = %CONVERTTOCPP[QVariant](_value);

qDebug() << "cppValue=" << cppValue;

bool rv = %CPPSELF->setAttribute( _i, cppValue );

if ( !rv )
{
  PyErr_SetString( PyExc_KeyError, QByteArray::number( _i ) );
  return -1;
}

return 0;
// @snippet QgsFeature-__setitem__

// @snippet qgsapplication-init
static void QgsApplicationConstructor(PyObject *self, PyObject *pyargv, bool GUIenabled, QgsApplicationWrapper **cptr)
{
    static int argc;
    static char **argv;
    PyObject *stringlist = PyTuple_GET_ITEM(pyargv, 0);
    if (Shiboken::listToArgcArgv(stringlist, &argc, &argv, "PySideApp")) {
        *cptr = new QgsApplicationWrapper(argc, argv, GUIenabled);
        Shiboken::Object::releaseOwnership(reinterpret_cast<SbkObject *>(self));
        PySide::registerCleanupFunction(&PySide::destroyQCoreApplication);
    }
}
// @snippet qgsapplication-init

// @snippet qgsapplication
QgsApplicationConstructor(%PYSELF, args, %2, &%0);
// @snippet qgsapplication

// @snippet QgsField-convertCompatible(QVariant&)
bool res;
QString errorMessage;

res = %CPPSELF->convertCompatible( %1, &errorMessage );

if ( !res )
{
  PyErr_SetString( PyExc_ValueError,
                   QString( "Value could not be converted to field type %1: %2" ).arg( QMetaType::typeName( %CPPSELF->type() ), errorMessage ).toUtf8().constData() );
  return nullptr;
}
else
{
  %PYARG_0 = %CONVERTTOPYTHON[bool](res);
}
// @snippet QgsField-convertCompatible(QVariant&)
