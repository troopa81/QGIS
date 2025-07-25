/***************************************************************************
    qgselevationprofile.h
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

#ifndef QGSELEVATIONPROFILE_H
#define QGSELEVATIONPROFILE_H

#include "qgis_core.h"
#include "qgsreadwritecontext.h"
#include "qgsgeometry.h"

#include <QDomElement>

/**
 * \ingroup core
 * \brief Elevation profile informations
 * \since QGIS 4.0
*/
class CORE_EXPORT QgsElevationProfile
{
    // TODO ?
    // Q_OBJECT

  public:

    /**
     * TODO
     */
    QgsElevationProfile( const QString &name = QString() );

    /**
     * Writes elevation profile details from a DOM element.
     * \param parentElement parent DOM element ('ElevationProfile' element)
     * \param document DOM document
     */
    bool writeXml( QDomElement &parentElement, QDomDocument &document ) const;

    /**
     * Restores elevation profile details from a DOM element.
     * \param element DOM node corresponding to item ('ElevationProfile' element)
     * \param document DOM document
     */
    bool readXml( const QDomElement &element, const QDomDocument &document );

    /**
     * TODO doc
     */
    void setProfileCurve( const QgsGeometry &curve, bool resetView );

    /**
     * Sets elevation profile name
     * \see name()
     * \since QGIS 4.0
     */
    void setName( const QString &name );

    /**
     * Returns elevation profile name
     * \see setName()
     * \since QGIS 4.0
     */
    QString name() const;


  private:

    QString mName;
    QgsGeometry mProfileCurve;

};

#endif
