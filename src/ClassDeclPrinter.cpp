/*
 * Copyright (c) 2022, Andrew Kaster <akaster@serenityos.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include "ClassDeclPrinter.h"
#include <clang/AST/Decl.h>
#include <clang/AST/DeclCXX.h>
#include <clang/AST/PrettyPrinter.h>
#include <clang/ASTMatchers/ASTMatchers.h>
#include <clang/ASTMatchers/ASTMatchersInternal.h>
#include <clang/Basic/Specifiers.h>
#include <llvm/Support/ErrorHandling.h>
#include <sstream>

namespace jakt_bindgen {

using namespace clang::ast_matchers;

static void printNamespace(std::string_view header_name, clang::NamespaceDecl const* NS)
{
    llvm::outs() << "import extern \"" << header_name  << "\" {\n";
    llvm::outs() << "namespace " << NS->getQualifiedNameAsString() << " {\n";
}

static void printClass(clang::CXXRecordDecl const* class_definition)
{
    llvm::outs() << "class " << class_definition->getName() << " ";
    bool first_base = true;
    for (auto const& base : class_definition->bases()) {
        if (base.isVirtual())
            llvm::report_fatal_error("ERROR: Virtual base class!\n", false);
        if (!(base.getAccessSpecifier() == clang::AccessSpecifier::AS_public))
            llvm::report_fatal_error("ERROR: Don't know how to handle non-public bases\n", false);

        if (first_base)
            llvm::outs() << ": ";
        else
            llvm::outs() << ", ";

        // FIXME: Find this type in the AST, and import it at the top of the file
        llvm::outs() << base.getTypeSourceInfo()->getType().getAsString();
    }
    llvm::outs() << " {\n";
    llvm::outs() << "}\n";
}

ClassDeclPrinter::ClassDeclPrinter(std::string Namespace, std::string Header)
    : Namespace(std::move(Namespace))
    , Header(std::move(Header))
{
}

ClassDeclPrinter::~ClassDeclPrinter()
{
    llvm::outs() << "} // namespace\n";
    llvm::outs() << "} // import\n";
}

void ClassDeclPrinter::registerMatchers(clang::ast_matchers::MatchFinder *Finder)
{
    Finder->addMatcher(traverse(clang::TK_IgnoreUnlessSpelledInSource,
        recordDecl(decl().bind("record"),
            hasParent(namespaceDecl(hasName(Namespace))),
            isExpansionInFileMatching(Header)))
        .bind("names"), this);
}

void ClassDeclPrinter::run(MatchFinder::MatchResult const& Result) {
    if (clang::RecordDecl const* RD = Result.Nodes.getNodeAs<clang::RecordDecl>("names")) {
        std::stringstream ss;
        ss << Namespace << "/" << Header;
        printNamespace(ss.view(), llvm::cast<clang::NamespaceDecl>(RD->getEnclosingNamespaceContext()));
        if (RD->isClass())
            printClass(llvm::cast<clang::CXXRecordDecl>(RD->getDefinition()));
    }
}

}
