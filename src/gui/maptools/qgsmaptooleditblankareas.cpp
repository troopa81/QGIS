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

QgsMapToolEditBlankAreasBase::QgsMapToolEditBlankAreasBase( QgsMapCanvas *canvas, QgsVectorLayer *layer, QgsLineSymbolLayer *symbolLayer, int blankAreaFieldIndex )
  : QgsMapTool( canvas )
  , mLayer( layer )
  , mSymbolLayerId( symbolLayer->id() )
  , mBlankAreasFieldIndex( blankAreaFieldIndex )
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

// TODO doc (distance = distance to line)
QPointF projectedPoint( const QPointF &lineStartPt, const QPointF &lineEndPt, const QPointF &point, double &distance, bool &ok )
{
  ok = true;

  double Ax = lineStartPt.x();
  double Ay = lineStartPt.y();
  double Bx = lineEndPt.x();
  double By = lineEndPt.y();
  double Cx = point.x();
  double Cy = point.y();

  double length = std::sqrt( std::pow( Bx - Ax, 2 ) + std::pow( By - Ay, 2 ) );
  if ( length == 0 )
  {
    ok = false;
    return QPointF();
  }

  double r = ( ( Cx - Ax ) * ( Bx - Ax ) + ( Cy - Ay ) * ( By - Ay ) ) / std::pow( length, 2 );

  // TODO point is outside the segment, see if it's a problem when we create, we don't have to return false
  // when we search for closest blank area
  // if ( r < 0 or r > 1 )
  // {
  //   ok = false;
  //   return QPointF();
  // }

  // projected point
  double Px = Ax + r * ( Bx - Ax );
  double Py = Ay + r * ( By - Ay );

  distance = std::sqrt( std::pow( Px - Cx, 2 ) + std::pow( Py - Cy, 2 ) );

  return QPointF( Px, Py );
}

void QgsMapToolEditBlankAreasBase::canvasMoveEvent( QgsMapMouseEvent *e )
{
  // TODO add function and call function instead of just returning
  switch ( mState )
  {
    case State::SELECT_FEATURE:
      return;
    case State::START_CREATE_BLANK_AREA:
      break;
  }

  QPointF pos = e->pos();

  if ( canvas()->extent() != mExtent )
    loadFeaturePoints();

  if ( !mSymbol || mPoints.isEmpty() )
    return;


  // TODO do a function getClosestBlankArea() returns distance & index
  // TODO maybe a spatial index would be better ?
  // search for closest blankArea
  double distance = -1;
  int iBlankArea = -1;
  for ( uint i = 0; i < mBlankAreas.size(); i++ )
  {
    const std::unique_ptr<BlankArea> &blankArea = mBlankAreas.at( i );
    for ( int iPoint = 1; iPoint < blankArea->pointsCount(); iPoint++ )
    {
      double d = 0;
      bool ok = false;
      QPointF P = projectedPoint( blankArea->pointAt( iPoint - 1 ), blankArea->pointAt( iPoint ), pos, d, ok );
      if ( !ok )
        continue;

      if ( distance == -1 || d < distance )
      {
        distance = d;
        iBlankArea = i;
      }
    }
  }

  if ( mCurrentBlankArea > -1 )
  {
    // TODO constant or function for set selected or not
    mBlankAreas.at( mCurrentBlankArea )->setWidth( QgsGuiUtils::scaleIconSize( 2 ) );
    mBlankAreas.at( mCurrentBlankArea )->update();
  }

  // TODO constant or use tolerance general parameter
  if ( iBlankArea > -1 && distance < 20 )
  {
    mCurrentBlankArea = iBlankArea;
    mBlankAreas.at( iBlankArea )->setWidth( QgsGuiUtils::scaleIconSize( 4 ) );
    mBlankAreas.at( iBlankArea )->update();
  }

  double minDistance = -1;
  for ( int i = 1; i < mPoints.count(); i++ )
  {
    double distance = 0;
    bool ok = false;
    QPointF P = projectedPoint( mPoints[i - 1], mPoints[i], pos, distance, ok );
    if ( !ok )
      continue;

    if ( minDistance == -1 || distance < minDistance )
    {
      minDistance = distance;
      mCurrentPt = P;
      mCurrentIndex = i;
    }
  }

  if ( mCurrentIndex == -1 )
    return;

  if ( mFirstIndex > -1 )
  {
    QList<QPointF> pointsToDraw;

    int startIndex = -1, endIndex = -1;
    QPointF startPt, endPt;
    getStartEnd( startIndex, endIndex, startPt, endPt );

    pointsToDraw << startPt;
    if ( startIndex != endIndex )
    {
      for ( int i = startIndex; i < endIndex; i++ )
      {
        pointsToDraw << mPoints.at( i );
      }
    }
    pointsToDraw << endPt;

    // mRubberBand->setCurrentBlankArea( pointsToDraw, 0 );
  }
  else
  {
    // mRubberBand->setCurrentPosition( mCurrentPt );
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

      mState = State::START_CREATE_BLANK_AREA;
      break;
    }
    case State::START_CREATE_BLANK_AREA:
    case State::EDIT_BLANK_AREA:

      if ( mCurrentBlankArea > -1 )
      {
        mState = State::EDIT_BLANK_AREA;
        updateStartEndRubberBand();
      }
      else
      {
        // finish creating a blank area
        if ( mFirstIndex > -1 )
        {
          const std::pair<double, double> startEndDistance = getStartEndDistance();
          addNewBlankArea( startEndDistance.first, startEndDistance.second );
          mFirstIndex = -1;
        }
        // currently creating a blank area
        else
        {
          mFirstIndex = mCurrentIndex;
          mFirstPt = mCurrentPt;
        }
      }
      break;

      break;
  }
}

void QgsMapToolEditBlankAreasBase::keyPressEvent( QKeyEvent *e )
{
  switch ( mState )
  {
    case State::SELECT_FEATURE:
      return;

    case State::EDIT_BLANK_AREA:
    case State::START_CREATE_BLANK_AREA:
      if ( e->key() == Qt::Key_Escape )
      {
        mCurrentFeatureId = FID_NULL;
        loadFeaturePoints();
        mState = State::SELECT_FEATURE;
      }
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

std::pair<double, double> QgsMapToolEditBlankAreasBase::getStartEndDistance() const
{
  int startIndex = -1, endIndex = -1;
  QPointF startPt, endPt;
  getStartEnd( startIndex, endIndex, startPt, endPt );

  double startDistance = 0;
  for ( int i = 1; i < startIndex; i++ )
  {
    startDistance += distanceFct( mPoints.at( i - 1 ), mPoints.at( i ) );
  }

  startDistance += distanceFct( mPoints.at( startIndex - 1 ), startPt );

  double endDistance = startDistance;

  if ( startIndex == endIndex )
  {
    endDistance += distanceFct( startPt, endPt );
  }
  else
  {
    for ( int i = startIndex + 1; i < endIndex; i++ )
    {
      endDistance += distanceFct( mPoints.at( i - 1 ), mPoints.at( i ) );
    }

    endDistance += distanceFct( mPoints.at( endIndex - 1 ), endPt )
                   + distanceFct( mPoints.at( startIndex ), startPt );
  }

  QgsRenderContext renderContext = QgsRenderContext::fromMapSettings( canvas()->mapSettings() );

  // TODO use combobox unit
  startDistance = renderContext.convertToMapUnits( startDistance, Qgis::RenderUnit::Pixels );
  endDistance = renderContext.convertToMapUnits( endDistance, Qgis::RenderUnit::Pixels );

  return std::pair<double, double>( startDistance, endDistance );
}

void QgsMapToolEditBlankAreasBase::updateStartEndRubberBand()
{
  mStartRubberBand->reset( Qgis::GeometryType::Point );
  mEndRubberBand->reset( Qgis::GeometryType::Point );

  if ( mCurrentBlankArea < 0 || mCurrentBlankArea > static_cast<int>( mBlankAreas.size() ) )
    return;

  const QgsMapToPixel &m2p = *( canvas()->getCoordinateTransform() );

  const std::unique_ptr<BlankArea> &blankArea = mBlankAreas.at( mCurrentBlankArea );

  const QPointF &startPoint = blankArea->getStartPoint();
  mStartRubberBand->addPoint( m2p.toMapCoordinates( startPoint.x(), startPoint.y() ) );

  const QPointF &endPoint = blankArea->getEndPoint();
  mEndRubberBand->addPoint( m2p.toMapCoordinates( endPoint.x(), endPoint.y() ) );
}


void QgsMapToolEditBlankAreasBase::addNewBlankArea( double startDistance, double endDistance )
{
  if ( mBlankAreasFieldIndex < 0 || mBlankAreasFieldIndex >= mLayer->fields().count() )
    return;

  QList<double> blankAreas;
  // TODO this code exists already on qgslinesymbollayer
  // TODO shall we use a property type list
  for ( QString strBlankArea : mCurrentBlankAreas.split( ",", Qt::SkipEmptyParts ) )
  {
    // TODO deal with error
    blankAreas << strBlankArea.toDouble();
  }

  blankAreas << startDistance << endDistance;
  std::sort( blankAreas.begin(), blankAreas.end() );

  QStringList strBlankAreaList;
  for ( double blankArea : blankAreas )
  {
    strBlankAreaList << QString::number( blankArea );
  }

  const QString strNewBlankAreas = strBlankAreaList.join( "," );

  mLayer->beginEditCommand( tr( "Add blank area" ) );
  if ( mLayer->changeAttributeValue( mCurrentFeatureId, mBlankAreasFieldIndex, strNewBlankAreas ) )
  {
    mCurrentBlankAreas = strNewBlankAreas;
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
  mCurrentBlankAreas = QString();
  mExtent = canvas()->extent();
  mBlankAreas.clear();

  if ( FID_IS_NULL( mCurrentFeatureId ) || !mSymbolLayer )
    return;

  QgsFeature feature;
  feature = mLayer->getFeature( mCurrentFeatureId );
  mCurrentBlankAreas = feature.attribute( mBlankAreasFieldIndex ).toString();


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
  const QStringList strBlankAreaList = mCurrentBlankAreas.split( ",", Qt::SkipEmptyParts );
  for ( int i = 0; i + 1 < strBlankAreaList.count(); i += 2 )
  {
    blankAreas.append( QPair<double, double>( context.convertFromMapUnits( strBlankAreaList.at( i ).toDouble(), Qgis::RenderUnit::Pixels ), context.convertFromMapUnits( strBlankAreaList.at( i + 1 ).toDouble(), Qgis::RenderUnit::Pixels ) ) );
  }

  double currentLength = 0;
  int iPoint = 0;
  for ( QPair<double, double> ba : blankAreas )
  {
    while ( iPoint < mPoints.count() && currentLength < ba.first )
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

    while ( iPoint < mPoints.count() && currentLength < ba.second )
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

QgsMapToolEditBlankAreasBase::BlankArea::BlankArea( int startIndex, int endIndex, QPointF startPt, QPointF endPt, QgsMapCanvas *canvas, const QPolygonF &points )
  : QgsRubberBand( canvas )
  , mPoints( points )
{
  setWidth( QgsGuiUtils::scaleIconSize( 2 ) );
  setColor( QgsSettingsRegistryCore::settingsDigitizingLineColor->value() );

  setPoints( startIndex, endIndex, startPt, endPt );
}

void QgsMapToolEditBlankAreasBase::BlankArea::setPoints( int startIndex, int endIndex, QPointF startPt, QPointF endPt )
{
  mStartIndex = startIndex;
  mEndIndex = endIndex;
  mStartPt = startPt;
  mEndPt = endPt;

  const QgsMapToPixel &m2p = *( mMapCanvas->getCoordinateTransform() );

  for ( int iPoint = 0; iPoint < pointsCount(); iPoint++ )
  {
    const QPointF &point = pointAt( iPoint );
    const QgsPointXY mapPoint = m2p.toMapCoordinates( point.x(), point.y() );
    addPoint( mapPoint, iPoint == pointsCount() - 1 ); // update only last one
  }
}

const QPointF &QgsMapToolEditBlankAreasBase::BlankArea::getStartPoint() const
{
  return mStartPt;
}

const QPointF &QgsMapToolEditBlankAreasBase::BlankArea::getEndPoint() const
{
  return mEndPt;
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
