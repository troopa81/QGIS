/***************************************************************************
  qgswebdavexternalstorage.h
  --------------------------------------
  Date                 : March 2021
  Copyright            : (C) 2021 by Julien Cabieces
  Email                : julien dot cabieces at oslandia dot com
 ***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef QGSWEBDAVEXTERNALSTORAGE_H
#define QGSWEBDAVEXTERNALSTORAGE_H

#include "qgis_core.h"
#include "qgis_sip.h"

#include "externalstorage/qgsexternalstorage.h"

#include <QPointer>

class QgsNetworkContentFetcherTask;
class QgsFetchedContent;

///@cond PRIVATE
#define SIP_NO_FILE

/**
 * \ingroup core
 * \brief External storage implementation using the protocol WebDAV.
 *
 * \since QGIS 3.22
 */
class CORE_EXPORT QgsWebDAVExternalStorage : public QgsExternalStorage
{
  public:

    QString type() const override;

    QgsExternalStorageStoredContent *store( const QString &filePath, const QUrl &url, const QString &authcfg = QString() ) override;

    QgsExternalStorageFetchedContent *fetch( const QUrl &url, const QString &authConfig = QString() ) override;
};

// TODO doc
class QgsWebDAVExternalStorageStoredContent  : public QgsExternalStorageStoredContent
{
  Q_OBJECT

  public:

    QgsWebDAVExternalStorageStoredContent( const QString &filePath, const QUrl &url, const QString &authcfg = QString() );

    void cancel() override;

  private:

    QPointer<QgsNetworkContentFetcherTask> mUploadTask;
};

// TODO doc
class QgsWebDAVExternalStorageFetchedContent : public QgsExternalStorageFetchedContent
{
  Q_OBJECT

  public:

    QgsWebDAVExternalStorageFetchedContent( QgsFetchedContent *fetchedContent );

    QString filePath() const override;

    void cancel() override;

  private slots:

    void onFetched();

  private:

    QgsFetchedContent *mFetchedContent = nullptr;
};


#endif // QGSWEBDAVEXTERNALSTORAGE_H
