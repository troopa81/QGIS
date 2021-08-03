/***************************************************************************
  qgscopydirtask.cpp
  --------------------------------------
  Date                 : August 2021
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

#include "qgscopydirtask.h"
#include "qgscopyfiletask.h"

#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QDirIterator>

QgsCopyDirTask::QgsCopyDirTask( const QString &source, const QString &destination )
  : mSource( source ),
    mDestination( destination )
{
}

bool QgsCopyDirTask::run()
{
  QDir sourceDir( mSource );
  if ( !sourceDir.exists() )
  {
    mErrorString = tr( "Source directory '%1' does not exist" ).arg( mSource );
    return false;
  }

  QDir destinationDir = QDir( mDestination );
  if ( !destinationDir.exists() )
  {
    mErrorString = tr( "Destination directory '%1' does not exist" ).arg( destinationDir.absolutePath() );
    return false;
  }

  if ( destinationDir.exists( sourceDir.dirName() ) )
  {
    mErrorString = tr( "Copied directory '%1' already exists in destination" ).arg( sourceDir.dirName() );
    return false;
  }

  if ( !destinationDir.mkdir( sourceDir.dirName() ) )
  {
    mErrorString = tr( "Failed to create '%1' directory" ).arg( destinationDir.filePath( sourceDir.dirName() ) );
    return false;
  }

  destinationDir = QDir( destinationDir.filePath( sourceDir.dirName() ) );

  sourceDir.setFilter( QDir::Dirs | QDir::Files | QDir::NoDotAndDotDot );
  QDirIterator it( sourceDir, QDirIterator::Subdirectories );
  while ( it.hasNext() )
  {
    if ( isCanceled() )
      return false;

    const QString sourceFilePath = it.next();
    if ( it.fileInfo().isDir() )
    {
      destinationDir.mkdir( it.fileName() );
      destinationDir = QDir( destinationDir.filePath( it.fileName() ) );
    }
    else
    {
      const QString destinationFilePath = destinationDir.absoluteFilePath( it.fileName() );
      mCopyFileTaskMutex.lock();
      mCopyFileTask.reset( new QgsCopyFileTask( sourceFilePath, destinationFilePath ) );
      mCopyFileTaskMutex.unlock();

      if ( !mCopyFileTask->run() )
      {
        mErrorString = mCopyFileTask->errorString();
        return false;
      }
    }
  }

  return true;
}

QString QgsCopyDirTask::errorString() const
{
  return mErrorString;
}

void QgsCopyDirTask::cancel()
{
  QgsTask::cancel();
  QMutexLocker locker( &mCopyFileTaskMutex );
  mCopyFileTask->cancel();
}
