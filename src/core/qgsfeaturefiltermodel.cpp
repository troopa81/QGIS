/***************************************************************************
  qgsfeaturefiltermodel.cpp - QgsFeatureFilterModel
 ---------------------
 begin                : 10.3.2017
 copyright            : (C) 2017 by Matthias Kuhn
 email                : matthias@opengis.ch
 ***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/
#include "qgsfeaturefiltermodel.h"
#include "qgsfeaturefiltermodel_p.h"

#include "qgsvectorlayer.h"
#include "qgsconditionalstyle.h"
#include "qgsapplication.h"
#include "qgssettings.h"


bool qVariantListCompare( const QVariantList &a, const QVariantList &b )
{
  if ( a.size() != b.size() )
    return false;

  for ( int i = 0; i < a.size(); ++i )
  {
    if ( !qgsVariantEqual( a.at( i ), b.at( i ) ) )
      return false;
  }
  return true;
}

QgsFeatureFilterModel::QgsFeatureFilterModel( QObject *parent )
  : QAbstractItemModel( parent )
{
  mReloadTimer.setInterval( 100 );
  mReloadTimer.setSingleShot( true );
  connect( &mReloadTimer, &QTimer::timeout, this, &QgsFeatureFilterModel::scheduledReload );
  setExtraIdentifierValuesUnguarded( QVariantList() );
}

QgsFeatureFilterModel::~QgsFeatureFilterModel()
{
  if ( mGatherer )
    connect( mGatherer, &QgsFieldExpressionValuesGatherer::finished, mGatherer, &QgsFieldExpressionValuesGatherer::deleteLater );
}

QgsVectorLayer *QgsFeatureFilterModel::sourceLayer() const
{
  return mSourceLayer;
}

void QgsFeatureFilterModel::setSourceLayer( QgsVectorLayer *sourceLayer )
{
  if ( mSourceLayer == sourceLayer )
    return;

  mSourceLayer = sourceLayer;
  mExpressionContext = sourceLayer->createExpressionContext();
  reload();
  emit sourceLayerChanged();
}

QString QgsFeatureFilterModel::displayExpression() const
{
  return mDisplayExpression.expression();
}

void QgsFeatureFilterModel::setDisplayExpression( const QString &displayExpression )
{
  if ( mDisplayExpression.expression() == displayExpression )
    return;

  mDisplayExpression = QgsExpression( displayExpression );
  reload();
  emit displayExpressionChanged();
}

QString QgsFeatureFilterModel::filterValue() const
{
  return mFilterValue;
}

void QgsFeatureFilterModel::setFilterValue( const QString &filterValue )
{
  if ( mFilterValue == filterValue )
    return;

  mFilterValue = filterValue;
  reload();
  emit filterValueChanged();
}

QString QgsFeatureFilterModel::filterExpression() const
{
  return mFilterExpression;
}

void QgsFeatureFilterModel::setFilterExpression( const QString &filterExpression )
{
  if ( mFilterExpression == filterExpression )
    return;

  mFilterExpression = filterExpression;
  reload();
  emit filterExpressionChanged();
}

bool QgsFeatureFilterModel::isLoading() const
{
  return mGatherer;
}

QString QgsFeatureFilterModel::identifierField() const
{
  return mIdentifierFields.value( 0 );
}

QModelIndex QgsFeatureFilterModel::index( int row, int column, const QModelIndex &parent ) const
{
  Q_UNUSED( parent )
  return createIndex( row, column, nullptr );
}

QModelIndex QgsFeatureFilterModel::parent( const QModelIndex &child ) const
{
  Q_UNUSED( child )
  return QModelIndex();
}

int QgsFeatureFilterModel::rowCount( const QModelIndex &parent ) const
{
  Q_UNUSED( parent )

  return mEntries.size();
}

int QgsFeatureFilterModel::columnCount( const QModelIndex &parent ) const
{
  Q_UNUSED( parent )
  return 1;
}

QVariant QgsFeatureFilterModel::data( const QModelIndex &index, int role ) const
{
  if ( !index.isValid() )
    return QVariant();

  switch ( role )
  {
    case Qt::DisplayRole:
    case Qt::EditRole:
    case ValueRole:
      return mEntries.value( index.row() ).value;

    case IdentifierValueRole:
    {
      const QVariantList values = mEntries.value( index.row() ).identifierValues;
      return values.value( 0 );
    }

    case IdentifierValuesRole:
      return mEntries.value( index.row() ).identifierValues;

    case Qt::BackgroundColorRole:
    case Qt::TextColorRole:
    case Qt::DecorationRole:
    case Qt::FontRole:
    {
      bool isNull = true;
      const QVariantList values = mEntries.value( index.row() ).identifierValues;
      for ( const QVariant &value : values )
      {
        if ( !value.isNull() )
        {
          isNull = false;
          break;
        }
      }
      if ( isNull )
      {
        // Representation for NULL value
        if ( role == Qt::TextColorRole )
        {
          return QBrush( QColor( Qt::gray ) );
        }
        if ( role == Qt::FontRole )
        {
          QFont font = QFont();
          if ( index.row() == mExtraIdentifierValueIndex )
            font.setBold( true );
          else
            font.setItalic( true );
          return font;
        }
      }
      else
      {
        // Respect conditional style
        const QgsConditionalStyle style = featureStyle( mEntries.value( index.row() ).feature );

        if ( style.isValid() )
        {
          if ( role == Qt::BackgroundColorRole && style.validBackgroundColor() )
            return style.backgroundColor();
          if ( role == Qt::TextColorRole && style.validTextColor() )
            return style.textColor();
          if ( role == Qt::DecorationRole )
            return style.icon();
          if ( role == Qt::FontRole )
            return style.font();
        }
      }
      break;
    }
  }

  return QVariant();
}

void QgsFeatureFilterModel::updateCompleter()
{
  emit beginUpdate();

  QgsFieldExpressionValuesGatherer *gatherer = qobject_cast<QgsFieldExpressionValuesGatherer *>( sender() );
  Q_ASSERT( gatherer );
  if ( gatherer->wasCanceled() )
  {
    delete gatherer;
    return;
  }

  QVector<Entry> entries = mGatherer->entries();

  if ( mExtraIdentifierValueIndex == -1 )
  {
    setExtraIdentifierValuesUnguarded( QVariantList() );
  }

  // Only reloading the current entry?
  bool reloadCurrentFeatureOnly = mGatherer->data().toBool();
  if ( reloadCurrentFeatureOnly )
  {
    if ( !entries.isEmpty() )
    {
      mEntries.replace( mExtraIdentifierValueIndex, entries.at( 0 ) );
      emit dataChanged( index( mExtraIdentifierValueIndex, 0, QModelIndex() ), index( mExtraIdentifierValueIndex, 0, QModelIndex() ) );
      mShouldReloadCurrentFeature = false;
      setExtraValueDoesNotExist( false );
    }
    else
    {
      setExtraValueDoesNotExist( true );
    }

    mKeepCurrentEntry = true;
    mShouldReloadCurrentFeature = false;

    if ( mFilterValue.isEmpty() )
      reload();
  }
  else
  {
    // We got strings for a filter selection
    std::sort( entries.begin(), entries.end(), []( const Entry & a, const Entry & b ) { return a.value.localeAwareCompare( b.value ) < 0; } );

    if ( mAllowNull )
    {
      entries.prepend( nullEntry() );
    }

    const int newEntriesSize = entries.size();

    // fixed entry is either NULL or extra value
    const int nbFixedEntry = ( mKeepCurrentEntry ? 1 : 0 ) + ( mAllowNull ? 1 : 0 );

    // Find the index of the current entry in the new list
    int currentEntryInNewList = -1;
    if ( mExtraIdentifierValueIndex != -1 && mExtraIdentifierValueIndex < mEntries.count() )
    {
      for ( int i = 0; i < newEntriesSize; ++i )
      {
        if ( qVariantListCompare( entries.at( i ).identifierValues, mEntries.at( mExtraIdentifierValueIndex ).identifierValues ) )
        {
          mEntries.replace( mExtraIdentifierValueIndex, entries.at( i ) );
          currentEntryInNewList = i;
          setExtraValueDoesNotExist( false );
          break;
        }
      }
    }

    int firstRow = 0;

    // Move current entry to the first position if this is a fixed entry or because
    // the entry exists in the new list
    if ( mExtraIdentifierValueIndex > -1 && ( mExtraIdentifierValueIndex < nbFixedEntry || currentEntryInNewList != -1 ) )
    {
      if ( mExtraIdentifierValueIndex != 0 )
      {
        beginMoveRows( QModelIndex(), mExtraIdentifierValueIndex, mExtraIdentifierValueIndex, QModelIndex(), 0 );
        mEntries.move( mExtraIdentifierValueIndex, 0 );
        endMoveRows();
      }
      firstRow = 1;
    }

    // Remove all entries (except for extra entry if existent)
    beginRemoveRows( QModelIndex(), firstRow, mEntries.size() - firstRow );
    mEntries.remove( firstRow, mEntries.size() - firstRow );

    // if we remove all rows before endRemoveRows, setExtraIdentifierValuesUnguarded will be called
    // and a null value will be added to mEntries, so we block setExtraIdentifierValuesUnguarded call

    mIsSettingExtraIdentifierValue = true;
    endRemoveRows();
    mIsSettingExtraIdentifierValue = false;


    if ( currentEntryInNewList == -1 )
    {
      beginInsertRows( QModelIndex(), firstRow, entries.size() + 1 );
      mEntries += entries;
      endInsertRows();

      // if all entries have been cleaned (firstRow == 0)
      // and there is a value in entries, prefer this value over NULL
      // else chose the first one (the previous one)
      setExtraIdentifierValuesIndex( firstRow == 0 && mAllowNull && !entries.isEmpty() ? 1 : 0, firstRow == 0 );
    }
    else
    {
      if ( currentEntryInNewList != 0 )
      {
        beginInsertRows( QModelIndex(), 0, currentEntryInNewList - 1 );
        mEntries = entries.mid( 0, currentEntryInNewList ) + mEntries;
        endInsertRows();
      }
      else
      {
        mEntries.replace( 0, entries.at( 0 ) );
      }

      // don't notify for a change if it's a fixed entry
      if ( currentEntryInNewList >= nbFixedEntry )
      {
        emit dataChanged( index( currentEntryInNewList, 0, QModelIndex() ), index( currentEntryInNewList, 0, QModelIndex() ) );
      }

      beginInsertRows( QModelIndex(), currentEntryInNewList + 1, newEntriesSize - currentEntryInNewList - 1 );
      mEntries += entries.mid( currentEntryInNewList + 1 );
      endInsertRows();
      setExtraIdentifierValuesIndex( currentEntryInNewList );
    }

    emit filterJobCompleted();

    mKeepCurrentEntry = false;
  }
  emit endUpdate();


  // scheduleReload and updateCompleter lives in the same thread so if the gatherer hasn't been stopped
  // (checked before), mGatherer still references the current gatherer
  Q_ASSERT( gatherer == mGatherer );
  delete mGatherer;
  mGatherer = nullptr;
  emit isLoadingChanged();
}

void QgsFeatureFilterModel::scheduledReload()
{
  if ( !mSourceLayer )
    return;

  bool wasLoading = false;

  if ( mGatherer )
  {
    mGatherer->stop();
    wasLoading = true;
  }

  QgsFeatureRequest request;

  if ( mShouldReloadCurrentFeature )
  {
    QStringList conditions;
    for ( int i = 0; i < mIdentifierFields.count(); i++ )
    {
      if ( i >= mExtraIdentifierValues.count() )
      {
        conditions << QgsExpression::createFieldEqualityExpression( mIdentifierFields.at( i ), QVariant() );
      }
      else
      {
        conditions << QgsExpression::createFieldEqualityExpression( mIdentifierFields.at( i ), mExtraIdentifierValues.at( i ) );
      }
    }
    request.setFilterExpression( conditions.join( QStringLiteral( " AND " ) ) );
  }
  else
  {
    QString filterClause;

    if ( mFilterValue.isEmpty() && !mFilterExpression.isEmpty() )
      filterClause = mFilterExpression;
    else if ( mFilterExpression.isEmpty() && !mFilterValue.isEmpty() )
      filterClause = QStringLiteral( "(%1) ILIKE '%%2%'" ).arg( mDisplayExpression, mFilterValue );
    else if ( !mFilterExpression.isEmpty() && !mFilterValue.isEmpty() )
      filterClause = QStringLiteral( "(%1) AND ((%2) ILIKE '%%3%')" ).arg( mFilterExpression, mDisplayExpression, mFilterValue );

    if ( !filterClause.isEmpty() )
      request.setFilterExpression( filterClause );
  }
  QSet<QString> attributes;
  if ( request.filterExpression() )
    attributes = request.filterExpression()->referencedColumns();
  for ( const QString &fieldName : qgis::as_const( mIdentifierFields ) )
    attributes << fieldName;
  request.setSubsetOfAttributes( attributes, mSourceLayer->fields() );
  request.setFlags( QgsFeatureRequest::NoGeometry );

  request.setLimit( QgsSettings().value( QStringLiteral( "maxEntriesRelationWidget" ), 100, QgsSettings::Gui ).toInt() );

  mGatherer = new QgsFieldExpressionValuesGatherer( mSourceLayer, mDisplayExpression, mIdentifierFields, request );
  mGatherer->setData( mShouldReloadCurrentFeature );
  connect( mGatherer, &QgsFieldExpressionValuesGatherer::finished, this, &QgsFeatureFilterModel::updateCompleter );


  mGatherer->start();
  if ( !wasLoading )
    emit isLoadingChanged();
}

QSet<QString> QgsFeatureFilterModel::requestedAttributes() const
{
  QSet<QString> requestedAttrs;

  const auto rowStyles = mSourceLayer->conditionalStyles()->rowStyles();

  for ( const QgsConditionalStyle &style : rowStyles )
  {
    QgsExpression exp( style.rule() );
    requestedAttrs += exp.referencedColumns();
  }

  if ( mDisplayExpression.isField() )
  {
    QString fieldName = *mDisplayExpression.referencedColumns().constBegin();
    const auto constFieldStyles = mSourceLayer->conditionalStyles()->fieldStyles( fieldName );
    for ( const QgsConditionalStyle &style : constFieldStyles )
    {
      QgsExpression exp( style.rule() );
      requestedAttrs += exp.referencedColumns();
    }
  }

  return requestedAttrs;
}

void QgsFeatureFilterModel::setExtraIdentifierValuesIndex( int index, bool force )
{
  if ( mExtraIdentifierValueIndex == index && !force )
    return;

  mExtraIdentifierValueIndex = index;
  emit extraIdentifierValueIndexChanged( index );
}

void QgsFeatureFilterModel::reloadCurrentFeature()
{
  mShouldReloadCurrentFeature = true;
  mReloadTimer.start();
}

void QgsFeatureFilterModel::setExtraIdentifierValuesUnguarded( const QVariantList &extraIdentifierValues )
{
  const QVector<Entry> entries = mEntries;

  int index = 0;
  for ( const Entry &entry : entries )
  {
    if ( entry.identifierValues == extraIdentifierValues )
    {
      setExtraIdentifierValuesIndex( index );
      break;
    }

    index++;
  }

  // Value not found in current entries
  if ( mExtraIdentifierValueIndex != index )
  {
    if ( !extraIdentifierValues.isEmpty() || mAllowNull )
    {
      beginInsertRows( QModelIndex(), 0, 0 );
      if ( !extraIdentifierValues.isEmpty() )
      {
        QStringList values;
        for ( const QVariant &v : qgis::as_const( extraIdentifierValues ) )
          values << QStringLiteral( "(%1)" ).arg( v.toString() );

        mEntries.prepend( Entry( extraIdentifierValues, values.join( QStringLiteral( " " ) ), QgsFeature() ) );
        reloadCurrentFeature();
      }
      else
      {
        mEntries.prepend( nullEntry() );
      }
      endInsertRows();

      setExtraIdentifierValuesIndex( 0, true );
    }
  }
}

QgsFeatureFilterModel::Entry QgsFeatureFilterModel::nullEntry()
{
  return Entry( QVariantList(), QgsApplication::nullRepresentation(), QgsFeature() );
}

QgsConditionalStyle QgsFeatureFilterModel::featureStyle( const QgsFeature &feature ) const
{
  if ( !mSourceLayer )
    return QgsConditionalStyle();

  QgsVectorLayer *layer = mSourceLayer;
  QgsFeatureId fid = feature.id();
  mExpressionContext.setFeature( feature );

  auto styles = QgsConditionalStyle::matchingConditionalStyles( layer->conditionalStyles()->rowStyles(), QVariant(),  mExpressionContext );

  if ( mDisplayExpression.referencedColumns().count() == 1 )
  {
    // Style specific for this field
    QString fieldName = *mDisplayExpression.referencedColumns().constBegin();
    const auto allStyles = layer->conditionalStyles()->fieldStyles( fieldName );
    const auto matchingFieldStyles = QgsConditionalStyle::matchingConditionalStyles( allStyles, feature.attribute( fieldName ),  mExpressionContext );

    styles += matchingFieldStyles;
  }

  QgsConditionalStyle style;
  style = QgsConditionalStyle::compressStyles( styles );
  mEntryStylesMap.insert( fid, style );

  return style;
}

bool QgsFeatureFilterModel::allowNull() const
{
  return mAllowNull;
}

void QgsFeatureFilterModel::setAllowNull( bool allowNull )
{
  if ( mAllowNull == allowNull )
    return;

  mAllowNull = allowNull;
  emit allowNullChanged();

  reload();
}

bool QgsFeatureFilterModel::extraValueDoesNotExist() const
{
  return mExtraValueDoesNotExist;
}

void QgsFeatureFilterModel::setExtraValueDoesNotExist( bool extraValueDoesNotExist )
{
  if ( mExtraValueDoesNotExist == extraValueDoesNotExist )
    return;

  mExtraValueDoesNotExist = extraValueDoesNotExist;
  emit extraValueDoesNotExistChanged();
}

int QgsFeatureFilterModel::extraIdentifierValueIndex() const
{
  return mExtraIdentifierValueIndex;
}

QStringList QgsFeatureFilterModel::identifierFields() const
{
  return mIdentifierFields;
}

void QgsFeatureFilterModel::setIdentifierField( const QString &identifierField )
{
  setIdentifierFields( QStringList() << identifierField );
}

void QgsFeatureFilterModel::setIdentifierFields( const QStringList &identifierFields )
{
  if ( mIdentifierFields == identifierFields )
    return;

  mIdentifierFields = identifierFields;
  emit identifierFieldChanged();
  setExtraIdentifierValues( QVariantList() );
}

void QgsFeatureFilterModel::reload()
{
  mReloadTimer.start();
}

QVariant QgsFeatureFilterModel::extraIdentifierValue() const
{
  if ( mExtraIdentifierValues.isEmpty() )
    return QVariant();
  else
    return  mExtraIdentifierValues.at( 0 );
}

QVariantList QgsFeatureFilterModel::extraIdentifierValues() const
{
  if ( mExtraIdentifierValues.count() != mIdentifierFields.count() )
  {
    QVariantList nullValues;
    for ( int i = 0; i < mIdentifierFields.count(); i++ )
      nullValues << QVariant( QVariant::Int );
    return nullValues;
  }
  return mExtraIdentifierValues;
}

void QgsFeatureFilterModel::setExtraIdentifierValue( const QVariant &extraIdentifierValue )
{
  if ( extraIdentifierValue.isNull() )
    setExtraIdentifierValues( QVariantList() );
  else
    setExtraIdentifierValues( QVariantList() << extraIdentifierValue );
}

void QgsFeatureFilterModel::setExtraIdentifierValues( const QVariantList &extraIdentifierValues )
{
  if ( extraIdentifierValues == mExtraIdentifierValues && !mExtraIdentifierValues.isEmpty() )
    return;

  if ( mIsSettingExtraIdentifierValue )
    return;

  mIsSettingExtraIdentifierValue = true;

  mExtraIdentifierValues = extraIdentifierValues;

  setExtraIdentifierValuesUnguarded( extraIdentifierValues );

  mIsSettingExtraIdentifierValue = false;

  emit extraIdentifierValueChanged();
}

void QgsFeatureFilterModel::setExtraIdentifierValuesToNull()
{
  mExtraIdentifierValues = QVariantList();
}
