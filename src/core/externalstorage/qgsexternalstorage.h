/***************************************************************************
  qgsexternalstorage.h
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

#ifndef QGSEXTERNALSTORAGE_H
#define QGSEXTERNALSTORAGE_H

#include "qgis_core.h"
#include "qgis_sip.h"
#include "qgstaskmanager.h"

#include <QString>
#include <QUrl>

class QgsExternalStorageFetchedContent;
class QgsExternalStorageStoredContent;

/**
 * \ingroup core
 * \brief Abstract interface for external storage - to be implemented by various backends
 * and registered in QgsExternalStorageRegistry.
 *
 * \since QGIS 3.20
 */
class CORE_EXPORT QgsExternalStorage
{
  public:

    /* #ifndef SIP_RUN */
    /*     typedef std::function< void ( const QString & ) > ErrorCallback; */
    /* #endif */

    virtual ~QgsExternalStorage() = default;

    /**
     * Unique identifier of the external storage type.
     */
    virtual QString type() const = 0;

    /**
     * Store file \a filePath to the \a url for this project external storage
     * Returns asynchronous task to store file. Signal QgsTask::taskCompleted() will be emmitted
     * whenever the upload is complete
     * TODO complete doc for auth
     * TODO const ?
     * TODO QUrl or QString ( QString is more general in case of Postgres LargeObject for instance)
     * TODO rename en store()
     */
    virtual QgsExternalStorageStoredContent *storeFile( const QString &filePath, const QUrl &url, const QString &authcfg = QString() ) = 0;

    /**
     * TODO Complete documentation
     * TODO const ?
     * TODO QUrl or QString ( QString is more general in case of Postgres LargeObject for instance)
     */
    virtual QgsExternalStorageFetchedContent *fetch( const QUrl &url, const QString &authcfg = QString() ) = 0;
};

// TODO doc
class CORE_EXPORT QgsExternalStorageOperation : public QObject
{
    Q_OBJECT

  public:

    // TODO same for storing? put in a common class?
    //! Status of fetched content
    enum ContentStatus
    {
      NotStarted, //!< No operation started
      OnGoing, //!< Operation in progress
      Finished, //!< Operation is finished and successful
      Failed, //!< Operation has failed
      Canceled, //!< Operation has been canceled
    };

    /**
     * Returns status of fetched content
     */
    ContentStatus status() const {return mStatus;}

    /**
     * Returns the potential error textual description if an error occured and status() returns Failed
     */
    const QString &errorString() const {return mErrorString;};

  public slots:

    virtual void cancel() = 0;

  signals:

    void errorOccured( const QString & );

    void progressChanged( double progress );

    void canceled();

  protected:

    ContentStatus mStatus = NotStarted;
    QString mErrorString;
};


// TODO doc
class CORE_EXPORT QgsExternalStorageFetchedContent : public QgsExternalStorageOperation
{
    Q_OBJECT

  public:

    /* // TODO really necessary ? */
    /* QgsExternalStorageFetchedContent() = default; */

    /* // TODO really necessary ? */
    /* ~QgsExternalStorageFetchedContent() = default; */

    // TODO doc
    virtual QString filePath() const = 0;

  signals:

    void fetched();
};

// TODO doc
class CORE_EXPORT QgsExternalStorageStoredContent : public QgsExternalStorageOperation
{
    Q_OBJECT

  public:

    /* // TODO really necessary ? */
    /* QgsExternalStorageFetchedContent() = default; */

    /* // TODO really necessary ? */
    /* ~QgsExternalStorageFetchedContent() = default; */

  signals:

    void stored();
};

#endif // QGSEXTERNALSTORAGE_H
