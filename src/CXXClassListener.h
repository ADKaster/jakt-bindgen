/*
 * Copyright (c) 2022, Andrew Kaster <akaster@serenityos.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <clang/AST/DeclCXX.h>
#include <clang/ASTMatchers/ASTMatchers.h>
#include <clang/ASTMatchers/ASTMatchFinder.h>
#include <clang/Basic/SourceManager.h>
#include <memory>
#include <vector>

namespace jakt_bindgen {

class CXXClassListener : public clang::ast_matchers::MatchFinder::MatchCallback {
public:
    CXXClassListener(std::string Namespace, clang::ast_matchers::MatchFinder&);
    virtual ~CXXClassListener() override;

    virtual void run(clang::ast_matchers::MatchFinder::MatchResult const& Result) override;

    std::vector<clang::CXXRecordDecl const*> const& records() const { return Records; }
    std::vector<clang::CXXRecordDecl const*> const& imports() const { return Imports; }
    std::vector<clang::CXXMethodDecl const*> const& methods_for(clang::CXXRecordDecl const* R) const { return Methods.at(R); }

    void resetForNextFile();

private:

    void visitClass(clang::CXXRecordDecl const* class_definition, clang::SourceManager const* source_manager);
    void visitClassMethod(clang::CXXMethodDecl const* method_declaration);

    void registerMatches();

    std::string Namespace;

    std::vector<clang::CXXRecordDecl const*> Records;
    std::vector<clang::CXXRecordDecl const*> Imports;

    std::unordered_map<clang::CXXRecordDecl const*, std::vector<clang::CXXMethodDecl const*>> Methods;

    clang::ast_matchers::MatchFinder& Finder;
};

}
