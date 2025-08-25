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
#include "qgsfeatureid.h"

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

    // TODO
    void setVisible( bool isVisible );

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
    QgsMapToolEditBlankAreas( QgsMapCanvas *canvas, QgsVectorLayer *layer, QgsLineSymbolLayer *symbolLayer, int blankAreaFieldIndex );

    ~QgsMapToolEditBlankAreas();

    void canvasMoveEvent( QgsMapMouseEvent *e ) override;
    void canvasPressEvent( QgsMapMouseEvent *e ) override;
    void deactivate() override;

  private:
    // TODO returns start/end info and set in correct order (in points order)
    void getStartEnd( int &startIndex, int &endIndex, QPointF &startPt, QPointF &endPt ) const;
    // TODO compute and return current blank area start and end distance
    std::pair<double, double> getStartEndDistance() const;
    // TODO
    void addNewBlankArea( double startDistance, double endDistance );

    std::unique_ptr<QgsMapToolBlankAreaRubberBand> mRubberBand;
    QgsVectorLayer *mLayer = nullptr;
    QgsLineSymbolLayer *mSymbolLayer = nullptr;
    std::unique_ptr<QgsSymbol> mSymbol;
    QPolygonF mPoints; // TODO don't this need, don't we ?
    int mBlankAreasFieldIndex = -1;
    QgsFeatureId mCurrentFeatureId = FID_NULL;
    QString mCurrentBlankAreas; // TODO remove this when we will use current rubber band blank areas to serialize
    QPointF mCurrentPt;
    QPointF mFirstPt;
    int mCurrentIndex = -1;
    int mFirstIndex = -1;
};

#endif
