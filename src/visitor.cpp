#include "chimera/visitor.h"
#include "chimera/configuration.h"

#include <iostream>
#include <string>

using namespace chimera;
using namespace clang;

chimera::Visitor::Visitor(CompilerInstance *CI)
: astContext(&(CI->getASTContext()))
{
    // Do nothing.
}

bool chimera::Visitor::TraverseNamespaceDecl(NamespaceDecl *ns)
{
    const std::string ns_string = ns->getCanonicalDecl()->getQualifiedNameAsString();
    const chimera::Configuration& config = chimera::Configuration::GetInstance();
    const auto namespaces = config.GetRoot()["namespaces"];

    // Traverse only namespaces contained by the configuration namespaces key.
    for(auto it = namespaces.begin(); it != namespaces.end(); ++it)
    {
        std::string ns_key = it->as<std::string>();
        if (ns_key == ns_string) 
        {
            std::cout << "Traversing namespace: " << ns_string << std::endl;
            return clang::RecursiveASTVisitor<Visitor>::TraverseNamespaceDecl(ns);
        }
    }
    return true;
}

bool chimera::Visitor::VisitFunctionDecl(FunctionDecl *func)
{
    std::string funcName = func->getNameInfo().getName().getAsString();
    //std::cout << "** Rewrote function def: " << funcName << std::endl;
    return true;
}

bool chimera::Visitor::VisitStmt(Stmt *st)
{
    if (ReturnStmt *ret = dyn_cast<ReturnStmt>(st))
    {
        //std::cout << "** Rewrote ReturnStmt" << std::endl;
    }

    if (CallExpr *call = dyn_cast<CallExpr>(st))
    {
        //std::cout << "** Rewrote function call" << std::endl;
    }
    return true;
}

bool chimera::Visitor::VisitReturnStmt(ReturnStmt *ret) 
{
    //std::cout << "** Rewrote ReturnStmt" << std::endl;
    return true;
}

bool chimera::Visitor::VisitCallExpr(CallExpr *call) {
    //std::cout << "** Rewrote function call" << std::endl;
    return true;
}

