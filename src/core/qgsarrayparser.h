/***************************************************************************
    qgsjsonutils.h
     -------------
    Date                 : May 206
    Copyright            : (C) 2016 Nyall Dawson
    Email                : nyall dot dawson at gmail dot com
 ***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef QGSARRAYPARSER_H
#define QGSARRAYPARSER_H

#include "qgis_core.h"
#include "qgsmessagelog.h"


/**
 * \ingroup core
 * \class QgsArrayParser
 * \brief TODO Helper utilities for working with JSON and GeoJSON conversions.
 * \since QGIS 3.10
 */

class CORE_EXPORT QgsArrayParser
{
  public:

  static void jumpSpace( const QString &txt, int &i )
{
  while ( i < txt.length() && txt.at( i ).isSpace() )
    ++i;
}

static QString getNextString( const QString &txt, int &i, const QString &sep )
{
  jumpSpace( txt, i );
  QString cur = txt.mid( i );
  if ( cur.startsWith( '"' ) )
  {
    QRegExp stringRe( "^\"((?:\\\\.|[^\"\\\\])*)\".*" );
    if ( !stringRe.exactMatch( cur ) )
    {
      QgsMessageLog::logMessage( QObject::tr( "Cannot find end of double quoted string: %1" ).arg( txt ), QObject::QObject::tr( "PostGIS" ) );
      return QString();
    }
    i += stringRe.cap( 1 ).length() + 2;
    jumpSpace( txt, i );
    if ( !txt.midRef( i ).startsWith( sep ) && i < txt.length() )
    {
      QgsMessageLog::logMessage( QObject::QObject::tr( "Cannot find separator: %1" ).arg( txt.mid( i ) ), QObject::QObject::tr( "PostGIS" ) );
      return QString();
    }
    i += sep.length();
    return stringRe.cap( 1 ).replace( QLatin1String( "\\\"" ), QLatin1String( "\"" ) ).replace( QLatin1String( "\\\\" ), QLatin1String( "\\" ) );
  }
  else
  {
    int sepPos = cur.indexOf( sep );
    if ( sepPos < 0 )
    {
      i += cur.length();
      return cur.trimmed();
    }
    i += sepPos + sep.length();
    return cur.left( sepPos ).trimmed();
  }
}


static QStringList parseArray( const QString &txt )
{
  if ( !txt.startsWith( '{' ) || !txt.endsWith( '}' ) )
  {
    if ( !txt.isEmpty() )
      QgsMessageLog::logMessage( QObject::QObject::tr( "Error parsing array, missing curly braces: %1" ).arg( txt ), QObject::QObject::tr( "PostGIS" ) );
    return QStringList();
  }
  QString inner = txt.mid( 1, txt.length() - 2 );
  if ( inner.startsWith( "{" ) )
    return parseMultidimensionalArray( inner );
  else
    return parseStringArray( inner );

  /* if ( ( type == QVariant::StringList || type == QVariant::List ) && inner.startsWith( "{" ) ) */
  /*   return parseMultidimensionalArray( inner ); */
  /* else if ( type == QVariant::StringList ) */
  /*   return parseStringArray( inner ); */
  /* else */
  /*   return parseOtherArray( inner, subType, typeName ); */

}

/* static QVariant parseOtherArray( const QString &txt, QVariant::Type subType, const QString &typeName ) */
/* { */
/*   int i = 0; */
/*   QVariantList result; */
/*   while ( i < txt.length() ) */
/*   { */
/*     const QString value = getNextString( txt, i, QStringLiteral( "," ) ); */
/*     if ( value.isNull() ) */
/*     { */
/*       QgsMessageLog::logMessage( QObject::QObject::tr( "Error parsing array: %1" ).arg( txt ), QObject::QObject::tr( "PostGIS" ) ); */
/*       break; */
/*     } */
/*     result.append( QgsPostgresProvider::convertValue( subType, QVariant::Invalid, value, typeName ) ); */
/*   } */
/*   return result; */
/* } */

static QStringList parseStringArray( const QString &txt )
{
  int i = 0;
  QStringList result;
  while ( i < txt.length() )
  {
    const QString value = getNextString( txt, i, QStringLiteral( "," ) );
    if ( value.isNull() )
    {
      QgsMessageLog::logMessage( QObject::QObject::tr( "Error parsing array: %1" ).arg( txt ), QObject::QObject::tr( "PostGIS" ) );
      break;
    }
    result.append( value );
  }
  return result;
}

static QStringList parseMultidimensionalArray( const QString &txt )
{
  QStringList result;
  if ( !txt.startsWith( '{' ) || !txt.endsWith( '}' ) )
  {
    QgsMessageLog::logMessage( QObject::QObject::tr( "Error parsing array, missing curly braces: %1" ).arg( txt ), QObject::QObject::tr( "PostGIS" ) );
    return result;
  }

  QStringList values;
  QString text = txt;
  while ( !text.isEmpty() )
  {
    bool escaped = false;
    int openedBrackets = 1;
    int i = 0;
    while ( i < text.length()  && openedBrackets > 0 )
    {
      ++i;

      if ( text.at( i ) == '}' && !escaped ) openedBrackets--;
      else if ( text.at( i ) == '{' && !escaped ) openedBrackets++;

      escaped = !escaped ? text.at( i ) == '\\' : false;
    }

    values.append( text.left( ++i ) );
    i = text.indexOf( ',', i );
    i = i > 0 ? text.indexOf( '{', i ) : -1;
    if ( i == -1 )
      break;

    text = text.mid( i );
  }
  return values;

}

};

#endif // QGSJSONUTILS_H
