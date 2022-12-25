/*
 * Copyright (c) 2022, Andrew Kaster <akaster@serenityos.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <clang/AST/DeclCXX.h>
#include <clang/ASTMatchers/ASTMatchers.h>
#include <clang/ASTMatchers/ASTMatchFinder.h>
#include <memory>
#include <vector>

namespace jakt_bindgen {

class ClassDeclPrinter : public clang::ast_matchers::MatchFinder::MatchCallback {
public:
    ClassDeclPrinter(std::string Namespace, std::string Header);
    virtual ~ClassDeclPrinter() override;

    void registerMatchers(clang::ast_matchers::MatchFinder* Finder);
    virtual void run(clang::ast_matchers::MatchFinder::MatchResult const& Result) override;
private:
    std::string Namespace;
    std::string Header;

    void printClass(clang::CXXRecordDecl const* class_definition);
    void printNamespace(clang::NamespaceDecl const* NS);


    void printClassDeclaration(clang::CXXRecordDecl const* class_definition);
    void printClassMethods(clang::CXXRecordDecl const* class_definition);

    std::vector<clang::CXXRecordDecl const*> Imports;
};

}
