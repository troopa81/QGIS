/***************************************************************************
    testqgsmaptooleditblankareas.cpp
    ---------------------
    begin                : 2025/09/17
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

#include "qgssymbollayer.h"
#include "qgstest.h"
#include "qgsmapcanvas.h"
#include "qgsmaptooleditblanksegments.h"
#include "qgsvectorlayer.h"
#include "testqgsmaptoolutils.h"
#include "qgssinglesymbolrenderer.h"
#include <qstringliteral.h>

class TestQgsMapToolEditBlankSegments : public QgsTest
{
    Q_OBJECT

  public:
    TestQgsMapToolEditBlankSegments()
      : QgsTest( QStringLiteral( "Blank Segments Map Tool Tests" ) ) {}

  private slots:
    void initTestCase();    // will be called before the first testfunction is executed.
    void cleanupTestCase(); // will be called after the last testfunction was executed.
    void init();            // will be called before each testfunction is executed.
    void cleanup();         // will be called after every testfunction.

    void testSelectFeature();
    void testCreateBlankSegment();

  private:
    QObjectUniquePtr<QgsMapToolEditBlankSegments<QgsMarkerLineSymbolLayer>> mMapToolEditBlankSegments;
    std::unique_ptr<QgsMapCanvas> mCanvas;
    std::unique_ptr<QgsVectorLayer> mLayer;
};

void TestQgsMapToolEditBlankSegments::initTestCase()
{
  // QgsApplication::init();
  // QgsApplication::initQgis();

  // // Set up the QSettings environment
  // QCoreApplication::setOrganizationName( QStringLiteral( "QGIS" ) );
  // QCoreApplication::setOrganizationDomain( QStringLiteral( "qgis.org" ) );
  // QCoreApplication::setApplicationName( QStringLiteral( "QGIS-TEST" ) );

  mCanvas = std::make_unique<QgsMapCanvas>();
  mCanvas->setDestinationCrs( QgsCoordinateReferenceSystem( QStringLiteral( "EPSG:3946" ) ) );
  mCanvas->setFrameStyle( QFrame::NoFrame );
  mCanvas->resize( 512, 512 );
  mCanvas->setExtent( QgsRectangle( 0, 0, 25, 10 ) );
  mCanvas->show(); // to make the canvas resize
  mCanvas->hide();

  mLayer = std::make_unique<QgsVectorLayer>( QStringLiteral( "MultiPolygon?field=pk:int&field=bas:string&crs=EPSG:3946" ), QStringLiteral( "polys" ), QStringLiteral( "memory" ) );
  QVERIFY( mLayer->isValid() );
  QCOMPARE( mLayer->fields().count(), 2 );

  const QString wkt1 = "MultiPolygon (((0 0, 10 0, 10 3, 12 4, 8 5, 12 6, 8 7, 10 8, 10 10, 0 10, 0 0),(2 3, 6 3, 6 8, 2 8, 2 3)),((13 0, 16 0, 16 10, 13 10, 13 0)))";
  QgsFeature f1;
  f1.setGeometry( QgsGeometry::fromWkt( wkt1 ) );

  const QString wkt2 = "MultiPolygon (((20 0, 25 0, 25 5, 20 5, 20 0)))";
  QgsFeature f2;
  f2.setGeometry( QgsGeometry::fromWkt( wkt2 ) );

  mLayer->dataProvider()->addFeatures( QgsFeatureList() << f1 << f2 );
  QCOMPARE( mLayer->featureCount(), ( long ) 2 );
  QCOMPARE( mLayer->getFeature( 1 ).geometry().asWkt(), wkt1 );
  QCOMPARE( mLayer->getFeature( 2 ).geometry().asWkt(), wkt2 );

  mCanvas->setCurrentLayer( mLayer.get() );

  QgsSingleSymbolRenderer *renderer = dynamic_cast<QgsSingleSymbolRenderer *>( mLayer->renderer() );
  QVERIFY( renderer );
  QVERIFY( renderer->symbol() );

  QgsMarkerLineSymbolLayer *symbolLayer = new QgsMarkerLineSymbolLayer();
  symbolLayer->setPlacements( Qgis::MarkerLinePlacement::Interval );

  renderer->symbol()->changeSymbolLayer( 0, symbolLayer );

  mMapToolEditBlankSegments.reset( new QgsMapToolEditBlankSegments<QgsMarkerLineSymbolLayer>( mCanvas.get(), mLayer.get(), symbolLayer, 1 ) );
  mCanvas->setMapTool( mMapToolEditBlankSegments );
}

void TestQgsMapToolEditBlankSegments::cleanupTestCase()
{
}

void TestQgsMapToolEditBlankSegments::init()
{
}

void TestQgsMapToolEditBlankSegments::cleanup()
{
}

void TestQgsMapToolEditBlankSegments::testSelectFeature()
{
  TestQgsMapToolUtils utils( mMapToolEditBlankSegments.get() );

  QVERIFY( FID_IS_NULL( mMapToolEditBlankSegments->mCurrentFeatureId ) );

  // select first feature
  utils.mouseClick( 1, 1, Qt::LeftButton );
  QCOMPARE( mMapToolEditBlankSegments->mCurrentFeatureId, 1 );

  // escape
  utils.keyClick( Qt::Key_Escape );
  QVERIFY( FID_IS_NULL( mMapToolEditBlankSegments->mCurrentFeatureId ) );

  // select second feature
  utils.mouseClick( 21, 1, Qt::LeftButton );
  QCOMPARE( mMapToolEditBlankSegments->mCurrentFeatureId, 2 );

  // escape
  utils.keyClick( Qt::Key_Escape );
  QVERIFY( FID_IS_NULL( mMapToolEditBlankSegments->mCurrentFeatureId ) );
}

void TestQgsMapToolEditBlankSegments::testCreateBlankSegment()
{
  TestQgsMapToolUtils utils( mMapToolEditBlankSegments.get() );
  mLayer->startEditing();

  QVERIFY( FID_IS_NULL( mMapToolEditBlankSegments->mCurrentFeatureId ) );

  utils.mouseClick( 1, 1, Qt::LeftButton );
  QCOMPARE( mMapToolEditBlankSegments->mCurrentFeatureId, 1 );

  // TODO Shall it work without move before ?
  utils.mouseMove( 4, 1 );
  utils.mouseClick( 4, 1, Qt::LeftButton );
  utils.mouseMove( 7, 1 );
  utils.mouseClick( 7, 1, Qt::LeftButton );

  QgsFeature feat = mLayer->getFeature( 1 );
  QVERIFY( feat.isValid() );
  QCOMPARE( feat.attribute( 1 ).toString(), QStringLiteral( "tutu" ) );

  mLayer->rollBack();
}

QGSTEST_MAIN( TestQgsMapToolEditBlankSegments )
#include "testqgsmaptooleditblanksegments.moc"
