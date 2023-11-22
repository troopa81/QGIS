
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
