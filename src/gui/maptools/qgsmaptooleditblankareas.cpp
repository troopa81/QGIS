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

// TODO rename marker with templated
// This not a rubberband, rename!!
template<class T>
class QgsMarkerLineSymbolLayerRubberBand : public T
{
  public:
    // TODO
    QgsMarkerLineSymbolLayerRubberBand( const T *original )
      : T( original->rotateSymbols(), original->interval() )
    {
      original->copyTemplateSymbolProperties( this );
    }

    void renderPolylineInterval( const QPolygonF &points, QgsSymbolRenderContext &, double ) override
    {
      mPoints = points;
    }

    const QPolygonF &getRenderedPoints() const { return mPoints; };

  private:
    QPolygonF mPoints;
};

typedef std::tuple<QgsSymbolLayer *, QgsSymbol *, int> FoundSymbolLayer;

// TODO doc & shall we put in QgsVectorLayerUtils ? or QgsSymbolLayerUtils
FoundSymbolLayer findSymbolLayer( QgsSymbol *symbol, const QString slId )
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


QgsMapToolBlankAreaRubberBand::QgsMapToolBlankAreaRubberBand( QgsMapCanvas *canvas )
  : mMapCanvas( canvas )
  , mBlankAreasRubberBand( new QgsRubberBand( canvas ) )
  , mStartEndRubberBand( new QgsRubberBand( canvas, Qgis::GeometryType::Point ) )
{
  mBlankAreasRubberBand->setWidth( QgsGuiUtils::scaleIconSize( 2 ) );
  mBlankAreasRubberBand->setColor( QgsSettingsRegistryCore::settingsDigitizingLineColor->value() );

  mStartEndRubberBand->setWidth( QgsGuiUtils::scaleIconSize( 2 ) );
  mStartEndRubberBand->setColor( QgsSettingsRegistryCore::settingsDigitizingLineColor->value() );
  mStartEndRubberBand->setIcon( QgsRubberBand::IconType::ICON_BOX );
}

// void QgsMapToolBlankAreaRubberBand::paint( QPainter *painter )
// {
//   // Probleme de partir de la géométrie et pas du rendu:
//   // - on a pas exactement ce qui va être rendu (quid si le symbol fait des rotations avant, si y a des
//   // geom generator...
//   // - C'est galère de récupérer tous les points: toutes les méthodes _getPolygon[...]


//   // il faut
//   // - le layer
//   // - le symbol layer
//   // - les data defined pour afficher celles définies et sur quelles features
//   // --> il va falloir passer en revue toutes les features pour évaluer toute les data defined properties
//   // --> Est-ce que l'on met du cache ? Oui map fid -> data define properties et geom ? invalidaiton du cache ?
//   // invalidation cache: quand le layer change dataChanged() / l'extent visualisé change ?
//   // Methode collectBlankAreas qui remplit une liste de fid/geom(avec offset)/blankArea si defined


//   // on applique le code de renderPolylineInterval dans maptool_test.py pour trouver la plus proche feature
//   // pour la création

//   // on pourrait éventuellement utiliser un index

//   // Solution qgsmaptoolpointsymbol : il faut cliquer sur la feature avant de pouvoir éditer
//   // Choisit un symbole (sauf sur le rule based) alors qu'on peut avoir plusieurs symbol layers


//   switch ( mCurrentDisplay )
//   {
//     case CurrentDisplay::BlankArea:
//     {
//       QPainterPath path;
//       path.moveTo( mCurrentBlankArea.at( 0 ) );
//       for ( int i = 0; i < mCurrentBlankArea.count(); i++ )
//       {
//         path.lineTo( mCurrentBlankArea.at( i ) );
//       }

//       // TODO need to save/restore ?
//       painter->save();

//       painter->setPen( QPen( Qt::GlobalColor::red ) );
//       painter->setBrush( QBrush() );
//       painter->drawPath( path );
//       // painter->drawText(QPointF(50, 50), f"si={startIndex} ei={endIndex}");
//       painter->restore();
//       break;
//     }
//     case CurrentDisplay::Position:

//       // TODO need to save/restore ?
//       painter->save();

//       painter->setPen( QPen( Qt::GlobalColor::red ) );
//       painter->setBrush( QBrush( Qt::GlobalColor::red ) );
//       painter->drawEllipse( mCurrentPosition, 4, 4 );
//       // painter->drawText(QPointF(bestPx, bestPy), f"d={distance}")
//       painter->restore();

//       break;

//     case CurrentDisplay::None:
//       break;
//   }
// }

void QgsMapToolBlankAreaRubberBand::setCurrentBlankArea( const QList<QPointF> &points )
{
  const QgsMapToPixel &m2p = *( mMapCanvas->getCoordinateTransform() );
  mBlankAreasRubberBand->reset();
  mStartEndRubberBand->reset( Qgis::GeometryType::Point );

  const QgsPointXY firstPoint = m2p.toMapCoordinates( points.at( 0 ).x(), points.at( 0 ).y() );
  mBlankAreasRubberBand->addPoint( firstPoint, false );
  mStartEndRubberBand->addPoint( firstPoint, false );

  for ( int i = 1; i < points.size() - 1; i++ )
  {
    const QgsPointXY point = m2p.toMapCoordinates( points.at( i ).x(), points.at( i ).y() );
    mBlankAreasRubberBand->addPoint( point, false );
  }

  const QgsPointXY lastPoint = m2p.toMapCoordinates( points.at( points.size() - 1 ).x(), points.at( points.size() - 1 ).y() );
  mBlankAreasRubberBand->addPoint( lastPoint );
  mStartEndRubberBand->addPoint( lastPoint );
}

void QgsMapToolBlankAreaRubberBand::setCurrentPosition( const QPointF &point )
{
  mStartEndRubberBand->reset( Qgis::GeometryType::Point );
  const QgsMapToPixel &m2p = *( mMapCanvas->getCoordinateTransform() );
  mStartEndRubberBand->addPoint( m2p.toMapCoordinates( point.x(), point.y() ) );
}

// void QgsMapToolBlankAreaRubberBand::collectBlankAreas()
// {
//   if ( !mMapCanvas  )
//     return;

// il faut faire comme maptool test pour récupérer les points des différentes features


// sur le hover on utilise le snapping avec un rect de tolerance égale à l'offset pour sélectionner la feature
// on click -> active et affiche les blankAreas et la on peut éditer

// On pourrait afficher un rubberband pour la sélection de la feature


// QgsFeature feature;
// QgsFeatureRequest request;
// // TODO get only geom
// request.setFilterRect( mMapCanvas->extent() );
// QgsFeatureIterator it = mLayer->getFeatures( request );
// while ( it.nextFeature( feature ) )
// {
//   feature.geometry();

//   // context.renderContext().setGeometry( nullptr ); //always use segmented geometry with offset
//   // QList<QPolygonF> mline = ::offsetLine( points, context.renderContext().convertToPainterUnits( offset, mOffsetUnit, mOffsetMapUnitScale ), context.originalGeometryType() != Qgis::GeometryType::Unknown ? context.originalGeometryType() : Qgis::GeometryType::Line );

// }

// }

QgsMapToolEditBlankAreas::QgsMapToolEditBlankAreas( QgsMapCanvas *canvas, QgsVectorLayer *layer, QgsLineSymbolLayer *symbolLayer, int blankAreaFieldIndex )
  : QgsMapTool( canvas )
  , mRubberBand( std::make_unique<QgsMapToolBlankAreaRubberBand>( canvas ) )
  , mLayer( layer )
  , mSymbolLayer( symbolLayer )
  , mBlankAreasFieldIndex( blankAreaFieldIndex )
{
  // TODO what happen with other renderer
  // TODO deal with errors
  const QgsSingleSymbolRenderer *renderer = dynamic_cast<const QgsSingleSymbolRenderer *>( mLayer->renderer() );
  mSymbol.reset( renderer->symbol()->clone() );

  // search and replace symbol layer in clone
  FoundSymbolLayer found = findSymbolLayer( mSymbol.get(), mSymbolLayer->id() );
  QgsMarkerLineSymbolLayerRubberBand<QgsMarkerLineSymbolLayer> *newSymbolLayer = nullptr;
  if ( !std::get<QgsSymbolLayer *>( found ) )
  {
    mSymbol.reset();
  }
  else
  {
    // TODO deal w errors
    // TODO make it template and instanciate here either a marker or hash
    const QgsMarkerLineSymbolLayer *currentSl = dynamic_cast<QgsMarkerLineSymbolLayer *>( std::get<QgsSymbolLayer *>( found ) );
    newSymbolLayer = new QgsMarkerLineSymbolLayerRubberBand( currentSl );
    std::get<QgsSymbol *>( found )->changeSymbolLayer( std::get<int>( found ), newSymbolLayer );
  }

  // TODO to be move when we would know what feature is edited

  mCurrentFeatureId = 1;
  QgsFeature feature;
  feature = mLayer->getFeature( mCurrentFeatureId );
  mCurrentBlankAreas = feature.attribute( mBlankAreasFieldIndex ).toString();

  QgsRenderContext context = QgsRenderContext::fromMapSettings( canvas->mapSettings() );
  QgsCoordinateTransform tranform( canvas->mapSettings().layerTransform( mLayer ) );
  QgsNullPaintDevice nullPaintDevice;
  QPainter painter( &nullPaintDevice );
  context.setPainter( &painter );
  context.setCoordinateTransform( tranform );
  mSymbol->startRender( context );
  mSymbol->renderFeature( feature, context );
  mSymbol->stopRender( context );

  if ( newSymbolLayer )
  {
    mPoints = newSymbolLayer->getRenderedPoints();
  }
}

QgsMapToolEditBlankAreas::~QgsMapToolEditBlankAreas()
{
}


double distanceFct( const QPointF &prevPt, const QPointF &pt )
{
  double Ax = prevPt.x();
  double Ay = prevPt.y();
  double Bx = pt.x();
  double By = pt.y();

  return std::sqrt( std::pow( Bx - Ax, 2 ) + std::pow( By - Ay, 2 ) );
}

void QgsMapToolEditBlankAreas::canvasMoveEvent( QgsMapMouseEvent *e )
{
  QPointF pos = e->pos();

  if ( !mSymbol || mPoints.isEmpty() )
    return;

  double minDistance = -1;
  for ( int i = 1; i < mPoints.count(); i++ )
  {
    QPointF prevPt { mPoints[i - 1] };
    QPointF pt { mPoints[i] };

    double Ax = prevPt.x();
    double Ay = prevPt.y();
    double Bx = pt.x();
    double By = pt.y();
    double Cx = pos.x();
    double Cy = pos.y();

    double length = std::sqrt( std::pow( Bx - Ax, 2 ) + std::pow( By - Ay, 2 ) );
    if ( length == 0 )
      continue;

    double r = ( ( Cx - Ax ) * ( Bx - Ax ) + ( Cy - Ay ) * ( By - Ay ) ) / std::pow( length, 2 );
    if ( r < 0 or r > 1 )
      continue;

    // projected point
    double Px = Ax + r * ( Bx - Ax );
    double Py = Ay + r * ( By - Ay );

    // distance to line
    double distance = std::sqrt( std::pow( Px - Cx, 2 ) + std::pow( Py - Cy, 2 ) );
    if ( minDistance == -1 || distance < minDistance )
    {
      minDistance = distance;
      mCurrentPt = QPointF( Px, Py );
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

    mRubberBand->setCurrentBlankArea( pointsToDraw );
  }
  else
  {
    mRubberBand->setCurrentPosition( mCurrentPt );
  }
}

void QgsMapToolEditBlankAreas::canvasPressEvent( QgsMapMouseEvent * )
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

void QgsMapToolEditBlankAreas::getStartEnd( int &startIndex, int &endIndex, QPointF &startPt, QPointF &endPt ) const
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

std::pair<double, double> QgsMapToolEditBlankAreas::getStartEndDistance() const
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
  for ( int i = startIndex + 1; i < endIndex; i++ )
  {
    endDistance += distanceFct( mPoints.at( i - 1 ), mPoints.at( i ) );
  }

  endDistance += startIndex == endIndex ? distanceFct( startPt, endPt ) : distanceFct( mPoints.at( endIndex - 1 ), endPt ) + distanceFct( mPoints.at( endIndex - 1 ), startPt );

  QgsRenderContext renderContext = QgsRenderContext::fromMapSettings( canvas()->mapSettings() );

  // TODO use combobox unit
  startDistance = renderContext.convertToMapUnits( startDistance, Qgis::RenderUnit::Pixels );
  endDistance = renderContext.convertToMapUnits( endDistance, Qgis::RenderUnit::Pixels );

  return std::pair<double, double>( startDistance, endDistance );
}


void QgsMapToolEditBlankAreas::addNewBlankArea( double startDistance, double endDistance )
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
