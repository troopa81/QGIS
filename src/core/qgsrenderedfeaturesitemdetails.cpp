/***************************************************************************
    qgsrenderedfeaturesitemdetails.cpp
    ----------------
    copyright            : (C) 2023 by Julien Cabieces
    email                : julien dot cabieces at oslandia dot com
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "qgsrenderedfeaturesitemdetails.h"

QgsRenderedFeaturesItemDetails::QgsRenderedFeaturesItemDetails( const QString &layerId, RenderedFeatures renderedFeatures )
  : QgsRenderedItemDetails( layerId ),
    mRenderedFeatures( renderedFeatures )
{
}

QgsRenderedFeaturesItemDetails::RenderedFeatures QgsRenderedFeaturesItemDetails::renderedFeatures() const
{
  return mRenderedFeatures;
}
