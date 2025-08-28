/***************************************************************************
    qgsmaptooleditblankareas.cpp
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
#include "qgsmaptooleditblankareas.h"

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

QgsMapToolEditBlankAreasBase::QgsMapToolEditBlankAreasBase( QgsMapCanvas *canvas, QgsVectorLayer *layer, QgsLineSymbolLayer *symbolLayer, int blankAreaFieldIndex )
  : QgsMapTool( canvas )
  , mLayer( layer )
  , mSymbolLayerId( symbolLayer->id() )
  , mBlankAreasFieldIndex( blankAreaFieldIndex )
  , mEditedBlankArea( canvas, mPoints )
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

  mEditedBlankArea.setWidth( QgsGuiUtils::scaleIconSize( 4 ) );

  // TODO what happen with other renderer
  // TODO deal with errors
  const QgsSingleSymbolRenderer *renderer = dynamic_cast<const QgsSingleSymbolRenderer *>( mLayer->renderer() );
  mSymbol.reset( renderer->symbol()->clone() );
}

QgsMapToolEditBlankAreasBase::~QgsMapToolEditBlankAreasBase()
{
}

void QgsMapToolEditBlankAreasBase::activate()
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
QgsMapToolEditBlankAreasBase::FoundSymbolLayer QgsMapToolEditBlankAreasBase::findSymbolLayer( QgsSymbol *symbol, const QString slId )
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

void QgsMapToolEditBlankAreasBase::canvasMoveEvent( QgsMapMouseEvent *e )
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

    case State::BLANK_AREA_SELECTED:
      updateHoveredBlankArea( pos );
      break;

    case State::FEATURE_SELECTED:
    {
      updateHoveredBlankArea( pos );

      // display current start point to create a new blank area
      mStartRubberBand->setVisible( mHoveredBlankArea == -1 );
      if ( mHoveredBlankArea == -1 )
      {
        const QgsMapToPixel &m2p = *( canvas()->getCoordinateTransform() );
        mStartRubberBand->reset( Qgis::GeometryType::Point );
        double distance;
        mFirstPt = getClosestPoint( pos, distance, mFirstIndex );

        // for now end point is the same as start one
        mCurrentPt = mFirstPt;
        mCurrentIndex = mFirstIndex;
        mStartRubberBand->addPoint( m2p.toMapCoordinates( mFirstPt.x(), mFirstPt.y() ) );
      }

      break;
    }

    case State::BLANK_AREA_MODIFICATION_STARTED:
    case State::BLANK_AREA_CREATION_STARTED:
    {
      double distance = -1;
      int pointIndex = -1;
      QPointF P = getClosestPoint( pos, distance, pointIndex );
      if ( distance > -1 && pointIndex > -1 )
      {
        mCurrentPt = P;
        mCurrentIndex = pointIndex;
      }

      if ( mCurrentIndex > -1 )
      {
        int startIndex = -1, endIndex = -1;
        QPointF startPoint, endPoint;
        getStartEnd( startIndex, endIndex, startPoint, endPoint );
        mEditedBlankArea.setPoints( startIndex, endIndex, startPoint, endPoint );
        updateStartEndRubberBand();
      }
    }
    break;
  }
}

void QgsMapToolEditBlankAreasBase::canvasPressEvent( QgsMapMouseEvent *e )
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

      mCurrentFeatureId = 1;
      loadFeaturePoints();

      mState = State::FEATURE_SELECTED;
      break;
    }
    case State::FEATURE_SELECTED:

      // new blank area selected
      if ( mHoveredBlankArea > -1 )
      {
        mState = State::BLANK_AREA_SELECTED;
        setCurrentBlankArea( mHoveredBlankArea );
      }
      // init first point of new blank area
      else
      {
        int startIndex = -1, endIndex = -1;
        QPointF startPoint, endPoint;
        getStartEnd( startIndex, endIndex, startPoint, endPoint );
        mEditedBlankArea.setPoints( startIndex, endIndex, startPoint, endPoint );
        mState = State::BLANK_AREA_CREATION_STARTED;
        updateStartEndRubberBand();
      }

      break;

    case State::BLANK_AREA_SELECTED:
    {
      const std::unique_ptr<BlankArea> &currentBlankArea = mBlankAreas.at( mCurrentBlankAreaIndex );

      // selected blank area has changed
      if ( mHoveredBlankArea > -1 && mHoveredBlankArea != mCurrentBlankAreaIndex )
      {
        currentBlankArea->setWidth( QgsGuiUtils::scaleIconSize( 2 ) );
        setCurrentBlankArea( mHoveredBlankArea );
      }
      else
      {
        const double distanceFromStart = ( currentBlankArea->getStartPoint() - e->pos() ).manhattanLength();
        const double distanceFromEnd = ( currentBlankArea->getEndPoint() - e->pos() ).manhattanLength();

        // user clicked on start or end point to move it
        if ( std::min( distanceFromStart, distanceFromEnd ) < TOLERANCE )
        {
          mFirstIndex = currentBlankArea->getStartIndex();
          mCurrentIndex = currentBlankArea->getEndIndex();
          mFirstPt = currentBlankArea->getStartPoint();
          mCurrentPt = currentBlankArea->getEndPoint();

          if ( distanceFromStart < distanceFromEnd )
          {
            std::swap( mFirstIndex, mCurrentIndex );
            std::swap( mFirstPt, mCurrentPt );
          }

          mState = State::BLANK_AREA_MODIFICATION_STARTED;
        }
      }
    }

    break;

    case State::BLANK_AREA_MODIFICATION_STARTED:
    case State::BLANK_AREA_CREATION_STARTED:

      // this is a new one
      if ( mCurrentBlankAreaIndex == -1 )
      {
        mBlankAreas.push_back( std::make_unique<BlankArea>( canvas(), mPoints ) );
        mBlankAreas.back()->copyFrom( mEditedBlankArea );
        mState = State::FEATURE_SELECTED;
      }
      // modify an existing one
      else
      {
        std::unique_ptr<BlankArea> &blankArea = mBlankAreas.at( mCurrentBlankAreaIndex );
        blankArea->copyFrom( mEditedBlankArea );
        mState = State::BLANK_AREA_SELECTED;
      }

      mFirstIndex = -1;
      mCurrentIndex = -1;
      updateAttribute();
      break;
  }
}

void QgsMapToolEditBlankAreasBase::keyPressEvent( QKeyEvent *e )
{
  switch ( mState )
  {
    case State::SELECT_FEATURE:
      return;

    case State::BLANK_AREA_SELECTED:
      // TODO conflic with QgisApp::mapCanvas_keyPressed even if I switch to !accepted
      // ( it's not possible anymore to delete a feature )
      if ( e->matches( QKeySequence::Delete ) && mCurrentBlankAreaIndex > -1 )
      {
        mBlankAreas.erase( mBlankAreas.begin() + mCurrentBlankAreaIndex );
        mState = State::FEATURE_SELECTED;
        setCurrentBlankArea( -1 );
        updateAttribute();
        e->accept();
      }
      else if ( e->matches( QKeySequence::Cancel ) )
      {
        mState = State::FEATURE_SELECTED;
        setCurrentBlankArea( -1 );
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

    case State::BLANK_AREA_CREATION_STARTED:
      if ( e->matches( QKeySequence::Cancel ) )
      {
        mState = State::FEATURE_SELECTED;
        e->accept();
      }
      break;

    case State::BLANK_AREA_MODIFICATION_STARTED:
      if ( e->matches( QKeySequence::Cancel ) )
      {
        mState = State::BLANK_AREA_SELECTED;
        // force original blank area
        setCurrentBlankArea( mCurrentBlankAreaIndex );
        e->accept();
      }
      break;
  }
}

void QgsMapToolEditBlankAreasBase::getStartEnd( int &startIndex, int &endIndex, QPointF &startPt, QPointF &endPt ) const
{
  startIndex = mFirstIndex;
  endIndex = mCurrentIndex;
  if ( mFirstIndex == mCurrentIndex )
  {
    startPt = mCurrentPt;
    endPt = mFirstPt;

    if ( distanceFct( startPt, mPoints.at( mFirstIndex ) ) < distanceFct( endPt, mPoints.at( mFirstIndex ) ) )
    {
      std::swap( startPt, endPt );
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

std::pair<double, double> QgsMapToolEditBlankAreasBase::BlankArea::getStartEndDistance() const
{
  double startDistance = 0;
  for ( int i = 1; i < mStartIndex; i++ )
  {
    startDistance += distanceFct( mPoints.at( i - 1 ), mPoints.at( i ) );
  }

  startDistance += distanceFct( mPoints.at( mStartIndex - 1 ), mStartPt );

  double endDistance = startDistance;

  if ( mStartIndex == mEndIndex )
  {
    endDistance += distanceFct( mStartPt, mEndPt );
  }
  else
  {
    for ( int i = mStartIndex + 1; i < mEndIndex; i++ )
    {
      endDistance += distanceFct( mPoints.at( i - 1 ), mPoints.at( i ) );
    }

    endDistance += distanceFct( mPoints.at( mEndIndex - 1 ), mEndPt )
                   + distanceFct( mPoints.at( mStartIndex ), mStartPt );
  }

  QgsRenderContext renderContext = QgsRenderContext::fromMapSettings( mMapCanvas->mapSettings() );

  // TODO use combobox unit
  startDistance = renderContext.convertToMapUnits( startDistance, Qgis::RenderUnit::Pixels );
  endDistance = renderContext.convertToMapUnits( endDistance, Qgis::RenderUnit::Pixels );

  return std::pair<double, double>( startDistance, endDistance );
}

int QgsMapToolEditBlankAreasBase::getClosestBlankAreaIndex( const QPointF &point, double &distance ) const
{
  // TODO maybe a spatial index would be better ?
  // search for closest blankArea
  distance = -1;
  int iBlankArea = -1;
  for ( int i = 0; i < static_cast<int>( mBlankAreas.size() ); i++ )
  {
    const std::unique_ptr<BlankArea> &blankArea = mBlankAreas.at( i );
    for ( int iPoint = 1; iPoint < blankArea->pointsCount(); iPoint++ )
    {
      double d = 0;
      Status status = Status::OK;
      projectedPoint( blankArea->pointAt( iPoint - 1 ), blankArea->pointAt( iPoint ), point, d, status );
      switch ( status )
      {
        case Status::LINE_EMPTY:
          continue;

        case Status::NOT_ON_LINE:
          d = std::min( ( blankArea->getStartPoint() - point ).manhattanLength(), ( blankArea->getEndPoint() - point ).manhattanLength() );
          break;

        case Status::OK:
          break;
      }

      if ( distance == -1 || d < distance )
      {
        distance = d;
        iBlankArea = i;
      }
    }
  }

  return iBlankArea;
}

QPointF QgsMapToolEditBlankAreasBase::getClosestPoint( const QPointF &point, double &distance, int &pointIndex ) const
{
  distance = -1;
  QPointF currentPoint;
  for ( int i = 1; i < mPoints.count(); i++ )
  {
    double d = 0;
    Status status = Status::OK;
    QPointF P = projectedPoint( mPoints[i - 1], mPoints[i], point, d, status );
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
      pointIndex = i;
    }
  }

  return currentPoint;
}


void QgsMapToolEditBlankAreasBase::updateStartEndRubberBand()
{
  mStartRubberBand->reset( Qgis::GeometryType::Point );
  mEndRubberBand->reset( Qgis::GeometryType::Point );

  bool displayEndPoint = true;
  switch ( mState )
  {
    case State::SELECT_FEATURE:
    case State::BLANK_AREA_CREATION_STARTED:
      return;

    case State::FEATURE_SELECTED:
      displayEndPoint = false;
      [[fallthrough]];

    case State::BLANK_AREA_SELECTED:
    case State::BLANK_AREA_MODIFICATION_STARTED:
      break;
  }

  const QgsMapToPixel &m2p = *( canvas()->getCoordinateTransform() );

  const QPointF &startPoint = mEditedBlankArea.getStartPoint();
  mStartRubberBand->addPoint( m2p.toMapCoordinates( startPoint.x(), startPoint.y() ) );

  if ( displayEndPoint )
  {
    const QPointF &endPoint = mEditedBlankArea.getEndPoint();
    mEndRubberBand->addPoint( m2p.toMapCoordinates( endPoint.x(), endPoint.y() ) );
  }
}

void QgsMapToolEditBlankAreasBase::updateHoveredBlankArea( const QPoint &pos )
{
  double distance = -1;
  int iBlankArea = getClosestBlankAreaIndex( pos, distance );

  if ( mHoveredBlankArea > -1 && mHoveredBlankArea != mCurrentBlankAreaIndex )
  {
    // TODO constant or function for set selected or not
    mBlankAreas.at( mHoveredBlankArea )->setWidth( QgsGuiUtils::scaleIconSize( 2 ) );
    mBlankAreas.at( mHoveredBlankArea )->update();
  }

  // blank area is hovered
  if ( iBlankArea > -1 && distance < TOLERANCE )
  {
    mHoveredBlankArea = iBlankArea;
    mBlankAreas.at( mHoveredBlankArea )->setWidth( QgsGuiUtils::scaleIconSize( 4 ) );
    mBlankAreas.at( mHoveredBlankArea )->update();
  }
  // no blank area hovered, display the first point to create a new blank area
  else
  {
    mHoveredBlankArea = -1;
  }
}

void QgsMapToolEditBlankAreasBase::setCurrentBlankArea( int currentBlankAreaIndex )
{
  // copy current blank area so we can edit it later (and hide the original one)

  if ( mCurrentBlankAreaIndex > -1 )
  {
    mBlankAreas.at( mCurrentBlankAreaIndex )->setVisible( true );
    mBlankAreas.at( mCurrentBlankAreaIndex )->setWidth( QgsGuiUtils::scaleIconSize( 2 ) );
  }

  mCurrentBlankAreaIndex = currentBlankAreaIndex;
  if ( mCurrentBlankAreaIndex > -1 )
  {
    mBlankAreas.at( mCurrentBlankAreaIndex )->setVisible( false );
    mEditedBlankArea.copyFrom( *( mBlankAreas.at( mCurrentBlankAreaIndex ).get() ) );
  }
  else
  {
    mEditedBlankArea.setVisible( false );
  }

  updateStartEndRubberBand();
}

void QgsMapToolEditBlankAreasBase::updateAttribute()
{
  QStringList strBlankAreaList;
  for ( std::unique_ptr<BlankArea> &blankArea : mBlankAreas )
  {
    std::pair<double, double> startEndDistance = blankArea->getStartEndDistance();
    strBlankAreaList << QString::number( startEndDistance.first ) << QString::number( startEndDistance.second );
  }

  const QString strNewBlankAreas = strBlankAreaList.join( "," );

  mLayer->beginEditCommand( tr( "Set blank area list" ) );
  if ( mLayer->changeAttributeValue( mCurrentFeatureId, mBlankAreasFieldIndex, strNewBlankAreas ) )
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


void QgsMapToolEditBlankAreasBase::loadFeaturePoints()
{
  mPoints.clear();
  mExtent = canvas()->extent();
  mBlankAreas.clear();

  if ( FID_IS_NULL( mCurrentFeatureId ) || !mSymbolLayer )
    return;

  QgsFeature feature;
  feature = mLayer->getFeature( mCurrentFeatureId );
  QString currentBlankAreas = feature.attribute( mBlankAreasFieldIndex ).toString();


  // render feature to update mPoints
  QgsRenderContext context = QgsRenderContext::fromMapSettings( canvas()->mapSettings() );
  QgsCoordinateTransform tranform( canvas()->mapSettings().layerTransform( mLayer ) );
  QgsNullPaintDevice nullPaintDevice;
  QPainter painter( &nullPaintDevice );
  context.setPainter( &painter );
  context.setCoordinateTransform( tranform );
  mSymbol->startRender( context );
  mSymbol->renderFeature( feature, context );
  mSymbol->stopRender( context );

  if ( mPoints.isEmpty() )
    return;

  // TODO this code exists already on qgslinesymbollayer
  // TODO shall we use a property type list
  QList<QPair<double, double>> blankAreas;
  const QStringList strBlankAreaList = currentBlankAreas.split( ",", Qt::SkipEmptyParts );
  for ( int i = 0; i + 1 < strBlankAreaList.count(); i += 2 )
  {
    blankAreas.append( QPair<double, double>( context.convertFromMapUnits( strBlankAreaList.at( i ).toDouble(), Qgis::RenderUnit::Pixels ), context.convertFromMapUnits( strBlankAreaList.at( i + 1 ).toDouble(), Qgis::RenderUnit::Pixels ) ) );
  }

  double currentLength = 0;
  int iPoint = 0;
  for ( QPair<double, double> ba : blankAreas )
  {
    while ( iPoint < mPoints.count() - 1 && currentLength < ba.first )
    {
      iPoint++;
      // TODO replace distanceFct with MyLine().lentgh() and put MyLine in a private header file
      currentLength += distanceFct( mPoints[iPoint], mPoints[iPoint - 1] );
    }

    if ( iPoint == mPoints.count() )
      break;


    int startIndex = iPoint;
    // TODO maybe replace difforInterval with a better name
    MyLine l( mPoints[iPoint], mPoints[iPoint - 1] );
    QPointF startPt = mPoints[iPoint] + l.diffForInterval( currentLength - ba.first );

    while ( iPoint < mPoints.count() - 1 && currentLength < ba.second )
    {
      iPoint++;
      currentLength += distanceFct( mPoints[iPoint], mPoints[iPoint - 1] );
    }

    if ( iPoint == mPoints.count() )
      break;

    int endIndex = iPoint;
    MyLine l2( mPoints[iPoint], mPoints[iPoint - 1] );
    QPointF endPt = mPoints[iPoint] + l2.diffForInterval( currentLength - ba.second );

    mBlankAreas.push_back( std::make_unique<BlankArea>( startIndex, endIndex, startPt, endPt, canvas(), mPoints ) );
  }
}

QgsMapToolEditBlankAreasBase::BlankArea::BlankArea( QgsMapCanvas *canvas, const QPolygonF &points )
  : QgsRubberBand( canvas )
  , mPoints( points )
{
  setWidth( QgsGuiUtils::scaleIconSize( 2 ) );
  setColor( QgsSettingsRegistryCore::settingsDigitizingLineColor->value() );
}

QgsMapToolEditBlankAreasBase::BlankArea::BlankArea( int startIndex, int endIndex, QPointF startPt, QPointF endPt, QgsMapCanvas *canvas, const QPolygonF &points )
  : BlankArea( canvas, points )
{
  setPoints( startIndex, endIndex, startPt, endPt );
}

void QgsMapToolEditBlankAreasBase::BlankArea::setPoints( int startIndex, int endIndex, QPointF startPt, QPointF endPt )
{
  mStartIndex = startIndex;
  mEndIndex = endIndex;
  mStartPt = startPt;
  mEndPt = endPt;

  const QgsMapToPixel &m2p = *( mMapCanvas->getCoordinateTransform() );

  reset();
  for ( int iPoint = 0; iPoint < pointsCount(); iPoint++ )
  {
    const QPointF &point = pointAt( iPoint );
    const QgsPointXY mapPoint = m2p.toMapCoordinates( point.x(), point.y() );
    addPoint( mapPoint, iPoint == pointsCount() - 1 ); // update only last one
  }
}

void QgsMapToolEditBlankAreasBase::BlankArea::copyFrom( const BlankArea &blankArea )
{
  setPoints( blankArea.getStartIndex(), blankArea.getEndIndex(), blankArea.getStartPoint(), blankArea.getEndPoint() );
}

const QPointF &QgsMapToolEditBlankAreasBase::BlankArea::getStartPoint() const
{
  return mStartPt;
}

const QPointF &QgsMapToolEditBlankAreasBase::BlankArea::getEndPoint() const
{
  return mEndPt;
}

int QgsMapToolEditBlankAreasBase::BlankArea::getStartIndex() const
{
  return mStartIndex;
}

int QgsMapToolEditBlankAreasBase::BlankArea::getEndIndex() const
{
  return mEndIndex;
}

int QgsMapToolEditBlankAreasBase::BlankArea::pointsCount() const
{
  return ( mEndIndex - mStartIndex ) + 2;
}

const QPointF &QgsMapToolEditBlankAreasBase::BlankArea::pointAt( int index ) const
{
  // TODO test whether index is valid (and do what if not? see QList?)
  return index == 0 ? mStartPt : ( index == pointsCount() - 1 ? mEndPt : mPoints.at( mStartIndex + index - 1 ) );
}
