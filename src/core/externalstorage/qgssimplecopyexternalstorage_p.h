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

#ifndef QGSSIMPLECOPYEXTERNALSTORAGE_H
#define QGSSIMPLECOPYEXTERNALSTORAGE_H

#include "externalstorage/qgsexternalstorage.h"
#include "qgis_core.h"
#include "qgis_sip.h"
#include "qgstaskmanager.h"

#include <QPointer>

///@cond PRIVATE
#define SIP_NO_FILE

/**
 * \ingroup core
 * \brief External storage implementation that does a simple filesystem copy when storing
 *
 * \since QGIS 3.22
 */
class CORE_EXPORT QgsSimpleCopyExternalStorage : public QgsExternalStorage
{
  public:

    QString type() const override;

    QgsExternalStorageStoredContent *store( const QString &filePath, const QUrl &url, const QString &authcfg = QString() ) override;

    QgsExternalStorageFetchedContent *fetch( const QUrl &url, const QString &authConfig = QString() ) override;
};

// TODO doc
class QgsCopyFileTask : public QgsTask
{
    Q_OBJECT

  public:

    QgsCopyFileTask( const QString &source, const QString &destination );

    bool run() override;

    const QString &errorString() const;

  private:

    QString mSource;
    QString mDestination;
    QString mErrorString;
};

// TODO doc
class QgsSimpleCopyExternalStorageStoredContent  : public QgsExternalStorageStoredContent
{
    Q_OBJECT

  public:

    QgsSimpleCopyExternalStorageStoredContent( const QString &filePath, const QUrl &url, const QString &authcfg = QString() );

    void cancel() override;

  private:

    QPointer<QgsCopyFileTask> mCopyTask;

};

// TODO doc
class QgsSimpleCopyExternalStorageFetchedContent : public QgsExternalStorageFetchedContent
{
    Q_OBJECT

  public:

    QgsSimpleCopyExternalStorageFetchedContent( const QString &filePath );

    QString filePath() const override;

  private:

    QString mFilePath;
};


#endif // QGSSIMPLECOPYEXTERNALSTORAGE_H
