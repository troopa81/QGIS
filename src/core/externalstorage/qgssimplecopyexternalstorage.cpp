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

#include "qgssimplecopyexternalstorage_p.h"

#include "qgsnetworkcontentfetcherregistry.h"
#include "qgsnetworkcontentfetchertask.h"
#include "qgsapplication.h"

#include <algorithm>

#include <QFile>
#include <QPointer>
#include <QFileInfo>
#include <QDir>

QgsCopyFileTask::QgsCopyFileTask( const QString &source, const QString &destination )
  : mSource( source ),
    mDestination( destination )
{
}

bool QgsCopyFileTask::run()
{
  QFile fileSource( mSource );
  if ( !fileSource.exists() )
  {
    mErrorString = tr( "Source file '%1' does not exist" ).arg( mSource );
    return false;
  }

  // TODO WebDAV overwrite the file if it exists alreadu, we need the same behavior (a boolean option)
  QFile fileDestination( mDestination );
  if ( fileDestination.exists() )
  {
    mErrorString = tr( "Destination file '%1' already exist" ).arg( mDestination );
    return false;
  }

  const QDir destinationDir = QFileInfo( mDestination ).absoluteDir();
  if ( !destinationDir.exists() )
  {
    mErrorString = tr( "Destination directory '%1' does not exist" ).arg( destinationDir.absolutePath() );
    return false;
  }

  fileSource.open( QIODevice::ReadOnly );
  fileDestination.open( QIODevice::WriteOnly );

  const int size = fileSource.size();
  const int chunkSize = std::clamp( size / 100, 1024, 1024 * 1024 );

  int bytesRead = 0;
  char data[chunkSize];
  while ( true )
  {
    const int len = fileSource.read( data, chunkSize );
    if ( len == -1 )
    {
      mErrorString = tr( "Fail reading from '%1'" ).arg( mSource );
      return false;
    }

    // finish reading
    if ( !len )
      break;

    if ( fileDestination.write( data, len ) != len )
    {
      mErrorString = tr( "Fail writing to '%1'" ).arg( mDestination );
      return false;
    }

    bytesRead += len;
    setProgress( static_cast<double>( bytesRead ) / size );
  }

  setProgress( 100 );

  fileSource.close();
  fileDestination.close();

  return true;
}

const QString &QgsCopyFileTask::errorString() const
{
  return mErrorString;
}

QgsSimpleCopyExternalStorageStoredContent::QgsSimpleCopyExternalStorageStoredContent( const QString &filePath, const QString &uri, const QString &authcfg )
{
  // TODO Can it be possible to need authcfg to copy on a filesystem protected by a login/mdp
  Q_UNUSED( authcfg );

  mCopyTask = new QgsCopyFileTask( filePath, uri );

  QgsApplication::instance()->taskManager()->addTask( mCopyTask );

  connect( mCopyTask, &QgsTask::taskCompleted, this, [ = ]
  {
    mStatus = Finished;
    emit stored();
  } );

  connect( mCopyTask, &QgsTask::taskTerminated, this, [ = ]
  {
    reportError( mCopyTask->errorString() );
  } );

  connect( mCopyTask, &QgsTask::progressChanged, this, [ = ]( double progress )
  {
    emit progressChanged( progress );
  } );

  mStatus = OnGoing;
}

void QgsSimpleCopyExternalStorageStoredContent::cancel()
{
  if ( !mCopyTask )
    return;

  disconnect( mCopyTask, &QgsTask::taskTerminated, nullptr, nullptr );
  connect( mCopyTask, &QgsTask::taskTerminated, this, [ = ]
  {
    mStatus = Canceled;
    emit canceled();
  } );

  mCopyTask->cancel();
}

QgsSimpleCopyExternalStorageFetchedContent::QgsSimpleCopyExternalStorageFetchedContent( const QString &filePath )
{
  // no fetching process, we read directly from its location
  if ( !QFileInfo::exists( filePath ) )
  {
    reportError( tr( "File '%1' does not exist" ).arg( filePath ) );
  }
  else
  {
    mStatus = Finished;
    mFilePath = filePath;
  }
}

QString QgsSimpleCopyExternalStorageFetchedContent::filePath() const
{
  return mFilePath;
}

QString QgsSimpleCopyExternalStorage::type() const
{
  return QStringLiteral( "SimpleCopy" );
};

QgsExternalStorageStoredContent *QgsSimpleCopyExternalStorage::store( const QString &filePath, const QString &uri, const QString &authcfg ) const
{
  return new QgsSimpleCopyExternalStorageStoredContent( filePath, uri, authcfg );
};

QgsExternalStorageFetchedContent *QgsSimpleCopyExternalStorage::fetch( const QString &uri, const QString &authConfig ) const
{
  // TODO Can it be possible to need authcfg to read from a filesystem protected by a login/mdp
  Q_UNUSED( authConfig );

  return new QgsSimpleCopyExternalStorageFetchedContent( uri );
}
