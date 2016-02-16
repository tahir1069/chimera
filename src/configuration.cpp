#include "chimera/configuration.h"
#include "chimera/mstch.h"
#include "chimera/util.h"

// TODO: Clean this up and move to something other than a header.
#include "chimera/boost_python_mstch.h"

#include <fstream>
#include <iostream>

using namespace clang;

namespace
{

/**
 * Counts the number of whitespace-separated words in a string.
 *
 * See: http://stackoverflow.com/a/3672259 
 */
size_t countWordsInString(const std::string & str)
{
    std::stringstream stream(str);
    return std::distance(std::istream_iterator<std::string>(stream),
                         std::istream_iterator<std::string>());
}

} // namespace


const YAML::Node chimera::CompiledConfiguration::emptyNode_(
    YAML::NodeType::Undefined);

chimera::Configuration::Configuration()
: outputPath_(".")
, outputModuleName_("chimera_binding")
{
    // Do nothing.
}

chimera::Configuration& chimera::Configuration::GetInstance()
{
    static chimera::Configuration config;
    return config;
}

void chimera::Configuration::LoadFile(const std::string &filename)
{
    try
    {
        configNode_ = YAML::LoadFile(filename);
        configFilename_ = filename;
    }
    catch(YAML::Exception& e) 
    {
        // If unable to read the configuration YAML, terminate with an error.
        std::cerr << "Unable to read configuration '" << filename << "'."
                  << std::endl << e.what() << std::endl;
        exit(-1);
    }
}

void chimera::Configuration::SetOutputPath(const std::string &path)
{
    // Setting the path to the empty string makes no sense and will break the
    // binding path concatenation, so if an empty string is passed, assume the
    // caller wanted to reset back to the default.
    outputPath_ = path.empty() ? "." : path;
}

void chimera::Configuration::SetOutputModuleName(const std::string &moduleName)
{
    // Setting the module name to the empty string makes no sense and will
    // break the binding, so if an empty string is passed, assume the caller
    // wanted to reset back to the default.
    outputModuleName_ = moduleName.empty() ? "chimera_binding" : moduleName;
}

std::unique_ptr<chimera::CompiledConfiguration>
chimera::Configuration::Process(CompilerInstance *ci) const
{
    return std::unique_ptr<chimera::CompiledConfiguration>(
        new CompiledConfiguration(*this, ci));
}

const YAML::Node& chimera::Configuration::GetRoot() const
{
    return configNode_;
}

const std::string &chimera::Configuration::GetConfigFilename() const
{
    return configFilename_;
}

const std::string &chimera::Configuration::GetOutputPath() const
{
    return outputPath_;
}

const std::string &chimera::Configuration::GetOutputModuleName() const
{
    return outputModuleName_;
}

chimera::CompiledConfiguration::CompiledConfiguration(
    const chimera::Configuration &parent, CompilerInstance *ci)
: parent_(parent)
, ci_(ci)
{   
    // Get a reference to the configuration YAML structure.
    const YAML::Node &configNode = parent.GetRoot();

    // Resolve namespace configuration entries within provided AST.
    for(const auto &it : configNode["namespaces"])
    {
        std::string ns_str = it.as<std::string>();
        auto ns = chimera::util::resolveNamespace(ci, ns_str);
        if (ns)
        {
            namespaces_.insert(ns);
        }
        else
        {
            std::cerr << "Unable to resolve namespace: "
                      << "'" << ns_str << "'." << std::endl;
        }
    }

    // Resolve namespace configuration entries within provided AST.
    for(const auto &it : configNode["declarations"])
    {
        std::string decl_str = it.first.as<std::string>();

        // If there are multiple words, assume a full declaration.
        // If there is only one word, assume a record declaration.
        auto decl = (countWordsInString(decl_str) == 1)
                     ? chimera::util::resolveRecord(ci, decl_str)
                     : chimera::util::resolveDeclaration(ci, decl_str);
        if (decl)
        {
            declarations_[decl] = it.second;
        }
        else
        {
            std::cerr << "Unable to resolve declaration: "
                      << "'" << decl_str << "'" << std::endl;
        }
    }

    // Resolve type configuration entries within provided AST.
    for(const auto &it : configNode["types"])
    {
        std::string type_str = it.first.as<std::string>();
        auto type = chimera::util::resolveType(ci, type_str);
        if (type.getTypePtrOrNull())
        {
            types_.push_back(std::make_pair(type, it.second));
        }
        else
        {
            std::cerr << "Unable to resolve type: "
                      << "'" << type_str << "'" << std::endl;
        }
    }
}

chimera::CompiledConfiguration::~CompiledConfiguration()
{
    // Create the top-level binding source file.
    std::string binding_filename =
        parent_.GetOutputPath() + "/" + parent_.GetOutputModuleName() + ".cpp";

    // Create an output file depending on the provided parameters.
    auto stream = ci_->createOutputFile(
        binding_filename,
        false, // Open the file in binary mode
        false, // Register with llvm::sys::RemoveFileOnSignal
        "", // The derived basename (shouldn't be used)
        "", // The extension to use for derived name (shouldn't be used)
        false, // Use a temporary file that should be renamed
        false // Create missing directories in the output path
    );

    // Resolve customizable snippets that will be inserted into the file.
    // Augment top-level context as necessary.
    const YAML::Node &template_config = parent_.GetRoot()["template"]["main"];
    ::mstch::map full_context {
        {"header", Lookup(template_config["header"])},
        {"precontent", Lookup(template_config["precontent"])},
        {"prebody", Lookup(template_config["prebody"])},
        {"footer", Lookup(template_config["footer"])},
        {"module", ::mstch::map {
            {"name", parent_.GetOutputModuleName()},
            {"bindings", bindings_},
        }}
    };

    // Render the mstch template to the given output file.
    std::string view = Lookup(parent_.GetRoot()["template"]["module"]);
    if (view.empty())
        view = MODULE_CPP;
    *stream << ::mstch::render(view, full_context);
}

const std::set<const clang::NamespaceDecl*>&
chimera::CompiledConfiguration::GetNamespaces() const
{
    return namespaces_;
}

const YAML::Node&
chimera::CompiledConfiguration::GetDeclaration(const clang::Decl *decl) const
{
    const auto d = declarations_.find(decl->getCanonicalDecl());
    return d != declarations_.end() ? d->second : emptyNode_;
}

const YAML::Node&
chimera::CompiledConfiguration::GetType(const clang::QualType type) const
{
    const auto canonical_type = type.getCanonicalType();
    for (const auto &entry : types_)
    {
        if (entry.first == canonical_type)
            return entry.second;
    }
    return emptyNode_;
}

std::string
chimera::CompiledConfiguration::GetConstant(const std::string &value) const
{
    const YAML::Node &constants = parent_.GetRoot()["constants"];
    return constants[value].as<std::string>(value);
}

clang::CompilerInstance *chimera::CompiledConfiguration::GetCompilerInstance() const
{
    return ci_;
}

clang::ASTContext &chimera::CompiledConfiguration::GetContext() const
{
    return ci_->getASTContext();
}

bool chimera::CompiledConfiguration::IsEnclosed(const clang::Decl *decl) const
{
    // Filter over the namespaces and only traverse ones that are enclosed
    // by one of the configuration namespaces.
    for (const auto &it : GetNamespaces())
    {
        if (decl->getDeclContext() && it->Encloses(decl->getDeclContext()))
        {
            return true;
        }
    }
    return false;
}

std::string chimera::CompiledConfiguration::Lookup(const YAML::Node &node) const
{
    std::string config_path = parent_.GetConfigFilename();

    // If the node simply contains a string, return it.
    if (node.Type() == YAML::NodeType::Scalar)
        return node.as<std::string>();
    
    // If the node contains a "source:" entry, load from that.
    if (const YAML::Node &source_node = node["source"])
    {
        // Concatenate YAML filepath with source relative path.
        // TODO: this is somewhat brittle.
        std::string source_path = source_node.as<std::string>();

        if (source_path.front() != '/')
        {
            std::size_t found = config_path.rfind("/");
            if (found != std::string::npos)
            {
                source_path = config_path.substr(0, found) + "/" + source_path;
            }
            else
            {
                source_path = "./" + source_path;
            }
        }

        // Try to open configuration file.
        std::ifstream source(source_path);
        if (source.fail())
        {
            std::cerr << "Warning: Failed to open source '"
                      << source_path << "': " << strerror(errno) << std::endl;
            return "";
        }

        // Copy file content to the output stream.
        std::string snippet;
        snippet.assign(std::istreambuf_iterator<char>(source),
                       std::istreambuf_iterator<char>());
        return snippet;
    }

    // Return an empty string if not available.
    return "";
}

bool
chimera::CompiledConfiguration::Render(std::string view, std::string key,
                                       const std::shared_ptr<::mstch::object> &context)
{
    // Get the mangled name property if it exists.
    if (!context->has("mangled_name"))
    {
        std::cerr << "Cannot serialize template with no mangled name." << std::endl;
        return false;
    }
    std::string mangled_name = ::mstch::render("{{mangled_name}}", context);

    // Create an output file depending on the provided parameters.
    std::string binding_filename =
        parent_.GetOutputPath() + "/" + mangled_name + ".cpp";
    auto stream = ci_->createOutputFile(
        binding_filename,
        false, // Open the file in binary mode
        false, // Register with llvm::sys::RemoveFileOnSignal
        "", // The derived basename (shouldn't be used)
        "", // The extension to use for derived name (shouldn't be used)
        false, // Use a temporary file that should be renamed
        false // Create missing directories in the output path
    );

    // If file creation failed, report the error and return a nullptr.
    if (!stream)
    {
        std::cerr << "Failed to create output file "
                  << "'" << binding_filename << "'"
                  << " for "
                  << "'" << ::mstch::render("{{name}}", context) << "'."
                  << std::endl;
        return false;
    }

    // Resolve customizable snippets that will be inserted into the file.
    // Augment top-level context as necessary.
    const YAML::Node &template_config = parent_.GetRoot()["template"]["file"];
    ::mstch::map full_context {
        {"header", Lookup(template_config["header"])},
        {"precontent", Lookup(template_config["precontent"])},
        {"footer", Lookup(template_config["footer"])},
        {key, context}
    };

    // Render the mstch template to the given output file.
    *stream << ::mstch::render(view, full_context);
    std::cout << binding_filename << std::endl;

    // Record this binding name for use at the top-level.
    bindings_.push_back(mangled_name);
    return true;
}

bool chimera::CompiledConfiguration::Render(const std::shared_ptr<chimera::mstch::CXXRecord> context)
{
    std::string view = Lookup(parent_.GetRoot()["template"]["cxx_record"]);
    if (view.empty())
        view = CLASS_BINDING_CPP;
    return Render(view, "class", context);
}

bool chimera::CompiledConfiguration::Render(const std::shared_ptr<chimera::mstch::Enum> context)
{
    std::string view = Lookup(parent_.GetRoot()["template"]["enum"]);
    if (view.empty())
        view = ENUM_BINDING_CPP;
    return Render(view, "enum", context);
}

bool chimera::CompiledConfiguration::Render(const std::shared_ptr<chimera::mstch::Function> context)
{
    std::string view = Lookup(parent_.GetRoot()["template"]["function"]);
    if (view.empty())
        view = FUNCTION_BINDING_CPP;
    return Render(view, "function", context);
}

bool chimera::CompiledConfiguration::Render(const std::shared_ptr<chimera::mstch::Variable> context)
{
    std::string view = Lookup(parent_.GetRoot()["template"]["variable"]);
    if (view.empty())
        view = VAR_BINDING_CPP;
    return Render(VAR_BINDING_CPP, "variable", context);
}
