/*
 * Copyright (c) 2022, Andrew Kaster <akaster@serenityos.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include "ClassDeclPrinter.h"
#include <clang/AST/Decl.h>
#include <clang/AST/DeclCXX.h>
#include <clang/AST/PrettyPrinter.h>
#include <clang/AST/Type.h>
#include <clang/ASTMatchers/ASTMatchers.h>
#include <clang/Basic/Specifiers.h>
#include <llvm/Support/Casting.h>
#include <llvm/Support/Error.h>
#include <llvm/Support/ErrorHandling.h>
#include <vector>
#include <filesystem>

namespace jakt_bindgen {

using namespace clang::ast_matchers;

ClassDeclPrinter::ClassDeclPrinter(std::string Namespace, std::string Header)
    : Namespace(std::move(Namespace))
    , Header(std::move(Header))
{
}

ClassDeclPrinter::~ClassDeclPrinter()
{
    llvm::outs() << "} // namespace\n";
    llvm::outs() << "} // import\n";

    for (auto const* klass : Imports) {
        llvm::outs() << "import " <<  llvm::cast<clang::NamespaceDecl>(klass->getEnclosingNamespaceContext())->getQualifiedNameAsString();
        llvm::outs() << " { " << klass->getNameAsString() << " }\n";
    }
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
        printNamespace(llvm::cast<clang::NamespaceDecl>(RD->getEnclosingNamespaceContext()));
        if (RD->isClass())
            printClass(llvm::cast<clang::CXXRecordDecl>(RD->getDefinition()));
    }
}

void ClassDeclPrinter::printNamespace(clang::NamespaceDecl const* NS)
{
    llvm::outs() << "import extern \"" << (std::filesystem::path(Namespace) / Header).string() << "\" {\n";
    llvm::outs() << "namespace " << NS->getQualifiedNameAsString() << " {\n";
}

void ClassDeclPrinter::printClass(clang::CXXRecordDecl const* class_definition)
{
    // class <name> : <base(s)>
    printClassDeclaration(class_definition);
    llvm::outs() << " {\n";
    printClassMethods(class_definition);
    llvm::outs() << "}\n";
}

void ClassDeclPrinter::printClassDeclaration(clang::CXXRecordDecl const* class_definition)
{
    llvm::outs() << "class " << class_definition->getName() << " ";
    bool first_base = true;
    for (auto const& base : class_definition->bases()) {
        if (base.isVirtual())
            llvm::report_fatal_error("ERROR: Virtual base class!\n", false);
        if (!(base.getAccessSpecifier() == clang::AccessSpecifier::AS_public))
            llvm::report_fatal_error("ERROR: Don't know how to handle non-public bases\n", false);

        if (first_base) {
            llvm::outs() << ": ";
            first_base = false;
        }
        else
            llvm::outs() << ", ";

        clang::RecordType const* Ty = base.getType()->getAs<clang::RecordType>();
        clang::CXXRecordDecl const* base_record = llvm::cast_or_null<clang::CXXRecordDecl>(Ty->getDecl()->getDefinition());
        if (!base_record)
            llvm::report_fatal_error("ERROR: Base class unusable", false);
        llvm::outs() << base_record->getNameAsString();

        // FIXME: Only do this if base_record and class_defintion are not from the same header
        Imports.push_back(base_record);
    }
}

void ClassDeclPrinter::printClassMethods(clang::CXXRecordDecl const* class_definition)
{
    for (clang::CXXMethodDecl const* method : class_definition->methods()) {
        if (method->isInstance()) {
            if (method->getAccess() == clang::AccessSpecifier::AS_private)
                continue;
            std::string access = (method->getAccess() == clang::AS_public) ? std::string("public") : (method->getAccess() == clang::AS_protected) ? std::string("protected") : "unknown??";
            llvm::outs() << "\t" << access << " Instance method: " << method->getNameAsString() << "\n";
        } else if (method->isStatic()) {
            llvm::outs() << "\tStatic method: " << method->getNameAsString() << "\n";
        }
        else {
            llvm::outs() << "vas ist das? " << method->getNameAsString() << "\n";
        }
    }
}

}
