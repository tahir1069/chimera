#ifndef __CHIMERA_CONSUMER_H__
#define __CHIMERA_CONSUMER_H__

#include <clang/AST/ASTConsumer.h>
#include <clang/Frontend/CompilerInstance.h>

namespace chimera
{

class Consumer : public clang::ASTConsumer
{
public:
    // Overrides the constructor in order to receive CompilerInstance.
    Consumer(clang::CompilerInstance *ci);

    // Overrides method to call our ChimeraVisitor on the entire source file.
    void HandleTranslationUnit(clang::ASTContext &context) override;

private:
    clang::CompilerInstance *ci_;
};

} // namespace chimera

#endif // __CHIMERA_CONSUMER_H__
