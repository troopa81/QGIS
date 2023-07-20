/***************************************************************************
    qgsrenderedfeaturesitemdetails.h
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

#ifndef QGSRENDEREDFEATURESITEMDETAILS_H
#define QGSRENDEREDFEATURESITEMDETAILS_H

#include "qgis_core.h"
#include "qgis_sip.h"
#include "qgsrendereditemdetails.h"
#include "qgsfeatureid.h"
#include "qgsgeometry.h"

/**
 * \ingroup core
 * \brief Contains information about rendered features.
 * \since QGIS 3.34 TODO
 * TODO Shall we prefer QgsRenderedFeatures because it's mainly a struct of rendered feature used everywhere
 * not just a QgsRenderedItemDetails
 */
class CORE_EXPORT QgsRenderedFeaturesItemDetails : public QgsRenderedItemDetails
{
  public:

    struct RenderedFeature
    {
      RenderedFeature( QgsFeatureId featureId, QgsGeometry geom )
        : fid( featureId )
        , geometry( geom )
      {}

      QgsFeatureId fid;
      QgsGeometry geometry;
    };

    typedef QList<RenderedFeature> RenderedFeatures;

    /**
     * Constructor for QgsRenderedFeaturesItemDetails.
     */
    QgsRenderedFeaturesItemDetails( const QString &layerId, RenderedFeatures renderedFeatures );

    // TODO
// #ifdef SIP_RUN
//     SIP_PYOBJECT __repr__();
//     % MethodCode
//     QString str = QStringLiteral( "<QgsRenderedFeaturesItemDetails: %1 - %2>" ).arg( sipCpp->layerId(), sipCpp->itemId() );
//     sipRes = PyUnicode_FromString( str.toUtf8().constData() );
//     % End
// #endif

    // TODO complete
    // Returns rendered feature per layer
    // - empty if collecting feature is disabled
    // - not every feature would be collected if we reach the cache size parameters
    RenderedFeatures renderedFeatures() const;

  private:

    RenderedFeatures mRenderedFeatures;

};

#endif // QGSRENDEREDFEATURESITEMDETAILS_H
