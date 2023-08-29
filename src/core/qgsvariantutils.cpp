/***************************************************************************
    qgsvariantutils.h
    ------------------
    Date                 : January 2022
    Copyright            : (C) 2022 Nyall Dawson
    Email                : nyall dot dawson at gmail dot com
 ***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "qgsvariantutils.h"
#include "qgslogger.h"

#include <QDate>
#include <QTime>
#include <QDateTime>
#include <QBitArray>
#include <QRect>
#include <QLine>
#include <QUuid>
#include <QImage>
#include <QPixmap>
#include <QBrush>
#include <QColor>
#include <QBitmap>
#include <QIcon>
#include <QVector2D>
#include <QVector3D>
#include <QVector4D>
#include <QQuaternion>

QString QgsVariantUtils::typeToDisplayString( QMetaType::Type type, QMetaType::Type subType )
{
  switch ( type )
  {
    case QMetaType::UnknownType:
      break;
    case QMetaType::Bool:
      return QObject::tr( "Boolean" );
    case QMetaType::Int:
      return QObject::tr( "Integer (32 bit)" );
    case QMetaType::UInt:
      return QObject::tr( "Integer (unsigned 32 bit)" );
    case QMetaType::LongLong:
      return QObject::tr( "Integer (64 bit)" );
    case QMetaType::ULongLong:
      return QObject::tr( "Integer (unsigned 64 bit)" );
    case QMetaType::Double:
      return QObject::tr( "Decimal (double)" );
    case QMetaType::QChar:
      return QObject::tr( "Character" );
    case QMetaType::QVariantMap:
      return QObject::tr( "Map" );
    case QMetaType::QVariantList:
    {
      switch ( subType )
      {
        case QMetaType::Int:
          return QObject::tr( "Integer List" );
        case QMetaType::LongLong:
          return QObject::tr( "Integer (64 bit) List" );
        case QMetaType::Double:
          return QObject::tr( "Decimal (double) List" );
        default:
          return QObject::tr( "List" );
      }
    }
    case QMetaType::QString:
      return QObject::tr( "Text (string)" );
    case QMetaType::QStringList:
      return QObject::tr( "String List" );
    case QMetaType::QByteArray:
      return QObject::tr( "Binary Object (BLOB)" );
    case QMetaType::QBitArray:
      return QObject::tr( "Bit Array" );
    case QMetaType::QDate:
      return QObject::tr( "Date" );
    case QMetaType::QTime:
      return QObject::tr( "Time" );
    case QMetaType::QDateTime:
      return QObject::tr( "Date & Time" );
    case QMetaType::QUrl:
      return QObject::tr( "URL" );
    case QMetaType::QLocale:
      return QObject::tr( "Locale" );
    case QMetaType::QRect:
    case QMetaType::QRectF:
      return QObject::tr( "Rectangle" );
    case QMetaType::QSize:
    case QMetaType::QSizeF:
      return QObject::tr( "Size" );
    case QMetaType::QLine:
    case QMetaType::QLineF:
      return QObject::tr( "Line" );
    case QMetaType::QPoint:
    case QMetaType::QPointF:
      return QObject::tr( "Point" );
    case QMetaType::QRegularExpression:
      return QObject::tr( "Regular Expression" );
    case QMetaType::QVariantHash:
      return QObject::tr( "Hash" );
    case QMetaType::QEasingCurve:
      return QObject::tr( "Easing Curve" );
    case QMetaType::QUuid:
      return QObject::tr( "UUID" );
    case QMetaType::QModelIndex:
    case QMetaType::QPersistentModelIndex:
      return QObject::tr( "Model Index" );
    case QMetaType::QFont:
      return QObject::tr( "Font" );
    case QMetaType::QPixmap:
      return QObject::tr( "Pixmap" );
    case QMetaType::QBrush:
      return QObject::tr( "Brush" );
    case QMetaType::QColor:
      return QObject::tr( "Color" );
    case QMetaType::QPalette:
      return QObject::tr( "Palette" );
    case QMetaType::QImage:
      return QObject::tr( "Image" );
    case QMetaType::QPolygon:
    case QMetaType::QPolygonF:
      return QObject::tr( "Polygon" );
    case QMetaType::QRegion:
      return QObject::tr( "Region" );
    case QMetaType::QBitmap:
      return QObject::tr( "Bitmap" );
    case QMetaType::QCursor:
      return QObject::tr( "Cursor" );
    case QMetaType::QKeySequence:
      return QObject::tr( "Key Sequence" );
    case QMetaType::QPen:
      return QObject::tr( "Pen" );
    case QMetaType::QTextLength:
      return QObject::tr( "Text Length" );
    case QMetaType::QTextFormat:
      return QObject::tr( "Text Format" );
    case QMetaType::QMatrix4x4:
      return QObject::tr( "Matrix" );
    case QMetaType::QTransform:
      return QObject::tr( "Transform" );
    case QMetaType::QVector2D:
    case QMetaType::QVector3D:
    case QMetaType::QVector4D:
      return QObject::tr( "Vector" );
    case QMetaType::QQuaternion:
      return QObject::tr( "Quaternion" );
    case QMetaType::QIcon:
      return QObject::tr( "Icon" );
    case QMetaType::QSizePolicy:
      return QObject::tr( "Size Policy" );

    default:
      break;
  }
  return QString();
}

bool QgsVariantUtils::isNull( const QVariant &variant )
{
  if ( variant.isNull() || !variant.isValid() )
    return true;

  switch ( variant.userType() )
  {
    case QMetaType::UnknownType:
    case QMetaType::Bool:
    case QMetaType::Int:
    case QMetaType::UInt:
    case QMetaType::LongLong:
    case QMetaType::ULongLong:
    case QMetaType::Double:
    case QMetaType::QVariantMap:
    case QMetaType::QVariantList:
    case QMetaType::QStringList:
    case QMetaType::QUrl:
    case QMetaType::QLocale:
    case QMetaType::QRegularExpression:
    case QMetaType::QVariantHash:
    case QMetaType::QEasingCurve:
    case QMetaType::QModelIndex:
    case QMetaType::QPersistentModelIndex:

    // TODO there is some LastCoreType or LastGuiType but are they relevant
    case QMetaType::LastCoreType:

      return false;

    case QMetaType::QDate:
      if ( variant.toDate().isNull() )
      {
        QgsDebugError( QStringLiteral( "NULL QDateTime was stored in a QVariant -- stop it! Always use an invalid QVariant() instead." ) );
        return true;
      }
      return false;
    case QMetaType::QTime:
      if ( variant.toTime().isNull() )
      {
        QgsDebugError( QStringLiteral( "NULL QTime was stored in a QVariant -- stop it! Always use an invalid QVariant() instead." ) );
        return true;
      }
      return false;
    case QMetaType::QDateTime:
      if ( variant.toDate().isNull() )
      {
        QgsDebugError( QStringLiteral( "NULL QDate was stored in a QVariant -- stop it! Always use an invalid QVariant() instead." ) );
        return true;
      }
      return false;
    case QMetaType::QChar:
      if ( variant.toChar().isNull() )
      {
        QgsDebugError( QStringLiteral( "NULL QChar was stored in a QVariant -- stop it! Always use an invalid QVariant() instead." ) );
        return true;
      }
      return false;
    case QMetaType::QString:
      if ( variant.toString().isNull() )
      {
        QgsDebugError( QStringLiteral( "NULL QString was stored in a QVariant -- stop it! Always use an invalid QVariant() instead." ) );
        return true;
      }
      return false;
    case QMetaType::QByteArray:
      if ( variant.toByteArray().isNull() )
      {
        QgsDebugError( QStringLiteral( "NULL QByteArray was stored in a QVariant -- stop it! Always use an invalid QVariant() instead." ) );
        return true;
      }
      return false;
    case QMetaType::QBitArray:
      if ( variant.toBitArray().isNull() )
      {
        QgsDebugError( QStringLiteral( "NULL QBitArray was stored in a QVariant -- stop it! Always use an invalid QVariant() instead." ) );
        return true;
      }
      return false;
    case QMetaType::QRect:
      if ( variant.toRect().isNull() )
      {
        QgsDebugError( QStringLiteral( "NULL QRect was stored in a QVariant -- stop it! Always use an invalid QVariant() instead." ) );
        return true;
      }
      return false;
    case QMetaType::QRectF:
      if ( variant.toRectF().isNull() )
      {
        QgsDebugError( QStringLiteral( "NULL QRectF was stored in a QVariant -- stop it! Always use an invalid QVariant() instead." ) );
        return true;
      }
      return false;
    case QMetaType::QSize:
      if ( variant.toSize().isNull() )
      {
        QgsDebugError( QStringLiteral( "NULL QSize was stored in a QVariant -- stop it! Always use an invalid QVariant() instead." ) );
        return true;
      }
      return false;
    case QMetaType::QSizeF:
      if ( variant.toSizeF().isNull() )
      {
        QgsDebugError( QStringLiteral( "NULL QSizeF was stored in a QVariant -- stop it! Always use an invalid QVariant() instead." ) );
        return true;
      }
      return false;
    case QMetaType::QLine:
      if ( variant.toLine().isNull() )
      {
        QgsDebugError( QStringLiteral( "NULL QLine was stored in a QVariant -- stop it! Always use an invalid QVariant() instead." ) );
        return true;
      }
      return false;
    case QMetaType::QLineF:
      if ( variant.toLineF().isNull() )
      {
        QgsDebugError( QStringLiteral( "NULL QLineF was stored in a QVariant -- stop it! Always use an invalid QVariant() instead." ) );
        return true;
      }
      return false;
    case QMetaType::QPoint:
      if ( variant.toPoint().isNull() )
      {
        QgsDebugError( QStringLiteral( "NULL QPoint was stored in a QVariant -- stop it! Always use an invalid QVariant() instead." ) );
        return true;
      }
      return false;
    case QMetaType::QPointF:
      if ( variant.toPointF().isNull() )
      {
        QgsDebugError( QStringLiteral( "NULL QPointF was stored in a QVariant -- stop it! Always use an invalid QVariant() instead." ) );
        return true;
      }
      return false;
    case QMetaType::QUuid:
      if ( variant.toUuid().isNull() )
      {
        QgsDebugError( QStringLiteral( "NULL QUuid was stored in a QVariant -- stop it! Always use an invalid QVariant() instead." ) );
        return true;
      }
      return false;
    case QMetaType::QPixmap:
      if ( variant.value< QPixmap >().isNull() )
      {
        QgsDebugError( QStringLiteral( "NULL QPixmap was stored in a QVariant -- stop it! Always use an invalid QVariant() instead." ) );
        return true;
      }
      return false;
    case QMetaType::QImage:
      if ( variant.value< QImage >().isNull() )
      {
        QgsDebugError( QStringLiteral( "NULL QImage was stored in a QVariant -- stop it! Always use an invalid QVariant() instead." ) );
        return true;
      }
      return false;
    case QMetaType::QRegion:
      if ( variant.value< QRegion >().isNull() )
      {
        QgsDebugError( QStringLiteral( "NULL QRegion was stored in a QVariant -- stop it! Always use an invalid QVariant() instead." ) );
        return true;
      }
      return false;
    case QMetaType::QBitmap:
      if ( variant.value< QBitmap >().isNull() )
      {
        QgsDebugError( QStringLiteral( "NULL QBitmap was stored in a QVariant -- stop it! Always use an invalid QVariant() instead." ) );
        return true;
      }
      return false;
    case QMetaType::QIcon:
      if ( variant.value< QIcon >().isNull() )
      {
        QgsDebugError( QStringLiteral( "NULL QIcon was stored in a QVariant -- stop it! Always use an invalid QVariant() instead." ) );
        return true;
      }
      return false;
    case QMetaType::QVector2D:
      if ( variant.value< QVector2D >().isNull() )
      {
        QgsDebugError( QStringLiteral( "NULL QVector2D was stored in a QVariant -- stop it! Always use an invalid QVariant() instead." ) );
        return true;
      }
      return false;
    case QMetaType::QVector3D:
      if ( variant.value< QVector3D >().isNull() )
      {
        QgsDebugError( QStringLiteral( "NULL QVector3D was stored in a QVariant -- stop it! Always use an invalid QVariant() instead." ) );
        return true;
      }
      return false;
    case QMetaType::QVector4D:
      if ( variant.value< QVector4D >().isNull() )
      {
        QgsDebugError( QStringLiteral( "NULL QVector4D was stored in a QVariant -- stop it! Always use an invalid QVariant() instead." ) );
        return true;
      }
      return false;
    case QMetaType::QQuaternion:
      if ( variant.value< QQuaternion >().isNull() )
      {
        QgsDebugError( QStringLiteral( "NULL QQuaternion was stored in a QVariant -- stop it! Always use an invalid QVariant() instead." ) );
        return true;
      }
      return false;

    case QMetaType::QColor:
    case QMetaType::QFont:
    case QMetaType::QBrush:
    case QMetaType::QPolygon:
    case QMetaType::QPalette:
    case QMetaType::QCursor:
    case QMetaType::QKeySequence:
    case QMetaType::QPen:
    case QMetaType::QTextLength:
    case QMetaType::QPolygonF:
    case QMetaType::QTextFormat:
    case QMetaType::QTransform:
    case QMetaType::QMatrix4x4:
    case QMetaType::QSizePolicy:
      break;

    case QMetaType::User:
      break;

    default:
      break;
  }

  return false;
}
