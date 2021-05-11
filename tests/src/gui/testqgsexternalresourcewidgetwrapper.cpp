/***************************************************************************
                         testqgsexternalresourcewidgetwrapper.cpp
                         ---------------------------
    begin                : Oct 2020
    copyright            : (C) 2020 by Julien Cabieces
    email                : julien dot cabieces at oslandia dot com
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "qgstest.h"
#include "qgsvectorlayer.h"
#include "qgsexternalresourcewidgetwrapper.h"
#include "qgsexternalresourcewidget.h"
#include "qgsexternalstorage.h"
#include "qgsexternalstorageregistry.h"
#include "qgspixmaplabel.h"
#include "qgsmessagebar.h"

#include <QWebFrame>
#include <QLineEdit>
#include <QSignalSpy>
#include <QMovie>

#ifdef WITH_QTWEBKIT
#include <QWebView>
#endif

#define CACHED_URL "/home/test/cached.txt"
#define ERROR_URL "/home/test/error.txt"

/**
 * @ingroup UnitTests
 * This is a unit test for the external resource widget wrapper
 *
 * \see QgsExternalResourceWidgetWrapper
 */
class TestQgsExternalResourceWidgetWrapper : public QObject
{
    Q_OBJECT
  private slots:
    void initTestCase();// will be called before the first testfunction is executed.
    void cleanupTestCase();// will be called after the last testfunction was executed.
    void init();// will be called before each testfunction is executed.
    void cleanup();// will be called after every testfunction.
    void testSetNullValues();

    void testUrlStorageExpression();
    void testLoadExternalDocument_data();
    void testLoadExternalDocument();

  private:
    std::unique_ptr<QgsVectorLayer> vl;
};

class QgsTestExternalStorageFetchedContent
  : public QgsExternalStorageFetchedContent
{
  public:

    enum ExpectedBehavior
    {
      OK,
      CACHED,
      ERROR
    };

    QgsTestExternalStorageFetchedContent( ExpectedBehavior behavior ): QgsExternalStorageFetchedContent()
    {
      mBehavior = behavior;
      if ( mBehavior == CACHED )
        mStatus = Finished;
      else
        mStatus = OnGoing;
    }

    virtual QString filePath() const override
    {
      return TEST_DATA_DIR + QStringLiteral( "/sample_image.png" );
    }

    void emitFetched()
    {
      if ( mBehavior == ERROR )
        mStatus = Failed;
      else
        mStatus = Finished;

      emit fetched();
    }

  private:

    ExpectedBehavior mBehavior = OK;
};

class QgsTestExternalStorage : public QgsExternalStorage
{
  public:

    QString type() const override { return QStringLiteral( "test" ); }

    QgsExternalStorageTask *storeFile( const QString &filePath, const QUrl &url, const QString &authcfg = QString() ) override
    {
      Q_UNUSED( filePath );
      Q_UNUSED( url );
      Q_UNUSED( authcfg );
      return nullptr;
    }

    QgsExternalStorageFetchedContent *fetch( const QUrl &url, const QString &authcfg = QString() ) override
    {
      Q_UNUSED( url );
      Q_UNUSED( authcfg );

      sFetchContent = new QgsTestExternalStorageFetchedContent(
        url.toString() == CACHED_URL ? QgsTestExternalStorageFetchedContent::CACHED :
        ( url.toString() == ERROR_URL ? QgsTestExternalStorageFetchedContent::ERROR :
          QgsTestExternalStorageFetchedContent::OK ) );

      return sFetchContent;
    }

    static QPointer<QgsTestExternalStorageFetchedContent> sFetchContent;
};

QPointer<QgsTestExternalStorageFetchedContent> QgsTestExternalStorage::sFetchContent;

void TestQgsExternalResourceWidgetWrapper::initTestCase()
{
  // Set up the QgsSettings environment
  QCoreApplication::setOrganizationName( QStringLiteral( "QGIS" ) );
  QCoreApplication::setOrganizationDomain( QStringLiteral( "qgis.org" ) );
  QCoreApplication::setApplicationName( QStringLiteral( "QGIS-TEST-EXTERNAL-RESOURCE-WIDGET-WRAPPER" ) );

  QgsApplication::init();
  QgsApplication::initQgis();

  QgsApplication::externalStorageRegistry()->registerExternalStorage( new QgsTestExternalStorage() );
}

void TestQgsExternalResourceWidgetWrapper::cleanupTestCase()
{
  QgsApplication::exitQgis();
}

void TestQgsExternalResourceWidgetWrapper::init()
{
  vl = std::make_unique<QgsVectorLayer>( QStringLiteral( "NoGeometry?field=type:string&field=url:string" ),
                                         QStringLiteral( "myvl" ),
                                         QLatin1String( "memory" ) );

  QgsFeature feat1( vl->fields(),  1 );
  feat1.setAttribute( QStringLiteral( "type" ), QStringLiteral( "type1" ) );
  vl->dataProvider()->addFeature( feat1 );

  QgsFeature feat2( vl->fields(),  2 );
  feat2.setAttribute( QStringLiteral( "type" ), QStringLiteral( "type2" ) );
  vl->dataProvider()->addFeature( feat2 );
}

void TestQgsExternalResourceWidgetWrapper::cleanup()
{
}

void TestQgsExternalResourceWidgetWrapper::testSetNullValues()
{
  QgsExternalResourceWidgetWrapper ww( vl.get(), 0, nullptr, nullptr );
  QWidget *widget = ww.createWidget( nullptr );
  QVERIFY( widget );

  ww.initWidget( widget );
  QVERIFY( ww.mQgsWidget );
  QVERIFY( ww.mLineEdit );

  QSignalSpy spy( &ww, &QgsExternalResourceWidgetWrapper::valuesChanged );

  ww.updateValues( QStringLiteral( "test" ) );
  QCOMPARE( ww.mLineEdit->text(), QStringLiteral( "test" ) );
  QCOMPARE( ww.mQgsWidget->documentPath(), QVariant( "test" ) );
  QCOMPARE( spy.count(), 1 );

  ww.updateValues( QVariant() );
  QCOMPARE( ww.mLineEdit->text(), QgsApplication::nullRepresentation() );
  QCOMPARE( ww.mQgsWidget->documentPath(), QVariant( QVariant::String ) );
  QCOMPARE( spy.count(), 2 );

  ww.updateValues( QgsApplication::nullRepresentation() );
  QCOMPARE( ww.mLineEdit->text(), QgsApplication::nullRepresentation() );
  QCOMPARE( ww.mQgsWidget->documentPath(), QVariant( QVariant::String ) );
  QCOMPARE( spy.count(), 2 );

  delete widget;
}

void TestQgsExternalResourceWidgetWrapper::testUrlStorageExpression()
{
  // test that everything related to Url storage expresssion is correctly set
  // according to configuration

  QVariantMap globalVariables;
  globalVariables.insert( "myurl", "http://url.test.com/" );
  QgsApplication::setCustomVariables( globalVariables );

  QgsExternalResourceWidgetWrapper ww( vl.get(), 0, nullptr, nullptr );
  QWidget *widget = ww.createWidget( nullptr );
  QVERIFY( widget );

  QVariantMap config;
  config.insert( QStringLiteral( "StorageType" ), QStringLiteral( "test" ) );
  config.insert( QStringLiteral( "StorageUrlExpression" ),
                 "@myurl || @layer_name || '/' || \"type\" || '/' "
                 "|| attribute( @current_feature, 'type' ) "
                 "|| '/' || $id || '/test'" );
  ww.setConfig( config );

  QgsFeature feat = vl->getFeature( 1 );
  QVERIFY( feat.isValid() );
  ww.setFeature( feat );

  ww.initWidget( widget );
  QVERIFY( ww.mQgsWidget );
  QVERIFY( ww.mQgsWidget->fileWidget() );

  QCOMPARE( ww.mQgsWidget->fileWidget()->storageType(), QStringLiteral( "test" ) );
  QgsExpression *expression = ww.mQgsWidget->fileWidget()->storageUrlExpression();
  QVERIFY( expression );

  QgsExpressionContext expressionContext = ww.mQgsWidget->fileWidget()->expressionContext();
  QVERIFY( expression->prepare( &expressionContext ) );
  QCOMPARE( expression->evaluate( &expressionContext ).toString(),
            QStringLiteral( "http://url.test.com/myvl/type1/type1/1/test" ) );

  feat = vl->getFeature( 2 );
  QVERIFY( feat.isValid() );
  ww.setFeature( feat );

  expressionContext = ww.mQgsWidget->fileWidget()->expressionContext();
  QVERIFY( expression->prepare( &expressionContext ) );
  QCOMPARE( expression->evaluate( &expressionContext ).toString(),
            QStringLiteral( "http://url.test.com/myvl/type2/type2/2/test" ) );

  // TODO test that store as updated the field url
}

void TestQgsExternalResourceWidgetWrapper::testLoadExternalDocument_data()
{
  QTest::addColumn<int>( "documentType" );

  QTest::newRow( "image" ) << static_cast<int>( QgsExternalResourceWidget::Image );
#ifdef WITH_QTWEBKIT
  QTest::newRow( "webview" ) << static_cast<int>( QgsExternalResourceWidget::Web );
#endif
}

void TestQgsExternalResourceWidgetWrapper::testLoadExternalDocument()
{
  // test to load a document to be accessed with an external storage
  QEventLoop loop;

  QFETCH( int, documentType );

  QgsMessageBar *messageBar = new QgsMessageBar;
  QgsExternalResourceWidgetWrapper ww( vl.get(), 0, nullptr, messageBar, nullptr );

  QWidget *widget = ww.createWidget( nullptr );
  QVERIFY( widget );

  QVariantMap config;
  config.insert( QStringLiteral( "StorageType" ), QStringLiteral( "test" ) );
  config.insert( QStringLiteral( "DocumentViewer" ), documentType );
  ww.setConfig( config );

  QgsFeature feat = vl->getFeature( 1 );
  QVERIFY( feat.isValid() );
  ww.setFeature( feat );

  ww.initWidget( widget );
  QVERIFY( ww.mQgsWidget );
  QVERIFY( ww.mQgsWidget->fileWidget() );
  QCOMPARE( ww.mQgsWidget->fileWidget()->storageType(), QStringLiteral( "test" ) );

  widget->show();
  if ( documentType == QgsExternalResourceWidget::Image )
  {
    QVERIFY( ww.mQgsWidget->mPixmapLabel->isVisible() );
    QVERIFY( ww.mQgsWidget->mPixmapLabel->pixmap( Qt::ReturnByValue ).isNull() );
  }
#ifdef WITH_QTWEBKIT
  else if ( documentType == QgsExternalResourceWidget::Web )

  {
    QVERIFY( ww.mQgsWidget->mWebView->isVisible() );
    QVERIFY( ww.mQgsWidget->mWebView->url().isValid() );
  }
#endif

  QVERIFY( !ww.mQgsWidget->mLoadingLabel->isVisible() );
  QVERIFY( ww.mQgsWidget->mLoadingMovie->state() == QMovie::NotRunning );
  QVERIFY( !ww.mQgsWidget->mErrorLabel->isVisible() );

  // ----------------------------------------------------
  // load url
  // ----------------------------------------------------
  ww.mQgsWidget->loadDocument( QStringLiteral( "/home/test/myfile.txt" ) );

  // content still null, fetching in progress...
  if ( documentType == QgsExternalResourceWidget::Image )
    QVERIFY( !ww.mQgsWidget->mPixmapLabel->isVisible() );

#ifdef WITH_QTWEBKIT
  else if ( documentType == QgsExternalResourceWidget::Web )
    QVERIFY( !ww.mQgsWidget->mWebView->isVisible() );
#endif

  QVERIFY( ww.mQgsWidget->mLoadingLabel->isVisible() );
  QVERIFY( ww.mQgsWidget->mLoadingMovie->state() == QMovie::Running );
  QVERIFY( !ww.mQgsWidget->mErrorLabel->isVisible() );
  QVERIFY( !messageBar->currentItem() );

  QVERIFY( QgsTestExternalStorage::sFetchContent );

  QgsTestExternalStorage::sFetchContent->emitFetched();
  QCoreApplication::processEvents();

  if ( documentType == QgsExternalResourceWidget::Image )
  {
    QVERIFY( ww.mQgsWidget->mPixmapLabel->isVisible() );
    QVERIFY( !ww.mQgsWidget->mPixmapLabel->pixmap( Qt::ReturnByValue ).isNull() );
  }
#ifdef WITH_QTWEBKIT
  else if ( documentType == QgsExternalResourceWidget::Web )
  {
    QVERIFY( ww.mQgsWidget->mWebView->isVisible() );
    QVERIFY( ww.mQgsWidget->mWebView->url().isValid() );
    QCOMPARE( ww.mQgsWidget->mWebView->url().toString(), QStringLiteral( "file://%1/sample_image.png" ).arg( TEST_DATA_DIR ) );
  }
#endif

  QVERIFY( !ww.mQgsWidget->mLoadingLabel->isVisible() );
  QVERIFY( ww.mQgsWidget->mLoadingMovie->state() == QMovie::NotRunning );
  QVERIFY( !ww.mQgsWidget->mErrorLabel->isVisible() );
  QVERIFY( !messageBar->currentItem() );

  // wait for the fetch content object to be destroyed
  connect( QgsTestExternalStorage::sFetchContent, &QObject::destroyed, &loop, &QEventLoop::quit );
  loop.exec();
  QVERIFY( !QgsTestExternalStorage::sFetchContent );

  // ----------------------------------------------------
  // load a cached url
  // ----------------------------------------------------
  ww.mQgsWidget->loadDocument( QStringLiteral( CACHED_URL ) );

  // cached, no fetching, content is OK
  QVERIFY( !ww.mQgsWidget->mLoadingLabel->isVisible() );
  QVERIFY( ww.mQgsWidget->mLoadingMovie->state() == QMovie::NotRunning );
  QVERIFY( !ww.mQgsWidget->mErrorLabel->isVisible() );
  QVERIFY( !messageBar->currentItem() );

  QVERIFY( QgsTestExternalStorage::sFetchContent );

  if ( documentType == QgsExternalResourceWidget::Image )
  {
    QVERIFY( ww.mQgsWidget->mPixmapLabel->isVisible() );
    QVERIFY( !ww.mQgsWidget->mPixmapLabel->pixmap( Qt::ReturnByValue ).isNull() );
  }
#ifdef WITH_QTWEBKIT
  else if ( documentType == QgsExternalResourceWidget::Web )
  {
    QVERIFY( ww.mQgsWidget->mWebView->isVisible() );
    QVERIFY( ww.mQgsWidget->mWebView->url().isValid() );
    QCOMPARE( ww.mQgsWidget->mWebView->url().toString(), QStringLiteral( "file://%1/sample_image.png" ).arg( TEST_DATA_DIR ) );
  }
#endif

  // wait for the fetch content object to be destroyed
  connect( QgsTestExternalStorage::sFetchContent, &QObject::destroyed, &loop, &QEventLoop::quit );
  loop.exec();
  QVERIFY( !QgsTestExternalStorage::sFetchContent );

  // ----------------------------------------------------
  // load an error url
  // ----------------------------------------------------
  ww.mQgsWidget->loadDocument( QStringLiteral( ERROR_URL ) );

  // still no error, fetching in progress...
  if ( documentType == QgsExternalResourceWidget::Image )
    QVERIFY( !ww.mQgsWidget->mPixmapLabel->isVisible() );

#ifdef WITH_QTWEBKIT
  else if ( documentType == QgsExternalResourceWidget::Web )
    QVERIFY( !ww.mQgsWidget->mWebView->isVisible() );

#endif

  QVERIFY( ww.mQgsWidget->mLoadingLabel->isVisible() );
  QVERIFY( ww.mQgsWidget->mLoadingMovie->state() == QMovie::Running );
  QVERIFY( !ww.mQgsWidget->mErrorLabel->isVisible() );
  QVERIFY( !messageBar->currentItem() );

  QVERIFY( QgsTestExternalStorage::sFetchContent );

  QgsTestExternalStorage::sFetchContent->emitFetched();
  QCoreApplication::processEvents();

  QVERIFY( !ww.mQgsWidget->mPixmapLabel->isVisible() );
#ifdef WITH_QTWEBKIT
  QVERIFY( !ww.mQgsWidget->mWebView->isVisible() );
#endif

  QVERIFY( !ww.mQgsWidget->mLoadingLabel->isVisible() );
  QVERIFY( ww.mQgsWidget->mLoadingMovie->state() == QMovie::NotRunning );
  QVERIFY( ww.mQgsWidget->mErrorLabel->isVisible() );
  QVERIFY( messageBar->currentItem() );

  // wait for the fetch content object to be destroyed
  connect( QgsTestExternalStorage::sFetchContent, &QObject::destroyed, &loop, &QEventLoop::quit );
  loop.exec();
  QVERIFY( !QgsTestExternalStorage::sFetchContent );

  delete widget;
  delete messageBar;
}

QGSTEST_MAIN( TestQgsExternalResourceWidgetWrapper )
#include "testqgsexternalresourcewidgetwrapper.moc"
