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

#include "qgswebdavexternalstorage_p.h"

#include "qgsnetworkcontentfetcherregistry.h"
#include "qgsblockingnetworkrequest.h"
#include "qgsnetworkaccessmanager.h"
#include "qgsapplication.h"

#include <QFile>
#include <QPointer>
#include <QFileInfo>

QgsWebDAVExternalStorageStoreTask::QgsWebDAVExternalStorageStoreTask( const QUrl &url, const QString &filePath, const QString &authCfg )
  : QgsTask( tr( "Storing %1" ).arg( QFileInfo( filePath ).baseName() ) )
  , mUrl( url )
  , mFilePath( filePath )
  , mAuthCfg( authCfg )
  , mFeedback( new QgsFeedback( this ) )
{
}

bool QgsWebDAVExternalStorageStoreTask::run()
{
  QgsBlockingNetworkRequest request;
  request.setAuthCfg( mAuthCfg );

  QNetworkRequest req( mUrl );
  QgsSetRequestInitiatorClass( req, QStringLiteral( "QgsWebDAVExternalStorageStoreTask" ) );

  QFile *f = new QFile( mFilePath );
  f->open( QIODevice::ReadOnly );

  connect( &request, &QgsBlockingNetworkRequest::downloadProgress, this, [ = ]( qint64 bytesReceived, qint64 bytesTotal )
  {
    if ( !isCanceled() && bytesTotal > 0 )
    {
      const int progress = ( bytesReceived * 100 ) / bytesTotal;
      setProgress( progress );
    }
  } );

  QgsBlockingNetworkRequest::ErrorCode err = request.put( req, f, mFeedback );

  if ( err != QgsBlockingNetworkRequest::NoError )
  {
    emit errorOccurred( request.errorMessage() );
  }

  return err == QgsBlockingNetworkRequest::NoError;
}

void QgsWebDAVExternalStorageStoreTask::cancel()
{
  mFeedback->cancel();
  QgsTask::cancel();
}

QgsWebDAVExternalStorageStoredContent::QgsWebDAVExternalStorageStoredContent( const QString &filePath, const QString &url, const QString &authcfg )
{
  QString storageUrl = url;
  if ( storageUrl.endsWith( "/" ) )
    storageUrl.append( QFileInfo( filePath ).fileName() );


  mUploadTask = new QgsWebDAVExternalStorageStoreTask( storageUrl, filePath, authcfg );

  QgsApplication::instance()->taskManager()->addTask( mUploadTask );

  connect( mUploadTask, &QgsWebDAVExternalStorageStoreTask::errorOccurred, [ = ]( const QString & errorMsg )
  {
    reportError( errorMsg );
  } );

  connect( mUploadTask, &QgsTask::taskCompleted, this, [ = ]
  {
    mUrl = storageUrl;
    mStatus = Qgis::ContentStatus::Finished;
    emit stored();
  } );

  connect( mUploadTask, &QgsTask::progressChanged, this, [ = ]( double progress )
  {
    emit progressChanged( progress );
  } );

  mStatus = Qgis::ContentStatus::OnGoing;
}

void QgsWebDAVExternalStorageStoredContent::cancel()
{
  if ( !mUploadTask )
    return;

  connect( mUploadTask, &QgsTask::taskTerminated, this, [ = ]
  {
    mStatus = Qgis::ContentStatus::Canceled;
    emit canceled();
  } );

  mUploadTask->cancel();
}

QString QgsWebDAVExternalStorageStoredContent::url() const
{
  return mUrl;
}


QgsWebDAVExternalStorageFetchedContent::QgsWebDAVExternalStorageFetchedContent( QgsFetchedContent *fetchedContent )
  : mFetchedContent( fetchedContent )
{
  connect( mFetchedContent, &QgsFetchedContent::fetched, this, &QgsWebDAVExternalStorageFetchedContent::onFetched );
  mStatus = Qgis::ContentStatus::OnGoing;
  mFetchedContent->download();

  // could be already fetched/cached
  if ( mFetchedContent->status() == QgsFetchedContent::Finished )
    mStatus = Qgis::ContentStatus::Finished;
}

QString QgsWebDAVExternalStorageFetchedContent::filePath() const
{
  return mFetchedContent->filePath();
}

void QgsWebDAVExternalStorageFetchedContent::onFetched()
{
  if ( mFetchedContent->status() == QgsFetchedContent::Finished )
  {
    mStatus = Qgis::ContentStatus::Finished;
    emit fetched();
  }
  else
  {
    reportError( mFetchedContent->errorString() );
  }
}

void QgsWebDAVExternalStorageFetchedContent::cancel()
{
  // TODO implement cancel for fetch
}

QString QgsWebDAVExternalStorage::type() const
{
  return QStringLiteral( "WebDAV" );
};

QgsExternalStorageStoredContent *QgsWebDAVExternalStorage::store( const QString &filePath, const QString &url, const QString &authcfg ) const
{
  return new QgsWebDAVExternalStorageStoredContent( filePath, url, authcfg );
};

QgsExternalStorageFetchedContent *QgsWebDAVExternalStorage::fetch( const QString &url, const QString &authConfig ) const
{
  QgsFetchedContent *fetchedContent = QgsApplication::instance()->networkContentFetcherRegistry()->fetch( url, QgsNetworkContentFetcherRegistry::DownloadLater, authConfig );

  return new QgsWebDAVExternalStorageFetchedContent( fetchedContent );
}
