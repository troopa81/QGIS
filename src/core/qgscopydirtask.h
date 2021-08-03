/***************************************************************************
  qgscopydirtask.h
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

#ifndef QGSCOPYDIRTASK_H
#define QGSCOPYDIRTASK_H

#include "qgstaskmanager.h"

class QgsCopyFileTask;

/**
 * \ingroup core
 * \brief Task to copy a dir on disk
 *
 * \since QGIS 3.22
 */
class CORE_EXPORT QgsCopyDirTask : public QgsTask
{
    Q_OBJECT

  public:

    /**
     * Creates a task that copy \a source directory file to \a destination
     * The whole directory is copied into the destination directory, not only the files
     * contained inside the directory
     */
    QgsCopyDirTask( const QString &source, const QString &destination );

    bool run() override;

    /**
     * Returns errorString if an error occurred, else returns null QString
     */
    QString errorString() const;

    void cancel() override;

  private:

    QString mSource;
    QString mDestination;
    QString mErrorString;
    std::unique_ptr<QgsCopyFileTask> mCopyFileTask;
    QMutex mCopyFileTaskMutex;
};

#endif // QGSSIMPLECOPYEXTERNALSTORAGE_H
