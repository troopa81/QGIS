/***************************************************************************
    qgsmaptooleditblankareas.h
    ---------------------
    begin                : 2025/08/19
    copyright            : (C) 2025 by Julien Cabieces
    email                : julien dot cabieces at oslandia dot com
 ***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef QGSMAPTOOLEDITBLANKAREAS_H
#define QGSMAPTOOLEDITBLANKAREAS_H

#include "qgsmaptool.h"
#include "qgsmapcanvasitem.h"

class QgsMapToolBlankAreaRubberBand;
class QgsVectorLayer;
class QgsLineSymbolLayer;
class QgsSymbol;
class QgsRubberBand;

// TODO Rename ? It's not actually a rubberband, its more a container of rubber band , no? (et virer "MapTool")
// TODO do we have to expose it in header
class QgsMapToolBlankAreaRubberBand
{
  public:
    QgsMapToolBlankAreaRubberBand( QgsMapCanvas *canvas );

    // TODO coords are in screen ref (is it better to convert them before ? )
    void setCurrentBlankArea( const QList<QPointF> &points );

    // TODO
    void setCurrentPosition( const QPointF &point );

  private:
    // TODO
    enum CurrentDisplay
    {
      None,
      BlankArea,
      Position
    };

    QPointF mCurrentPosition;

    QgsMapCanvas *mMapCanvas = nullptr;
    std::unique_ptr<QgsRubberBand> mBlankAreasRubberBand;
    std::unique_ptr<QgsRubberBand> mStartEndRubberBand;
};

/**
 * \ingroup gui
 * \brief TODO edit blank areas
 * \since QGIS 4.0
*/
class GUI_EXPORT QgsMapToolEditBlankAreas : public QgsMapTool
{
    Q_OBJECT

  public:
    /**
     * TODO
     */
    QgsMapToolEditBlankAreas( QgsMapCanvas *canvas, const QgsVectorLayer *layer, QgsLineSymbolLayer *symbolLayer );

    ~QgsMapToolEditBlankAreas();

    void canvasMoveEvent( QgsMapMouseEvent *e ) override;
    void canvasPressEvent( QgsMapMouseEvent *e ) override;

  private:
    std::unique_ptr<QgsMapToolBlankAreaRubberBand> mRubberBand;
    const QgsVectorLayer *mLayer = nullptr;
    QgsLineSymbolLayer *mSymbolLayer = nullptr;
    std::unique_ptr<QgsSymbol> mSymbol;
    QPolygonF mPoints; // TODO don't this need, don't we ?

    double mCurrentPx = 0;
    double mCurrentPy = 0;
    double mFirstPx = 0;
    double mFirstPy = 0;
    int mCurrentIndex = -1;
    int mFirstIndex = -1;
};

#endif
