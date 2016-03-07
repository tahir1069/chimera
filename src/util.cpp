#include "chimera/util.h"
#include "cling_utils_AST.h"

#include <clang/AST/ASTConsumer.h>
#include "clang/AST/DeclTemplate.h"
#include <clang/AST/Mangle.h>
#include <clang/Parse/Parser.h>
#include <clang/Sema/Sema.h>
#include <iostream>
#include <sstream>

using namespace clang;

namespace
{
/**
 * Empty QualType used when returning a type-resolution failure.
 */
static const QualType emptyType_;

/**
 * Generates incremental typedef names to avoid namespace conflicts.
 *
 * This is used by functions that need to compile a temporary typedef to
 * resolve a type within the AST.
 */
std::string generateUniqueName()
{
    // Use a static variable to generate non-duplicate names.
    static unsigned counter = 0;

    std::stringstream ss;
    ss << "chimera_placeholder_" << (counter++);
    return ss.str();
}

} // namespace

const NamedDecl*
chimera::util::resolveDeclaration(CompilerInstance *ci,
                                  const llvm::StringRef declStr)
{
    // Immediately return if passed an empty string.
    if (declStr.empty())
        return nullptr;

    // Create a new parser to handle this type parsing.
    Preprocessor &preprocessor = ci->getPreprocessor();
    Sema &sema = ci->getSema();
    Parser parser(preprocessor, sema, /* SkipFunctionBodies = */ false);

    // Set up the preprocessor to only care about incrementally handling type.
    preprocessor.getDiagnostics().setIgnoreAllWarnings(true);
    preprocessor.getDiagnostics().setSeverityForGroup(diag::Flavor::WarningOrError,
                                                      "out-of-line-declaration",
                                                      diag::Severity::Ignored);
    preprocessor.enableIncrementalProcessing();
    const_cast<LangOptions&>(preprocessor.getLangOpts()).SpellChecking = 0;

    // Put the type string into a buffer and run it through the preprocessor.
    FileID fid = sema.getSourceManager().createFileID(
        llvm::MemoryBuffer::getMemBufferCopy(declStr.str() + ";",
                                             "chimera.util.resolveDeclaration"));
    preprocessor.EnterSourceFile(fid, /* DirLookup = */ 0, SourceLocation());
    parser.Initialize();
    
    // Try parsing the type name.
    Parser::DeclGroupPtrTy ADecl;
    while (!parser.ParseTopLevelDecl(ADecl))
    {
        // A DeclGroupRef may have multiple Decls, so we iterate through each one.
        // TODO: We probably shouldn't actually iterate here.
        if (ADecl)
        {
            DeclGroupRef DG = ADecl.get();
            for (auto i = DG.begin(), e = DG.end(); i != e; ++i)
            {
                Decl *D = (*i)->getCanonicalDecl();
                return (isa<NamedDecl>(D)) ? cast<NamedDecl>(D) : nullptr;
            }
        }
    }

    std::cerr << "Failed to parse '" << declStr.str() << "'." << std::endl;
    return nullptr;
}

const QualType
chimera::util::resolveType(CompilerInstance *ci,
                           const llvm::StringRef typeStr)
{
    auto decl = chimera::util::resolveDeclaration(
        ci, "typedef " + typeStr.str() + " " + generateUniqueName());
    if (!decl)
        return emptyType_;

    if (!isa<TypedefDecl>(decl))
    {
        std::cerr << "Expected 'typedef' declaration, found '"
                  << decl->getNameAsString() << "'." << std::endl;
        return emptyType_;
    }

    auto typedef_decl = cast<TypedefDecl>(decl);
    return typedef_decl->getUnderlyingType().getCanonicalType();
}

const RecordDecl*
chimera::util::resolveRecord(CompilerInstance *ci,
                             const llvm::StringRef recordStr)
{
    auto type = chimera::util::resolveType(ci, recordStr).getTypePtrOrNull();
    if (!type)
        return nullptr;

    auto cxx_record_type = type->getAsCXXRecordDecl();
    if (!cxx_record_type)
        return nullptr;

    return cast<RecordDecl>(cxx_record_type->getCanonicalDecl());
}

const NamespaceDecl*
chimera::util::resolveNamespace(CompilerInstance *ci,
                                const llvm::StringRef nsStr)
{
    auto decl = chimera::util::resolveDeclaration(
        ci, "namespace " + generateUniqueName() + " = " + nsStr.str());
    if (!decl)
        return nullptr;

    if (!isa<NamespaceAliasDecl>(decl))
    {
        std::cerr << "Expected 'namespace' alias declaration, found '"
                  << decl->getNameAsString() << "'." << std::endl;
        return nullptr;
    }

    return cast<NamespaceAliasDecl>(decl)->getNamespace()->getCanonicalDecl();
}

std::string
chimera::util::constructMangledName(ASTContext &context,
                                    const NamedDecl *decl)
{
    std::string mangled_name;
    llvm::raw_string_ostream mangled_name_stream(mangled_name);
    context.createMangleContext()->mangleName(decl, mangled_name_stream);
    return mangled_name_stream.str();
}

std::string
chimera::util::constructBindingName(ASTContext &context,
                                    const CXXRecordDecl *decl)
{
    // If this is an anonymous struct, then use the name of its typedef.
    if (TypedefNameDecl *typedef_decl = decl->getTypedefNameForAnonDecl())
        return typedef_decl->getNameAsString();

    // If the class is not a template class, use the unqualified string name.
    if (!isa<ClassTemplateSpecializationDecl>(decl))
        return decl->getNameAsString();

    // If the class is a template, use the mangled string name so that it does
    // not collide with other template instantiations.
    std::string mangled_name = chimera::util::constructMangledName(context, decl);

    // Throw a warning that this class name was mangled, because users will
    // probably want to override these names with more sensible ones.
    std::cerr << "Warning: The class '"
              << chimera::util::getFullyQualifiedTypeName(context,
                    QualType(decl->getTypeForDecl(), 0)) << "'"
              << " was bound to the mangled name "
              << "'" << mangled_name << "'"
              << " because the unqualified class name of "
              << "'" << decl->getNameAsString() << "'"
              << " may be ambiguous.\n";

    return mangled_name;
}

QualType
chimera::util::getFullyQualifiedType(ASTContext &context,
                                     QualType qt)
{
    return cling::utils::TypeName::GetFullyQualifiedType(qt, context);
}

std::string
chimera::util::getFullyQualifiedTypeName(ASTContext &context,
                                         QualType qt)
{
    return cling::utils::TypeName::GetFullyQualifiedName(qt, context);
}

std::string chimera::util::getFullyQualifiedDeclTypeAsString(
    ASTContext &context, const TypeDecl *decl)
{
    return chimera::util::getFullyQualifiedTypeName(
        context, QualType(decl->getTypeForDecl(), 0));
}

bool chimera::util::isAssignable(const CXXRecordDecl *decl)
{
    if (decl->isAbstract())
        return false;
    else if (!decl->hasCopyAssignmentWithConstParam())
        return false;

    for (CXXMethodDecl *method_decl : decl->methods())
        if (method_decl->isCopyAssignmentOperator() && !method_decl->isDeleted())
            return true;

    return false;
}

bool chimera::util::isAssignable(ASTContext &context, QualType qual_type)
{
    if (qual_type.isConstQualified())
        return false;
    else if (CXXRecordDecl *decl = qual_type.getTypePtr()->getAsCXXRecordDecl())
        return isAssignable(decl);
    else if (qual_type.getTypePtr()->isArrayType())
        return false;
    else
        return qual_type.isTriviallyCopyableType(context);
}

bool chimera::util::isCopyable(const CXXRecordDecl *decl)
{
    if (decl->isAbstract())
        return false;
    else if (!decl->hasCopyConstructorWithConstParam())
        return false;

    for (CXXConstructorDecl *ctor_decl : decl->ctors())
        if (ctor_decl->isCopyConstructor() && !ctor_decl->isDeleted())
            return true;

    return false;
}

bool chimera::util::isCopyable(ASTContext &context, QualType qual_type)
{
    // TODO: Is this logic correct?

    if (CXXRecordDecl *decl = qual_type.getTypePtr()->getAsCXXRecordDecl())
        return isCopyable(decl);
    else
        return qual_type.isTriviallyCopyableType(context);
}

bool chimera::util::isInsideTemplateClass(const DeclContext *decl_context)
{
    if (!decl_context->isRecord())
        return false;

    if (isa<CXXRecordDecl>(decl_context))
    {
        const CXXRecordDecl *record_decl =
            cast<const CXXRecordDecl>(decl_context);
        if (record_decl->getDescribedClassTemplate())
            return true;
    }

    const DeclContext *parent_context = decl_context->getParent();
    if (parent_context)
        return isInsideTemplateClass(parent_context);
    else
        return false;
}

std::set<const CXXRecordDecl *>
chimera::util::getBaseClassDecls(const CXXRecordDecl *decl)
{
    std::set<const CXXRecordDecl *> base_decls;

    for (const CXXBaseSpecifier &base_decl : decl->bases())
    {
        if (base_decl.getAccessSpecifier() != AS_public)
            continue;

        // TODO: Filter out transitive base classes.

        base_decls.insert(base_decl.getType()->getAsCXXRecordDecl());
    }

    return base_decls;
}

std::set<const CXXRecordDecl *>
chimera::util::getBaseClassDecls(
    const CXXRecordDecl *decl,
    std::set<const CXXRecordDecl *> available_decls)
{
    // Get all base classes.
    std::set<const CXXRecordDecl *> all_base_decls =
        chimera::util::getBaseClassDecls(decl);

    // Filter the classes by availability.
    std::set<const CXXRecordDecl *> base_decls;
    for (const CXXRecordDecl *base_decl : all_base_decls)
    {
        const bool base_available =
            available_decls.count(base_decl);

        if (base_available)
            base_decls.insert(base_decl);
        else
        {
            std::cerr << "Warning: Omitted base class '"
                      << base_decl->getNameAsString()
                      << "' of class '"
                      << decl->getNameAsString()
                      << "' because it could not be wrapped.\n";
        }
    }

    return base_decls;
}

bool chimera::util::containsIncompleteType(QualType qual_type)
{
    const Type *type = qual_type.getTypePtr();

    // TODO: We're probably missing a few cases here.

    if (isa<PointerType>(type))
    {
        const PointerType *pointer_type = cast<PointerType>(type);
        return containsIncompleteType(pointer_type->getPointeeType());
    }
    else if (isa<ReferenceType>(type))
    {
        const ReferenceType *reference_type = cast<ReferenceType>(type);
        return containsIncompleteType(reference_type->getPointeeType());
    }
    else
    {
        return type->isIncompleteType();
    }
}

bool chimera::util::containsIncompleteType(
    ASTContext &context, const FunctionDecl *decl)
{
    for (unsigned int iparam = 0; iparam < decl->getNumParams(); ++iparam)
    {
        const ParmVarDecl *const param_decl = decl->getParamDecl(iparam);
        if (containsIncompleteType(param_decl->getOriginalType()))
        {
            std::cerr
                << "Warning: Skipped function "
                << decl->getQualifiedNameAsString()
                << " because argument '"
                << param_decl->getNameAsString()
                << "' has an incomplete type '"
                << chimera::util::getFullyQualifiedTypeName(
                    context, param_decl->getType())
                <<  "'.\n";
            return true;
        }
    }
    return false;
}

bool chimera::util::containsRValueReference(
    ASTContext &context, FunctionDecl *decl)
{
    for (unsigned int iparam = 0; iparam < decl->getNumParams(); ++iparam)
    {
        ParmVarDecl *const param_decl = decl->getParamDecl(iparam);
        if (param_decl->getType().getTypePtr()->isRValueReferenceType())
        {
            std::cerr
                << "Warning: Skipped method '"
                << decl->getQualifiedNameAsString()
                << "' because";

            if (param_decl->getNameAsString().empty())
                std::cerr << " the parameter at index " << iparam;
            else
                std::cerr
                    << " parameter '" << param_decl->getNameAsString() << "'";

            std::cerr
                << " is an rvalue reference of type '"
                << getFullyQualifiedTypeName(context, param_decl->getType())
                << "'.\n";

            return true;
        }
    }
    return false;
}

bool chimera::util::needsReturnValuePolicy(
    ASTContext &context, const NamedDecl *decl, QualType return_type)
{
    if (return_type.getTypePtr()->isReferenceType())
    {
        std::cerr
            << "Warning: Skipped method "
            << decl->getQualifiedNameAsString()
            << "' because it returns the reference type '"
            << getFullyQualifiedTypeName(context, return_type)
            << "'and no 'return_value_policy' was specified.\n";
        return true;
    }
    else if (return_type.getTypePtr()->isPointerType())
    {
        std::cerr
            << "Warning: Skipped method "
            << decl->getQualifiedNameAsString()
            << "' because it returns the pointer type '"
            << getFullyQualifiedTypeName(context, return_type)
            << "' and no 'return_value_policy' was specified.\n";
        return true;
    }
    return false;
}

std::pair<unsigned, unsigned>
chimera::util::getFunctionArgumentRange(const clang::FunctionDecl *decl)
{
    const unsigned max_arguments = decl->getNumParams();

    unsigned min_arguments = 0;
    for (; min_arguments < max_arguments; ++min_arguments)
    {
        if (decl->getParamDecl(min_arguments)->hasDefaultArg())
            break;
    }

    return std::pair<unsigned, unsigned>(min_arguments, max_arguments);
}
