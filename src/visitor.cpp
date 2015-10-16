#include "chimera/visitor.h"

#include <algorithm>
#include <boost/algorithm/string/join.hpp>
#include <iostream>
#include <llvm/Support/raw_ostream.h>
#include <string>

using namespace chimera;
using namespace clang;
using boost::algorithm::join;

// TODO: Support template functions.
// TODO: Detect missing copy constructors, possibly using:
//  hasUserDeclaredCopyConstructor()
//  hasCopyConstructorWithConstParam ()

chimera::Visitor::Visitor(clang::CompilerInstance *ci,
                          std::unique_ptr<CompiledConfiguration> cc)
: context_(&(ci->getASTContext()))
, printing_policy_(ci->getLangOpts())
, config_(std::move(cc))
{
    // Do nothing.
}

bool chimera::Visitor::VisitDecl(Decl *decl)
{
    // Only visit declarations in namespaces we are configured to read.
    if (!IsEnclosed(decl))
        return true;

    if (isa<ClassTemplateDecl>(decl))
        GenerateClassTemplate(cast<ClassTemplateDecl>(decl));
    else if (isa<CXXRecordDecl>(decl))
        GenerateCXXRecord(cast<CXXRecordDecl>(decl));
    else if (isa<EnumDecl>(decl))
        GenerateEnum(cast<EnumDecl>(decl));
    else if (isa<VarDecl>(decl))
        GenerateGlobalVar(cast<VarDecl>(decl));
    else if (isa<FunctionDecl>(decl))
        GenerateGlobalFunction(cast<FunctionDecl>(decl));

    return true;
}

bool chimera::Visitor::GenerateCXXRecord(CXXRecordDecl *const decl)
{
    // Only traverse CXX records that contain the actual class definition.
    if (!decl->hasDefinition() || decl->getDefinition() != decl)
        return false;

    // Open a stream object unique to this CXX record's mangled name.
    auto stream = config_->GetOutputFile(decl);
    if (!stream)
        return false;

    // Get configuration, and use any overrides if they exist.
    if (config_->DumpOverride(decl, *stream))
        return true;
    const YAML::Node &node = config_->GetDeclaration(decl);

    *stream << "::boost::python::class_<"
            << decl->getTypeForDecl()->getCanonicalTypeInternal().getAsString(printing_policy_);

    const YAML::Node &noncopyable_node = node["noncopyable"];
    if (noncopyable_node && noncopyable_node.as<bool>(false))
        *stream << ", ::boost::noncopyable";

    if (const YAML::Node &held_type_node = node["held_type"])
        *stream << ", " << held_type_node.as<std::string>();

    std::vector<std::string> base_names;

    if (const YAML::Node &bases_node = node["bases"])
        base_names = bases_node.as<std::vector<std::string> >();
    else
        base_names = GetBaseClassNames(decl);

    if (!base_names.empty())
    {
        *stream << ", ::boost::python::bases<"
                  << join(base_names, ", ") << " >";
    }

    *stream << " >(\"" << decl->getNameAsString()
            << "\", boost::python::no_init)\n";

    // Methods
    std::set<std::string> overloaded_method_names;

    for (CXXMethodDecl *const method_decl : decl->methods())
    {
        if (method_decl->getAccess() != AS_public)
            continue; // skip protected and private members

        if (isa<CXXConversionDecl>(method_decl))
            ; // do nothing
        else if (isa<CXXDestructorDecl>(method_decl))
            ; // do nothing
        else if (isa<CXXConstructorDecl>(method_decl))
        {
            GenerateCXXConstructor(
                *stream, decl, cast<CXXConstructorDecl>(method_decl));
        }
        else if (method_decl->isOverloadedOperator())
            ; // TODO: Wrap overloaded operators.
        else if (method_decl->isStatic())
        {
            // TODO: Missing the dot.
            if (GenerateFunction(*stream, decl, method_decl))
                overloaded_method_names.insert(method_decl->getNameAsString());
        }
        else
        {
            // TODO: Missing the dot.
            GenerateFunction(*stream, decl, method_decl);
        }
    }

    // Static methods MUST be declared after overloads are defined.
    for (const std::string &method_name : overloaded_method_names)
        *stream << ".staticmethod(\"" << method_name << "\")\n";

    // Fields
    for (FieldDecl *const field_decl : decl->fields())
    {
        if (field_decl->getAccess() != AS_public)
            continue; // skip protected and private fields

        GenerateField(*stream, decl, field_decl);
    }

    for (Decl *const child_decl : decl->decls())
    {
        if (isa<VarDecl>(child_decl))
            GenerateStaticField(*stream, decl, cast<VarDecl>(child_decl));
    }

    *stream << ";\n";
    
    return true;
}

bool chimera::Visitor::GenerateCXXConstructor(
    chimera::Stream &stream,
    CXXRecordDecl *class_decl,
    CXXConstructorDecl *decl)
{
    decl = decl->getCanonicalDecl();

    if (decl->isDeleted())
        return false;

    std::vector<std::string> argument_types;

    for (ParmVarDecl *param_decl : decl->params())
        argument_types.push_back(param_decl->getType().getAsString(printing_policy_));

    stream << ".def(::boost::python::init<"
           << join(argument_types, ", ")
           << ">())\n";
    return true;
}

bool chimera::Visitor::GenerateFunction(
    chimera::Stream &stream,
    CXXRecordDecl *class_decl, FunctionDecl *decl)
{
    decl = decl->getCanonicalDecl();

    if (decl->isDeleted())
        return false;

    // Get configuration, and use any overrides if they exist.
    if (config_->DumpOverride(decl, stream))
        return true;
    const YAML::Node &node = config_->GetDeclaration(decl);

    // Extract the pointer type of this function declaration.
    QualType pointer_type;
    if (class_decl && !cast<CXXMethodDecl>(decl)->isStatic())
    {
        pointer_type = context_->getMemberPointerType(
            decl->getType(), class_decl->getTypeForDecl());
    }
    else
    {
        pointer_type = context_->getPointerType(decl->getType());
    }
    pointer_type = pointer_type.getCanonicalType();

    // Extract the return type of this function declaration.
    const Type *return_type = decl->getReturnType().getTypePtr();

    // Check that a valid return value policy was defined, or that one can be
    // determined automatically by Boost.Python.
    const YAML::Node &rvp_node = node["return_value_policy"];
    if (!rvp_node)
    {
        if (return_type->isReferenceType())
        {
            std::cerr
                << "Warning: Skipped method '"
                << decl->getQualifiedNameAsString()
                << "' because it returns a reference and no"
                   "  'return_value_policy' was specified.\n";
            return false;
        }
        else if (return_type->isPointerType())
        {
            std::cerr
                << "Warning: Skipped method '"
                << decl->getQualifiedNameAsString()
                << "' because it returns a pointer and no"
                   "  'return_value_policy' was specified.\n";
            return false;
        }

        // TODO: Check if return_type is non-copyable.
    }

    // If we are inside a class declaration, this is being called within a
    // builder pattern and will start with '.' since it is a member function.
    if (class_decl)
        stream << ".";

    // Create the actual function declaration here using its name and its
    // full pointer reference.
    stream << "def(\"" << decl->getNameAsString() << "\""
           << ", static_cast<" << pointer_type.getAsString(printing_policy_) << ">(&"
           << decl->getQualifiedNameAsString() << ")";

    // If a return value policy was specified, insert it after the function.
    if (rvp_node)
    {
        stream << ", ::boost::python::return_value_policy<"
               << rvp_node.as<std::string>() << " >";
    }

    // Construct a list of the arguments that are provided to this function,
    // and define named arguments for them based on their c++ names.
    const auto params = GetParameterNames(decl);
    if (!params.empty())
    {
        // TODO: Suppress any default parameters that occur after the first
        // non-default to default transition. This can only occur if evaluating
        // the default value of one or more parameters failed.

        // TODO: Assign names to unnamed arguments.

        std::vector<std::string> python_args;
        for (const auto &param : params)
        {
            std::stringstream python_arg;
            python_arg << "::boost::python::arg(\"" << param.first << "\")";

            if (!param.second.empty())
                python_arg << " = " << param.second;

            python_args.push_back(python_arg.str());
        }

        stream << ", (" << join(python_args, ", ") << ")";
    }

    stream << ")\n";
    return true;
}

bool chimera::Visitor::GenerateField(
    chimera::Stream &stream,
    clang::CXXRecordDecl *class_decl,
    clang::FieldDecl *decl)
{
    if (decl->getType().isConstQualified())
        stream << ".def_readonly";
    else
        stream << ".def_readwrite";

    // TODO: Check if a copy constructor is defined for this type.

    stream << "(\"" << decl->getNameAsString() << "\","
           << " &" << decl->getQualifiedNameAsString() << ")\n";
    return true;
}

bool chimera::Visitor::GenerateStaticField(
    chimera::Stream &stream,
    clang::CXXRecordDecl *class_decl,
    clang::VarDecl *decl)
{
    if (decl->getAccess() != AS_public)
        return false;
    else if (!decl->isStaticDataMember())
        return false;

    stream << ".add_static_property(\"" << decl->getNameAsString() << "\", "
           << "[]() { return " << decl->getQualifiedNameAsString() << "; }";

    if (!decl->getType().isConstQualified())
    {
        stream << "[](" << decl->getType().getAsString(printing_policy_) << " value) { "
               << decl->getQualifiedNameAsString() << " = value; }";
    }

    stream << ")\n";

    return true;
}

bool chimera::Visitor::GenerateClassTemplate(clang::ClassTemplateDecl *decl)
{
    if (decl != decl->getCanonicalDecl())
        return false;

    for (ClassTemplateSpecializationDecl *spec_decl : decl->specializations())
    {
        if (spec_decl->getSpecializationKind() == TSK_Undeclared)
            continue;

        CXXRecordDecl *decl = spec_decl->getTypeForDecl()->getAsCXXRecordDecl();
        GenerateCXXRecord(decl);
    }

    return true;
}

bool chimera::Visitor::GenerateEnum(clang::EnumDecl *decl)
{
    auto stream = config_->GetOutputFile(decl);
    if (!stream)
        return false;

    *stream << "::boost::python::enum_<"
            << decl->getQualifiedNameAsString()
            << ">(\"" << decl->getNameAsString() << "\")\n";

    for (EnumConstantDecl *constant_decl : decl->enumerators())
    {
        *stream << ".value(\"" << constant_decl->getNameAsString() << "\", "
                << constant_decl->getQualifiedNameAsString() << ")\n";
    }

    *stream << ";\n";

    return true;
}

bool chimera::Visitor::GenerateGlobalVar(clang::VarDecl *decl)
{
    if (!decl->isFileVarDecl())
        return false;
    else if (!decl->isThisDeclarationADefinition())
        return false;

    auto stream = config_->GetOutputFile(decl);
    if (!stream)
        return false;

    *stream << "::boost::python::scope().attr(\"" << decl->getNameAsString()
            << "\") = " << decl->getQualifiedNameAsString() << ";\n";
    return true;
}

bool chimera::Visitor::GenerateGlobalFunction(clang::FunctionDecl *decl)
{
    if (isa<clang::CXXMethodDecl>(decl))
        return false;
    else if (decl->isOverloadedOperator())
        return false; // TODO: Wrap overloaded operators.

    auto stream = config_->GetOutputFile(decl);
    if (!stream)
        return false;

    return GenerateFunction(*stream, nullptr, decl);
}

std::vector<std::string> chimera::Visitor::GetBaseClassNames(
    CXXRecordDecl *decl) const
{
    std::vector<std::string> base_names;

    for (CXXBaseSpecifier &base_decl : decl->bases())
    {
        if (base_decl.getAccessSpecifier() != AS_public)
            continue;

        // TODO: Filter out transitive base classes.

        CXXRecordDecl *const base_record_decl
          = base_decl.getType()->getAsCXXRecordDecl();

        std::stringstream base_name;
        base_name << base_record_decl->getQualifiedNameAsString();

        if (isa<ClassTemplateSpecializationDecl>(base_record_decl))
        {
            auto spec_decl = cast<ClassTemplateSpecializationDecl>(base_record_decl);
            const TemplateArgumentList &template_args = spec_decl->getTemplateArgs();

            base_name << "<";

            for (size_t i = 0; i < template_args.size(); ++i)
            {
                const QualType arg_type = template_args[i].getAsType();
                base_name << arg_type.getAsString(printing_policy_);

                if (i != template_args.size() - 1)
                    base_name << ", ";
            }

            base_name << " >";
        }

        base_names.push_back(base_name.str());
    }

    return base_names;
}

std::vector<std::pair<std::string, std::string>>
    chimera::Visitor::GetParameterNames(clang::FunctionDecl *decl) const
{
    std::vector<std::pair<std::string, std::string>> params;

    for (ParmVarDecl *param_decl : decl->params())
    {
        const std::string param_name = param_decl->getNameAsString();
        const Type *param_type = param_decl->getType().getTypePtr();
        std::string param_value;

        if (param_decl->hasDefaultArg()
            && !param_decl->hasUninstantiatedDefaultArg()
            && !param_decl->hasUnparsedDefaultArg())
        {
            Expr *default_expr = param_decl->getDefaultArg();
            assert(default_expr);

            Expr::EvalResult result;
            bool success;

            if (param_type->isReferenceType())
                success = default_expr->EvaluateAsLValue(result, *context_);
            else
                success = default_expr->EvaluateAsRValue(result, *context_);

            if (success)
            {
                param_value = result.Val.getAsString(
                    *context_, param_decl->getType());
            }
            else if (default_expr->hasNonTrivialCall(*context_))
            {
                // TODO: How do we print the decl with argument + return types?
                std::cerr
                  << "Warning: Unable to evaluate non-trivial call in default"
                     "  value for parameter"
                  << " '" << param_name << "' of method"
                  << " '" << decl->getQualifiedNameAsString() << "'.\n";
            }
            else
            {
                // TODO: How do we print the decl with argument + return types?
                std::cerr
                  << "Warning: Failed to evaluate default value for parameter"
                  << " '" << param_name << "' of method"
                  << " '" << decl->getQualifiedNameAsString() << "'.\n";
            }
        }

        params.push_back(std::make_pair(param_name, param_value));
    }

    return params;
}

bool chimera::Visitor::IsEnclosed(Decl *decl) const
{
    // Filter over the namespaces and only traverse ones that are enclosed
    // by one of the configuration namespaces.
    for (const auto &it : config_->GetNamespaces())
    {
        if (decl->getDeclContext() && it->Encloses(decl->getDeclContext()))
        {
            return true;
        }
    }
    return false;
}
