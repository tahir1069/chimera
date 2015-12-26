#ifndef __CHIMERA_UTIL_H__
#define __CHIMERA_UTIL_H__

#include <clang/AST/ASTContext.h>
#include <clang/AST/Type.h>
#include <clang/Frontend/CompilerInstance.h>

namespace chimera
{
namespace util
{

/**
 * Resolve a declaration string within the scope of a compiler instance.
 *
 * This function parses the provided declaration string as a single line of
 * the form `[declStr];` within the AST that is currently loaded by the
 * provided compiler instance.
 *
 * If it can be resolved to a named declaration, the canonical clang::Decl
 * pointer associated with the declaration will be returned, otherwise NULL.
 */
const clang::NamedDecl* resolveDeclaration(clang::CompilerInstance *ci,
                                           const llvm::StringRef declStr);

/**
 * Resolve a record string within the scope of a compiler instance.
 *
 * This function parses the provided record name string as a single line of
 * the form `typedef [recordStr] <uid>;` within the AST that is currently
 * loaded by the provided compiler instance.
 *
 * If it is resolved to a record declaration, the canonical clang::RecordDecl
 * pointer associated with the declaration will be returned, otherwise NULL.
 */
const clang::RecordDecl* resolveRecord(clang::CompilerInstance *ci,
                                       const llvm::StringRef recordStr);

/**
 * Resolve a type string within the scope of a compiler instance.
 *
 * This function parses the provided record name string as a single line of
 * the form `typedef [recordStr] <uid>;` within the AST that is currently
 * loaded by the provided compiler instance.
 *
 * If it is resolved to a qualified type declaration, the canonical
 * clang::QualType associated with the declaration will be returned, otherwise
 * an empty clang::QualType is returned (use getTypeOrNull() to check this).
 */
const clang::QualType resolveType(clang::CompilerInstance *ci,
                                  const llvm::StringRef typeStr);

/**
 * Resolve a namespace string within the scope of a compiler instance.
 *
 * This function parses the provided namespace string as a single line of the
 * form `namespace [nsStr] {};` within the AST that is currently loaded by the
 * provided compiler instance.
 *
 * If it is resolved to a namespace declaration, the canonical 
 * clang::NamespaceDecl pointer associated with the namespace will be
 * returned, otherwise NULL.
 */
const clang::NamespaceDecl* resolveNamespace(clang::CompilerInstance *ci,
                                             const llvm::StringRef nsStr);


/**
 * Convert the type into one with fully qualified template parameters.
 *
 * Internally uses cling::utils::getFullyQualifiedType().
 */
clang::QualType getFullyQualifiedType(const clang::ASTContext &context,
                                      clang::QualType qt);

/**
 * Get the fully qualified name for a type.
 * This includes full qualification of all template parameters, etc.
 *
 * Internally uses cling::utils::getFullyQualifiedName().
 */
std::string getFullyQualifiedTypeName(const clang::ASTContext &context,
                                      clang::QualType qt);

/**
 * Get the fully qualified name for the type used in a type declaration.
 * This includes full qualification of all template parameters, etc.
 */
std::string getFullyQualifiedDeclTypeAsString(const clang::ASTContext &context,
											  const clang::TypeDecl *decl);

/**
 * Determines if a CXXRecordDecl is referring to a type that could be assigned to.
 */
bool isAssignable(clang::CXXRecordDecl *decl);

/**
 * Determines if a QualType is referring to a type that could be assigned to.
 */
bool isAssignable(clang::ASTContext &context, clang::QualType qual_type);

/**
 * Determines if a CXXRecordDecl is referring to a class that is copyable.
 */
bool isCopyable(clang::CXXRecordDecl *decl);

/**
 * Determines if a QualType is referring to a type that is copyable.
 */
bool isCopyable(clang::ASTContext &context, clang::QualType qual_type);

/**
 * Determines if a particular declaration is within the context of a template class.
 */
bool isInsideTemplateClass(clang::DeclContext *decl_context);

} // namespace util
} // namespace chimera

#endif // __CHIMERA_UTIL_H__
