/***************************************************************************
     testqgscopydirtask.cpp
     ----------------------
    Date                 : June 2021
    Copyright            : (C) 2021 Julien Cabieces
    Author               : Julien Cabieces
    Email                : julien cabieces at oslandia dot com
 ***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/
#include "qgstest.h"
#include <QObject>
#include <QString>
#include <QTemporaryFile>
#include <QTemporaryDir>

#include "qgscopydirtask.h"

/**
 * \ingroup UnitTests
 * Unit tests for QgsCopyDirTask
 */
class TestQgsCopyDirTask: public QObject
{
    Q_OBJECT

  private slots:
    void initTestCase();
    void cleanupTestCase();
    void init() {}
    void cleanup() {}

    void testCopy();
};


void TestQgsCopyDirTask::initTestCase()
{
}

void TestQgsCopyDirTask::cleanupTestCase()
{
}

void TestQgsCopyDirTask::testCopy()
{
  QTemporaryDir sourceDir;

  QFile f1( sourceDir.filePath( QStringLiteral( "file1.txt" ) ) );
  f1.open( QIODevice::WriteOnly );
  f1.write( "test1" );

  QFile f2( sourceDir.filePath( QStringLiteral( "file2.txt" ) ) );
  f2.open( QIODevice::WriteOnly );
  f2.write( "test2" );

  QDir( sourceDir.path() ).mkdir( QStringLiteral( "mydir" ) );
  QFile f3( QDir( sourceDir.path() + "/mydir" ).filePath( QStringLiteral( "file3.txt" ) ) );
  f3.open( QIODevice::WriteOnly );
  f2.write( "test3" );

  QTemporaryDir destinationDir;

  QgsTaskManager manager;

  QgsCopyDirTask *task = new QgsCopyDirTask( sourceDir.path(), destinationDir.path() );
  manager.addTask( task );
  task->waitForFinished();

  QCOMPARE( task->status(), QgsTask::Complete );

  const QString sourceBaseName = QFileInfo( sourceDir.path() ).baseName();

  QDir newDir( destinationDir.path() + "/" + sourceBaseName );
  QVERIFY( newDir.exists() );

  QFile newF1( newDir.filePath( "file1.txt" ) );
  QVERIFY( newF1.exists() );

  QFile newF2( newDir.filePath( "file2.txt" ) );
  QVERIFY( newF2.exists() );

  QFile newF3( newDir.filePath( "mydir/file3.txt" ) );
  QVERIFY( newF3.exists() );
}

QGSTEST_MAIN( TestQgsCopyDirTask )
#include "testqgscopydirtask.moc"
