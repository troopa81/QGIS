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
 * \brief External storage implementation which simply copy the given resource
 * on a given directory file path.
 *
 * \since QGIS 3.22
 */
class CORE_EXPORT QgsSimpleCopyExternalStorage : public QgsExternalStorage
{
  public:

    QString type() const override;

    QgsExternalStorageStoredContent *store( const QString &filePath, const QString &url, const QString &authcfg = QString() ) const override;

    QgsExternalStorageFetchedContent *fetch( const QString &url, const QString &authConfig = QString() ) const override;
};

// TODO doc
class QgsCopyFileTask : public QgsTask
{
    Q_OBJECT

  public:

    QgsCopyFileTask( const QString &source, const QString &destination );

    bool run() override;

    const QString &errorString() const;

    /**
     * It could be different from the original one. If original destination was a directory
     * the returned destination is now the absolute file path of the copied file
     */
    const QString &destination() const;

  private:

    QString mSource;
    QString mDestination;
    QString mErrorString;
};

/**
 * \ingroup core
 * \brief Class for Simple copy stored content
 *
 * \since QGIS 3.22
 */
class QgsSimpleCopyExternalStorageStoredContent  : public QgsExternalStorageStoredContent
{
    Q_OBJECT

  public:

    QgsSimpleCopyExternalStorageStoredContent( const QString &filePath, const QString &url, const QString &authcfg = QString() );

    void cancel() override;

    QString url() const override;

  private:

    QPointer<QgsCopyFileTask> mCopyTask;
    QString mUrl;
};

/**
 * \ingroup core
 * \brief Class for Simple copy fetched content
 *
 * \since QGIS 3.22
 */
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
