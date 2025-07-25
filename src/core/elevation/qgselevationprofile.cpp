/***************************************************************************
    qgselevationprofile.cpp
    ---------------------
    begin                : 2025/07/24
    copyright            : (C) 2025 by Julien Cabieces
    email                : julien dot cabieces at oslandia dot com
 ***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "qgselevationprofile.h"

QgsElevationProfile::QgsElevationProfile( const QString &name )
  : mName( name )
{
}

bool QgsElevationProfile::writeXml( QDomElement &parentElement, QDomDocument &document ) const
{
  QDomElement element = document.createElement( QStringLiteral( "ElevationProfile" ) );
  element.setAttribute( QStringLiteral( "name" ), name() );

  parentElement.appendChild( element );

  return true;
}

bool QgsElevationProfile::readXml( const QDomElement &element, const QDomDocument & )
{
  if ( element.nodeName() != QLatin1String( "ElevationProfile" ) )
  {
    return false;
  }

  mName = element.attribute( QStringLiteral( "name" ) );

  return true;
}

void QgsElevationProfile::setProfileCurve( const QgsGeometry &curve, bool resetView )
{
  Q_UNUSED( resetView );

  // TODO dirty project
  mProfileCurve = curve;
}

void QgsElevationProfile::setName( const QString &name )
{
  if ( mName != name )
  {
    mName = name;
    // TODO dirty project
  }
}

QString QgsElevationProfile::name() const
{
  return mName;
}
