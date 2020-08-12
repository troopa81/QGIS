/***************************************************************************
    qgsoracleprojectstorage.cpp
    ---------------------
    begin                : August 2002
    copyright            : (C) 2020 by Julien Cabieces
    email                : julien dot cabieces at oslandia dot com
 ***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/
#include "qgsoracleprojectstorage.h"

#include "qgsoracleconn.h"
#include "qgsoracleconnpool.h"

#include "qgsreadwritecontext.h"

#include <QIODevice>
#include <QJsonDocument>
#include <QJsonObject>
#include <QUrl>
#include <QUrlQuery>

static bool _parseMetadataDocument( const QJsonDocument &doc, QgsProjectStorage::Metadata &metadata )
{
  // if ( !doc.isObject() )
  //   return false;

  // QJsonObject docObj = doc.object();
  // metadata.lastModified = QDateTime();
  // if ( docObj.contains( "last_modified_time" ) )
  // {
  //   QString lastModifiedTimeStr = docObj["last_modified_time"].toString();
  //   if ( !lastModifiedTimeStr.isEmpty() )
  //   {
  //     QDateTime lastModifiedUtc = QDateTime::fromString( lastModifiedTimeStr, Qt::ISODate );
  //     lastModifiedUtc.setTimeSpec( Qt::UTC );
  //     metadata.lastModified = lastModifiedUtc.toLocalTime();
  //   }
  // }
  // return true;
  return false;
}


static bool _projectsTableExists( QgsOracleConn &conn, const QString &ownerName, QgsReadWriteContext &context )
{
  return false;

  QString tableName( "qgis_projects" );
  QSqlQuery qry( conn );
  if ( !conn.exec( qry, QStringLiteral( "SELECT COUNT(*) FROM all_tables WHERE table_name=%1 and owner=%2" ),
                   QVariantList() << tableName << ownerName ) || !qry.next() )
  {
    context.pushMessage( QObject::tr( "Could not check if table %1 exists for owner %2" ).arg( tableName ).arg( ownerName ), Qgis::Critical );
    return false;
  }

  return qry.value( 0 ).toInt() > 0;
}


QStringList QgsOracleProjectStorage::listProjects( const QString &uri )
{
  QStringList lst;

  // QgsOracleProjectUri projectUri = decodeUri( uri );
  // if ( !projectUri.valid )
  //   return lst;

  // QgsOracleConn *conn = QgsOracleConnPool::instance()->acquireConnection( projectUri.connInfo.connectionInfo( false ) );
  // if ( !conn )
  //   return lst;

  // if ( _projectsTableExists( *conn, projectUri.schemaName ) )
  // {
  //   QString sql( QStringLiteral( "SELECT name FROM %1.qgis_projects" ).arg( QgsOracleConn::quotedIdentifier( projectUri.schemaName ) ) );
  //   QgsOracleResult result( conn->PQexec( sql ) );
  //   if ( result.PQresultStatus() == PGRES_TUPLES_OK )
  //   {
  //     int count = result.PQntuples();
  //     for ( int i = 0; i < count; ++i )
  //     {
  //       QString name = result.PQgetvalue( i, 0 );
  //       lst << name;
  //     }
  //   }
  // }

  // QgsOracleConnPool::instance()->releaseConnection( conn );

  return lst;
}


bool QgsOracleProjectStorage::readProject( const QString &uri, QIODevice *device, QgsReadWriteContext &context )
{
  QgsOracleProjectUri projectUri = decodeUri( uri );
  if ( !projectUri.valid )
  {
    context.pushMessage( QObject::tr( "Invalid URI for OracleQL provider: " ) + uri, Qgis::Critical );
    return false;
  }

  QgsOracleConn *conn = QgsOracleConnPool::instance()->acquireConnection( projectUri.connInfo.connectionInfo( false ) );
  if ( !conn )
  {
    context.pushMessage( QObject::tr( "Could not connect to the database: " ) + projectUri.connInfo.connectionInfo( false ), Qgis::Critical );
    return false;
  }

  if ( !_projectsTableExists( *conn, projectUri.ownerName, context ) )
  {
    context.pushMessage( QObject::tr( "Table qgis_projects does not exist or it is not accessible." ), Qgis::Critical );
    QgsOracleConnPool::instance()->releaseConnection( conn );
    return false;
  }

  // bool ok = false;
  // QString sql( QStringLiteral( "SELECT content FROM %1.qgis_projects WHERE name = %2" ).arg( QgsOracleConn::quotedIdentifier( projectUri.schemaName ), QgsOracleConn::quotedValue( projectUri.projectName ) ) );
  // QgsOracleResult result( conn->PQexec( sql ) );
  // if ( result.PQresultStatus() == PGRES_TUPLES_OK )
  // {
  //   if ( result.PQntuples() == 1 )
  //   {
  //     // TODO: would be useful to have QByteArray version of PQgetvalue to avoid bytearray -> string -> bytearray conversion
  //     QString hexEncodedContent( result.PQgetvalue( 0, 0 ) );
  //     QByteArray binaryContent( QByteArray::fromHex( hexEncodedContent.toUtf8() ) );
  //     device->write( binaryContent );
  //     device->seek( 0 );
  //     ok = true;
  //   }
  //   else
  //   {
  //     context.pushMessage( QObject::tr( "The project '%1' does not exist in schema '%2'." ).arg( projectUri.projectName, projectUri.schemaName ), Qgis::Critical );
  //   }
  // }

  // QgsOracleConnPool::instance()->releaseConnection( conn );

  // return ok;
  return false;
}


bool QgsOracleProjectStorage::writeProject( const QString &uri, QIODevice *device, QgsReadWriteContext &context )
{
  QgsOracleProjectUri projectUri = decodeUri( uri );
  if ( !projectUri.valid )
  {
    context.pushMessage( QObject::tr( "Invalid URI for OracleQL provider: " ) + uri, Qgis::Critical );
    return false;
  }

  QgsOracleConn *conn = QgsOracleConnPool::instance()->acquireConnection( projectUri.connInfo.connectionInfo( false ) );
  if ( !conn )
  {
    context.pushMessage( QObject::tr( "Could not connect to the database: " ) + projectUri.connInfo.connectionInfo( false ), Qgis::Critical );
    return false;
  }

  QSqlQuery qry( *conn );
  if ( !_projectsTableExists( *conn, projectUri.ownerName, context ) )
  {
    // try to create projects table
    if ( !conn->exec( qry, QStringLiteral( "CREATE TABLE %1.qgis_projects"
                                           "(name varchar2(4000) PRIMARY KEY, "
                                           "metadata CLOB CONSTRAINT ensure_json CHECK (metadata IS JSON), "
                                           "content BLOB)" ).arg( QgsOracleConn::quotedIdentifier( projectUri.ownerName ) ),
                      QVariantList() ) )
    {
      QString errCause = QObject::tr( "Unable to save project. It's not possible to create the destination table on the database. Maybe this is due to database permissions (user=%1). Please contact your database admin." ).arg( projectUri.connInfo.username() );
      context.pushMessage( errCause, Qgis::Critical );
      QgsOracleConnPool::instance()->releaseConnection( conn );
      return false;
    }
  }

  // read from device and write to the table
  QByteArray content = device->readAll();

  QString metadataExpr = QStringLiteral( "%1 || "
                                         "TO_CHAR(SYSTIMESTAMP AT TIME ZONE 'UTC', 'YYYY-MM-DD HH24:MI:SS.FF') "
                                         "|| %2 || USER || %3" ).arg(
                           QgsOracleConn::quotedValue( "{ \"last_modified_time\": \"" ),
                           QgsOracleConn::quotedValue( "\", \"last_modified_user\": \"" ),
                           QgsOracleConn::quotedValue( "\" }" )
                         );


  // QString sql( "MERGE INTO %1.qgis_projects a "
  //              "USING (SELECT 'abc' name, '{ "last_modified_time": "' || TO_CHAR(SYSTIMESTAMP AT TIME ZONE 'UTC', 'YYYY-MM-DD HH24:MI:SS.FF') || '", "last_modified_user": "' || USER || '" }' metadata FROM dual ) b
  //   ON (a.name = b.name)
  //   WHEN MATCHED THEN UPDATE SET a.metadata = b.metadata
  //   WHEN NOT MATCHED THEN INSERT (name, metadata) VALUES (b.name, b.metadata)

  // UPSERT the Oracle way
  QString sql( "MERGE INTO %1.qgis_projects a "
               "USING (SELECT %2 name, %3 metadata FROM dual) b "
               "ON (a.name = b.name) "
               "WHEN MATCHED THEN UPDATE SET a.metadata = b.metadata "
               "WHEN NOT MATCHED THEN INSERT (name, metadata) VALUES (b.name, b.metadata" );
  sql = sql.arg( QgsOracleConn::quotedIdentifier( projectUri.ownerName ),
                 QgsOracleConn::quotedValue( projectUri.projectName ),
                 metadataExpr  // no need to quote: already quoted
               );
  // sql += QString::fromLatin1( content.toHex() );
  sql += ")";

  if ( !conn->exec( qry, sql, QVariantList() ) )
  {
    QString errCause = QObject::tr( "Unable to insert or update project (project=%1) in the destination table on the database. Maybe this is due to table permissions (user=%2). Please contact your database admin." ).arg( projectUri.projectName, projectUri.connInfo.username() );
    context.pushMessage( errCause, Qgis::Critical );
    QgsOracleConnPool::instance()->releaseConnection( conn );
    return false;
  }

  QgsOracleConnPool::instance()->releaseConnection( conn );
  return true;
}


bool QgsOracleProjectStorage::removeProject( const QString &uri )
{
  // QgsOracleProjectUri projectUri = decodeUri( uri );
  // if ( !projectUri.valid )
  //   return false;

  // QgsOracleConn *conn = QgsOracleConnPool::instance()->acquireConnection( projectUri.connInfo.connectionInfo( false ) );
  // if ( !conn )
  //   return false;

  // bool removed = false;
  // if ( _projectsTableExists( *conn, projectUri.schemaName ) )
  // {
  //   QString sql( QStringLiteral( "DELETE FROM %1.qgis_projects WHERE name = %2" ).arg( QgsOracleConn::quotedIdentifier( projectUri.schemaName ), QgsOracleConn::quotedValue( projectUri.projectName ) ) );
  //   QgsOracleResult res( conn->PQexec( sql ) );
  //   removed = res.PQresultStatus() == PGRES_COMMAND_OK;
  // }

  // QgsOracleConnPool::instance()->releaseConnection( conn );

  // return removed;

  return false;
}


bool QgsOracleProjectStorage::readProjectStorageMetadata( const QString &uri, QgsProjectStorage::Metadata &metadata )
{
  // QgsOracleProjectUri projectUri = decodeUri( uri );
  // if ( !projectUri.valid )
  //   return false;

  // QgsOracleConn *conn = QgsOracleConnPool::instance()->acquireConnection( projectUri.connInfo.connectionInfo( false ) );
  // if ( !conn )
  //   return false;

  // bool ok = false;
  // QString sql( QStringLiteral( "SELECT metadata FROM %1.qgis_projects WHERE name = %2" ).arg( QgsOracleConn::quotedIdentifier( projectUri.schemaName ), QgsOracleConn::quotedValue( projectUri.projectName ) ) );
  // QgsOracleResult result( conn->PQexec( sql ) );
  // if ( result.PQresultStatus() == PGRES_TUPLES_OK )
  // {
  //   if ( result.PQntuples() == 1 )
  //   {
  //     metadata.name = projectUri.projectName;
  //     QString metadataStr = result.PQgetvalue( 0, 0 );
  //     QJsonDocument doc( QJsonDocument::fromJson( metadataStr.toUtf8() ) );
  //     ok = _parseMetadataDocument( doc, metadata );
  //   }
  // }

  // QgsOracleConnPool::instance()->releaseConnection( conn );

  // return ok;
  return false;
}


QString QgsOracleProjectStorage::encodeUri( const QgsOracleProjectUri &postUri )
{
  // QUrl u;
  // QUrlQuery urlQuery;

  // u.setScheme( "oracle" );
  // u.setHost( postUri.connInfo.host() );
  // if ( !postUri.connInfo.port().isEmpty() )
  //   u.setPort( postUri.connInfo.port().toInt() );
  // u.setUserName( postUri.connInfo.username() );
  // u.setPassword( postUri.connInfo.password() );

  // if ( !postUri.connInfo.service().isEmpty() )
  //   urlQuery.addQueryItem( "service", postUri.connInfo.service() );
  // if ( !postUri.connInfo.authConfigId().isEmpty() )
  //   urlQuery.addQueryItem( "authcfg", postUri.connInfo.authConfigId() );
  // if ( postUri.connInfo.sslMode() != QgsDataSourceUri::SslPrefer )
  //   urlQuery.addQueryItem( "sslmode", QgsDataSourceUri::encodeSslMode( postUri.connInfo.sslMode() ) );

  // urlQuery.addQueryItem( "dbname", postUri.connInfo.database() );

  // urlQuery.addQueryItem( "schema", postUri.schemaName );
  // if ( !postUri.projectName.isEmpty() )
  //   urlQuery.addQueryItem( "project", postUri.projectName );

  // u.setQuery( urlQuery );

  // return QString::fromUtf8( u.toEncoded() );
  return QString();
}


QgsOracleProjectUri QgsOracleProjectStorage::decodeUri( const QString &uri )
{
  QUrl u = QUrl::fromEncoded( uri.toUtf8() );
  QUrlQuery urlQuery( u.query() );

  QgsOracleProjectUri postUri;
  postUri.valid = u.isValid();

  QString host = u.host();
  QString port = u.port() != -1 ? QString::number( u.port() ) : QString();
  QString username = u.userName();
  QString password = u.password();
  QgsDataSourceUri::SslMode sslMode = QgsDataSourceUri::decodeSslMode( urlQuery.queryItemValue( "sslmode" ) );
  QString authConfigId = urlQuery.queryItemValue( "authcfg" );
  QString dbName = urlQuery.queryItemValue( "dbname" );
  QString service = urlQuery.queryItemValue( "service" );
  if ( !service.isEmpty() )
    postUri.connInfo.setConnection( service, dbName, username, password, sslMode, authConfigId );
  else
    postUri.connInfo.setConnection( host, port, dbName, username, password, sslMode, authConfigId );

  postUri.ownerName = urlQuery.queryItemValue( "schema" );
  postUri.projectName = urlQuery.queryItemValue( "project" );
  return postUri;
}
