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
    ModifiedFunction(){};
    ModifiedFunction( bool _isRemove ):isRemove(_isRemove){};

    bool isRemove = false;
};

static const QMap<QPair<QString, QString>, ModifiedFunction> sModifiedFunction =
{
  {{"QgsFeature","QgsFeature(QgsFeatureId)"}, ModifiedFunction( false ) }
};

static const QStringList sUnSkippedClasses =
{
  // Because of https://bugreports.qt.io/browse/PYSIDE-2445
  // force-abstract="true" is not enough because shiboken has to implement
  // the pure virtual method in is wrapper but it can't if all the argument type don't exist
  QStringLiteral( "QgsTopologicalMesh" ),
  QStringLiteral( "TopologicalFaces")
};


class TypeSystemGenerator
{
  public:
    TypeSystemGenerator( const QString &outputFileName );

    bool formatXmlOutput( const FileModelItem &dom );
    bool isValid() const;

  private:

    void formatXmlClass( const ClassModelItem &klass );
    void formatXmlEnum( const EnumModelItem &en, const QMap<QString, QString> &flags );
    void formatXmlScopeMembers( const ScopeModelItem &nsp );
    void formatXmlLocationComment( const CodeModelItem &i );
    void startXmlNamespace( const NamespaceModelItem &nsp );
    void formatXmlNamespaceMembers( const NamespaceModelItem &nsp );
    bool loadSkipRanges();
    bool isSkipped( CodeModelItem item ) const;
    bool isQgis( CodeModelItem item ) const;
    bool isSkippedFunction( CodeModelItem item ) const;
    bool isSkippedClass( CodeModelItem item ) const;

    static bool isValueType( ClassModelItem klass );

    bool mValid = true;
    std::unique_ptr<QFile> mOutputFile;
    QString mOutputStr;
    std::unique_ptr<QXmlStreamWriter> mWriter;

    typedef QPair<int, int> SkipRange;
    typedef QList<SkipRange> SkipRanges;
    QMap<QString, SkipRanges> mSkipRanges;

    QMap<QString, QList<QString> > mNamespaceFiles;
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

TypeSystemGenerator::TypeSystemGenerator( const QString &outputFileName )
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

  if ( mValid )
    mValid = loadSkipRanges();
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
      if ( match.hasMatch() && match.captured(1) == nsp->name() )
      {
        flags[match.captured(2)] = td->name();
      }
    }
  }

  for ( const auto &en : nsp->enums() )
    formatXmlEnum( en, flags );

  if ( dynamic_cast<_ClassModelItem *>( nsp.get() ) )
  {
    for ( const auto &fct : nsp->functions() )
    {

      const QPair<QString,QString> key { nsp->name(), fct->typeSystemSignature() };
      if ( sModifiedFunction.contains( key ) )
      {
        const ModifiedFunction modifiedFunction = sModifiedFunction.value( key );

        mWriter->writeStartElement( u"modify-function"_s );
        mWriter->writeAttribute( "signature", fct->typeSystemSignature() );
        mWriter->writeAttribute( "remove", modifiedFunction.isRemove ? "true" : "false" );
        mWriter->writeEndElement();
      }

      // we should check if arguments and return type (arguments() and type() methods)
      // if there are not skipped to avoid useless removing and plenty of warnings
      // to do this we should parse the dom before and build a map of skipped object and check it here
      // if all types are defined

      else if ( isQgis( fct ) && isSkipped( fct ) && !isSkippedFunction( fct ) )
      {
        mWriter->writeStartElement( u"modify-function"_s );
        mWriter->writeAttribute( "signature", fct->typeSystemSignature() );
        mWriter->writeAttribute( "remove", "true" );
        mWriter->writeEndElement();
      }
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

  const QStringList allowedClass =
  {
    QStringLiteral( "Qgis" ),
    QStringLiteral( "QgsAttributes" ),
    QStringLiteral( "QgsCoordinateReferenceSystem" ),
    QStringLiteral( "QgsDataProvider" ),
    QStringLiteral( "QgsField" ),
    QStringLiteral( "QgsFields" ),
    QStringLiteral( "QgsImageFetcher" ),
    QStringLiteral( "QgsMeshUtils" ),
    QStringLiteral( "QgsRasterAttributeTable" ),
    QStringLiteral( "QgsRectangle" ),
    QStringLiteral( "QgsRasterInterface" ),
    QStringLiteral( "QgsRasterBlock" ),
    QStringLiteral( "QgsRasterBlockFeedback" ),
    QStringLiteral( "QgsRasterDataProvider" ),
    QStringLiteral( "QgsRasterAttributeTableModel" )
  };

  if ( !allowedClass.contains( klass->name() ) && !allowedClass.contains( klass->enclosingScope()->name() ) )
    return;

  formatXmlLocationComment( klass );
  mWriter->writeStartElement( isValueType( klass ) ? u"value-type"_s
                              : u"object-type"_s );
  mWriter->writeAttribute( nameAttribute(), klass->name() );
  formatXmlScopeMembers( klass );
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
    const QRegularExpression reIf( QStringLiteral( "^\\s*#if" ) );
    const QRegularExpression reEndIf( QStringLiteral( "^\\s*#endif" ) );
    const QRegularExpression reElse( QStringLiteral( "^\\s*#else" ) );
    const QRegularExpression reIfndef( QStringLiteral( "^\\s*#ifndef\\s*SIP_RUN" ) );
    const QRegularExpression reSipSkip( QStringLiteral( "SIP_SKIP" ) );
    const QRegularExpression reSipNoFile( QStringLiteral( "^\\s*#define\\s*SIP_NO_FILE" ) );
    const QRegularExpression reNamespace( QStringLiteral( "^\\s*namespace\\s*(\\w+)" ) );

    SkipRanges ranges;
    int nbIf = 0;
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

  // Ugly ... but don't see any other way to do it for some issues
  if ( sUnSkippedClasses.contains( item->name() ) )
    return false;

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
  // Heuristics for value types:
  // value-type when we don't inherit from QObject and there is no abstract function
  // TODO we should check that the default copycontructor is not delete
  // and maybe that child classes are not object type

  const auto functions = klass->functions();
  if ( klass->name() == QStringLiteral( "QObject" )
       || std::any_of( functions.cbegin(), functions.cend(),
                       []( const FunctionModelItem & f )
{
  return f->isAbstract();
  } ) )
  return false;


  for ( auto parent : klass->baseClasses() )
  {
    if ( parent.klass && !isValueType( parent.klass ) )
      return false;
  }

  return true;
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

  TypeSystemGenerator generator( outputFileName );
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
