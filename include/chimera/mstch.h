/**
 * Template wrapper functions for Clang declarations.
 *
 * These wrapper classes take in a Clang declaration and provide helper methods
 * that can be used within an `mstch` template to generate bindings.  Each
 * helper method returns a `mstch::node`, which is a Boost::Variant type that
 * can be a primitive type, std::string, `mstch::object` or `mstch::array`.
 */
#ifndef __CHIMERA_MSTCH_H__
#define __CHIMERA_MSTCH_H__

#include "chimera/configuration.h"

#include <clang/AST/AST.h>
#include <mstch/mstch.hpp>

namespace chimera
{
namespace mstch
{

template<typename T>
class ClangWrapper: public ::mstch::object
{
static_assert(std::is_base_of<clang::NamedDecl, T>::value,
              "'T' must derive from clang::NamedDecl");
public:
    ClangWrapper(const ::chimera::CompiledConfiguration &config,
                 const T *decl)
    : config_(config), decl_(decl)
    , decl_config_(config_.GetDeclaration(decl_))
    {
        register_methods(this, {
            {"name", &ClangWrapper::name},
            {"qualified_name", &ClangWrapper::qualifiedName}
        });
    }

    ::mstch::node name()
    {
        if (const YAML::Node &node = decl_config_["name"])
            return node.as<std::string>();

        return decl_->getNameAsString();
    }

    ::mstch::node qualifiedName()
    {
        if (const YAML::Node &node = decl_config_["qualified_name"])
            return node.as<std::string>();

        return decl_->getQualifiedNameAsString();
    }

protected:
    const ::chimera::CompiledConfiguration &config_;
    const T *decl_;
    const YAML::Node &decl_config_;
};

class CXXRecord: public ClangWrapper<clang::CXXRecordDecl>
{
public:
    CXXRecord(const ::chimera::CompiledConfiguration &config,
              const clang::CXXRecordDecl *decl);

    ::mstch::node bases();
    ::mstch::node type();
    ::mstch::node isCopyable();

    ::mstch::node bindingName();
    ::mstch::node uniquishName();
    ::mstch::node mangledName();
    
    ::mstch::node constructors();
    
    ::mstch::node methods();
    ::mstch::node staticMethods();
    
    ::mstch::node fields();
    ::mstch::node staticFields();
};

class Enum: public ClangWrapper<clang::EnumDecl>
{
public:
    Enum(const ::chimera::CompiledConfiguration &config,
         const clang::EnumDecl *decl);

    ::mstch::node type();
    ::mstch::node values();
};

using EnumConstant = ClangWrapper<clang::EnumConstantDecl>;

class Field: public ClangWrapper<clang::FieldDecl>
{
public:
    Field(const ::chimera::CompiledConfiguration &config,
          const clang::FieldDecl *decl,
          const clang::CXXRecordDecl *class_decl);

    ::mstch::node returnValuePolicy();

private:
    const clang::CXXRecordDecl *class_decl_;
};

class Function: public ClangWrapper<clang::FunctionDecl>
{
public:
    Function(const ::chimera::CompiledConfiguration &config,
             const clang::FunctionDecl *decl,
             const clang::CXXRecordDecl *class_decl = NULL);

    ::mstch::node type();
    ::mstch::node params();
    ::mstch::node returnValuePolicy();

private:
    const clang::CXXRecordDecl *class_decl_;
};

class Var: public ClangWrapper<clang::VarDecl>
{
public:
    Var(const ::chimera::CompiledConfiguration &config,
        const clang::VarDecl *decl);
};

} // namespace mstch
} // namespace chimera


#endif // __CHIMERA_MSTCH_H__
