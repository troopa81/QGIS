/***************************************************************************
    qgselevationprofilemanager.cpp
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

#include "qgselevationprofilemanager.h"

QgsElevationProfileManager::QgsElevationProfileManager()
{
}

QgsElevationProfileManager::~QgsElevationProfileManager()
{
  clear();
}

void QgsElevationProfileManager::addElevationProfile( QgsElevationProfile *elevationProfile )
{
  mElevationProfiles.append( elevationProfile );
}

void QgsElevationProfileManager::removeElevationProfile( QgsElevationProfile *elevationProfile )
{
  mElevationProfiles.removeAll( elevationProfile );
  delete elevationProfile;
}

void QgsElevationProfileManager::clear()
{
  qDeleteAll( mElevationProfiles );
  mElevationProfiles.clear();
}

QList<QgsElevationProfile *> QgsElevationProfileManager::elevationProfiles() const
{
  return mElevationProfiles;
}

bool QgsElevationProfileManager::readXml( const QDomElement &element, const QDomDocument &document )
{
  clear();

  QDomElement elevationProfilesElem = element;
  // TODO use constant for tagName
  if ( element.tagName() != QLatin1String( "ElevationProfiles" ) )
  {
    elevationProfilesElem = element.firstChildElement( QStringLiteral( "ElevationProfiles" ) );
  }

  QDomNodeList elevationProfileNodes = element.elementsByTagName( QStringLiteral( "ElevationProfile" ) );
  for ( int i = 0; i < elevationProfileNodes.size(); ++i )
  {
    const QDomElement elevationProfileElement = elevationProfileNodes.at( i ).toElement();

    auto elevationProfile = std::make_unique<QgsElevationProfile>();
    elevationProfile->readXml( elevationProfileElement, document );
    addElevationProfile( elevationProfile.release() );
  }

  return true;
}

QDomElement QgsElevationProfileManager::writeXml( QDomDocument &document ) const
{
  QDomElement elevationProfilesElem = document.createElement( QStringLiteral( "ElevationProfiles" ) );

  for ( const QgsElevationProfile *elevationProfile : mElevationProfiles )
  {
    elevationProfile->writeXml( elevationProfilesElem, document );
  }

  return elevationProfilesElem;
}
