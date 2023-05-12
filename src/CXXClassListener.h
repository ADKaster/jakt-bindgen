/*
 * Copyright (c) 2022, Andrew Kaster <akaster@serenityos.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <clang/AST/Decl.h>
#include <clang/AST/DeclCXX.h>
#include <clang/ASTMatchers/ASTMatchFinder.h>
#include <clang/ASTMatchers/ASTMatchers.h>
#include <clang/Basic/SourceManager.h>
#include <filesystem>
#include <memory>
#include <vector>

namespace jakt_bindgen {

class CXXClassListener : public clang::ast_matchers::MatchFinder::MatchCallback {
public:
    CXXClassListener(std::vector<std::string> namespaces, clang::ast_matchers::MatchFinder&, std::vector<std::string>& source_paths, std::set<std::filesystem::path>& seen_files);
    virtual ~CXXClassListener() override;

    virtual void run(clang::ast_matchers::MatchFinder::MatchResult const& result) override;

    struct Import {
        clang::TagDecl const* tag_decl;
        std::string source_file_path;
    };

    std::vector<clang::TagDecl const*> const& tag_decls() const { return m_tag_decls; }
    std::vector<Import> const& imports() const { return m_imports; }
    std::vector<clang::CXXMethodDecl const*> const& methods_for(clang::CXXRecordDecl const* r) const { return m_methods.at(r); }
    bool contains_methods_for(clang::CXXRecordDecl const* r) const { return m_methods.contains(r); }

    void resetForNextFile();

private:
    void visitClass(clang::CXXRecordDecl const* class_definition, clang::SourceManager const* source_manager);
    void visitClassMethod(clang::CXXMethodDecl const* method_declaration, clang::SourceManager const* source_manager);
    void visitEnumeration(clang::EnumDecl const* enum_declaration, clang::SourceManager const* source_manager);

    void registerMatches();

    std::vector<std::string> m_namespaces;

    std::vector<clang::TagDecl const*> m_tag_decls;
    std::vector<Import> m_imports;

    std::unordered_map<clang::CXXRecordDecl const*, std::vector<clang::CXXMethodDecl const*>> m_methods;

    clang::ast_matchers::MatchFinder& m_finder;
    std::vector<std::string>& m_source_paths;
    std::set<std::filesystem::path>& m_seen_files;
};

}
