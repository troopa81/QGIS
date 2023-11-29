// TODO cartouche/check lincese

// Copyright (C) 2017 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "parser/codemodel_fwd.h"
#include <abstractmetabuilder_p.h>
#include <parser/codemodel.h>
#include <clangparser/compilersupport.h>

#include <QtCore/QCoreApplication>
#include <QtCore/QCommandLineOption>
#include <QtCore/QCommandLineParser>
#include <QtCore/QDateTime>
#include <QtCore/QDebug>
#include <QtCore/QDir>
#include <QtCore/QDirIterator>
#include <QtCore/QFile>
#include <QtCore/QLibraryInfo>
#include <QtCore/QRegularExpression>
#include <QtCore/QVersionNumber>
#include <QtCore/QXmlStreamWriter>

#include <iostream>
#include <algorithm>
#include <iterator>
#include <qstringliteral.h>

using namespace Qt::StringLiterals;

static bool optJoinNamespaces = false;

static const QStringList sSkippedFunctions =
{
  QStringLiteral( "qt_getEnumMetaObject" ),
  QStringLiteral( "qt_getEnumName" )
};

static const QStringList sSkippedClasses =
{
  // Index -2 out of range for QgsDataItem::deleteLater()
  // Issue in pyside-setup/sources/shiboken6/generator/shiboken/cppgenerator.cpp:1463
  QStringLiteral( "QgsDataItem" ),

  QStringLiteral( "QgsSpatialIndexKDBush" ),
  QStringLiteral( "QgsSnappingConfig" ),

  QStringLiteral( "QMetaTypeId" ),
  QStringLiteral( "QPrivateSignal" ),
  QStringLiteral( "QTypeInfo" )
};

struct ModifiedFunction
{
  ModifiedFunction() {};
  ModifiedFunction( bool _isRemove ): isRemove( _isRemove ) {};

  bool isRemove = false;
};

static const QMap<QString, QMap<QString, ModifiedFunction>> sModifiedFunction =
{
  // remove because if SIP_RUN but needed to build a QgsFeature
  {
    "QgsFeature", {
      {"QgsFeature(QgsFeatureId)", ModifiedFunction( false ) }
    }
  },

  // QGsFeatureRequest::OrderBy
  // TODO should prefix with class name in case an other OrderBy is somewhere
  {
    "OrderBy", {
      {"value(qsizetype)const", ModifiedFunction( true )},
      {"resize(qsizetype)", ModifiedFunction( true )}
    }
  }
};


class TypeSystemGenerator
{
  public:
    TypeSystemGenerator( const QString &outputFileName, const QString &snippetFileName, const QString &classBlockListFileName );

    bool formatXmlOutput( const FileModelItem &dom );
    bool isValid() const;

  private:

    struct AddedFunction
    {
      QString returnType;
      QString signature;
      QString body;
      QString snippetName;
    };

    void formatXmlClass( const ClassModelItem &klass );
    void formatXmlEnum( const EnumModelItem &en, const QMap<QString, QString> &flags );
    void formatXmlScopeMembers( const ScopeModelItem &nsp );
    void formatXmlLocationComment( const CodeModelItem &i );
    void startXmlNamespace( const NamespaceModelItem &nsp );
    void formatXmlNamespaceMembers( const NamespaceModelItem &nsp );
    bool loadSkipRanges();
    bool loadSnippet( const QString &snippetFileName );
    bool loadClassBlockList( const QString& classBlockListFileName );
    bool loadInjectedCode();
    bool isSkipped( CodeModelItem item ) const;
    bool isQgis( CodeModelItem item ) const;
    bool isSkippedFunction( CodeModelItem item ) const;
    bool isSkippedClass( CodeModelItem item ) const;
    void addInjectCode( const QString &klass, const QString &signature, const QStringList &body );

    static bool isValueType( ClassModelItem klass );

    bool mValid = true;
    std::unique_ptr<QFile> mOutputFile;
    QString mOutputStr;
    std::unique_ptr<QXmlStreamWriter> mWriter;

    typedef QPair<int, int> SkipRange;
    typedef QList<SkipRange> SkipRanges;
    QMap<QString, SkipRanges> mSkipRanges;

    QMap<QString, QList<QString> > mNamespaceFiles;
    QMap<QString, QList<AddedFunction>> mAddedFunctions;
    QSet<QString> mSnippets;
    QSet<QString> mClassBlockList;
};

static inline QString languageLevelDescription()
{
  return u"C++ Language level (c++11..c++20, default="_s
         + QLatin1StringView( clang::languageLevelOption( clang::emulatedCompilerLanguageLevel() ) )
         + u')';
}

static void formatDebugOutput( const FileModelItem &dom, bool verbose )
{
  QString output;
  {
    QDebug debug( &output );
    if ( verbose )
      debug.setVerbosity( 3 );
    debug << dom.get();
  }
  std::cout << qPrintable( output ) << '\n';
}

static const char *primitiveTypes[] =
{
  "int", "unsigned", "short", "unsigned short", "long", "unsigned long",
  "float", "double"
};

static inline QString nameAttribute() { return QStringLiteral( "name" ); }

static bool useClass( const ClassModelItem &c )
{
  return c->classType() != CodeModel::Union && c->templateParameters().isEmpty()
         && !c->name().isEmpty(); // No anonymous structs
}

// Check whether a namespace is relevant for type system
// output, that is, has non template classes, functions or enumerations.
static bool hasMembers( const NamespaceModelItem &nsp )
{
  if ( !nsp->namespaces().isEmpty() || !nsp->enums().isEmpty()
       || !nsp->functions().isEmpty() )
  {
    return true;
  }
  const auto classes = nsp->classes();
  return std::any_of( classes.cbegin(), classes.cend(), useClass );
}

TypeSystemGenerator::TypeSystemGenerator( const QString &outputFileName, const QString &snippetFileName, const QString &classBlockListFileName )
{
  mOutputFile = std::make_unique<QFile>( outputFileName );
  if ( !mOutputFile->open( QIODevice::WriteOnly | QIODevice::Text ) )
  {
    mValid = false;
    qWarning() << "Fail to open file " << outputFileName;
  }

  if ( outputFileName.isEmpty() )
    mWriter = std::make_unique<QXmlStreamWriter>( &mOutputStr );
  else
    mWriter = std::make_unique<QXmlStreamWriter>( mOutputFile.get() );

  if ( mValid && !snippetFileName.isEmpty() )
    mValid = loadSnippet( snippetFileName );

  if ( mValid && !classBlockListFileName.isEmpty() )
    mValid = loadClassBlockList( classBlockListFileName );

  if ( mValid )
    mValid = loadSkipRanges();

  if ( mValid )
    mValid = loadInjectedCode();
}

bool TypeSystemGenerator::isValid() const
{
  return mValid;
}

void TypeSystemGenerator::formatXmlEnum( const EnumModelItem &en, const QMap<QString, QString> &flags )
{
  if ( isSkipped( en ) )
    return;

  mWriter->writeStartElement( u"enum-type"_s );
  mWriter->writeAttribute( nameAttribute(), en->name() );

  if ( flags.contains( en->name() ) )
    mWriter->writeAttribute( u"flags"_s, flags.value( en->name() ) );

  mWriter->writeEndElement();
}

void TypeSystemGenerator::formatXmlScopeMembers( const ScopeModelItem &nsp )
{
  for ( const auto &klass : nsp->classes() )
  {
    if ( useClass( klass ) )
      formatXmlClass( klass );
  }

  QMap<QString, QString> flags;
  if ( dynamic_cast<_ClassModelItem *>( nsp.get() ) )
  {
    const QRegularExpression reFlag( QStringLiteral( "^QFlags<(\\w+)::(\\w+)>$" ) );
    for ( const auto &td : nsp->typeDefs() )
    {
      const QRegularExpressionMatch match = reFlag.match( td->type().toString() );
      if ( match.hasMatch() && match.captured( 1 ) == nsp->name() )
      {
        flags[match.captured( 2 )] = td->name();
      }
    }
  }

  for ( const auto &en : nsp->enums() )
    formatXmlEnum( en, flags );

  if ( dynamic_cast<_ClassModelItem *>( nsp.get() ) )
  {
    const QMap<QString, ModifiedFunction> modifiedFunctions = sModifiedFunction.value( nsp->name() );
    for ( const auto &fct : nsp->functions() )
    {
      // we should check if arguments and return type (arguments() and type() methods)
      // if there are not skipped to avoid useless removing and plenty of warnings
      // to do this we should parse the dom before and build a map of skipped object and check it here
      // if all types are defined

      if ( !modifiedFunctions.contains( fct->typeSystemSignature() ) && isQgis( fct ) && isSkipped( fct ) && !isSkippedFunction( fct ) )
      {
        mWriter->writeStartElement( u"modify-function"_s );
        mWriter->writeAttribute( "signature", fct->typeSystemSignature() );
        mWriter->writeAttribute( "remove", "true" );
        mWriter->writeEndElement();
      }
    }

    QMapIterator<QString, ModifiedFunction> it( modifiedFunctions );
    while ( it.hasNext() )
    {
      it.next();
      mWriter->writeStartElement( u"modify-function"_s );
      mWriter->writeAttribute( "signature", it.key() );
      mWriter->writeAttribute( "remove", it.value().isRemove ? "true" : "false" );
      mWriter->writeEndElement();
    }
  }
}

void TypeSystemGenerator::formatXmlLocationComment( const CodeModelItem &i )
{
  QString comment;
  QTextStream( &comment ) << ' ' << i->fileName() << ':' << i->startLine() << ' ';
  mWriter->writeComment( comment );
}

void TypeSystemGenerator::formatXmlClass( const ClassModelItem &klass )
{
  if ( isSkipped( klass ) )
    return;

  // const QStringList allowedClass =
  // {
  //   QStringLiteral( "Qgis" ),

  //   QStringLiteral( "QgsQuadrilateral" )
  // };

  // if ( !allowedClass.contains( klass->name() ) && !allowedClass.contains( klass->enclosingScope()->name() ) )
  //   return;

  if ( mClassBlockList.contains( klass->name() ) || mClassBlockList.contains( klass->enclosingScope()->name() ) )
    return;

  // If there is at least one abstract method not skipped or no abstract method at all
  // shiboken will figure out the class is abstract and we wouldn't need to force abstract
  bool needForceAbstract = false;
  for ( auto fct : klass->functions() )
  {
    if ( fct->isAbstract() )
    {
      if ( isSkipped( fct ) )
      {
        needForceAbstract = true;
      }
      else
      {
        needForceAbstract = false;
        break;
      }
    }
  }

  formatXmlLocationComment( klass );
  mWriter->writeStartElement( isValueType( klass ) ? u"value-type"_s
                              : u"object-type"_s );
  mWriter->writeAttribute( nameAttribute(), klass->name() );

  if ( needForceAbstract )
  {
    mWriter->writeAttribute( u"force-abstract"_s, "true" );
    mWriter->writeAttribute( u"disable-wrapper"_s, "true" ); // see https://bugreports.qt.io/browse/PYSIDE-2445
  }

  formatXmlScopeMembers( klass );

  // write added function
  const _ClassModelItem *parentClass = dynamic_cast<const _ClassModelItem *>( klass->enclosingScope() );
  const QString klassName = ( parentClass ? parentClass->name() + "::" : QString() ) + klass->name();
  for ( AddedFunction func : mAddedFunctions.value( klassName ) )
  {
    mWriter->writeStartElement( u"add-function"_s );
    mWriter->writeAttribute( u"signature"_s, func.signature );
    if ( !func.returnType.isEmpty() && func.returnType != QStringLiteral( "Py_ssize_t" ) )
      mWriter->writeAttribute( u"return-type"_s, func.returnType );

    mWriter->writeStartElement( u"inject-code"_s );
    mWriter->writeAttribute( u"class"_s, u"target"_s );
    mWriter->writeAttribute( u"position"_s, u"beginning"_s );

    if ( !func.snippetName.isEmpty() )
    {
      // TODO configure snippet file correctly
      mWriter->writeAttribute( u"file"_s, "./qgis_core.cpp" );
      mWriter->writeAttribute( u"snippet"_s, func.snippetName );
    }
    else
    {
      mWriter->writeCDATA( func.body );
    }

    mWriter->writeEndElement();

    mWriter->writeEndElement();
  }

  mWriter->writeEndElement();

}

void TypeSystemGenerator::startXmlNamespace( const NamespaceModelItem &nsp )
{
  formatXmlLocationComment( nsp );
  mWriter->writeStartElement( u"namespace-type"_s );
  mWriter->writeAttribute( nameAttribute(), nsp->name() );

  // Hack : if we have a same namespace defined in several place, then only one
  // is kept by AbstractMetaBuilderPrivate::buildDom, so we add all needed include files
  // to fix this
  for ( QString fileName : mNamespaceFiles[nsp->name()] )
  {
    mWriter->writeStartElement( u"include"_s );
    mWriter->writeAttribute( u"file-name"_s, fileName );
    mWriter->writeEndElement();
  }
}

void TypeSystemGenerator::formatXmlNamespaceMembers( const NamespaceModelItem &nsp )
{
  if ( isSkipped( nsp ) )
    return;

  // TODO qgstriangularmesh is SIP_NO_FILE but not qgsmeshlayerinterpolator.h
  if ( nsp->name() == QStringLiteral( "QgsMeshUtils" ) )
    return;

  auto nestedNamespaces = nsp->namespaces();
  for ( auto i = nestedNamespaces.size() - 1; i >= 0; --i )
  {
    if ( !hasMembers( nestedNamespaces.at( i ) ) )
      nestedNamespaces.removeAt( i );
  }
  while ( !nestedNamespaces.isEmpty() )
  {
    auto current = nestedNamespaces.takeFirst();
    if ( isSkipped( current ) )
      continue;

    // TODO qgstriangularmesh is SIP_NO_FILE but not qgsmeshlayerinterpolator.h
    if ( current->name() == QStringLiteral( "QgsMeshUtils" ) )
      continue;

    startXmlNamespace( current );
    formatXmlNamespaceMembers( current );
    if ( optJoinNamespaces )
    {
      // Write out members of identical namespaces and remove
      const QString name = current->name();
      for ( qsizetype i = 0; i < nestedNamespaces.size(); )
      {
        if ( nestedNamespaces.at( i )->name() == name )
        {
          formatXmlNamespaceMembers( nestedNamespaces.at( i ) );
          nestedNamespaces.removeAt( i );
        }
        else
        {
          ++i;
        }
      }
    }
    mWriter->writeEndElement();
  }

  // already mapped, no need to explicitly describe them
  // for ( const auto &func : nsp->functions() )
  // {
  //   if ( isSkipped( func ) )
  //     continue;

  //   const QString signature = func->typeSystemSignature();
  //   if ( !signature.contains( u"operator" ) ) // Skip free operators
  //   {
  //     mWriter->writeStartElement( u"function"_s );
  //     mWriter->writeAttribute( u"signature"_s, signature );
  //     mWriter->writeEndElement();
  //   }
  // }
  formatXmlScopeMembers( nsp );
}

bool TypeSystemGenerator::formatXmlOutput( const FileModelItem &dom )
{
  if ( !mValid )
    return false;

  mWriter->setAutoFormatting( true );
  mWriter->writeStartDocument();
  mWriter->writeStartElement( u"typesystem"_s );
  mWriter->writeAttribute( u"package"_s, u"core"_s );
  mWriter->writeComment( u"Auto-generated "_s );
  for ( auto p : primitiveTypes )
  {
    mWriter->writeStartElement( u"primitive-type"_s );
    mWriter->writeAttribute( nameAttribute(), QLatin1StringView( p ) );
    mWriter->writeEndElement();
  }
  formatXmlNamespaceMembers( dom );
  mWriter->writeEndElement();
  mWriter->writeEndDocument();

  if ( !mOutputFile )
    std::cout << qPrintable( mOutputStr ) << '\n';

  return true;
}

bool TypeSystemGenerator::loadSkipRanges()
{
  // collect #ifndef SIP_RUN

  const QRegularExpression reIf( QStringLiteral( "^\\s*#if" ) );
  const QRegularExpression reEndIf( QStringLiteral( "^\\s*#endif" ) );
  const QRegularExpression reElse( QStringLiteral( "^\\s*#else" ) );
  const QRegularExpression reIfndef( QStringLiteral( "^\\s*#ifndef\\s*SIP_RUN" ) );
  const QRegularExpression reSipSkip( QStringLiteral( "SIP_SKIP" ) );
  const QRegularExpression reSipNoFile( QStringLiteral( "^\\s*#define\\s*SIP_NO_FILE" ) );
  const QRegularExpression reNamespace( QStringLiteral( "^\\s*namespace\\s*(\\w+)" ) );
  const QRegularExpression reMethodCode( QStringLiteral( "^\\s*% MethodCode" ) );
  const QRegularExpression reMethodCodeEnd( QStringLiteral( "^\\s*% End" ) );

  // TODO fix the path when we get the qgis dir as argument
  QString dir( QStringLiteral( "/home/julien/work/QGIS/.worktrees/qt-for-python-qt6/src/core" ) );
  QDirIterator it( dir, QStringList() << QStringLiteral( "*.h" ), QDir::Files, QDirIterator::Subdirectories );
  while ( it.hasNext() )
  {
    const QString fileName = it.next();
    QFile file( fileName );
    if ( !file.open( QIODevice::ReadOnly | QIODevice::Text ) )
    {
      qWarning() << "Error: failed to read file " << fileName;
      return false;
    }

    QTextStream in( &file );
    int numLine = 1;
    int sipRunLine = -1;
    int sipRunIf = -1;

    SkipRanges ranges;
    int nbIf = 0;
    QString methodCodeSignature;
    QStringList methodCodeBody;
    while ( !in.atEnd() )
    {
      QString line = in.readLine();

      if ( reSipNoFile.match( line ).hasMatch() )
      {
        ranges = SkipRanges();
        mSkipRanges[fileName] = ranges;
        break;
      }

      QRegularExpressionMatch match = reNamespace.match( line );
      if ( match.hasMatch() )
      {
        mNamespaceFiles[match.captured( 1 )].append( QFileInfo( fileName ).fileName() );
      }

      if ( reIfndef.match( line ).hasMatch() )
      {
        nbIf++;
        sipRunLine = numLine;
        sipRunIf = nbIf;
      }
      else if ( reIf.match( line ).hasMatch() )
      {
        nbIf++;
      }
      else if ( reEndIf.match( line ).hasMatch() )
      {
        if ( nbIf == sipRunIf && sipRunLine >= 0 )
        {
          ranges << SkipRange( sipRunLine, numLine );
          sipRunLine = -1;
          sipRunIf = -1;
        }

        nbIf--;
      }
      else if ( reElse.match( line ).hasMatch() && nbIf == sipRunIf && sipRunLine >= 0 )
      {
        ranges << SkipRange( sipRunLine, numLine );
        sipRunLine = -1;
        sipRunIf = -1;
      }
      else if ( reSipSkip.match( line ).hasMatch() )
      {
        ranges << SkipRange( numLine, numLine );
      }

      numLine++;
    }

    if ( !ranges.isEmpty() )
      mSkipRanges[ fileName ] = ranges;
  }

  return true;
}

bool TypeSystemGenerator::loadInjectedCode()
{
  const QRegularExpression reScope( QStringLiteral( "^\\s*(namespace|class|struct|enum|%MappedType)\\s+(\\w+).*" ) );
  const QRegularExpression reMethodCode( QStringLiteral( "^%MethodCode" ) );
  const QRegularExpression reDocString( QStringLiteral( "^%Docstring" ) );
  const QRegularExpression reEnd( QStringLiteral( "^%End" ) );
  const QRegularExpression rePublic( QStringLiteral( "^\\s*public:" ) );

  // TODO fix the path when we get the qgis dir as argument
  QString dir( QStringLiteral( "/home/julien/work/QGIS/.worktrees/qt-for-python-qt6/python/core/auto_generated" ) );
  QDirIterator it( dir, QStringList() << QStringLiteral( "*.sip.in" ), QDir::Files, QDirIterator::Subdirectories );
  while ( it.hasNext() )
  {
    const QString fileName = it.next();
    QFile file( fileName );
    if ( !file.open( QIODevice::ReadOnly | QIODevice::Text ) )
    {
      qWarning() << "Error: failed to read file " << fileName;
      return false;
    }

    QTextStream in( &file );
    QStringList scopes;
    QString previousLines;
    QString methodCodeSignature;
    QStringList methodCodeBody;
    bool isDocString = false;
    while ( !in.atEnd() )
    {
      QString line = in.readLine();

      if ( QRegularExpression( "(^|\\s+)};" ).match( line ).hasMatch() )
      {
        scopes.removeLast();
      }

      QRegularExpressionMatch matchScope = reScope.match( line );
      if ( matchScope.hasMatch() )
      {
        scopes << matchScope.captured( 2 );
      }

      if ( reDocString.match( line ).hasMatch() )
      {
        isDocString = true;
      }
      else if ( reMethodCode.match( line ).hasMatch() )
      {
        // remove // comments
        previousLines.replace( QRegularExpression( "\\/\\/.*" ), "" );
        previousLines.replace( "\n", "" );

        methodCodeSignature = previousLines;
        previousLines.clear();
      }
      else if ( reEnd.match( line ).hasMatch() )
      {
        if ( !methodCodeSignature.isEmpty() )
        {
          addInjectCode( scopes.join( "::" ), methodCodeSignature, methodCodeBody );
          methodCodeSignature.clear();
          methodCodeBody.clear();
          previousLines.clear();
        }

        isDocString = false;
      }
      else if ( !methodCodeSignature.isEmpty() )
      {
        methodCodeBody.append( line );
      }
      else if ( ( !isDocString && line.isEmpty() )
                || rePublic.match( line ).hasMatch() )
      {
        previousLines.clear();
      }
      else if ( !isDocString )
      {
        previousLines.append( line + "\n" );
      }
    }
  }

  return true;
}

bool TypeSystemGenerator::loadSnippet( const QString &snippetFileName )
{
  QFile file( snippetFileName );
  if ( !file.open( QIODevice::ReadOnly | QIODevice::Text ) )
  {
    qWarning() << "Error: failed to read snippet file " << snippetFileName;
    return false;
  }

  QTextStream in( &file );

  const QRegularExpression reSnippet( QStringLiteral( "^\\s*//\\s*@snippet\\s+(.*)" ) );

  while ( !in.atEnd() )
  {
    QString line = in.readLine();

    QRegularExpressionMatch match = reSnippet.match( line );
    if ( match.hasMatch() )
      mSnippets << match.captured( 1 );
  }

  return true;
}

bool TypeSystemGenerator::loadClassBlockList( const QString& classBlockListFileName )
{
  QFile file( classBlockListFileName );
  if ( !file.open( QIODevice::ReadOnly | QIODevice::Text ) )
  {
    qWarning() << "Error: failed to class block file file " << classBlockListFileName;
    return false;
  }

  QTextStream in( &file );
  while ( !in.atEnd() )
  {
    mClassBlockList << in.readLine();
  }

  return true;
}

bool TypeSystemGenerator::isQgis( CodeModelItem item ) const
{
  const QString fileName = item->fileName();

  // TODO fix the path when we get the qgis dir as argument
  return fileName.startsWith( QStringLiteral( "/home/julien/work/QGIS/.worktrees/qt-for-python-qt6/src/" ) );
}

bool TypeSystemGenerator::isSkippedFunction( CodeModelItem item ) const
{
  return dynamic_cast<_FunctionModelItem *>( item.get() ) && sSkippedFunctions.contains( item->name() );
}

bool TypeSystemGenerator::isSkippedClass( CodeModelItem item ) const
{
  _ClassModelItem *klass = dynamic_cast<_ClassModelItem *>( item.get() );
  return klass && ( sSkippedClasses.contains( klass->name() ) || klass->accessPolicy() == Access::Private );
}

bool TypeSystemGenerator::isSkipped( CodeModelItem item ) const
{
  const QString fileName = item->fileName();
  if ( fileName.isEmpty() )
    return false;

  if ( !isQgis( item ) )
    return true;

  if ( isSkippedFunction( item ) || isSkippedClass( item ) )
    return true;

  const int line = item->startLine();
  if ( mSkipRanges.contains( fileName ) )
  {
    const SkipRanges ranges = mSkipRanges[fileName];
    if ( ranges.isEmpty() ) // SIP_NO_FILE
      return true;

    for ( SkipRange range : ranges )
    {
      if ( range.first <= line && range.second >= line )
        return true;
    }
  }

  return false;
}

bool TypeSystemGenerator::isValueType( ClassModelItem klass )
{
  // not sure to get why but we get an error on QgsMapLayer::metadata() that it fails to find a
  // default constructor but there is one actually...
  if ( klass->name() == QStringLiteral( "QgsLayerMetadata" ) )
    return true;

  // Heuristics for value types:
  // value-type when :
  // - we don't inherit from QObject
  // - there is no abstract function
  // - A copy constructor exist (default or user defined)
  // TODO and maybe that child classes are not object type ?

  const auto functions = klass->functions();
  if ( klass->name() == QStringLiteral( "QObject" ) )
    return false;


  bool copyContructorDefined = false;
  bool defaultConstructorDeleted = false;
  for( const FunctionModelItem & f : functions )
  {
    if ( f->isAbstract()
         || ( f->functionType() == CodeModel::CopyConstructor && f->isDeleted() ) )
      return false;

    if ( f->functionType() == CodeModel::CopyConstructor )
      copyContructorDefined = true;

    if ( f->functionType() == CodeModel::Constructor && f->isDeleted() )
      defaultConstructorDeleted = true;
  }

  // as we delete the default contructor, the default constructor doesn't exist anymore
  if ( !copyContructorDefined && defaultConstructorDeleted )
    return false;


  for ( auto parent : klass->baseClasses() )
  {
    if ( parent.klass && !isValueType( parent.klass ) )
      return false;
  }

  return true;
}

void TypeSystemGenerator::addInjectCode( const QString &klass, const QString &signature, const QStringList &body )
{
  const QRegularExpression reSignature( QStringLiteral( "^\\s*(virtual\\s|)\\s*(static\\s|)(\\w*)\\s*(\\*|)\\s*(.*);" ) );
  const QRegularExpressionMatch matchSignature = reSignature.match( signature );

  if ( !matchSignature.hasMatch() )
  {
    qWarning() << "Error: fail to parse signature for class" << klass << ":" << signature;
    return;
  }

  AddedFunction func;

  func.signature = matchSignature.captured( 5 );
  func.signature.replace( "SIP_PYOBJECT", "PyObject*" );
  func.signature.replace( "SIP_PYLIST", "PyObject*" );

  // replace SIP anotation [..]
  func.signature.replace( QRegularExpression( "\\[.*\\]" ), "" );

  func.returnType = matchSignature.captured( 3 ) + matchSignature.captured( 4 );

  if ( func.returnType == QStringLiteral( "void" ) )
  {
    func.returnType.clear();
  }
  else if ( func.signature.contains( "__len__" ) )
  {
    func.returnType = QStringLiteral( "Py_ssize_t" );
  }
  else if ( func.signature.contains( "__setitem__" ) || func.signature.contains( "__contains__" ) )
  {
    func.returnType = QStringLiteral( "int" );
  }
  else
  {
    func.returnType.replace( "SIP_PYOBJECT", "PyObject*" );
    func.returnType.replace( "SIP_PYLIST", "PyObject*" );

    if ( !func.returnType.startsWith( "Qgs" ) && !func.returnType.startsWith( "Q" ) &&
         !( QStringList() << QStringLiteral( "long" ) << QStringLiteral( "PyObject*" ) << QStringLiteral( "int" ) << QStringLiteral( "bool" ) << QStringLiteral( "double" ) << QStringLiteral( "float" ) ).contains( func.returnType ) )
    {
      // wild guess, it's maybe an enum...
      func.returnType = klass + "::" + func.returnType;
    }
  }

  // remove sip annotation in args
  func.signature.replace( QRegularExpression( "\\/.*\\/" ), "" );

  // remove args name
  QRegularExpressionMatch reParams = QRegularExpression( "([^\\(]*)\\((.*)\\)" ).match( func.signature );
  if ( !reParams.hasMatch() )
  {
    qWarning() << "Error: fail to parse arguments for signature for class" << klass << ":" << signature;
    return;
  }

  // remove arg names in params
  QString params = reParams.captured( 2 );
  params.replace( QRegularExpression( "\\s*(const |)\\s*(unsigned |)\\s*(\\w+)\\s*(\\&|\\*|)\\s*(\\w+|)\\s*(=*\\s*[^,]*)\\s*(,*)" ), "\\1\\2\\3\\4\\6\\7" );
  func.signature = QString( "%1(%2)" ).arg( reParams.captured( 1 ) ).arg( params );

  // TODO looks like it's not possible to have a setitem with something different than than int as a a string
  // It's not done in Qt
  if ( func.signature.contains( QStringLiteral( "__setitem__" ) ) && func.signature != QStringLiteral( "__setitem__(int)" ) )
  {
    qWarning() << "Error: signature not supported : " << func.signature;
    return;
  }

  if ( func.signature.contains( QStringLiteral( "__setitem__" ) ) )
    func.signature = QStringLiteral( "__setitem__" ); // don't have to write the params, it expects int/PyObject

  const QString &snippetName = QString( "%1-%2" ).arg( klass, func.signature );
  if ( mSnippets.contains( snippetName ) )
  {
    func.snippetName = snippetName;
  }
  else
  {
    func.body = "\n" + body.join( "\n" ) + "\n";

    // TODO need to be wrapped
    if ( func.body.contains( QStringLiteral( "QgsPolylineXY" ) )
         || func.body.contains( QStringLiteral( "QgsPolygonXY" ) )
         || func.body.contains( QStringLiteral( "QgsMultiPolygonXY" ) )
         || func.body.contains( QStringLiteral( "QgsAbstractGeometry" ) )
         || func.body.contains( QStringLiteral( "QgsMultiPolylineXY" ) )
         || func.body.contains( QStringLiteral( "QVector<QgsPointXY>" ) )
       )
    {
      qWarning() << "Added function skipped (contains non binded type) in klass=" << klass << "signature:" << signature;
      return;
    }

    // const QString errorReturnedValue = func.returnType == "void" || func.returnType == "int" ? "-1" :
    //   ( func.returnType == "bool" ? "false" : "nullptr" );

    const QString errorReturnedValue( "nullptr" );
    func.body.replace( "sipIsErr = 1;", QString( "return %1;" ).arg( errorReturnedValue ) );

    if ( func.signature.contains( "__getitem__" ) )
    {
      func.body.replace( QStringLiteral( "a0" ), QStringLiteral( "_i" ) );
      func.body.replace( QStringLiteral( "sipRes =" ), QStringLiteral( "return" ) );
      func.body.replace( QRegularExpression( "(PyErr_SetString\\(.*\\);)" ), "\\1\nreturn 0;" );
    }

    for ( int i = 0; i < 7; i++ )
    {
      func.body.replace( QString( "a%1Wrapper == Py_None" ).arg( i ), QStringLiteral( "!%" ) + QString::number( i + 1 ) + QStringLiteral( ".isValid()" ) );
      func.body.replace( QString( "a%1->" ).arg( i ), QStringLiteral( "%" ) + QString::number( i + 1 ) + QStringLiteral( "." ) );
      func.body.replace( QString( "*a%1" ).arg( i ), QStringLiteral( "%" ) + QString::number( i + 1 ) );
      func.body.replace( QString( "a%1" ).arg( i ), QStringLiteral( "%" ) + QString::number( i + 1 ) ) ;
    }

    func.body.replace( "sipRes = true;", "Py_RETURN_TRUE;" );
    func.body.replace( "sipRes = false;", "Py_RETURN_FALSE;" );

    // TODO Should have been fixed before in commit which remove QVariant::Type
    func.body.replace( "QVariant( QMetaType::Int )", "QVariant( QMetaType( QMetaType::Int ) )" );

    if ( func.returnType == "Py_ssize_t" )
    {
      func.body.replace( "sipRes = ", "return " );
    }
    else if ( !func.returnType.isEmpty() && !func.signature.contains( "__repr__" ) )
    {
      // remove lines with sipFindType
      func.body.replace( QRegularExpression( ".*sipFindType\\(.*" ), "" );

      // replace sipConvertFromNewType( new XXX, sipTYpe_XXX, to ) with just new XXX (that would be replaced later with correct conversion)
      func.body.replace( QRegularExpression( "sipConvertFromNewType\\(\\s*(new\\s+.*),\\s*\\w+\\s*,\\s*\\w+\\s*\\)" ), "\\1" );

      // replace pointer return instruction
      func.body.replace( QRegularExpression( "([ ]*)sipRes = new\\s+(\\w+)(.*)" ), "\\1auto var = new \\2\\3\n\\1%PYARG_0 = %CONVERTTOPYTHON[\\2 *](var);" );

      // replace value return instruction
      func.body.replace( QRegularExpression( "([ ]*)sipRes =(.*)" ), QString( "\\1auto var = \\2\n\\1%PYARG_0 = %CONVERTTOPYTHON[%1](var);" ).arg( func.returnType ) );
    }
    func.body.replace( "sipRes", "%PYARG_0" );
    func.body.replace( "sipCpp", "%CPPSELF" );

    // need to be done manually
    if ( func.body.contains( QStringLiteral( "sip" ) ) )
    {
      qWarning() << "Added function skipped (contains SIP code) in klass=" << klass << "signature:" << signature;
      return;
    }
  }

  mAddedFunctions[klass].append( func );
}


static const char descriptionFormat[] = R"(
QGIS Type system generator

TODO update comments

Parses a C++ header and dumps out the classes found in typesystem XML syntax.
Arguments are arguments to the compiler the last of which should be the header
or source file.
It is recommended to create a .hh include file including the desired headers
and pass that along with the required include paths.

Based on Qt %1 and LibClang v%2.)";

int main( int argc, char **argv )
{
  QCoreApplication app( argc, argv );

  QCommandLineParser parser;
  parser.setSingleDashWordOptionMode( QCommandLineParser::ParseAsLongOptions );
  parser.setOptionsAfterPositionalArgumentsMode( QCommandLineParser::ParseAsPositionalArguments );
  const QString description =
    QString::fromLatin1( descriptionFormat ).arg( QLatin1StringView( qVersion() ),
        clang::libClangVersion().toString() );
  parser.setApplicationDescription( description );
  parser.addHelpOption();
  parser.addVersionOption();
  QCommandLineOption verboseOption( u"verbose"_s,
                                    u"Display verbose output about types"_s );
  parser.addOption( verboseOption );
  QCommandLineOption debugOption( u"debug"_s,
                                  u"Display debug output"_s );
  parser.addOption( debugOption );
  QCommandLineOption outputFileOption( u"output-file"_s,
                                       u"Output file. Default to stdout. Incompatible with debug option)"_s,
                                       u"file"_s );
  parser.addOption( outputFileOption );

  QCommandLineOption snippetFileOption( u"snippet-file"_s,
                                        u"Snippet file. Default to empty"_s,
                                        u"file"_s );
  parser.addOption( snippetFileOption );

  QCommandLineOption classBlockListFileOption( u"class-block-list-file"_s,
                                        u"class block list file. Default to empty"_s,
                                        u"file"_s );
  parser.addOption( classBlockListFileOption );

  QCommandLineOption joinNamespacesOption( {u"j"_s, u"join-namespaces"_s},
      u"Join namespaces"_s );
  parser.addOption( joinNamespacesOption );

  QCommandLineOption languageLevelOption( u"std"_s,
                                          languageLevelDescription(),
                                          u"level"_s );
  parser.addOption( languageLevelOption );
  parser.addPositionalArgument( u"argument"_s,
                                u"C++ compiler argument"_s,
                                u"argument(s)"_s );

  parser.process( app );
  const QStringList &positionalArguments = parser.positionalArguments();
  if ( positionalArguments.isEmpty() )
    parser.showHelp( 1 );

  QByteArrayList arguments;
  std::transform( positionalArguments.cbegin(), positionalArguments.cend(),
                  std::back_inserter( arguments ), QFile::encodeName );

  LanguageLevel level = LanguageLevel::Default;
  if ( parser.isSet( languageLevelOption ) )
  {
    const QByteArray value = parser.value( languageLevelOption ).toLatin1();
    level = clang::languageLevelFromOption( value.constData() );
    if ( level == LanguageLevel::Default )
    {
      std::cerr << "Invalid value \"" << value.constData()
                << "\" for language level option.\n";
      return -2;
    }
  }

  optJoinNamespaces = parser.isSet( joinNamespacesOption );

  QString outputFileName;
  if ( parser.isSet( outputFileOption ) )
  {
    outputFileName = parser.value( outputFileOption ).toUtf8();
  }

  QString snippetFileName;
  if ( parser.isSet( snippetFileOption ) )
  {
    snippetFileName = parser.value( snippetFileOption ).toUtf8();
  }

  QString classBlockListFileName;
  if ( parser.isSet( classBlockListFileOption ) )
  {
    classBlockListFileName = parser.value( classBlockListFileOption ).toUtf8();
  }

  TypeSystemGenerator generator( outputFileName, snippetFileName, classBlockListFileName );
  if ( !generator.isValid() )
    return -3;

  const FileModelItem dom = AbstractMetaBuilderPrivate::buildDom( arguments, true, level, 0 );
  if ( !dom )
  {
    QString message = u"Unable to parse "_s + positionalArguments.join( u' ' );
    std::cerr << qPrintable( message ) << '\n';
    return -2;
  }

  if ( parser.isSet( debugOption ) )
    formatDebugOutput( dom, parser.isSet( verboseOption ) );
  else
  {
    if ( !generator.formatXmlOutput( dom ) )
      return -4;
  }

  return 0;
}
