/***************************************************************************
    qgsmaptooleditblanksegments.h
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

#ifndef QGSMAPTOOLEDITBLANKSEGMENTS_H
#define QGSMAPTOOLEDITBLANKSEGMENTS_H

#include "qgsmaptool.h"
#include "qgsmapcanvasitem.h"
#include "qgsfeatureid.h"
#include "qobjectuniqueptr.h"
#include "qgslinesymbollayer.h"
#include "qgssymbol.h"
#include "qgsrubberband.h"

class QgsMapToolBlankSegmentRubberBand;
class QgsVectorLayer;
class QgsSymbol;
class QgsSymbolLayer;

/**
 * \ingroup gui
 * \brief TODO edit blank segments
 * \since QGIS 4.0
*/
class GUI_EXPORT QgsMapToolEditBlankSegmentsBase : public QgsMapTool
{
    Q_OBJECT

  public:
    /**
     * TODO
     */
    QgsMapToolEditBlankSegmentsBase( QgsMapCanvas *canvas, QgsVectorLayer *layer, QgsLineSymbolLayer *symbolLayer, int blankSegmentFieldIndex );

    ~QgsMapToolEditBlankSegmentsBase();

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
    // TODO compute and return current blank segment start and end distance
    std::pair<double, double> getStartEndDistance() const;
    // TODO
    void updateAttribute();
    // TODO
    void loadFeaturePoints();

    // TODO comment and maybe rename -> "fake" stuff isn't really super ?!
    virtual void initFakeSymbolLayer( const QgsTemplatedLineSymbolLayerBase *original ) = 0;

    int getClosestBlankSegmentIndex( const QPointF &point, double &distance ) const;
    QPointF getClosestPoint( const QPointF &point, double &distance, int &partIndex, int &ringIndex, int &pointIndex ) const;

    void updateStartEndRubberBand();
    void updateHoveredBlankSegment( const QPoint &pos );
    void setCurrentBlankSegment( int currentBlankSegmentIndex );

    typedef QList<QList<QPolygonF>> FeaturePoints;

    class BlankSegment : public QgsRubberBand
    {
      public:
        BlankSegment( int partIndex, int ringIndex, int startIndex, int endIndex, QPointF startPt, QPointF endPt, QgsMapCanvas *canvas, const FeaturePoints &points );
        BlankSegment( QgsMapCanvas *canvas, const FeaturePoints &points );

        void setPoints( int partIndex, int ringIndex, int startIndex, int endIndex, QPointF startPt, QPointF endPt );
        void copyFrom( const BlankSegment &blankSegment );

        const QPointF &getStartPoint() const;
        const QPointF &getEndPoint() const;
        int getStartIndex() const;
        int getEndIndex() const;
        int getPartIndex() const;
        int getRingIndex() const;

        std::pair<double, double> getStartEndDistance( Qgis::RenderUnit unit ) const;

        int pointsCount() const;
        const QPointF &pointAt( int index ) const;

      private:
        int mPartIndex = -1;
        int mRingIndex = -1;
        int mStartIndex = -1;
        int mEndIndex = -1;
        QPointF mStartPt;
        QPointF mEndPt;
        const FeaturePoints &mPoints; //! all feature rendered points
    };

    enum State
    {
      SELECT_FEATURE,
      FEATURE_SELECTED,
      BLANK_SEGMENT_SELECTED,
      BLANK_SEGMENT_MODIFICATION_STARTED,
      BLANK_SEGMENT_CREATION_STARTED
    };

    std::vector<QObjectUniquePtr<BlankSegment>> mBlankSegments;
    QgsVectorLayer *mLayer = nullptr;
    std::unique_ptr<QgsSymbol> mSymbol;
    QgsTemplatedLineSymbolLayerBase *mSymbolLayer = nullptr;
    const QString mSymbolLayerId;

    FeaturePoints mPoints;
    int mBlankSegmentsFieldIndex = -1;
    QgsFeatureId mCurrentFeatureId = FID_NULL;
    QPointF mCurrentPt;
    QPointF mFirstPt;
    int mCurrentIndex = -1;
    int mFirstIndex = -1;
    QgsRectangle mExtent;
    State mState = State::SELECT_FEATURE;
    int mCurrentBlankSegmentIndex = -1;
    int mHoveredBlankSegment = -1;
    int mPartIndex = -1;
    int mRingIndex = -1;

    QObjectUniquePtr<BlankSegment> mEditedBlankSegment;
    QObjectUniquePtr<QgsRubberBand> mStartRubberBand;
    QObjectUniquePtr<QgsRubberBand> mEndRubberBand;


    friend class TestQgsMapToolEditBlankSegments;
};

template<class T>
class GUI_EXPORT QgsMapToolEditBlankSegments : public QgsMapToolEditBlankSegmentsBase
{
    // TODO missing ? sip ?
    // Q_OBJECT

  public:
    QgsMapToolEditBlankSegments<T>( QgsMapCanvas *canvas, QgsVectorLayer *layer, QgsLineSymbolLayer *symbolLayer, int blankSegmentFieldIndex )
      : QgsMapToolEditBlankSegmentsBase( canvas, layer, symbolLayer, blankSegmentFieldIndex )
    {
    }

    void initFakeSymbolLayer( const QgsTemplatedLineSymbolLayerBase *originalSl ) override
    {
      const T *sl = dynamic_cast<const T *>( originalSl );
      mSymbolLayer = sl ? new QgsRenderedPointsSymbolLayer( sl, mPoints ) : nullptr;
    }


  private:
    // TODO not a wonderful name
    class QgsRenderedPointsSymbolLayer : public T
    {
      public:
        // TODO
        QgsRenderedPointsSymbolLayer( const T *original, FeaturePoints &points )
          : T( original->rotateSymbols(), original->interval() )
          , mPoints( points )
        {
          original->copyTemplateSymbolProperties( this );
        }

        void renderPolylineInterval( const QPolygonF &points, QgsSymbolRenderContext &context, double, const QList<QPair<double, double>> & ) override
        {
          const int iPart = context.geometryPartNum() - 1;
          if ( iPart >= mPoints.count() )
            mPoints.resize( iPart + 1 );

          QVector<QPolygonF> &rings = mPoints[iPart];
          if ( QgsRenderedPointsSymbolLayer::mRingIndex >= rings.count() )
            rings.resize( QgsRenderedPointsSymbolLayer::mRingIndex + 1 );

          rings[QgsRenderedPointsSymbolLayer::mRingIndex] = points;
        }

      private:
        FeaturePoints &mPoints;
    };
};


#endif
