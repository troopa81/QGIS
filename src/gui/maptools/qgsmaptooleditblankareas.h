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
#include "qobjectuniqueptr.h"
#include "qgslinesymbollayer.h"
#include "qgssymbol.h"
#include "qgsrubberband.h"

class QgsMapToolBlankAreaRubberBand;
class QgsVectorLayer;
class QgsSymbol;
class QgsSymbolLayer;

/**
 * \ingroup gui
 * \brief TODO edit blank areas
 * \since QGIS 4.0
*/
class GUI_EXPORT QgsMapToolEditBlankAreasBase : public QgsMapTool
{
    Q_OBJECT

  public:
    /**
     * TODO
     */
    QgsMapToolEditBlankAreasBase( QgsMapCanvas *canvas, QgsVectorLayer *layer, QgsLineSymbolLayer *symbolLayer, int blankAreaFieldIndex );

    ~QgsMapToolEditBlankAreasBase();

    void canvasMoveEvent( QgsMapMouseEvent *e ) override;
    void canvasPressEvent( QgsMapMouseEvent *e ) override;
    void keyPressEvent( QKeyEvent *e ) override;

    void activate() override;

    // TODO every thing really need to be protected, can we not private stuff here ?
  protected:
    typedef std::tuple<QgsSymbolLayer *, QgsSymbol *, int> FoundSymbolLayer;

    static FoundSymbolLayer findSymbolLayer( QgsSymbol *symbol, const QString slId );

    // TODO returns start/end info and set in correct order (in points order)
    void getStartEnd( int &startIndex, int &endIndex, QPointF &startPt, QPointF &endPt ) const;
    // TODO compute and return current blank area start and end distance
    std::pair<double, double> getStartEndDistance() const;
    // TODO
    void updateAttribute();
    // TODO
    void loadFeaturePoints();

    // TODO comment and maybe rename -> "fake" stuff isn't really super ?!
    virtual void initFakeSymbolLayer( const QgsTemplatedLineSymbolLayerBase *original ) = 0;

    int getClosestBlankAreaIndex( const QPointF &point, double &distance ) const;
    QPointF getClosestPoint( const QPointF &point, double &distance, int &pointIndex ) const;

    void updateStartEndRubberBand();
    void updateHoveredBlankArea( const QPoint &pos );
    void setCurrentBlankArea( int currentBlankAreaIndex );

    class BlankArea : public QgsRubberBand
    {
      public:
        BlankArea( int startIndex, int endIndex, QPointF startPt, QPointF endPt, QgsMapCanvas *canvas, const QPolygonF &points );
        BlankArea( QgsMapCanvas *canvas, const QPolygonF &points );

        void setPoints( int startIndex, int endIndex, QPointF startPt, QPointF endPt );
        void copyFrom( const BlankArea &blankArea );

        const QPointF &getStartPoint() const;
        const QPointF &getEndPoint() const;
        int getStartIndex() const;
        int getEndIndex() const;

        std::pair<double, double> getStartEndDistance() const;

        int pointsCount() const;
        const QPointF &pointAt( int index ) const;

      private:
        int mStartIndex = -1;
        int mEndIndex = -1;
        QPointF mStartPt;
        QPointF mEndPt;
        const QPolygonF &mPoints; //! all feature rendered points
    };

    enum State
    {
      SELECT_FEATURE,
      FEATURE_SELECTED,
      BLANK_AREA_SELECTED,
      BLANK_AREA_MODIFICATION_STARTED,
      BLANK_AREA_CREATION_STARTED
    };

    std::vector<std::unique_ptr<BlankArea>> mBlankAreas;
    QgsVectorLayer *mLayer = nullptr;
    std::unique_ptr<QgsSymbol> mSymbol;
    QgsTemplatedLineSymbolLayerBase *mSymbolLayer = nullptr;
    const QString mSymbolLayerId;
    QPolygonF mPoints;
    int mBlankAreasFieldIndex = -1;
    QgsFeatureId mCurrentFeatureId = FID_NULL;
    QPointF mCurrentPt;
    QPointF mFirstPt;
    int mCurrentIndex = -1;
    int mFirstIndex = -1;
    QgsRectangle mExtent;
    State mState = State::SELECT_FEATURE;
    int mCurrentBlankAreaIndex = -1;
    int mHoveredBlankArea = -1;
    BlankArea mEditedBlankArea;

    QObjectUniquePtr<QgsRubberBand> mStartRubberBand;
    QObjectUniquePtr<QgsRubberBand> mEndRubberBand;
};

template<class T>
class GUI_EXPORT QgsMapToolEditBlankAreas : public QgsMapToolEditBlankAreasBase
{
    // TODO missing ? sip ?
    // Q_OBJECT

  public:
    QgsMapToolEditBlankAreas<T>( QgsMapCanvas *canvas, QgsVectorLayer *layer, QgsLineSymbolLayer *symbolLayer, int blankAreaFieldIndex )
      : QgsMapToolEditBlankAreasBase( canvas, layer, symbolLayer, blankAreaFieldIndex )
    {
    }

    void initFakeSymbolLayer( const QgsTemplatedLineSymbolLayerBase *originalSl ) override
    {
      const T *sl = dynamic_cast<const T *>( originalSl );
      mSymbolLayer = sl ? new QgsRenderedPointsSymbolLayer( sl, mPoints ) : nullptr;
    }

  private:
    // TODO not a wonderfull name
    class QgsRenderedPointsSymbolLayer : public T
    {
      public:
        // TODO
        QgsRenderedPointsSymbolLayer( const T *original, QPolygonF &points )
          : T( original->rotateSymbols(), original->interval() )
          , mPoints( points )
        {
          original->copyTemplateSymbolProperties( this );
        }

        void renderPolylineInterval( const QPolygonF &points, QgsSymbolRenderContext &, double ) override
        {
          mPoints = points;
        }

      private:
        QPolygonF &mPoints;
    };
};


#endif
