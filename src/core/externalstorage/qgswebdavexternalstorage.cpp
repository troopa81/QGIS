/***************************************************************************
  qgswebdavexternalstorage.cpp
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

#include "qgswebdavexternalstorage.h"

#include "qgsnetworkcontentfetcherregistry.h"
#include "qgsnetworkcontentfetchertask.h"
#include "qgsapplication.h"

#include <QFile>



class QgsWebDAVExternalStorageStoreTask : public QgsExternalStorageTask
{

    // TODO Q_OBJECT rajouter ? Apparemment il faut rajouter aussi le fichier .moc (fait par ailleur mais peu souvent)

  public:

    QgsWebDAVExternalStorageStoreTask( const QString &filePath, const QUrl &url, const QString &authcfg = QString() )
    // TODO complete description with path? and url?
      : QgsExternalStorageTask( tr( "WebDAV Store task" ) )
    {
      QgsNetworkContentFetcherTask *uploadTask = new QgsNetworkContentFetcherTask( url, authcfg );
      uploadTask->setMode( "PUT" );

      QFile *f = new QFile( filePath );
      f->open( QIODevice::ReadOnly );
      uploadTask->setContent( f );

      addSubTask( uploadTask );

      connect( uploadTask, &QgsNetworkContentFetcherTask::errorOccurred, [ = ]( QNetworkReply::NetworkError code, const QString & errorMsg )
      {
        // TODO do we map some error code to some enum error code or not?
        emit errorOccured( errorMsg );
      } );
    }

  protected:

    bool run()
    {
      return true;
    }
};


class QgsWebDAVExternalStorageFetchedContent : public QgsExternalStorageFetchedContent
{
  public:

    QgsWebDAVExternalStorageFetchedContent( QgsFetchedContent *fetchedContent );

    QString filePath() const override;

  private slots:

    void onFetched();

  private:

    QgsFetchedContent *mFetchedContent = nullptr;
};


QgsWebDAVExternalStorageFetchedContent::QgsWebDAVExternalStorageFetchedContent( QgsFetchedContent *fetchedContent )
  : mFetchedContent( fetchedContent )
{
  connect( mFetchedContent, &QgsFetchedContent::fetched, this, &QgsWebDAVExternalStorageFetchedContent::onFetched );
  mStatus = OnGoing;
  mFetchedContent->download();

  // could be already fetched/cached
  if ( mFetchedContent->status() == QgsFetchedContent::Finished )
    mStatus = Finished;
}

QString QgsWebDAVExternalStorageFetchedContent::filePath() const
{
  return mFetchedContent->filePath();
}

void QgsWebDAVExternalStorageFetchedContent::onFetched()
{
  if ( mFetchedContent->status() == QgsFetchedContent::Finished )
    mStatus = Finished;
  else
  {
    mStatus = Failed;
    mErrorString = mFetchedContent->errorString();
  }

  emit fetched();
}


QString QgsWebDAVExternalStorage::type() const
{
  return QStringLiteral( "WebDAV" );
};

QgsExternalStorageTask *QgsWebDAVExternalStorage::storeFile( const QString &filePath, const QUrl &url, const QString &authcfg )
{
  QgsWebDAVExternalStorageStoreTask *storeTask = new QgsWebDAVExternalStorageStoreTask( filePath, url, authcfg );

  QgsApplication::instance()->taskManager()->addTask( storeTask );

  return storeTask;
};

QgsExternalStorageFetchedContent *QgsWebDAVExternalStorage::fetch( const QUrl &url, const QString &authConfig )
{
  QgsFetchedContent *fetchedContent = QgsApplication::instance()->networkContentFetcherRegistry()->fetch( url.toString(), QgsNetworkContentFetcherRegistry::DownloadLater, authConfig );

  // TODO this object should be deleted by us, client or networkContentfetcherregistry?
  return new QgsWebDAVExternalStorageFetchedContent( fetchedContent );
}
