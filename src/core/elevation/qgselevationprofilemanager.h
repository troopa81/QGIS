/***************************************************************************
    qgselevationprofilemanager.h
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

#ifndef QGSELEVATIONPROFILEMANAGER_H
#define QGSELEVATIONPROFILEMANAGER_H

#include "qgis_sip.h"
#include "qgselevationprofile.h"

/**
 * \ingroup core
 * \brief Manage all the opened elevation profile TODO improve brief
 * \since QGIS 4.0
*/
class CORE_EXPORT QgsElevationProfileManager
{
    /* Q_OBJECT TODO */

  public:

    /**
     * TODO
     */
    QgsElevationProfileManager();

    /**
     * TODO Don't need it if unique_ptr
     */
    ~QgsElevationProfileManager();

    /**
     * TODO
     */
    void addElevationProfile( QgsElevationProfile *elevationProfile SIP_TRANSFER );

    /**
     * TODO
     */
    void removeElevationProfile( QgsElevationProfile *elevationProfile );

    /**
     * TODO
     */
    void clear();

    /**
     * TODO
     */
    QList<QgsElevationProfile *> elevationProfiles() const;

    /**
     * Reads the manager's state from a DOM element, restoring all elevation profiles
     * present in the XML document.
     * \see writeXml()
     */
    bool readXml( const QDomElement &element, const QDomDocument &document );

    /**
     * Returns a DOM element representing the state of the manager.
     * \see readXml()
     */
    QDomElement writeXml( QDomDocument &document ) const;

  private:

    // TODO We could use unique_ptr but how we would return a list of pointer ?
    QList<QgsElevationProfile *> mElevationProfiles;

};

#endif
