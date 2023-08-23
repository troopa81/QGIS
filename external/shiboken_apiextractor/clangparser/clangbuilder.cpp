// Copyright (C) 2017 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "clangbuilder.h"
#include "compilersupport.h"
#include "clangutils.h"

#include <codemodel.h>
#include <reporthandler.h>

#include "qtcompat.h"

#include <QtCore/QDebug>
#include <QtCore/QDir>
#include <QtCore/QDirIterator>
#include <QtCore/QHash>
#include <QtCore/QMap>
#include <QtCore/QRegularExpression>
#include <QtCore/QString>
#include <QtCore/QStack>
#include <QtCore/QList>

#include <cstring>
#include <ctype.h>

using namespace Qt::StringLiterals;

namespace clang
{

  static inline QString templateBrackets() { return QStringLiteral( "<>" ); }

  static inline bool isClassCursor( const CXCursor &c )
  {
    return c.kind == CXCursor_ClassDecl || c.kind == CXCursor_StructDecl
           || c.kind == CXCursor_ClassTemplate
           || c.kind == CXCursor_ClassTemplatePartialSpecialization;
  }

  static inline bool isClassOrNamespaceCursor( const CXCursor &c )
  {
    return c.kind == CXCursor_Namespace || isClassCursor( c );
  }

  static inline bool withinClassDeclaration( const CXCursor &cursor )
  {
    return isClassCursor( clang_getCursorLexicalParent( cursor ) );
  }

  static QString fixTypeName( QString t )
  {
    // Fix "Foo &" -> "Foo&", similarly "Bar **" -> "Bar**"
    auto pos = t.size() - 1;
    for ( ; pos >= 0 && ( t.at( pos ) == u'&' || t.at( pos ) == u'*' ); --pos ) {}
    if ( pos > 0 && t.at( pos ) == u' ' )
      t.remove( pos, 1 );
    return t;
  }

// Insert template parameter to class name: "Foo<>" -> "Foo<T1>" -> "Foo<T1,T2>"
// This needs to be done immediately when template parameters are encountered since
// the class name "Foo<T1,T2>" is the scope for nested items.
  static bool insertTemplateParameterIntoClassName( const QString &parmName, QString *name )
  {
    if ( Q_UNLIKELY( !name->endsWith( u'>' ) ) )
      return false;
    const bool needsComma = name->at( name->size() - 2 ) != u'<';
    const auto insertionPos = name->size() - 1;
    name->insert( insertionPos, parmName );
    if ( needsComma )
      name->insert( insertionPos, u',' );
    return true;
  }

  static inline bool insertTemplateParameterIntoClassName( const QString &parmName,
      const ClassModelItem &item )
  {
    QString name = item->name();
    const bool result = insertTemplateParameterIntoClassName( parmName, &name );
    item->setName( name );
    return result;
  }

  static inline Access accessPolicy( CX_CXXAccessSpecifier access )
  {
    Access result = Access::Public;
    switch ( access )
    {
      case CX_CXXProtected:
        result = Access::Protected;
        break;
      case CX_CXXPrivate:
        result = Access::Private;
        break;
      default:
        break;
    }
    return result;
  }

  static bool isSigned( CXTypeKind kind )
  {
    switch ( kind )
    {
      case CXType_UChar:
      case CXType_Char16:
      case CXType_Char32:
      case CXType_UShort:
      case CXType_UInt:
      case CXType_ULong:
      case CXType_ULongLong:
      case CXType_UInt128:
        return false;
      default:
        break;
    }
    return true;
  }

  class BuilderPrivate
  {
    public:
      using CursorClassHash = QHash<CXCursor, ClassModelItem>;
      using CursorTypedefHash = QHash<CXCursor, TypeDefModelItem>;
      using TypeInfoHash = QHash<CXType, TypeInfo>;

      explicit BuilderPrivate( BaseVisitor *bv ) : m_baseVisitor( bv ), m_model( new CodeModel )
      {
        m_scopeStack.push( NamespaceModelItem( new _FileModelItem( m_model ) ) );

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
            break;
          }

          QTextStream in( &file );
          int numLine = 1;
          int startIfndef = -1;
          const QRegularExpression reIf( QStringLiteral( "^\\s*#if" ) );
          const QRegularExpression reEndIf( QStringLiteral( "^\\s*#(endif|else)" ) );
          const QRegularExpression reIfndef( QStringLiteral( "^\\s*#ifndef\\s*SIP_RUN" ) );
          SkipRanges ranges;
          int nbIf = 0;
          while ( !in.atEnd() )
          {
            QString line = in.readLine();

            if ( startIfndef >= 0 )
            {
              if ( reIf.match( line ).hasMatch() )
              {
                nbIf++;
              }

              if ( reEndIf.match( line ).hasMatch() )
              {
                if ( nbIf == 1 )
                {
                  ranges << SkipRange( startIfndef, numLine );
                  startIfndef = -1;
                }

                nbIf--;
              }
            }
            else
            {
              if ( reIfndef.match( line ).hasMatch() )
              {
                nbIf++;
                startIfndef = numLine;
              }
            }

            numLine++;
          }

          if ( !ranges.isEmpty() )
            mSkipRanges[ fileName ] = ranges;
        }
      }
      ~BuilderPrivate()
      {
        delete m_model;
      }

      // Determine scope from top item. Note that the scope list does not necessarily
      // match the scope stack in case of forward-declared inner classes whose definition
      // appears in the translation unit while the scope is the outer class.
      void updateScope()
      {
        if ( m_scopeStack.size() <= 1 )
          m_scope.clear();
        else
          m_scope = m_scopeStack.back()->scope() << m_scopeStack.back()->name();
      }

      void pushScope( const ScopeModelItem &i )
      {
        m_scopeStack.push( i );
        updateScope();
      }

      void popScope()
      {
        m_scopeStack.back()->purgeClassDeclarations();
        m_scopeStack.pop();
        updateScope();
      }

      bool addClass( const CXCursor &cursor, CodeModel::ClassType t );
      FunctionModelItem createFunction( const CXCursor &cursor,
                                        CodeModel::FunctionType t = CodeModel::Normal,
                                        bool isTemplateCode = false );
      FunctionModelItem createMemberFunction( const CXCursor &cursor,
                                              bool isTemplateCode = false );
      void qualifyConstructor( const CXCursor &cursor );
      TypeInfo createTypeInfoUncached( const CXType &type,
                                       bool *cacheable = nullptr ) const;
      TypeInfo createTypeInfo( const CXType &type ) const;
      TypeInfo createTypeInfo( const CXCursor &cursor ) const
      { return createTypeInfo( clang_getCursorType( cursor ) ); }
      void addTemplateInstantiations( const CXType &type,
                                      QString *typeName,
                                      TypeInfo *t ) const;
      bool addTemplateInstantiationsRecursion( const CXType &type, TypeInfo *t ) const;

      void addTypeDef( const CXCursor &cursor, const CXType &cxType );
      ClassModelItem currentTemplateClass() const;
      void startTemplateTypeAlias( const CXCursor &cursor );
      void endTemplateTypeAlias( const CXCursor &typeAliasCursor );

      TemplateParameterModelItem createTemplateParameter( const CXCursor &cursor ) const;
      TemplateParameterModelItem createNonTypeTemplateParameter( const CXCursor &cursor ) const;
      void addField( const CXCursor &cursor );

      static QString cursorValueExpression( BaseVisitor *bv, const CXCursor &cursor );
      std::pair<QString, ClassModelItem> getBaseClass( CXType type ) const;
      void addBaseClass( const CXCursor &cursor );

      template <class Item>
      void qualifyTypeDef( const CXCursor &typeRefCursor, const std::shared_ptr<Item> &item ) const;

      bool visitHeader( const QString &fileName ) const;

      void setFileName( const CXCursor &cursor, _CodeModelItem *item );

      bool isSkipped( const CXCursor &cursor ) const;

      BaseVisitor *m_baseVisitor;
      CodeModel *m_model;

      QMap<QString, QList<unsigned>> m_sipSkip;

      QStack<ScopeModelItem> m_scopeStack;
      QStringList m_scope;
      typedef QPair<int, int> SkipRange;
      typedef QList<SkipRange> SkipRanges;
      QMap<QString, SkipRanges> mSkipRanges;

      // Store all classes by cursor so that base classes can be found and inner
      // classes can be correctly parented in case of forward-declared inner classes
      // (QMetaObject::Connection)
      CursorClassHash m_cursorClassHash;
      CursorTypedefHash m_cursorTypedefHash;

      mutable TypeInfoHash m_typeInfoHash; // Cache type information
      mutable QHash<QString, TemplateTypeAliasModelItem> m_templateTypeAliases;

      ClassModelItem m_currentClass;
      EnumModelItem m_currentEnum;
      FunctionModelItem m_currentFunction;
      ArgumentModelItem m_currentArgument;
      VariableModelItem m_currentField;
      TemplateTypeAliasModelItem m_currentTemplateTypeAlias;
      QStringList m_systemIncludes; // files, like "memory"
      QStringList m_systemIncludePaths; // paths, like "/usr/include/Qt/"
      QString m_usingTypeRef; // Base classes in "using Base::member;"
      bool m_withinUsingDeclaration = false;

      int m_anonymousEnumCount = 0;
      CodeModel::FunctionType m_currentFunctionType = CodeModel::Normal;
      bool m_withinFriendDecl = false;
  };

  bool BuilderPrivate::addClass( const CXCursor &cursor, CodeModel::ClassType t )
  {
    QString className = getCursorSpelling( cursor );

    QStringList bindedClasses = QStringList()
                                << QStringLiteral( "QgsAttributes" ) // TODO has to be mapped to a list through using method code
                                << QStringLiteral( "QgsField" )
                                << QStringLiteral( "QgsFields" );

    // TODO bind everything
    if ( !bindedClasses.contains( className ) )
      return false;

    m_currentClass.reset( new _ClassModelItem( m_model, className ) );
    setFileName( cursor, m_currentClass.get() );
    m_currentClass->setClassType( t );
    // Some inner class? Note that it does not need to be (lexically) contained in a
    // class since it is possible to forward declare an inner class:
    // class QMetaObject { class Connection; }
    // class QMetaObject::Connection {}
    const CXCursor semPar = clang_getCursorSemanticParent( cursor );
    if ( isClassCursor( semPar ) )
    {
      const CursorClassHash::const_iterator it = m_cursorClassHash.constFind( semPar );
      if ( it == m_cursorClassHash.constEnd() )
      {
        QString message;
        QTextStream( &message ) << "Unable to find containing class \""
                                << getCursorSpelling( semPar ) << "\" of inner class \""
                                << className << "\".";
        // PYSIDE-1501: Has been observed to fail for inner class of
        // template with separated implementation where a forward
        // declaration of the outer template is reported (Boost).
        const auto severity = semPar.kind == CXCursor_ClassTemplate
                              ? CXDiagnostic_Warning : CXDiagnostic_Error;
        const Diagnostic d( message, cursor, severity );
        qWarning() << d;
        m_baseVisitor->appendDiagnostic( d );
        return false;
      }
      const ClassModelItem &containingClass = it.value();
      containingClass->addClass( m_currentClass );
      m_currentClass->setScope( containingClass->scope() << containingClass->name() );
    }
    else
    {
      m_currentClass->setScope( m_scope );
      m_scopeStack.back()->addClass( m_currentClass );
    }
    pushScope( m_currentClass );
    m_cursorClassHash.insert( cursor, m_currentClass );
    return true;
  }

  static QString msgCannotDetermineException( const std::string_view &snippetV )
  {
    const auto newLine = snippetV.find( '\n' ); // Multiline noexcept specifications have been found in Qt
    const bool truncate = newLine != std::string::npos;
    const qsizetype length = qsizetype( truncate ? newLine : snippetV.size() );
    QString snippet = QString::fromUtf8( snippetV.data(), length );
    if ( truncate )
      snippet += QStringLiteral( "..." );

    return u"Cannot determine exception specification: \""_s + snippet + u'"';
  }

// Return whether noexcept(<value>) throws. noexcept() takes a constexpr value.
// Try to determine the simple cases (true|false) via code snippet.
  static ExceptionSpecification computedExceptionSpecificationFromClang( BaseVisitor *bv,
      const CXCursor &cursor,
      bool isTemplateCode )
  {
    const std::string_view snippet = bv->getCodeSnippet( cursor );
    if ( snippet.empty() )
      return ExceptionSpecification::Unknown; // Macro expansion, cannot tell
    if ( snippet.find( "noexcept(false)" ) != std::string::npos )
      return ExceptionSpecification::Throws;
    if ( snippet.find( "noexcept(true)" ) != std::string::npos )
      return ExceptionSpecification::NoExcept;
    // Warn about it unless it is some form of template code where it is common
    // to have complicated code, which is of no concern to shiboken, like:
    // "QList::emplace(T) noexcept(is_pod<T>)".
    if ( !isTemplateCode && ReportHandler::isDebug( ReportHandler::FullDebug ) )
    {
      const Diagnostic d( msgCannotDetermineException( snippet ), cursor, CXDiagnostic_Warning );
      qWarning() << d;
      bv->appendDiagnostic( d );
    }
    return ExceptionSpecification::Unknown;
  }

  static ExceptionSpecification exceptionSpecificationFromClang( BaseVisitor *bv,
      const CXCursor &cursor,
      bool isTemplateCode )
  {
    const auto ce = clang_getCursorExceptionSpecificationType( cursor );
    switch ( ce )
    {
      case CXCursor_ExceptionSpecificationKind_ComputedNoexcept:
        return computedExceptionSpecificationFromClang( bv, cursor, isTemplateCode );
      case CXCursor_ExceptionSpecificationKind_BasicNoexcept:
      case CXCursor_ExceptionSpecificationKind_DynamicNone: // throw()
      case CXCursor_ExceptionSpecificationKind_NoThrow:
        return ExceptionSpecification::NoExcept;
      case CXCursor_ExceptionSpecificationKind_Dynamic: // throw(t1..)
      case CXCursor_ExceptionSpecificationKind_MSAny: // throw(...)
        return ExceptionSpecification::Throws;
      default:
        // CXCursor_ExceptionSpecificationKind_None,
        // CXCursor_ExceptionSpecificationKind_Unevaluated,
        // CXCursor_ExceptionSpecificationKind_Uninstantiated
        break;
    }
    return ExceptionSpecification::Unknown;
  }

  FunctionModelItem BuilderPrivate::createFunction( const CXCursor &cursor,
      CodeModel::FunctionType t,
      bool isTemplateCode )
  {
    QString name = getCursorSpelling( cursor );
    // Apply type fixes to "operator X &" -> "operator X&"
    if ( name.startsWith( u"operator " ) )
      name = fixTypeName( name );
    auto result = std::make_shared<_FunctionModelItem>( m_model, name );
    setFileName( cursor, result.get() );
    result->setType( createTypeInfo( clang_getCursorResultType( cursor ) ) );
    result->setFunctionType( t );
    result->setScope( m_scope );
    result->setStatic( clang_Cursor_getStorageClass( cursor ) == CX_SC_Static );
    result->setExceptionSpecification( exceptionSpecificationFromClang( m_baseVisitor, cursor, isTemplateCode ) );
    switch ( clang_getCursorAvailability( cursor ) )
    {
      case CXAvailability_Available:
        break;
      case CXAvailability_Deprecated:
        result->setDeprecated( true );
        break;
      case CXAvailability_NotAvailable: // "Foo(const Foo&) = delete;"
        result->setDeleted( true );
        break;
      case CXAvailability_NotAccessible:
        break;
    }
    return result;
  }

  static inline CodeModel::FunctionType functionTypeFromCursor( const CXCursor &cursor )
  {
    CodeModel::FunctionType result = CodeModel::Normal;
    switch ( cursor.kind )
    {
      case CXCursor_Constructor:
        if ( clang_CXXConstructor_isCopyConstructor( cursor ) != 0 )
          result = CodeModel::CopyConstructor;
        else if ( clang_CXXConstructor_isMoveConstructor( cursor ) != 0 )
          result = CodeModel::MoveConstructor;
        else
          result = CodeModel::Constructor;
        break;
      case CXCursor_Destructor:
        result = CodeModel::Destructor;
        break;
      default:
        break;
    }
    return result;
  }

  FunctionModelItem BuilderPrivate::createMemberFunction( const CXCursor &cursor,
      bool isTemplateCode )
  {
    const CodeModel::FunctionType functionType =
      m_currentFunctionType == CodeModel::Signal || m_currentFunctionType == CodeModel::Slot
      ? m_currentFunctionType // by annotation
      : functionTypeFromCursor( cursor );
    isTemplateCode |= m_currentClass->name().endsWith( u'>' );
    auto result = createFunction( cursor, functionType, isTemplateCode );
    result->setAccessPolicy( accessPolicy( clang_getCXXAccessSpecifier( cursor ) ) );
    result->setConstant( clang_CXXMethod_isConst( cursor ) != 0 );
    result->setStatic( clang_CXXMethod_isStatic( cursor ) != 0 );
    result->setVirtual( clang_CXXMethod_isVirtual( cursor ) != 0 );
    result->setAbstract( clang_CXXMethod_isPureVirtual( cursor ) != 0 );

    result->setSkipped( isSkipped( cursor ) );

    return result;
  }

// For CXCursor_Constructor, on endToken().
  void BuilderPrivate::qualifyConstructor( const CXCursor &cursor )
  {
    // Clang does not tell us whether a constructor is explicit, preventing it
    // from being used for implicit conversions. Try to guess whether a
    // constructor is explicit in the C++99 sense (1 parameter) by checking for
    // isConvertingConstructor() == 0. Fixme: The notion of "isConvertingConstructor"
    // should be used in the code model instead of "explicit"
    if ( clang_CXXConstructor_isDefaultConstructor( cursor ) == 0
         && m_currentFunction->arguments().size() == 1
         && clang_CXXConstructor_isCopyConstructor( cursor ) == 0
         && clang_CXXConstructor_isMoveConstructor( cursor ) == 0 )
    {
      m_currentFunction->setExplicit( clang_CXXConstructor_isConvertingConstructor( cursor ) == 0 );
    }
  }

  TemplateParameterModelItem BuilderPrivate::createTemplateParameter( const CXCursor &cursor ) const
  {
    return std::make_shared<_TemplateParameterModelItem>( m_model, getCursorSpelling( cursor ) );
  }

  TemplateParameterModelItem BuilderPrivate::createNonTypeTemplateParameter( const CXCursor &cursor ) const
  {
    TemplateParameterModelItem result = createTemplateParameter( cursor );
    result->setType( createTypeInfo( clang_getCursorType( cursor ) ) );
    return result;
  }

// CXCursor_VarDecl, CXCursor_FieldDecl cursors
  void BuilderPrivate::addField( const CXCursor &cursor )
  {
    auto field = std::make_shared<_VariableModelItem>( m_model, getCursorSpelling( cursor ) );
    field->setAccessPolicy( accessPolicy( clang_getCXXAccessSpecifier( cursor ) ) );
    field->setScope( m_scope );
    field->setType( createTypeInfo( cursor ) );
    field->setMutable( clang_CXXField_isMutable( cursor ) != 0 );
    m_currentField = field;
    m_scopeStack.back()->addVariable( field );
  }

// Create qualified name "std::list<std::string>" -> ("std", "list<std::string>")
  static QStringList qualifiedName( const QString &t )
  {
    QStringList result;
    int end = t.indexOf( u'<' );
    if ( end == -1 )
      end = t.indexOf( u'(' );
    if ( end == -1 )
      end = t.size();
    int lastPos = 0;
    while ( true )
    {
      const int nextPos = t.indexOf( u"::"_s, lastPos );
      if ( nextPos < 0 || nextPos >= end )
        break;
      result.append( t.mid( lastPos, nextPos - lastPos ) );
      lastPos = nextPos + 2;
    }
    result.append( t.right( t.size() - lastPos ) );
    return result;
  }

  static bool isArrayType( CXTypeKind k )
  {
    return k == CXType_ConstantArray || k == CXType_IncompleteArray
           || k == CXType_VariableArray || k == CXType_DependentSizedArray;
  }

  static bool isPointerType( CXTypeKind k )
  {
    return k == CXType_Pointer || k == CXType_LValueReference || k == CXType_RValueReference;
  }

  bool BuilderPrivate::addTemplateInstantiationsRecursion( const CXType &type, TypeInfo *t ) const
  {
    // Template arguments
    switch ( type.kind )
    {
      case CXType_Elaborated:
      case CXType_Record:
      case CXType_Unexposed:
        if ( const int numTemplateArguments = qMax( 0, clang_Type_getNumTemplateArguments( type ) ) )
        {
          for ( unsigned tpl = 0; tpl < unsigned( numTemplateArguments ); ++tpl )
          {
            const CXType argType = clang_Type_getTemplateArgumentAsType( type, tpl );
            // CXType_Invalid is returned when hitting on a specialization
            // of a non-type template (template <int v>).
            if ( argType.kind == CXType_Invalid )
              return false;
            t->addInstantiation( createTypeInfoUncached( argType ) );
          }
        }
        break;
      default:
        break;
    }
    return true;
  }

  static void dummyTemplateArgumentHandler( int, QStringView ) {}

  void BuilderPrivate::addTemplateInstantiations( const CXType &type,
      QString *typeName,
      TypeInfo *t ) const
  {
    // In most cases, for templates like "Vector<A>", Clang will give us the
    // arguments by recursing down the type. However this will fail for example
    // within template classes (for functions like the copy constructor):
    // template <class T>
    // class Vector {
    //    Vector(const Vector&);
    // };
    // In that case, have TypeInfo parse the list from the spelling.
    // Finally, remove the list "<>" from the type name.
    const bool parsed = addTemplateInstantiationsRecursion( type, t )
                        && !t->instantiations().isEmpty();
    if ( !parsed )
      t->setInstantiations( {} );
    const auto pos = parsed
                     ? parseTemplateArgumentList( *typeName, dummyTemplateArgumentHandler )
                     : t->parseTemplateArgumentList( *typeName );
    if ( pos.first != -1 && pos.second != -1 && pos.second > pos.first )
      typeName->remove( pos.first, pos.second - pos.first );
  }

  TypeInfo BuilderPrivate::createTypeInfoUncached( const CXType &type,
      bool *cacheable ) const
  {
    if ( type.kind == CXType_Pointer ) // Check for function pointers, first.
    {
      const CXType pointeeType = clang_getPointeeType( type );
      const int argCount = clang_getNumArgTypes( pointeeType );
      if ( argCount >= 0 )
      {
        TypeInfo result = createTypeInfoUncached( clang_getResultType( pointeeType ),
                          cacheable );
        result.setFunctionPointer( true );
        for ( int a = 0; a < argCount; ++a )
          result.addArgument( createTypeInfoUncached( clang_getArgType( pointeeType, unsigned( a ) ),
                              cacheable ) );
        return result;
      }
    }

    TypeInfo typeInfo;

    CXType nestedType = type;
    for ( ; isArrayType( nestedType.kind ); nestedType = clang_getArrayElementType( nestedType ) )
    {
      const long long size = clang_getArraySize( nestedType );
      typeInfo.addArrayElement( size >= 0 ? QString::number( size ) : QString() );
    }

    TypeInfo::Indirections indirections;
    for ( ; isPointerType( nestedType.kind ); nestedType = clang_getPointeeType( nestedType ) )
    {
      switch ( nestedType.kind )
      {
        case CXType_Pointer:
          indirections.prepend( clang_isConstQualifiedType( nestedType ) != 0
                                ? Indirection::ConstPointer : Indirection::Pointer );
          break;
        case CXType_LValueReference:
          typeInfo.setReferenceType( LValueReference );
          break;
        case CXType_RValueReference:
          typeInfo.setReferenceType( RValueReference );
          break;
        default:
          break;
      }
    }
    typeInfo.setIndirectionsV( indirections );

    typeInfo.setConstant( clang_isConstQualifiedType( nestedType ) != 0 );
    typeInfo.setVolatile( clang_isVolatileQualifiedType( nestedType ) != 0 );

    QString typeName = getTypeName( nestedType );
    while ( TypeInfo::stripLeadingConst( &typeName )
            || TypeInfo::stripLeadingVolatile( &typeName ) )
    {
    }

    // For typedefs within templates or nested classes within templates (iterators):
    // "template <class T> class QList { using Value=T; .."
    // the typedef source is named "type-parameter-0-0". Convert it back to the
    // template parameter name. The CXTypes are the same for all templates and
    // must not be cached.
    if ( m_currentClass && typeName.startsWith( u"type-parameter-0-" ) )
    {
      if ( cacheable != nullptr )
        *cacheable = false;
      bool ok;
      const int n = QStringView{typeName}.mid( 17 ).toInt( &ok );
      if ( ok )
      {
        auto currentTemplate = currentTemplateClass();
        if ( currentTemplate && n < currentTemplate->templateParameters().size() )
          typeName = currentTemplate->templateParameters().at( n )->name();
      }
    }

    // Obtain template instantiations if the name has '<' (thus excluding
    // typedefs like "std::string".
    if ( typeName.contains( u'<' ) )
      addTemplateInstantiations( nestedType, &typeName, &typeInfo );

    typeInfo.setQualifiedName( qualifiedName( typeName ) );
    // 3320:CINDEX_LINKAGE int clang_getNumArgTypes(CXType T); function ptr types?
    typeInfo.simplifyStdType();
    return typeInfo;
  }

  TypeInfo BuilderPrivate::createTypeInfo( const CXType &type ) const
  {
    const auto it = m_typeInfoHash.constFind( type );
    if ( it != m_typeInfoHash.constEnd() )
      return it.value();
    bool cacheable = true;
    TypeInfo result = createTypeInfoUncached( type, &cacheable );
    if ( cacheable )
      m_typeInfoHash.insert( type, result );
    return result;
  }

  void BuilderPrivate::addTypeDef( const CXCursor &cursor, const CXType &cxType )
  {
    const QString target = getCursorSpelling( cursor );
    auto item = std::make_shared<_TypeDefModelItem>( m_model, target );
    setFileName( cursor, item.get() );
    item->setType( createTypeInfo( cxType ) );
    item->setScope( m_scope );
    m_scopeStack.back()->addTypeDef( item );
    m_cursorTypedefHash.insert( cursor, item );
  }

  ClassModelItem BuilderPrivate::currentTemplateClass() const
  {
    for ( auto i = m_scopeStack.size() - 1; i >= 0; --i )
    {
      auto klass = std::dynamic_pointer_cast<_ClassModelItem>( m_scopeStack.at( i ) );
      if ( klass && klass->isTemplate() )
        return klass;
    }
    return {};
  }

  void BuilderPrivate::startTemplateTypeAlias( const CXCursor &cursor )
  {
    const QString target = getCursorSpelling( cursor );
    m_currentTemplateTypeAlias.reset( new _TemplateTypeAliasModelItem( m_model, target ) );
    setFileName( cursor, m_currentTemplateTypeAlias.get() );
    m_currentTemplateTypeAlias->setScope( m_scope );
  }

  void BuilderPrivate::endTemplateTypeAlias( const CXCursor &typeAliasCursor )
  {
    CXType type = clang_getTypedefDeclUnderlyingType( typeAliasCursor );
    // Usually "<elaborated>std::list<T>" or "<unexposed>Container1<T>",
    // as obtained with parser of PYSIDE-323
    if ( type.kind == CXType_Unexposed || type.kind == CXType_Elaborated )
    {
      m_currentTemplateTypeAlias->setType( createTypeInfo( type ) );
      m_scopeStack.back()->addTemplateTypeAlias( m_currentTemplateTypeAlias );
    }
    m_currentTemplateTypeAlias.reset();
  }

// extract an expression from the cursor via source
// CXCursor_EnumConstantDecl, ParmDecl (a = Flag1 | Flag2)
  QString BuilderPrivate::cursorValueExpression( BaseVisitor *bv, const CXCursor &cursor )
  {
    const std::string_view snippet = bv->getCodeSnippet( cursor );
    auto equalSign = snippet.find( '=' );
    if ( equalSign == std::string::npos )
      return QString();
    ++equalSign;
    QString result = QString::fromLocal8Bit( snippet.data() + equalSign,
                     qsizetype( snippet.size() - equalSign ) );
    // Fix a default expression as read from code. Simplify white space
    result.remove( u'\r' );
    return result.contains( u'"' ) ? result.trimmed() : result.simplified();
  }

// Resolve a type (loop over aliases/typedefs), for example for base classes

  struct TypeDeclaration
  {
    CXType type;
    CXCursor declaration;
  };

  static TypeDeclaration resolveType( CXType type )
  {
    CXCursor decl = clang_getTypeDeclaration( type );
    if ( type.kind != CXType_Unexposed )
    {
      while ( true )
      {
        auto kind = clang_getCursorKind( decl );
        if ( kind != CXCursor_TypeAliasDecl && kind != CXCursor_TypedefDecl )
          break;
        type = clang_getTypedefDeclUnderlyingType( decl );
        decl = clang_getTypeDeclaration( type );
      }
    }
    return {type, decl};
  }

// Note: Return the baseclass for cursors like CXCursor_CXXBaseSpecifier,
// where the cursor spelling has "struct baseClass".
  std::pair<QString, ClassModelItem> BuilderPrivate::getBaseClass( CXType type ) const
  {
    const auto decl = resolveType( type );
    // Note: spelling has "struct baseClass", use type
    QString baseClassName;
    if ( decl.type.kind == CXType_Unexposed )
    {
      // The type is unexposed when the base class is a template type alias:
      // "class QItemSelection : public QList<X>" where QList is aliased to QVector.
      // Try to resolve via code model.
      TypeInfo info = createTypeInfo( decl.type );
      auto parentScope = m_scopeStack.at( m_scopeStack.size() - 2 ); // Current is class.
      auto resolved = TypeInfo::resolveType( info, parentScope );
      if ( resolved != info )
        baseClassName = resolved.toString();
    }
    if ( baseClassName.isEmpty() )
      baseClassName = getTypeName( decl.type );

    auto it = m_cursorClassHash.constFind( decl.declaration );
    // Not found: Set unqualified name. This happens in cases like
    // "class X : public std::list<...>", "template<class T> class Foo : public T"
    // and standard types like true_type, false_type.
    if ( it == m_cursorClassHash.constEnd() )
      return {baseClassName, {}};

    // Completely qualify the class name by looking it up and taking its scope
    // plus the actual baseClass stripped off any scopes. Consider:
    //   namespace std {
    //   template <class T> class vector {};
    //   namespace n {
    //      class Foo : public vector<int> {};
    //   }
    //   }
    // should have "std::vector<int>" as base class (whereas the type of the base class is
    // "std::vector<T>").
    const QStringList &baseScope = it.value()->scope();
    if ( !baseScope.isEmpty() )
    {
      const int lastSep = baseClassName.lastIndexOf( u"::"_s );
      if ( lastSep >= 0 )
        baseClassName.remove( 0, lastSep + u"::"_s.size() );
      baseClassName.prepend( u"::"_s );
      baseClassName.prepend( baseScope.join( u"::"_s ) );
    }
    return {baseClassName, it.value()};
  }

// Add a base class to the current class from CXCursor_CXXBaseSpecifier
  void BuilderPrivate::addBaseClass( const CXCursor &cursor )
  {
    Q_ASSERT( clang_getCursorKind( cursor ) == CXCursor_CXXBaseSpecifier );
    const auto access = accessPolicy( clang_getCXXAccessSpecifier( cursor ) );
    const auto baseClass = getBaseClass( clang_getCursorType( cursor ) );
    m_currentClass->addBaseClass( {baseClass.first, baseClass.second, access} );
  }

  static inline CXCursor definitionFromTypeRef( const CXCursor &typeRefCursor )
  {
    Q_ASSERT( typeRefCursor.kind == CXCursor_TypeRef );
    return clang_getTypeDeclaration( clang_getCursorType( typeRefCursor ) );
  }

// Qualify function arguments or fields that are typedef'ed from another scope:
// enum ConversionFlag {};
// typedef QFlags<ConversionFlag> ConversionFlags;
// class QTextCodec {
//      enum ConversionFlag {};
//      typedef QFlags<ConversionFlag> ConversionFlags;
//      struct ConverterState {
//          explicit ConverterState(ConversionFlags);
//                                  ^^ qualify to QTextCodec::ConversionFlags
//          ConversionFlags m_flags;
//                          ^^ ditto

  template <class Item> // ArgumentModelItem, VariableModelItem
  void BuilderPrivate::qualifyTypeDef( const CXCursor &typeRefCursor, const std::shared_ptr<Item> &item ) const
  {
    TypeInfo type = item->type();
    if ( type.qualifiedName().size() == 1 ) // item's type is unqualified.
    {
      const auto it = m_cursorTypedefHash.constFind( definitionFromTypeRef( typeRefCursor ) );
      if ( it != m_cursorTypedefHash.constEnd() && !it.value()->scope().isEmpty() )
      {
        type.setQualifiedName( it.value()->scope() + type.qualifiedName() );
        item->setType( type );
      }
    }
  }

  void BuilderPrivate::setFileName( const CXCursor &cursor, _CodeModelItem *item )
  {
    const SourceRange range = getCursorRange( cursor );
    QString file = m_baseVisitor->getFileName( range.first.file );
    if ( !file.isEmpty() ) // Has been observed to be 0 for invalid locations
    {
      item->setFileName( QDir::cleanPath( file ) );
      item->setStartPosition( int( range.first.line ), int( range.first.column ) );
      item->setEndPosition( int( range.second.line ), int( range.second.column ) );
    }
  }

  bool BuilderPrivate::isSkipped( const CXCursor &cursor ) const
  {
    SourceLocation loc = getCursorLocation( cursor );
    QString fileName = getFileName( loc.file );
    const int line = loc.line;

    // TODO fix the path when we get the qgis dir as argument
    if ( !fileName.startsWith( QStringLiteral( "/home/julien/work/QGIS/.worktrees/qt-for-python-qt6/src/" ) )
         || ( m_sipSkip.contains( fileName ) && m_sipSkip[fileName].contains( line ) ) )
      return true;

    else if ( mSkipRanges.contains( fileName ) )
    {
      const SkipRanges ranges = mSkipRanges[fileName];
      for ( SkipRange range : ranges )
      {
        if ( range.first <= line && range.second >= line )
          return true;
      }
    }

    return false;
  }

  Builder::Builder()
  {
    d = new BuilderPrivate( this );
  }

  Builder::~Builder()
  {
    delete d;
  }

  static QString baseName( QString path )
  {
    qsizetype lastSlash = path.lastIndexOf( u'/' );
#ifdef Q_OS_WIN
    if ( lastSlash < 0 )
      lastSlash = path.lastIndexOf( u'\\' );
#endif
    if ( lastSlash > 0 )
      path.remove( 0, lastSlash + 1 );
    return path;
  }

  bool BuilderPrivate::visitHeader( const QString &fileName ) const
  {
    // Resolve OpenGL typedefs although the header is considered a system header.
    const QString baseName = clang::baseName( fileName );
    if ( baseName == u"gl.h"
         || baseName == u"gl2.h"
         || baseName == u"gl3.h"
         || baseName == u"gl31.h"
         || baseName == u"gl32.h"
         || baseName == u"stdint.h" // Windows: int32_t, uint32_t
         || baseName == u"stddef.h" )  // size_t
    {
      return true;
    }

    switch ( clang::platform() )
    {
      case Platform::Unix:
        if ( fileName == u"/usr/include/stdlib.h"
             || baseName == u"types.h"
             || baseName == u"stdint-intn.h" // int32_t
             || baseName == u"stdint-uintn.h" )  // uint32_t
        {
          return true;
        }
        break;
      case Platform::macOS:
        // Parse the following system headers to get the correct typdefs for types like
        // int32_t, which are used in the macOS implementation of OpenGL framework.
        // They are installed under /Applications/Xcode.app/Contents/Developer/Platforms...
        if ( baseName == u"gltypes.h"
             || fileName.contains( u"/usr/include/_types" )
             || fileName.contains( u"/usr/include/sys/_types" ) )
        {
          return true;
        }
        break;
      default:
        break;
    }

    for ( const auto &systemInclude : m_systemIncludes )
    {
      if ( systemInclude == baseName )
        return true;
    }
    for ( const auto &systemIncludePath : m_systemIncludePaths )
    {
      if ( fileName.startsWith( systemIncludePath ) )
        return true;
    }
    return false;
  }

  bool Builder::visitLocation( const QString &fileName, LocationType locationType ) const
  {
    QStringList sipNoFiles =
    {
      u"raster/qgsrasterlayerrenderer.h"_s,
      u"raster/qgsrasterrendererregistry.h"_s,
      u"raster/qgsrasterlayerprofilegenerator.h"_s,
      u"qgsthreadingutils.h"_s,
      u"pal/palrtree.h"_s,
      u"pal/geomfunction.h"_s,
      u"pal/internalexception.h"_s,
      u"pal/util.h"_s,
      u"pal/labelposition.h"_s,
      u"pal/pointset.h"_s,
      u"pal/palstat.h"_s,
      u"pal/feature.h"_s,
      u"pal/pal.h"_s,
      u"pal/priorityqueue.h"_s,
      u"pal/layer.h"_s,
      u"pal/palexception.h"_s,
      u"pal/costcalculator.h"_s,
      u"pal/problem.h"_s,
      u"qgsgenericspatialindex.h"_s,
      u"browser/qgsfilebaseddataitemprovider.h"_s,
      u"qgsgrouplayerrenderer.h"_s,
      u"qgslocalec.h"_s,
      u"qgsspatialindexkdbush_p.h"_s,
      u"proj/qgscoordinatetransformcontext_p.h"_s,
      u"proj/qgscoordinatetransform_p.h"_s,
      u"maprenderer/qgsmaprendererstagedrenderjob.h"_s,
      u"qgsmbtiles.h"_s,
      u"qgseventtracing.h"_s,
      u"auth/qgsauthmethodregistry.h"_s,
      u"auth/qgsauthmethodmetadata.h"_s,
      u"auth/qgsauthcrypto.h"_s,
      u"qgsfield_p.h"_s,
      u"qgspointlocatorinittask.h"_s,
      u"qobjectuniqueptr.h"_s,
      u"geometry/qgsinternalgeometryengine.h"_s,
      u"geometry/qgsgeos.h"_s,
      u"geometry/qgsgeometryeditutils.h"_s,
      u"geometry/qgsgeometryfactory.h"_s,
      u"qgstilecache.h"_s,
      u"qgswebview.h"_s,
      u"network/qgsrangerequestcache.h"_s,
      u"network/qgsnetworkdiskcache.h"_s,
      u"network/qgsnetworkreplyparser.h"_s,
      u"layout/qgslayoutmultiframeundocommand.h"_s
      u"layout/qgscompositionconverter.h"_s,
      u"layout/qgslayoutitemundocommand.h"_s,
      u"layout/qgslayoutgeopdfexporter.h"_s,
      u"layout/qgslayoutitemgroupundocommand.h"_s,
      u"mesh/qgsmeshdatasetgroupstore.h"_s,
      u"mesh/qgsmeshlayerprofilegenerator.h"_s,
      u"mesh/qgstopologicalmesh.h"_s,
      u"mesh/qgsmeshlayerutils.h"_s,
      u"mesh/qgsmeshlayerrenderer.h"_s,
      u"mesh/qgsmeshvirtualdatasetgroup.h"_s,
      u"mesh/qgsmeshcalcnode.h"_s,
      u"mesh/qgsmeshcalcutils.h"_s,
      u"mesh/qgstriangularmesh.h"_s,
      u"mesh/qgsmeshsimplificationsettings.h"_s,
      u"mesh/qgsmeshvectorrenderer.h"_s,
      u"qgsfeatureexpressionvaluesgatherer.h"_s,
      u"qgsindexedfeature.h"_s,
      u"qgsrelation_p.h"_s,
      u"qgspolymorphicrelation_p.h"_s,
      u"settings/qgssettingsentryenumflag.h"_s,
      u"textrenderer/qgstextmetrics.h"_s,
      u"textrenderer/qgstextrenderer_p.h"_s,
      u"expression/qgsexpressionutils.h"_s,
      u"symbology/qgspainterswapper.h"_s,
      u"qgswebpage.h"_s,
      u"qgstiledownloadmanager.h"_s,
      u"qgssqliteexpressioncompiler.h"_s,
      u"qgsshapegenerator.h"_s,
      u"qgis_sip.h"_s,
      u"qgsgdalutils.h"_s,
      u"pointcloud/qgspointcloudrequest.h"_s,
      u"pointcloud/qgspointcloudstatscalculator.h"_s,
      u"pointcloud/qgspointcloudblockrequest.h"_s,
      u"pointcloud/qgscopcpointcloudindex.h"_s,
      u"pointcloud/qgspointcloudlayerprofilegenerator.h"_s,
      u"pointcloud/qgsremotecopcpointcloudindex.h"_s,
      u"pointcloud/qgseptpointcloudindex.h"_s,
      u"pointcloud/qgslazdecoder.h"_s,
      u"pointcloud/qgspointcloudstatscalculationtask.h"_s,
      u"pointcloud/qgspointcloudlayerrenderer.h"_s,
      u"pointcloud/expression/qgspointcloudexpression.h"_s,
      u"pointcloud/expression/qgspointcloudexpressionnode.h"_s,
      u"pointcloud/expression/qgspointcloudexpressionnodeimpl.h"_s,
      u"pointcloud/qgseptpointcloudblockrequest.h"_s,
      u"pointcloud/qgscopcpointcloudblockrequest.h"_s,
      u"pointcloud/qgsremoteeptpointcloudindex.h"_s,
      u"pointcloud/qgspointcloudsubindex.h"_s,
      u"pointcloud/qgseptdecoder.h"_s,
      u"pointcloud/qgspointcloudindex.h"_s,
      u"pointcloud/qgslazinfo.h"_s,
      u"dxf/qgsdxfpaintengine.h"_s,
      u"dxf/qgsdxfpaintdevice.h"_s,
      u"elevation/qgsabstractprofilesurfacegenerator.h"_s,
      u"vectortile/qgsvectortilemvtencoder.h"_s,
      u"vectortile/qgsvectortiledataitems.h"_s,
      u"vectortile/qgsvectortilemvtdecoder.h"_s,
      u"vectortile/qgsvectortiledataprovider.h"_s,
      u"vectortile/qgsvectortileconnection.h"_s,
      u"vectortile/qgsvtpkvectortiledataprovider.h"_s,
      u"vectortile/qgsarcgisvectortileservicedataprovider.h"_s,
      u"vectortile/qgsxyzvectortiledataprovider.h"_s,
      u"vectortile/qgsvectortilemvtutils.h"_s,
      u"vectortile/qgsvectortileutils.h"_s,
      u"vectortile/qgsvectortilelayerrenderer.h"_s,
      u"vectortile/qgsmbtilesvectortiledataprovider.h"_s,
      u"vectortile/qgsvectortileprovidermetadata.h"_s,
      u"vectortile/qgsvectortileloader.h"_s,
      u"qgsconnectionpool.h"_s,
      u"qgsogrutils.h"_s,
      u"qgsspatialindexutils.h"_s,
      u"providers/ogr/qgsgeopackageprojectstorage.h"_s,
      u"providers/ogr/qgsogrprovidermetadata.h"_s,
      u"providers/ogr/qgsogrdbconnection.h"_s,
      u"providers/ogr/qgsogrfeatureiterator.h"_s,
      u"providers/ogr/qgsogrlayermetadataprovider.h"_s,
      u"providers/ogr/qgsgeopackagedataitems.h"_s,
      u"providers/ogr/qgsogrproviderconnection.h"_s,
      u"providers/ogr/qgsogrexpressioncompiler.h"_s,
      u"providers/ogr/qgsgeopackageproviderconnection.h"_s,
      u"providers/ogr/qgsogrprovider.h"_s,
      u"providers/ogr/qgsogrconnpool.h"_s,
      u"providers/ogr/qgsgeopackagerasterwriter.h"_s,
      u"providers/ogr/qgsogrproviderutils.h"_s,
      u"providers/ogr/qgsogrtransaction.h"_s,
      u"providers/ogr/qgsgeopackagerasterwritertask.h"_s,
      u"providers/gdal/qgsgdalprovider.h"_s,
      u"providers/gdal/qgsgdalproviderbase.h"_s,
      u"providers/copc/qgscopcprovider.h"_s,
      u"providers/meshmemory/qgsmeshmemorydataprovider.h"_s,
      u"providers/ept/qgseptprovider.h"_s,
      u"providers/vpc/qgsvirtualpointcloudprovider.h"_s,
      u"providers/memory/qgsmemoryfeatureiterator.h"_s,
      u"providers/memory/qgsmemoryprovider.h"_s,
      u"providers/arcgis/qgsarcgisrestquery.h"_s,
      u"fromencodedcomponenthelper.h"_s,
      u"qgswebframe.h"_s,
      u"externalstorage/qgshttpexternalstorage_p.h"_s,
      u"externalstorage/qgssimplecopyexternalstorage_p.h"_s,
      u"vector/qgsvectorlayerdiagramprovider.h"_s,
      u"vector/qgsvectorlayerrenderer.h"_s,
      u"vector/qgsvectorlayerref.h"_s,
      u"vector/qgsvectorlayerprofilegenerator.h"_s,
      u"qgsabstractgeopdfexporter.h"_s,
      u"processing/qgsprocessingparametertypeimpl.h"_s,
      u"qgscplhttpfetchoverrider.h"_s,
      u"qgsopenclutils.h"_s,
      u"qgsproperty_p.h"_s,
      u"qgsspatialiteutils.h"_s,
      u"qgscoordinateutils.h"_s,
      u"simplify/effectivearea.h"_s,
      u"labeling/qgslabelsink.h"_s,
      u"labeling/qgsvectorlayerlabelprovider.h"_s,
      u"labeling/qgslabelfeature.h"_s,
      u"labeling/qgstextlabelfeature.h"_s,
      u"labeling/qgslabelingengine.h"_s,
      u"qgssqlexpressioncompiler.h"_s,
      u"annotations/qgsannotationlayerrenderer.h"_s,
      u"annotations/qgsannotationregistry.h"_s,
      u"qgsogrproxytextcodec.h"_s,
      u"qgsexception.h"_s,
      u"qgsmaplayerref.h"_s
    };

    QString baseName = fileName;
    baseName.remove( u"/home/julien/work/QGIS/.worktrees/qt-for-python-qt6/src/core/"_s );

    if ( sipNoFiles.contains( baseName ) )
    {
      // qDebug() << "--- remove " << fileName;
      return false;
    }

    // qDebug() << "fileName=" << fileName << "visit?" << !fileName.contains(u"qgsogrutils.h"_s);
    return !fileName.contains( u"qgsogrutils.h"_s ) && ( locationType != LocationType::System  || d->visitHeader( fileName ) );
  }

  void Builder::setSystemIncludes( const QStringList &systemIncludes )
  {
    for ( const auto &i : systemIncludes )
    {
      if ( i.endsWith( u'/' ) )
        d->m_systemIncludePaths.append( i );
      else
        d->m_systemIncludes.append( i );
    }
  }

  FileModelItem Builder::dom() const
  {
    Q_ASSERT( !d->m_scopeStack.isEmpty() );
    auto rootScope = d->m_scopeStack.constFirst();
    rootScope->purgeClassDeclarations();
    return std::dynamic_pointer_cast<_FileModelItem>( rootScope );
  }

  static QString msgOutOfOrder( const CXCursor &cursor, const char *expectedScope )
  {
    return getCursorKindName( cursor.kind ) + u' '
           + getCursorSpelling( cursor ) + u" encountered outside "_s
           + QLatin1StringView( expectedScope ) + u'.';
  }

  static CodeModel::ClassType codeModelClassTypeFromCursor( CXCursorKind kind )
  {
    CodeModel::ClassType result = CodeModel::Class;
    if ( kind == CXCursor_UnionDecl )
      result = CodeModel::Union;
    else if ( kind == CXCursor_StructDecl )
      result = CodeModel::Struct;
    return result;
  }

  static NamespaceType namespaceType( const CXCursor &cursor )
  {
    if ( clang_Cursor_isAnonymous( cursor ) )
      return NamespaceType::Anonymous;
#if CINDEX_VERSION_MAJOR > 0 || CINDEX_VERSION_MINOR >= 59
    if ( clang_Cursor_isInlineNamespace( cursor ) )
      return NamespaceType::Inline;
#endif
    return NamespaceType::Default;
  }

  static QString enumType( const CXCursor &cursor )
  {
    QString name = getCursorSpelling( cursor ); // "enum Foo { v1, v2 };"
    if ( name.contains( u"unnamed enum" ) ) // Clang 16.0
      return {};
    if ( name.isEmpty() )
    {
      // PYSIDE-1228: For "typedef enum { v1, v2 } Foo;", type will return
      // "Foo" as expected. Care must be taken to exclude real anonymous enums.
      name = getTypeName( clang_getCursorType( cursor ) );
      if ( name.contains( u"(unnamed" ) // Clang 12.0.1
           || name.contains( u"(anonymous" ) ) // earlier
      {
        name.clear();
      }
    }
    return name;
  }

  BaseVisitor::StartTokenResult Builder::startToken( const CXCursor &cursor )
  {
    switch ( cursor.kind )
    {
      case CXCursor_MacroExpansion:
      {
        QString macro = getCursorSpelling( cursor );
        if ( macro == u"SIP_SKIP" )
        {
          // try getCursorRange if on several lines
          SourceLocation loc = getCursorLocation( cursor );
          QString fileName = getFileName( loc.file );
          d->m_sipSkip[fileName].append( loc.line );
        }
      }
      break;
      case CXCursor_CXXAccessSpecifier:
        d->m_currentFunctionType = CodeModel::Normal;
        break;
      case CXCursor_AnnotateAttr:
      {
        const QString annotation = getCursorSpelling( cursor );
        if ( annotation == u"qt_slot" )
          d->m_currentFunctionType = CodeModel::Slot;
        else if ( annotation == u"qt_signal" )
          d->m_currentFunctionType = CodeModel::Signal;
        else
          d->m_currentFunctionType = CodeModel::Normal;
      }
      break;
      case CXCursor_CXXBaseSpecifier:
        if ( !d->m_currentClass )
        {
          const Diagnostic d( msgOutOfOrder( cursor, "class" ), cursor, CXDiagnostic_Error );
          qWarning() << d;
          appendDiagnostic( d );
          return Error;
        }
        d->addBaseClass( cursor );
        break;
      case CXCursor_ClassDecl:
      case CXCursor_UnionDecl:
      case CXCursor_StructDecl:
        if ( d->m_withinFriendDecl || clang_isCursorDefinition( cursor ) == 0
             || !d->addClass( cursor, codeModelClassTypeFromCursor( cursor.kind ) ) )
        {
          return Skip;
        }
        break;
      case CXCursor_ClassTemplate:
      case CXCursor_ClassTemplatePartialSpecialization:
        if ( d->m_withinFriendDecl || clang_isCursorDefinition( cursor ) == 0
             || !d->addClass( cursor, CodeModel::Class ) )
        {
          return Skip;
        }
        d->m_currentClass->setName( d->m_currentClass->name() + templateBrackets() );
        d->m_scope.back() += templateBrackets();
        break;
      case CXCursor_EnumDecl:
      {
        QString name = enumType( cursor );
        EnumKind kind = CEnum;
        if ( name.isEmpty() )
        {
          kind = AnonymousEnum;
          name = QStringLiteral( "enum_" ) + QString::number( ++d->m_anonymousEnumCount );
#if !CLANG_NO_ENUMDECL_ISSCOPED
        }
        else if ( clang_EnumDecl_isScoped( cursor ) != 0 )
        {
#else
        }
        else if ( clang_EnumDecl_isScoped4( this, cursor ) != 0 )
        {
#endif
          kind = EnumClass;
        }
        d->m_currentEnum.reset( new _EnumModelItem( d->m_model, name ) );
        d->setFileName( cursor, d->m_currentEnum.get() );
        d->m_currentEnum->setScope( d->m_scope );
        d->m_currentEnum->setEnumKind( kind );
        if ( clang_getCursorAvailability( cursor ) == CXAvailability_Deprecated )
          d->m_currentEnum->setDeprecated( true );
        d->m_currentEnum->setSigned( isSigned( clang_getEnumDeclIntegerType( cursor ).kind ) );
        if ( std::dynamic_pointer_cast<_ClassModelItem>( d->m_scopeStack.back() ) )
          d->m_currentEnum->setAccessPolicy( accessPolicy( clang_getCXXAccessSpecifier( cursor ) ) );
      }
      break;
      case CXCursor_EnumConstantDecl:
      {
        const QString name = getCursorSpelling( cursor );
        if ( !d->m_currentEnum )
        {
          const Diagnostic d( msgOutOfOrder( cursor, "enum" ), cursor, CXDiagnostic_Error );
          qWarning() << d;
          appendDiagnostic( d );
          return Error;
        }
        EnumValue enumValue;
        if ( d->m_currentEnum->isSigned() )
          enumValue.setValue( clang_getEnumConstantDeclValue( cursor ) );
        else
          enumValue.setUnsignedValue( clang_getEnumConstantDeclUnsignedValue( cursor ) );
        auto enumConstant = std::make_shared<_EnumeratorModelItem>( d->m_model, name );
        enumConstant->setStringValue( d->cursorValueExpression( this, cursor ) );
        enumConstant->setValue( enumValue );
        if ( clang_getCursorAvailability( cursor ) == CXAvailability_Deprecated )
          enumConstant->setDeprecated( true );
        d->m_currentEnum->addEnumerator( enumConstant );
      }
      break;
      case CXCursor_VarDecl:
        // static class members are seen as CXCursor_VarDecl
        if ( isClassOrNamespaceCursor( clang_getCursorSemanticParent( cursor ) ) )
        {
          d->addField( cursor );
          d->m_currentField->setStatic( true );
        }
        break;
      case CXCursor_FieldDecl:
        d->addField( cursor );
        break;
      case CXCursor_FriendDecl:
        d->m_withinFriendDecl = true;
        break;
      case CXCursor_CompoundStmt: // Function bodies
        return Skip;
      case CXCursor_Constructor:
      case CXCursor_Destructor: // Note: Also use clang_CXXConstructor_is..Constructor?
      case CXCursor_CXXMethod:
      case CXCursor_ConversionFunction:
        // Member functions of other classes can be declared to be friends.
        // Skip inline member functions outside class, only go by declarations inside class
        if ( d->m_withinFriendDecl || !withinClassDeclaration( cursor ) )
          return Skip;
        d->m_currentFunction = d->createMemberFunction( cursor, false );
        d->m_currentFunction->setSkipped( d->isSkipped( cursor ) );
        d->m_scopeStack.back()->addFunction( d->m_currentFunction );
        break;

      // Not fully supported, currently, seen as normal function
      // Note: May appear inside class (member template) or outside (free template).
      case CXCursor_FunctionTemplate:
      {
        const CXCursor semParent = clang_getCursorSemanticParent( cursor );
        if ( isClassCursor( semParent ) )
        {
          if ( semParent == clang_getCursorLexicalParent( cursor ) )
          {
            d->m_currentFunction = d->createMemberFunction( cursor, true );
            d->m_currentFunction->setSkipped( d->isSkipped( cursor ) );
            d->m_scopeStack.back()->addFunction( d->m_currentFunction );
            break;
          }
          return Skip; // inline member functions outside class
        }

        if ( !d->isSkipped( cursor ) )
        {
          d->m_currentFunction = d->createFunction( cursor, CodeModel::Normal, true );
          d->m_scopeStack.back()->addFunction( d->m_currentFunction );
          return Skip;
        }
      }
      break;
      case CXCursor_FunctionDecl:

        if ( d->isSkipped( cursor ) )
          return Skip;

        // Free functions or functions completely defined within "friend" (class
        // operators). Note: CXTranslationUnit_SkipFunctionBodies must be off for
        // clang_isCursorDefinition() to work here.
        if ( !d->m_withinFriendDecl || clang_isCursorDefinition( cursor ) != 0 )
        {
          auto scope = d->m_scopeStack.size() - 1; // enclosing class
          if ( d->m_withinFriendDecl )
          {
            // Friend declaration: go back to namespace or file scope.
            for ( --scope;  d->m_scopeStack.at( scope )->kind() == _CodeModelItem::Kind_Class; --scope )
            {
            }
          }
          d->m_currentFunction = d->createFunction( cursor, CodeModel::Normal, false );
          d->m_currentFunction->setHiddenFriend( d->m_withinFriendDecl );
          d->m_scopeStack.at( scope )->addFunction( d->m_currentFunction );
        }
        break;

      case CXCursor_Namespace:
      {

        // TODO failing to generate namespace (when there is function maybe?) because it generated an inti_Nmaespace() call
        // maybe related to this https://bugreports.qt.io/browse/PYSIDE-1075
        // Qt namespace work with the same declaration function
        return Skip;

        const auto type = namespaceType( cursor );
        if ( type == NamespaceType::Anonymous || d->isSkipped( cursor ) )
          return Skip;
        const QString name = getCursorSpelling( cursor );
        const auto parentNamespaceItem = std::dynamic_pointer_cast<_NamespaceModelItem>( d->m_scopeStack.back() );
        if ( !parentNamespaceItem )
        {
          const QString message = msgOutOfOrder( cursor, "namespace" )
                                  + u" (current scope: "_s + d->m_scopeStack.back()->name() + u')';
          const Diagnostic d( message, cursor, CXDiagnostic_Error );
          qWarning() << d;
          appendDiagnostic( d );
          return Error;
        }
        // Treat namespaces separately to allow for extending namespaces
        // in subsequent modules.
        NamespaceModelItem namespaceItem = parentNamespaceItem->findNamespace( name );
        namespaceItem.reset( new _NamespaceModelItem( d->m_model, name ) );
        d->setFileName( cursor, namespaceItem.get() );
        namespaceItem->setScope( d->m_scope );
        namespaceItem->setType( type );
        parentNamespaceItem->addNamespace( namespaceItem );
        d->pushScope( namespaceItem );
      }
      break;
      case CXCursor_ParmDecl:
        // Skip in case of nested CXCursor_ParmDecls in case one parameter is a function pointer
        // and function pointer typedefs.
        if ( !d->m_currentArgument && d->m_currentFunction )
        {
          const QString name = getCursorSpelling( cursor );
          d->m_currentArgument.reset( new _ArgumentModelItem( d->m_model, name ) );
          d->m_currentArgument->setType( d->createTypeInfo( cursor ) );
          d->m_currentFunction->addArgument( d->m_currentArgument );
          QString defaultValueExpression = d->cursorValueExpression( this, cursor );
          if ( !defaultValueExpression.isEmpty() )
          {
            d->m_currentArgument->setDefaultValueExpression( defaultValueExpression );
            d->m_currentArgument->setDefaultValue( true );
          }
        }
        else
        {
          return Skip;
        }
        break;
      case CXCursor_TemplateTypeParameter:
      case CXCursor_NonTypeTemplateParameter:
      {
        const TemplateParameterModelItem tItem = cursor.kind == CXCursor_TemplateTemplateParameter
            ? d->createTemplateParameter( cursor ) : d->createNonTypeTemplateParameter( cursor );
        // Apply to function/member template?
        if ( d->m_currentFunction )
        {
          d->m_currentFunction->setTemplateParameters( d->m_currentFunction->templateParameters() << tItem );
        }
        else if ( d->m_currentTemplateTypeAlias )
        {
          d->m_currentTemplateTypeAlias->addTemplateParameter( tItem );
        }
        else if ( d->m_currentClass )   // Apply to class
        {
          const QString &tplParmName = tItem->name();
          if ( Q_UNLIKELY( !insertTemplateParameterIntoClassName( tplParmName, d->m_currentClass )
                           || !insertTemplateParameterIntoClassName( tplParmName, &d->m_scope.back() ) ) )
          {
            const QString message = QStringLiteral( "Error inserting template parameter \"" ) + tplParmName
                                    + QStringLiteral( "\" into " ) + d->m_currentClass->name();
            const Diagnostic d( message, cursor, CXDiagnostic_Error );
            qWarning() << d;
            appendDiagnostic( d );
            return Error;
          }
          d->m_currentClass->setTemplateParameters( d->m_currentClass->templateParameters() << tItem );
        }
      }
      break;
      case CXCursor_TypeAliasTemplateDecl:
        d->startTemplateTypeAlias( cursor );
        break;
      case CXCursor_TypeAliasDecl: // May contain nested CXCursor_TemplateTypeParameter
        if ( !d->m_currentTemplateTypeAlias )
        {
          const CXType type = clang_getCanonicalType( clang_getCursorType( cursor ) );
          if ( type.kind > CXType_Unexposed )
            d->addTypeDef( cursor, type );
          return Skip;
        }
        else
        {
          d->endTemplateTypeAlias( cursor );
        }
        break;
      case CXCursor_TypedefDecl:
      {
        auto underlyingType = clang_getTypedefDeclUnderlyingType( cursor );
        d->addTypeDef( cursor, underlyingType );
        // For "typedef enum/struct {} Foo;", skip the enum/struct
        // definition nested into the typedef (PYSIDE-1228).
        if ( underlyingType.kind == CXType_Elaborated )
          return Skip;
      }
      break;
      // Using declarations look as follows:
      // 1) Normal, non-template case ("using QObject::parent"): UsingDeclaration, TypeRef
      // 2) Simple template case ("using QList::append()"): UsingDeclaration, TypeRef "QList<T>"
      // 3) Template case with parameters ("using QList<T>::append()"):
      //    UsingDeclaration, TemplateRef "QList", TypeRef "T"
      case CXCursor_TemplateRef:
        if ( d->m_withinUsingDeclaration && d->m_usingTypeRef.isEmpty() )
          d->m_usingTypeRef = getCursorSpelling( cursor );
        break;
      case CXCursor_TypeRef:
        if ( d->m_currentFunction )
        {
          if ( !d->m_currentArgument )
            d->qualifyTypeDef( cursor, d->m_currentFunction ); // return type
          else
            d->qualifyTypeDef( cursor, d->m_currentArgument );
        }
        else if ( d->m_currentField )
        {
          d->qualifyTypeDef( cursor, d->m_currentField );
        }
        else if ( d->m_withinUsingDeclaration && d->m_usingTypeRef.isEmpty() )
        {
          d->m_usingTypeRef = d->getBaseClass( clang_getCursorType( cursor ) ).first;
        }
        break;
      case CXCursor_CXXFinalAttr:
        if ( d->m_currentFunction )
          d->m_currentFunction->setFinal( true );
        else if ( d->m_currentClass )
          d->m_currentClass->setFinal( true );
        break;
      case CXCursor_CXXOverrideAttr:
        if ( d->m_currentFunction )
          d->m_currentFunction->setOverride( true );
        break;
      case CXCursor_StaticAssert:
        // Check for Q_PROPERTY() (see PySide6/global.h.in for an explanation
        // how it is defined, and qdoc).
        if ( clang_isDeclaration( cursor.kind ) && d->m_currentClass )
        {
          auto snippet = getCodeSnippet( cursor );
          const auto length = snippet.size();
          if ( length > 12 && *snippet.rbegin() == ')'
               && snippet.compare( 0, 11, "Q_PROPERTY(" ) == 0 )
          {
            const QString qProperty = QString::fromUtf8( snippet.data() + 11, length - 12 );
            d->m_currentClass->addPropertyDeclaration( qProperty );
          }
        }
        break;
      // UsingDeclaration: consists of a TypeRef (base) and OverloadedDeclRef (member name)
      case CXCursor_UsingDeclaration:
        if ( d->m_currentClass )
          d->m_withinUsingDeclaration = true;
        break;
      case CXCursor_OverloadedDeclRef:
        if ( d->m_withinUsingDeclaration && !d->m_usingTypeRef.isEmpty() )
        {
          QString member = getCursorSpelling( cursor );
          if ( member == d->m_currentClass->name() )
            member = d->m_usingTypeRef; // Overloaded member is Constructor, use base
          const auto ap = accessPolicy( clang_getCXXAccessSpecifier( cursor ) );
          d->m_currentClass->addUsingMember( d->m_usingTypeRef, member, ap );
        }
        break;
      default:
        break;
    }
    return BaseVisitor::Recurse;
  }

  bool Builder::endToken( const CXCursor &cursor )
  {
    switch ( cursor.kind )
    {
      case CXCursor_UnionDecl:
      case CXCursor_ClassDecl:
      case CXCursor_StructDecl:
      case CXCursor_ClassTemplate:
      case CXCursor_ClassTemplatePartialSpecialization:
        d->popScope();
        // Continue in outer class after leaving inner class?
        if ( auto lastClass = std::dynamic_pointer_cast<_ClassModelItem>( d->m_scopeStack.back() ) )
          d->m_currentClass = lastClass;
        else
          d->m_currentClass.reset();
        d->m_currentFunctionType = CodeModel::Normal;
        break;
      case CXCursor_EnumDecl:
        if ( d->m_currentEnum )
          d->m_scopeStack.back()->addEnum( d->m_currentEnum );
        d->m_currentEnum.reset();
        break;
      case CXCursor_FriendDecl:
        d->m_withinFriendDecl = false;
        break;
      case CXCursor_VarDecl:
      case CXCursor_FieldDecl:
        d->m_currentField.reset();
        break;
      case CXCursor_Constructor:
        d->qualifyConstructor( cursor );
        if ( d->m_currentFunction )
        {
          d->m_currentFunction->_determineType();
          d->m_currentFunction.reset();
        }
        break;
      case CXCursor_Destructor:
      case CXCursor_CXXMethod:
      case CXCursor_FunctionDecl:
      case CXCursor_FunctionTemplate:
        if ( d->m_currentFunction )
        {
          d->m_currentFunction->_determineType();
          d->m_currentFunction.reset();
        }
        break;
      case CXCursor_ConversionFunction:
        if ( d->m_currentFunction )
        {
          d->m_currentFunction->setFunctionType( CodeModel::ConversionOperator );
          d->m_currentFunction.reset();
        }
        break;
      case CXCursor_Namespace:
        d->popScope();
        break;
      case CXCursor_ParmDecl:
        d->m_currentArgument.reset();
        break;
      case CXCursor_TypeAliasTemplateDecl:
        d->m_currentTemplateTypeAlias.reset();
        break;
      case CXCursor_UsingDeclaration:
        d->m_withinUsingDeclaration = false;
        d->m_usingTypeRef.clear();
        break;
      default:
        break;
    }
    return true;
  }

} // namespace clang
