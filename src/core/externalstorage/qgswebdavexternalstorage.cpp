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
#include "qgsapplication.h"

#include <QFile>
#include <QPointer>
#include <QFileInfo>


QgsWebDAVExternalStorageStoredContent::QgsWebDAVExternalStorageStoredContent( const QString &filePath, const QString &url, const QString &authcfg )
{
  QString storageUrl = url;
  if ( storageUrl.endsWith( "/" ) )
    storageUrl.append( QFileInfo( filePath ).fileName() );

  mUploadTask = new QgsNetworkContentFetcherTask( QUrl( storageUrl ), authcfg );
  mUploadTask->setMode( "PUT" );

  QFile *f = new QFile( filePath );
  f->open( QIODevice::ReadOnly );
  mUploadTask->setContent( f );

  QgsApplication::instance()->taskManager()->addTask( mUploadTask );

  connect( mUploadTask, &QgsNetworkContentFetcherTask::errorOccurred, [ = ]( QNetworkReply::NetworkError code, const QString & errorMsg )
  {
    Q_UNUSED( code );

    // TODO do we map some error code to some enum error code or not?
    reportError( errorMsg );
  } );

  connect( mUploadTask, &QgsTask::taskCompleted, this, [ = ]
  {
    mUrl = storageUrl;
    mStatus = Finished;
    emit stored();
  } );

  connect( mUploadTask, &QgsTask::progressChanged, this, [ = ]( double progress )
  {
    emit progressChanged( progress );
  } );

  mStatus = OnGoing;
}

void QgsWebDAVExternalStorageStoredContent::cancel()
{
  if ( !mUploadTask )
    return;

  connect( mUploadTask, &QgsTask::taskTerminated, this, [ = ]
  {
    mStatus = Canceled;
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
  {
    mStatus = Finished;
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
