/*
 * Copyright (c) 2022, Andrew Kaster <akaster@serenityos.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <clang/AST/DeclCXX.h>
#include <clang/ASTMatchers/ASTMatchFinder.h>
#include <clang/ASTMatchers/ASTMatchers.h>
#include <clang/Basic/SourceManager.h>
#include <memory>
#include <vector>

namespace jakt_bindgen {

class CXXClassListener : public clang::ast_matchers::MatchFinder::MatchCallback {
public:
    CXXClassListener(std::string namespace_, clang::ast_matchers::MatchFinder&);
    virtual ~CXXClassListener() override;

    virtual void run(clang::ast_matchers::MatchFinder::MatchResult const& result) override;

    std::vector<clang::CXXRecordDecl const*> const& records() const { return m_records; }
    std::vector<clang::CXXRecordDecl const*> const& imports() const { return m_imports; }
    std::vector<clang::CXXMethodDecl const*> const& methods_for(clang::CXXRecordDecl const* r) const { return m_methods.at(r); }

    void resetForNextFile();

private:
    void visitClass(clang::CXXRecordDecl const* class_definition, clang::SourceManager const* source_manager);
    void visitClassMethod(clang::CXXMethodDecl const* method_declaration);

    void registerMatches();

    std::string m_namespace;

    std::vector<clang::CXXRecordDecl const*> m_records;
    std::vector<clang::CXXRecordDecl const*> m_imports;

    std::unordered_map<clang::CXXRecordDecl const*, std::vector<clang::CXXMethodDecl const*>> m_methods;

    clang::ast_matchers::MatchFinder& m_finder;
};

}
