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
  : QgsMapCanvasItem( canvas )
{
  // TODO update correctly item size accoring to what is displayed
  mItemSize = mMapCanvas->size();
}

void QgsMapToolBlankAreaRubberBand::paint( QPainter *painter )
{
  // Probleme de partir de la géométrie et pas du rendu:
  // - on a pas exactement ce qui va être rendu (quid si le symbol fait des rotations avant, si y a des
  // geom generator...
  // - C'est galère de récupérer tous les points: toutes les méthodes _getPolygon[...]


  // il faut
  // - le layer
  // - le symbol layer
  // - les data defined pour afficher celles définies et sur quelles features
  // --> il va falloir passer en revue toutes les features pour évaluer toute les data defined properties
  // --> Est-ce que l'on met du cache ? Oui map fid -> data define properties et geom ? invalidaiton du cache ?
  // invalidation cache: quand le layer change dataChanged() / l'extent visualisé change ?
  // Methode collectBlankAreas qui remplit une liste de fid/geom(avec offset)/blankArea si defined


  // on applique le code de renderPolylineInterval dans maptool_test.py pour trouver la plus proche feature
  // pour la création

  // on pourrait éventuellement utiliser un index

  // Solution qgsmaptoolpointsymbol : il faut cliquer sur la feature avant de pouvoir éditer
  // Choisit un symbole (sauf sur le rule based) alors qu'on peut avoir plusieurs symbol layers


  switch ( mCurrentDisplay )
  {
    case CurrentDisplay::BlankArea:
    {
      QPainterPath path;
      path.moveTo( mCurrentBlankArea.at( 0 ) );
      for ( int i = 0; i < mCurrentBlankArea.count(); i++ )
      {
        path.lineTo( mCurrentBlankArea.at( i ) );
      }

      // TODO need to save/restore ?
      painter->save();

      painter->setPen( QPen( Qt::GlobalColor::red ) );
      painter->setBrush( QBrush() );
      painter->drawPath( path );
      // painter->drawText(QPointF(50, 50), f"si={startIndex} ei={endIndex}");
      painter->restore();
      break;
    }
    case CurrentDisplay::Position:

      // TODO need to save/restore ?
      painter->save();

      painter->setPen( QPen( Qt::GlobalColor::red ) );
      painter->setBrush( QBrush( Qt::GlobalColor::red ) );
      painter->drawEllipse( mCurrentPosition, 4, 4 );
      // painter->drawText(QPointF(bestPx, bestPy), f"d={distance}")
      painter->restore();

      break;

    case CurrentDisplay::None:
      break;
  }
}

void QgsMapToolBlankAreaRubberBand::setCurrentBlankArea( const QList<QPointF> &points )
{
  mCurrentBlankArea = points;
  mCurrentDisplay = CurrentDisplay::BlankArea;
}

void QgsMapToolBlankAreaRubberBand::setCurrentPosition( const QPointF &point )
{
  mCurrentPosition = point;
  mCurrentDisplay = CurrentDisplay::Position;
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

QgsMapToolEditBlankAreas::QgsMapToolEditBlankAreas( QgsMapCanvas *canvas, const QgsVectorLayer *layer, QgsLineSymbolLayer *symbolLayer )
  : QgsMapTool( canvas )
  , mRubberBand( std::make_unique<QgsRubberBand>( canvas ) )
  , mLayer( layer )
  , mSymbolLayer( symbolLayer )
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

  QgsFeature feature;
  mLayer->getFeatures().nextFeature( feature );

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

  mRubberBand->setWidth( QgsGuiUtils::scaleIconSize( 2 ) );
  // mRubberBand->setStrokeColor( QgsSettingsRegistryCore::settingsDigitizingLineWidth->value() );
  mRubberBand->setColor( QgsSettingsRegistryCore::settingsDigitizingLineColor->value() );
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
      mCurrentPx = Px;
      mCurrentPy = Py;
      mCurrentIndex = i;
    }
  }

  if ( mCurrentIndex == -1 )
    return;

  if ( mFirstIndex > -1 )
  {
    QList<QPointF> pointsToDraw;

    int startIndex = -1;
    int endIndex = -1;
    if ( mFirstIndex == mCurrentIndex )
    {
      QPointF firstPoint( mCurrentPx, mCurrentPy );
      QPointF secondPoint( mFirstPx, mFirstPy );

      if ( distanceFct( firstPoint, mPoints.at( mFirstIndex ) ) < distanceFct( secondPoint, mPoints.at( mFirstIndex ) ) )
      {
        std::swap( firstPoint, secondPoint );
      }

      pointsToDraw << firstPoint << secondPoint;
      startIndex = mFirstIndex;
      endIndex = mFirstIndex;
    }
    else
    {
      startIndex = mFirstIndex;
      double startPx = mFirstPx;
      double startPy = mFirstPy;
      endIndex = mCurrentIndex;
      double endPx = mCurrentPx;
      double endPy = mCurrentPy;

      if ( startIndex > endIndex )
      {
        std::swap( startIndex, endIndex );
        std::swap( startPx, endPx );
        std::swap( startPy, endPy );
      }

      pointsToDraw << QPointF( startPx, startPy );
      for ( int i = startIndex; i < endIndex; i++ )
      {
        pointsToDraw << mPoints.at( i );
      }

      pointsToDraw << QPointF( endPx, endPy );
    }


    const QgsMapToPixel &m2p = *( canvas()->getCoordinateTransform() );
    mRubberBand->reset();
    for ( QPointF point : pointsToDraw )
    {
      mRubberBand->addPoint( m2p.toMapCoordinates( point.x(), point.y() ), false );
    }
    // TODO isVisible needed ? is it the best way to update the points ? a setPoints method maybe would be great ?
    mRubberBand->setVisible( true );
    mRubberBand->updatePosition();
    mRubberBand->update();
  }
}

void QgsMapToolEditBlankAreas::canvasPressEvent( QgsMapMouseEvent * )
{
  if ( mFirstIndex > -1 )
  {
    mFirstIndex = -1;
  }
  else
  {
    mFirstIndex = mCurrentIndex;
    mFirstPx = mCurrentPx;
    mFirstPy = mCurrentPy;
  }
}
