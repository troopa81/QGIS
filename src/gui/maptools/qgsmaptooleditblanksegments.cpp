/***************************************************************************
    qgsmaptooleditblanksegments.cpp
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

#include "qgscoordinatetransform.h"
#include "qgsfeatureid.h"
#include "qgsmaptooleditblanksegments.h"

#include "qgsmultipolygon.h"
#include "qgspolygon.h"
#include "qgsmapcanvas.h"
#include "qgsvectorlayer.h"
#include "qgslinesymbollayer.h"
#include "qgssinglesymbolrenderer.h"
#include "qgssymbol.h"
#include "qgsnullpainterdevice.h"
#include "qgsmapmouseevent.h"
#include "qgsrubberband.h"
#include "qgssettingsregistrycore.h"
#include "qgssettingsentryimpl.h"
#include "qgsguiutils.h"
#include "qgssnappingutils.h"

constexpr int TOLERANCE = 20;

QgsMapToolEditBlankSegmentsBase::QgsMapToolEditBlankSegmentsBase( QgsMapCanvas *canvas, QgsVectorLayer *layer, QgsLineSymbolLayer *symbolLayer, int blankSegmentFieldIndex )
  : QgsMapTool( canvas )
  , mLayer( layer )
  , mSymbolLayerId( symbolLayer->id() )
  , mBlankSegmentsFieldIndex( blankSegmentFieldIndex )
  , mEditedBlankSegment( new BlankSegment( canvas, mPoints ) )
  , mStartRubberBand( new QgsRubberBand( canvas, Qgis::GeometryType::Point ) )
  , mEndRubberBand( new QgsRubberBand( canvas, Qgis::GeometryType::Point ) )
{
  auto initRubberBand = []( QgsRubberBand *rb ) {
    rb->setWidth( QgsGuiUtils::scaleIconSize( 4 ) );
    rb->setColor( QgsSettingsRegistryCore::settingsDigitizingLineColor->value() );
    rb->setIcon( QgsRubberBand::IconType::ICON_BOX );
  };

  initRubberBand( mStartRubberBand );
  initRubberBand( mEndRubberBand );

  mEditedBlankSegment->setWidth( QgsGuiUtils::scaleIconSize( 4 ) );

  // TODO what happen with other renderer
  // TODO deal with errors
  const QgsSingleSymbolRenderer *renderer = dynamic_cast<const QgsSingleSymbolRenderer *>( mLayer->renderer() );
  mSymbol.reset( renderer->symbol()->clone() );
}

QgsMapToolEditBlankSegmentsBase::~QgsMapToolEditBlankSegmentsBase() = default;

void QgsMapToolEditBlankSegmentsBase::activate()
{
  if ( !mSymbolLayer )
  {
    // search and replace symbol layer in clone
    FoundSymbolLayer found = findSymbolLayer( mSymbol.get(), mSymbolLayerId );
    if ( !std::get<QgsSymbolLayer *>( found ) )
    {
      mSymbol.reset();
    }
    else
    {
      // TODO deal w errors
      // TODO use real type and not string from qgstemplatedlinesymbollayer
      // TODO make mSymbolLayer a ref to avoid ownership question ?
      const QgsTemplatedLineSymbolLayerBase *currentSl = dynamic_cast<QgsTemplatedLineSymbolLayerBase *>( std::get<QgsSymbolLayer *>( found ) );
      initFakeSymbolLayer( currentSl );
      std::get<QgsSymbol *>( found )->changeSymbolLayer( std::get<int>( found ), mSymbolLayer );
    }
  }

  QgsMapTool::activate();
}


// TODO doc & shall we put in QgsVectorLayerUtils ? or QgsSymbolLayerUtils
QgsMapToolEditBlankSegmentsBase::FoundSymbolLayer QgsMapToolEditBlankSegmentsBase::findSymbolLayer( QgsSymbol *symbol, const QString slId )
{
  if ( !symbol )
    return { nullptr, nullptr, -1 };

  for ( int i = 0; i < symbol->symbolLayers().count(); i++ )
  {
    QgsSymbolLayer *sl = symbol->symbolLayers().at( i );
    if ( sl->id() == slId )
    {
      return { sl, symbol, i };
    }

    FoundSymbolLayer children = findSymbolLayer( sl->subSymbol(), slId );
    if ( std::get<QgsSymbolLayer *>( children ) )
      return children;
  }

  return { nullptr, nullptr, -1 };
};


double distanceFct( const QPointF &prevPt, const QPointF &pt )
{
  double Ax = prevPt.x();
  double Ay = prevPt.y();
  double Bx = pt.x();
  double By = pt.y();

  return std::sqrt( std::pow( Bx - Ax, 2 ) + std::pow( By - Ay, 2 ) );
}

enum Status
{
  OK,
  LINE_EMPTY,
  NOT_ON_LINE
};

// TODO doc (distance = distance to line)
// TODO maybe use project from qgsgeometryutils_base ? et distance2d au lieu de distanceFct aussi
// et aussi pointsAreCollinear
QPointF projectedPoint( const QPointF &lineStartPt, const QPointF &lineEndPt, const QPointF &point, double &distance, Status &status )
{
  status = Status::OK;
  distance = -1;

  double Ax = lineStartPt.x();
  double Ay = lineStartPt.y();
  double Bx = lineEndPt.x();
  double By = lineEndPt.y();
  double Cx = point.x();
  double Cy = point.y();

  double length = std::sqrt( std::pow( Bx - Ax, 2 ) + std::pow( By - Ay, 2 ) );
  if ( length == 0 )
  {
    status = Status::LINE_EMPTY;
    return QPointF();
  }

  double r = ( ( Cx - Ax ) * ( Bx - Ax ) + ( Cy - Ay ) * ( By - Ay ) ) / std::pow( length, 2 );

  if ( r < 0 or r > 1 )
  {
    status = Status::NOT_ON_LINE;
  }

  // projected point
  double Px = Ax + r * ( Bx - Ax );
  double Py = Ay + r * ( By - Ay );

  distance = std::sqrt( std::pow( Px - Cx, 2 ) + std::pow( Py - Cy, 2 ) );

  return QPointF( Px, Py );
}

void QgsMapToolEditBlankSegmentsBase::canvasMoveEvent( QgsMapMouseEvent *e )
{
  if ( !mSymbol || mPoints.isEmpty() )
    return;

  const QPoint &pos = e->pos();

  if ( canvas()->extent() != mExtent )
    loadFeaturePoints();

  // TODO add function and call function instead of just returning
  switch ( mState )
  {
    case State::SELECT_FEATURE:
      return;

    case State::BLANK_SEGMENT_SELECTED:
      updateHoveredBlankSegment( pos );
      break;

    case State::FEATURE_SELECTED:
    {
      updateHoveredBlankSegment( pos );

      // display current start point to create a new blank segment
      mStartRubberBand->setVisible( mHoveredBlankSegment == -1 );
      if ( mHoveredBlankSegment == -1 )
      {
        const QgsMapToPixel &m2p = *( canvas()->getCoordinateTransform() );
        mStartRubberBand->reset( Qgis::GeometryType::Point );
        double distance;
        mFirstPt = getClosestPoint( pos, distance, mPartIndex, mRingIndex, mFirstIndex );

        // for now end point is the same as start one
        mCurrentPt = mFirstPt;
        mCurrentIndex = mFirstIndex;
        mStartRubberBand->addPoint( m2p.toMapCoordinates( mFirstPt.x(), mFirstPt.y() ) );
      }

      break;
    }

    case State::BLANK_SEGMENT_MODIFICATION_STARTED:
    case State::BLANK_SEGMENT_CREATION_STARTED:
    {
      double distance = -1;
      int partIndex = -1;
      int pointIndex = -1;
      int ringIndex = -1;
      QPointF P = getClosestPoint( pos, distance, partIndex, ringIndex, pointIndex );
      if ( distance > -1 && pointIndex > -1 )
      {
        mCurrentPt = P;
        mPartIndex = partIndex;
        mRingIndex = ringIndex;
        mCurrentIndex = pointIndex;
      }

      if ( mCurrentIndex > -1 )
      {
        int startIndex = -1, endIndex = -1;
        QPointF startPoint, endPoint;
        getStartEnd( startIndex, endIndex, startPoint, endPoint );
        mEditedBlankSegment->setPoints( mPartIndex, mRingIndex, startIndex, endIndex, startPoint, endPoint );
        updateStartEndRubberBand();
      }
    }
    break;
  }
}

void QgsMapToolEditBlankSegmentsBase::canvasPressEvent( QgsMapMouseEvent *e )
{
  // TODO separate in functions
  switch ( mState )
  {
    case State::SELECT_FEATURE:
    {
      //find the closest feature to the pressed position
      const QgsPointLocator::Match m = mCanvas->snappingUtils()->snapToCurrentLayer( e->pos(), QgsPointLocator::Area );
      if ( !m.isValid() )
      {
        // TODO deal with message
        // emit messageEmitted( tr( "No point feature was detected at the clicked position. Please click closer to the feature or enhance the search tolerance under Settings->Options->Digitizing->Search radius for vertex edits" ), Qgis::MessageLevel::Critical );
        return;
      }

      mCurrentFeatureId = m.featureId();
      loadFeaturePoints();

      mState = State::FEATURE_SELECTED;
      break;
    }
    case State::FEATURE_SELECTED:

      // new blank segment selected
      if ( mHoveredBlankSegment > -1 )
      {
        mState = State::BLANK_SEGMENT_SELECTED;
        setCurrentBlankSegment( mHoveredBlankSegment );
      }
      // init first point of new blank segment
      else
      {
        int startIndex = -1, endIndex = -1;
        QPointF startPoint, endPoint;
        getStartEnd( startIndex, endIndex, startPoint, endPoint );
        mEditedBlankSegment->setPoints( mPartIndex, mRingIndex, startIndex, endIndex, startPoint, endPoint );
        mState = State::BLANK_SEGMENT_CREATION_STARTED;
        updateStartEndRubberBand();
      }

      break;

    case State::BLANK_SEGMENT_SELECTED:
    {
      const QObjectUniquePtr<BlankSegment> &currentBlankSegment = mBlankSegments.at( mCurrentBlankSegmentIndex );

      // selected blank segment has changed
      if ( mHoveredBlankSegment > -1 && mHoveredBlankSegment != mCurrentBlankSegmentIndex )
      {
        currentBlankSegment->setWidth( QgsGuiUtils::scaleIconSize( 2 ) );
        setCurrentBlankSegment( mHoveredBlankSegment );
      }
      else
      {
        const double distanceFromStart = ( currentBlankSegment->getStartPoint() - e->pos() ).manhattanLength();
        const double distanceFromEnd = ( currentBlankSegment->getEndPoint() - e->pos() ).manhattanLength();

        // user clicked on start or end point to move it
        if ( std::min( distanceFromStart, distanceFromEnd ) < TOLERANCE )
        {
          mFirstIndex = currentBlankSegment->getStartIndex();
          mCurrentIndex = currentBlankSegment->getEndIndex();
          mFirstPt = currentBlankSegment->getStartPoint();
          mCurrentPt = currentBlankSegment->getEndPoint();

          if ( distanceFromStart < distanceFromEnd )
          {
            std::swap( mFirstIndex, mCurrentIndex );
            std::swap( mFirstPt, mCurrentPt );
          }

          mState = State::BLANK_SEGMENT_MODIFICATION_STARTED;
        }
      }
    }

    break;

    case State::BLANK_SEGMENT_MODIFICATION_STARTED:
    case State::BLANK_SEGMENT_CREATION_STARTED:

      // this is a new one
      if ( mCurrentBlankSegmentIndex == -1 )
      {
        mBlankSegments.emplace_back( new BlankSegment( canvas(), mPoints ) );
        mBlankSegments.back()->copyFrom( *mEditedBlankSegment );
        mState = State::FEATURE_SELECTED;
      }
      // modify an existing one
      else
      {
        QObjectUniquePtr<BlankSegment> &blankSegment = mBlankSegments.at( mCurrentBlankSegmentIndex );
        blankSegment->copyFrom( *mEditedBlankSegment );
        mState = State::BLANK_SEGMENT_SELECTED;
      }

      mFirstIndex = -1;
      mCurrentIndex = -1;
      updateAttribute();
      break;
  }
}

void QgsMapToolEditBlankSegmentsBase::keyPressEvent( QKeyEvent *e )
{
  switch ( mState )
  {
    case State::SELECT_FEATURE:
      return;

    case State::BLANK_SEGMENT_SELECTED:
      // TODO conflic with QgisApp::mapCanvas_keyPressed even if I switch to !accepted
      // ( it's not possible anymore to delete a feature )
      if ( e->matches( QKeySequence::Delete ) && mCurrentBlankSegmentIndex > -1 )
      {
        mState = State::FEATURE_SELECTED;
        int toRemoveIndex = mCurrentBlankSegmentIndex;
        setCurrentBlankSegment( -1 );
        mBlankSegments.erase( mBlankSegments.begin() + toRemoveIndex );
        updateAttribute();
        e->accept();
      }
      else if ( e->matches( QKeySequence::Cancel ) )
      {
        mState = State::FEATURE_SELECTED;
        setCurrentBlankSegment( -1 );
        e->accept();
      }

      break;

    case State::FEATURE_SELECTED:
      if ( e->matches( QKeySequence::Cancel ) )
      {
        mCurrentFeatureId = FID_NULL;
        loadFeaturePoints();
        mState = State::SELECT_FEATURE;
        updateStartEndRubberBand();
        e->accept();
      }
      break;

    case State::BLANK_SEGMENT_CREATION_STARTED:
      if ( e->matches( QKeySequence::Cancel ) )
      {
        mState = State::FEATURE_SELECTED;
        e->accept();
      }
      break;

    case State::BLANK_SEGMENT_MODIFICATION_STARTED:
      if ( e->matches( QKeySequence::Cancel ) )
      {
        mState = State::BLANK_SEGMENT_SELECTED;
        // force original blank segment
        setCurrentBlankSegment( mCurrentBlankSegmentIndex );
        e->accept();
      }
      break;
  }
}


const QPointF &pointAt( const QList<QList<QPolygonF>> &points, int partIndex, int ringIndex, int pointIndex )
{
  if ( partIndex < 0 || ringIndex < 0 || pointIndex < 0 || partIndex >= points.count() )
    // TODO rewrite message
    throw std::invalid_argument( "Invalid part index" );

  const QList<QPolygonF> &rings = points.at( partIndex );
  if ( ringIndex >= rings.count() )
    // TODO rewrite message
    throw std::invalid_argument( "Invalid part index" );

  const QPolygonF &pts = rings.at( ringIndex );
  if ( pointIndex >= pts.count() )
    // TODO rewrite message
    throw std::invalid_argument( "Invalid part index" );

  return pts.at( pointIndex );
}


void QgsMapToolEditBlankSegmentsBase::getStartEnd( int &startIndex, int &endIndex, QPointF &startPt, QPointF &endPt ) const
{
  startIndex = mFirstIndex;
  endIndex = mCurrentIndex;
  if ( mFirstIndex == mCurrentIndex )
  {
    startPt = mCurrentPt;
    endPt = mFirstPt;

    try
    {
      if ( const QPointF &firstIndexPoint = ::pointAt( mPoints, mPartIndex, mRingIndex, mFirstIndex );
           distanceFct( startPt, firstIndexPoint ) < distanceFct( endPt, firstIndexPoint ) )
      {
        std::swap( startPt, endPt );
      }
    }
    catch ( std::invalid_argument e )
    {
      QgsDebugError( e.what() );
    }
  }
  else
  {
    startPt = mFirstPt;
    endPt = mCurrentPt;

    if ( startIndex > endIndex )
    {
      std::swap( startIndex, endIndex );
      std::swap( startPt, endPt );
    }
  }
}

std::pair<double, double> QgsMapToolEditBlankSegmentsBase::BlankSegment::getStartEndDistance( Qgis::RenderUnit unit ) const
{
  double startDistance = 0;
  for ( int i = 1; i < mStartIndex; i++ )
  {
    startDistance += distanceFct( ::pointAt( mPoints, mPartIndex, mRingIndex, i - 1 ), ::pointAt( mPoints, mPartIndex, mRingIndex, i ) );
  }

  startDistance += distanceFct( ::pointAt( mPoints, mPartIndex, mRingIndex, mStartIndex - 1 ), mStartPt );

  double endDistance = startDistance;
  for ( int i = 1; i < pointsCount(); i++ )
  {
    endDistance += distanceFct( pointAt( i ), pointAt( i - 1 ) );
  }

  QgsRenderContext renderContext = QgsRenderContext::fromMapSettings( mMapCanvas->mapSettings() );

  startDistance = renderContext.convertFromPainterUnits( startDistance, unit );
  endDistance = renderContext.convertFromPainterUnits( endDistance, unit );

  return std::pair<double, double>( startDistance, endDistance );
}

int QgsMapToolEditBlankSegmentsBase::getClosestBlankSegmentIndex( const QPointF &point, double &distance ) const
{
  // TODO maybe a spatial index would be better ?
  // search for closest blankSegment
  distance = -1;
  int iBlankSegment = -1;
  for ( int i = 0; i < static_cast<int>( mBlankSegments.size() ); i++ )
  {
    const QObjectUniquePtr<BlankSegment> &blankSegment = mBlankSegments.at( i );
    for ( int iPoint = 1; iPoint < blankSegment->pointsCount(); iPoint++ )
    {
      double d = 0;
      Status status = Status::OK;
      projectedPoint( blankSegment->pointAt( iPoint - 1 ), blankSegment->pointAt( iPoint ), point, d, status );
      switch ( status )
      {
        case Status::LINE_EMPTY:
          continue;

        case Status::NOT_ON_LINE:
          d = std::min( ( blankSegment->getStartPoint() - point ).manhattanLength(), ( blankSegment->getEndPoint() - point ).manhattanLength() );
          break;

        case Status::OK:
          break;
      }

      if ( distance == -1 || d < distance )
      {
        distance = d;
        iBlankSegment = i;
      }
    }
  }

  return iBlankSegment;
}

QPointF QgsMapToolEditBlankSegmentsBase::getClosestPoint( const QPointF &point, double &distance, int &partIndex, int &ringIndex, int &pointIndex ) const
{
  distance = -1;
  QPointF currentPoint;

  // iterate through all points from parts and ring to get the closest one
  for ( int iPart = 0; iPart < mPoints.count(); iPart++ )
  {
    const QList<QPolygonF> &rings = mPoints.at( iPart );
    for ( int iRing = 0; iRing < rings.count(); iRing++ )
    {
      const QPolygonF &points = rings.at( iRing );
      for ( int i = 1; i < points.count(); i++ )
      {
        double d = 0;
        Status status = Status::OK;
        QPointF P = projectedPoint( points.at( i - 1 ), points.at( i ), point, d, status );
        switch ( status )
        {
          case Status::LINE_EMPTY:
          case Status::NOT_ON_LINE:
            continue;

          case Status::OK:
            break;
        }

        if ( distance == -1 || d < distance )
        {
          distance = d;
          currentPoint = P;
          partIndex = iPart;
          ringIndex = iRing;
          pointIndex = i;
        }
      }
    }
  }
  return currentPoint;
}


void QgsMapToolEditBlankSegmentsBase::updateStartEndRubberBand()
{
  mStartRubberBand->reset( Qgis::GeometryType::Point );
  mEndRubberBand->reset( Qgis::GeometryType::Point );

  bool displayEndPoint = true;
  switch ( mState )
  {
    case State::SELECT_FEATURE:
    case State::BLANK_SEGMENT_CREATION_STARTED:
      return;

    case State::FEATURE_SELECTED:
      displayEndPoint = false;
      [[fallthrough]];

    case State::BLANK_SEGMENT_SELECTED:
    case State::BLANK_SEGMENT_MODIFICATION_STARTED:
      break;
  }

  const QgsMapToPixel &m2p = *( canvas()->getCoordinateTransform() );

  const QPointF &startPoint = mEditedBlankSegment->getStartPoint();
  mStartRubberBand->addPoint( m2p.toMapCoordinates( startPoint.x(), startPoint.y() ) );

  if ( displayEndPoint )
  {
    const QPointF &endPoint = mEditedBlankSegment->getEndPoint();
    mEndRubberBand->addPoint( m2p.toMapCoordinates( endPoint.x(), endPoint.y() ) );
  }
}

void QgsMapToolEditBlankSegmentsBase::updateHoveredBlankSegment( const QPoint &pos )
{
  double distance = -1;
  int iBlankSegment = getClosestBlankSegmentIndex( pos, distance );

  if ( mHoveredBlankSegment > -1 && mHoveredBlankSegment != mCurrentBlankSegmentIndex )
  {
    // TODO constant or function for set selected or not
    mBlankSegments.at( mHoveredBlankSegment )->setWidth( QgsGuiUtils::scaleIconSize( 2 ) );
    mBlankSegments.at( mHoveredBlankSegment )->update();
  }

  // blank segment is hovered
  if ( iBlankSegment > -1 && distance < TOLERANCE )
  {
    mHoveredBlankSegment = iBlankSegment;
    mBlankSegments.at( mHoveredBlankSegment )->setWidth( QgsGuiUtils::scaleIconSize( 4 ) );
    mBlankSegments.at( mHoveredBlankSegment )->update();
  }
  // no blank segment hovered, display the first point to create a new blank segment
  else
  {
    mHoveredBlankSegment = -1;
  }
}

void QgsMapToolEditBlankSegmentsBase::setCurrentBlankSegment( int currentBlankSegmentIndex )
{
  // copy current blank segment so we can edit it later (and hide the original one)

  if ( mCurrentBlankSegmentIndex > -1 )
  {
    mBlankSegments.at( mCurrentBlankSegmentIndex )->setVisible( true );
    mBlankSegments.at( mCurrentBlankSegmentIndex )->setWidth( QgsGuiUtils::scaleIconSize( 2 ) );
  }

  mCurrentBlankSegmentIndex = currentBlankSegmentIndex;
  if ( mCurrentBlankSegmentIndex > -1 )
  {
    mBlankSegments.at( mCurrentBlankSegmentIndex )->setVisible( false );
    mEditedBlankSegment->copyFrom( *( mBlankSegments.at( mCurrentBlankSegmentIndex ).get() ) );
  }
  else
  {
    mEditedBlankSegment->setVisible( false );
  }

  updateStartEndRubberBand();
}

void QgsMapToolEditBlankSegmentsBase::updateAttribute()
{
  if ( !mSymbolLayer )
    return;

  QList<QList<QList<std::pair<double, double>>>> blankSegments;
  for ( const QObjectUniquePtr<BlankSegment> &blankSegment : mBlankSegments )
  {
    try
    {
      std::pair<double, double> startEndDistance = blankSegment->getStartEndDistance( mSymbolLayer->blankSegmentsUnit() );

      const int partIndex = blankSegment->getPartIndex();
      if ( partIndex >= blankSegments.count() )
        blankSegments.resize( partIndex + 1 );

      QList<QList<std::pair<double, double>>> &rings = blankSegments[partIndex];
      const int ringIndex = blankSegment->getRingIndex();
      if ( ringIndex >= rings.count() )
        rings.resize( ringIndex + 1 );

      // TODO fix order?
      QList<std::pair<double, double>> &distances = rings[ringIndex];
      distances << startEndDistance;
    }
    catch ( std::invalid_argument e )
    {
      QgsDebugError( e.what() );
    }
  }

  QStringList strParts;
  for ( const QList<QList<std::pair<double, double>>> &part : blankSegments )
  {
    QStringList strRings;
    for ( const QList<std::pair<double, double>> &ring : part )
    {
      QStringList strDistances;
      for ( const std::pair<double, double> &distance : ring )
      {
        strDistances << ( QString::number( distance.first ) + " " + QString::number( distance.second ) );
      }

      strRings << "(" + strDistances.join( "," ) + ")";
    }

    strParts << "(" + strRings.join( "," ) + ")";
  }

  QString strNewBlankSegments = "(" + strParts.join( "," ) + ")";

  mLayer->beginEditCommand( tr( "Set blank segment list" ) );
  if ( mLayer->changeAttributeValue( mCurrentFeatureId, mBlankSegmentsFieldIndex, strNewBlankSegments ) )
  {
    mLayer->endEditCommand();
    mLayer->triggerRepaint();
  }
  else
  {
    mLayer->destroyEditCommand();
  }
}

// TODO put in a private header file
class MyLine
{
  public:
    MyLine( QPointF p1, QPointF p2 )
      : mVertical( false )
      , mIncreasing( false )
      , mT( 0.0 )
      , mLength( 0.0 )
    {
      if ( p1 == p2 )
        return; // invalid

      // tangent and direction
      if ( qgsDoubleNear( p1.x(), p2.x() ) )
      {
        // vertical line - tangent undefined
        mVertical = true;
        mIncreasing = ( p2.y() > p1.y() );
      }
      else
      {
        mVertical = false;
        mT = ( p2.y() - p1.y() ) / ( p2.x() - p1.x() );
        mIncreasing = ( p2.x() > p1.x() );
      }

      // length
      double x = ( p2.x() - p1.x() );
      double y = ( p2.y() - p1.y() );
      mLength = std::sqrt( x * x + y * y );
    }

    // return angle in radians
    double angle()
    {
      double a = ( mVertical ? M_PI_2 : std::atan( mT ) );

      if ( !mIncreasing )
        a += M_PI;
      return a;
    }

    // return difference for x,y when going along the line with specified interval
    QPointF diffForInterval( double interval )
    {
      if ( mVertical )
        return ( mIncreasing ? QPointF( 0, interval ) : QPointF( 0, -interval ) );

      double alpha = std::atan( mT );
      double dx = std::cos( alpha ) * interval;
      double dy = std::sin( alpha ) * interval;
      return ( mIncreasing ? QPointF( dx, dy ) : QPointF( -dx, -dy ) );
    }

    double length() const { return mLength; }

  protected:
    bool mVertical;
    bool mIncreasing;
    double mT;
    double mLength;
};


void QgsMapToolEditBlankSegmentsBase::loadFeaturePoints()
{
  mPoints.clear();
  mExtent = canvas()->extent();
  mBlankSegments.clear();

  if ( FID_IS_NULL( mCurrentFeatureId ) || !mSymbolLayer )
    return;

  QgsFeature feature;
  feature = mLayer->getFeature( mCurrentFeatureId );
  QString currentBlankSegments = feature.attribute( mBlankSegmentsFieldIndex ).toString();


  // render feature to update mPoints
  QgsRenderContext context = QgsRenderContext::fromMapSettings( canvas()->mapSettings() );
  QgsCoordinateTransform transform( canvas()->mapSettings().layerTransform( mLayer ) );
  QgsNullPaintDevice nullPaintDevice;
  QPainter painter( &nullPaintDevice );
  context.setPainter( &painter );
  context.setCoordinateTransform( transform );
  mSymbol->startRender( context );
  mSymbol->renderFeature( feature, context );
  mSymbol->stopRender( context );

  if ( mPoints.isEmpty() )
    return;

  QString error;
  QList<QList<QgsTemplatedLineSymbolLayerBase::BlankSegments>> allBlankSegments = QgsTemplatedLineSymbolLayerBase::parseBlankSegments( currentBlankSegments, context, mSymbolLayer->blankSegmentsUnit(), error );
  if ( !error.isEmpty() )
  {
    // TODO report error to user
    QgsDebugError( QStringLiteral( "Error while parsing feature blank segments: %1" ).arg( error ) );
    return;
  }

  // iterate through all points from parts and ring to get the closest one
  for ( int iPart = 0; iPart < mPoints.count(); iPart++ )
  {
    const QList<QPolygonF> &rings = mPoints.at( iPart );
    for ( int iRing = 0; iRing < rings.count(); iRing++ )
    {
      if ( iPart >= allBlankSegments.count() || iRing >= allBlankSegments.at( iPart ).count() )
        continue;

      const QgsTemplatedLineSymbolLayerBase::BlankSegments &blankSegments = allBlankSegments.at( iPart ).at( iRing );

      double currentLength = 0;
      int iPoint = 0;
      const QPolygonF &points = rings.at( iRing );
      for ( QPair<double, double> ba : blankSegments )
      {
        while ( iPoint < points.count() - 1 && currentLength < ba.first )
        {
          iPoint++;
          // TODO replace distanceFct with MyLine().lentgh() and put MyLine in a private header file
          currentLength += distanceFct( points.at( iPoint ), points.at( iPoint - 1 ) );
        }

        if ( iPoint == points.count() )
          break;


        int startIndex = iPoint;
        // TODO maybe replace difforInterval with a better name
        MyLine l( points.at( iPoint ), points.at( iPoint - 1 ) );
        QPointF startPt = points.at( iPoint ) + l.diffForInterval( currentLength - ba.first );

        while ( iPoint < points.count() - 1 && currentLength < ba.second )
        {
          iPoint++;
          currentLength += distanceFct( points.at( iPoint ), points.at( iPoint - 1 ) );
        }

        if ( iPoint == points.count() )
          break;

        int endIndex = iPoint;
        MyLine l2( points.at( iPoint ), points.at( iPoint - 1 ) );
        QPointF endPt = points.at( iPoint ) + l2.diffForInterval( currentLength - ba.second );

        mBlankSegments.emplace_back( new BlankSegment( iPart, iRing, startIndex, endIndex, startPt, endPt, canvas(), mPoints ) );
      }
    }
  }
}

QgsMapToolEditBlankSegmentsBase::BlankSegment::BlankSegment( QgsMapCanvas *canvas, const FeaturePoints &points )
  : QgsRubberBand( canvas )
  , mPoints( points )
{
  setWidth( QgsGuiUtils::scaleIconSize( 2 ) );
  setColor( QgsSettingsRegistryCore::settingsDigitizingLineColor->value() );
}

QgsMapToolEditBlankSegmentsBase::BlankSegment::BlankSegment( int partIndex, int ringIndex, int startIndex, int endIndex, QPointF startPt, QPointF endPt, QgsMapCanvas *canvas, const FeaturePoints &points )
  : BlankSegment( canvas, points )
{
  setPoints( partIndex, ringIndex, startIndex, endIndex, startPt, endPt );
}

void QgsMapToolEditBlankSegmentsBase::BlankSegment::setPoints( int partIndex, int ringIndex, int startIndex, int endIndex, QPointF startPt, QPointF endPt )
{
  mPartIndex = partIndex;
  mRingIndex = ringIndex;
  mStartIndex = startIndex;
  mEndIndex = endIndex;
  mStartPt = startPt;
  mEndPt = endPt;

  const QgsMapToPixel &m2p = *( mMapCanvas->getCoordinateTransform() );

  reset();
  for ( int iPoint = 0; iPoint < pointsCount(); iPoint++ )
  {
    try
    {
      const QPointF &point = pointAt( iPoint );
      const QgsPointXY mapPoint = m2p.toMapCoordinates( point.x(), point.y() );
      addPoint( mapPoint, iPoint == pointsCount() - 1 ); // update only last one
    }
    catch ( std::invalid_argument e )
    {
      QgsDebugError( e.what() );
    }
  }
}

void QgsMapToolEditBlankSegmentsBase::BlankSegment::copyFrom( const BlankSegment &blankSegment )
{
  setPoints( blankSegment.mPartIndex, blankSegment.mRingIndex, blankSegment.getStartIndex(), blankSegment.getEndIndex(), blankSegment.getStartPoint(), blankSegment.getEndPoint() );
}

const QPointF &QgsMapToolEditBlankSegmentsBase::BlankSegment::getStartPoint() const
{
  return mStartPt;
}

const QPointF &QgsMapToolEditBlankSegmentsBase::BlankSegment::getEndPoint() const
{
  return mEndPt;
}

int QgsMapToolEditBlankSegmentsBase::BlankSegment::getStartIndex() const
{
  return mStartIndex;
}

int QgsMapToolEditBlankSegmentsBase::BlankSegment::getEndIndex() const
{
  return mEndIndex;
}

int QgsMapToolEditBlankSegmentsBase::BlankSegment::getPartIndex() const
{
  return mPartIndex;
}

int QgsMapToolEditBlankSegmentsBase::BlankSegment::getRingIndex() const
{
  return mRingIndex;
}

int QgsMapToolEditBlankSegmentsBase::BlankSegment::pointsCount() const
{
  return ( mEndIndex - mStartIndex ) + 2;
}

const QPointF &QgsMapToolEditBlankSegmentsBase::BlankSegment::pointAt( int index ) const
{
  // TODO test whether index is valid (and do what if not? see QList?)
  return index == 0 ? mStartPt : ( index == pointsCount() - 1 ? mEndPt : ::pointAt( mPoints, mPartIndex, mRingIndex, mStartIndex + index - 1 ) );
}
