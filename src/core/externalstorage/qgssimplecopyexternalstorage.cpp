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

#include <QFile>
#include <QPointer>

// TODO remove
#include <QFileInfo>
#include <QDir>

QgsCopyFileTask::QgsCopyFileTask( const QString &source, const QString &destination )
  : mSource( source ),
    mDestination( destination )
{
}

bool QgsCopyFileTask::run()
{
  if ( !QFileInfo::exists( mSource ) )
  {
    mErrorString = tr( "Source file '%1' does not exist" ).arg( mSource );
    return false;
  }

  // TODO WebDAV overwrite the file if it exists alreadu, we need the same behavior (a boolean option)
  if ( QFileInfo::exists( mDestination ) )
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

  if ( !QFile::copy( mSource, mDestination ) )
  {
    mErrorString = tr( "Fail to copy '%1' to '%2'" ).arg( mSource, mDestination );
    return false;
  }

  return true;
}

const QString &QgsCopyFileTask::errorString() const
{
  return mErrorString;
}

QgsSimpleCopyExternalStorageStoredContent::QgsSimpleCopyExternalStorageStoredContent( const QString &filePath, const QUrl &url, const QString &authcfg )
{
  // TODO Can it be possible to need authcfg to copy on a filesystem protected by a login/mdp
  Q_UNUSED( authcfg );

  mCopyTask = new QgsCopyFileTask( filePath, url.toString() );

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

QgsExternalStorageStoredContent *QgsSimpleCopyExternalStorage::store( const QString &filePath, const QUrl &url, const QString &authcfg )
{
  // TODO who delete the object
  return new QgsSimpleCopyExternalStorageStoredContent( filePath, url, authcfg );
};

QgsExternalStorageFetchedContent *QgsSimpleCopyExternalStorage::fetch( const QUrl &url, const QString &authConfig )
{
  // TODO Can it be possible to need authcfg to read from a filesystem protected by a login/mdp
  Q_UNUSED( authConfig );

  // TODO this object should be deleted by us, client or networkContentfetcherregistry?
  return new QgsSimpleCopyExternalStorageFetchedContent( url.toString() );
}
