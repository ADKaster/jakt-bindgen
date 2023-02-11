/*
 * Copyright (c) 2022, Andrew Kaster <akaster@serenityos.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include "CXXClassListener.h"
#include <algorithm>
#include <clang/AST/Decl.h>
#include <clang/AST/DeclCXX.h>
#include <clang/AST/PrettyPrinter.h>
#include <clang/AST/Type.h>
#include <clang/ASTMatchers/ASTMatchFinder.h>
#include <clang/ASTMatchers/ASTMatchers.h>
#include <clang/Basic/Specifiers.h>
#include <filesystem>
#include <llvm/Support/Casting.h>
#include <llvm/Support/Error.h>
#include <llvm/Support/ErrorHandling.h>
#include <vector>

namespace jakt_bindgen {

using namespace clang::ast_matchers;

CXXClassListener::CXXClassListener(std::string namespace_, clang::ast_matchers::MatchFinder& finder)
    : m_namespace(std::move(namespace_))
    , m_finder(finder)
{
    registerMatches();
}

CXXClassListener::~CXXClassListener()
{
}

void CXXClassListener::registerMatches()
{
    m_finder.addMatcher(traverse(clang::TK_IgnoreUnlessSpelledInSource,
                            recordDecl(decl().bind("toplevel-name"),
                                hasParent(namespaceDecl(hasName(m_namespace))),
                                isExpansionInMainFile(),
                                forEachDescendant(cxxMethodDecl(unless(isPrivate())).bind("toplevel-method")))),
        this);

    // Note: Matches *namespace scope* enums.
    //       Nested class enums are handled separately.
    m_finder.addMatcher(traverse(clang::TK_IgnoreUnlessSpelledInSource,
                            enumDecl(decl().bind("toplevel-enum"),
                                hasParent(namespaceDecl(hasName(m_namespace))),
                                isExpansionInMainFile())),
        this);
}

void CXXClassListener::run(MatchFinder::MatchResult const& Result)
{
    if (clang::RecordDecl const* RD = Result.Nodes.getNodeAs<clang::RecordDecl>("toplevel-name")) {
        if (RD->isClass())
            visitClass(llvm::cast<clang::CXXRecordDecl>(RD->getDefinition()), Result.SourceManager);
    }
    if (clang::CXXMethodDecl const* MD = Result.Nodes.getNodeAs<clang::CXXMethodDecl>("toplevel-method")) {
        visitClassMethod(MD);
    }
    if (clang::EnumDecl const* ED = Result.Nodes.getNodeAs<clang::EnumDecl>("toplevel-enum")) {
        visitEnumeration(ED);
    }
}

void CXXClassListener::resetForNextFile()
{
    m_tag_decls.clear();
    m_imports.clear();
}

void CXXClassListener::visitClass(clang::CXXRecordDecl const* class_definition, clang::SourceManager const* source_manager)
{
    if (std::find(m_tag_decls.begin(), m_tag_decls.end(), class_definition) != m_tag_decls.end())
        return;

    m_tag_decls.push_back(class_definition);

    // Visit bases and add to import list
    for (clang::CXXBaseSpecifier const& base : class_definition->bases()) {
        if (base.isVirtual())
            llvm::report_fatal_error("ERROR: Virtual base class!\n", false);
        if (!(base.getAccessSpecifier() == clang::AccessSpecifier::AS_public))
            llvm::report_fatal_error("ERROR: Don't know how to handle non-public bases\n", false);

        clang::RecordType const* Ty = base.getType()->getAs<clang::RecordType>();
        clang::CXXRecordDecl const* base_record = llvm::cast_or_null<clang::CXXRecordDecl>(Ty->getDecl()->getDefinition());
        if (!base_record)
            llvm::report_fatal_error("ERROR: Base class unusable", false);

        if (source_manager->isInMainFile(source_manager->getExpansionLoc(base_record->getBeginLoc()))) {
            continue;
        }

        m_imports.push_back(base_record);
    }
}

void CXXClassListener::visitClassMethod(clang::CXXMethodDecl const* method_declaration)
{
    if (method_declaration->isInstance()) {
        if (llvm::isa<clang::CXXDestructorDecl>(method_declaration)
            || llvm::isa<clang::CXXConversionDecl>(method_declaration)
            || method_declaration->isOverloadedOperator()) {
            return;
        }
        if (clang::CXXConstructorDecl const* ctor = llvm::dyn_cast<clang::CXXConstructorDecl>(method_declaration)) {
            // Allow public default constructors to pass through, along with ctors with parameters
            if (ctor->isCopyOrMoveConstructor() || ctor->isDeleted())
                return;
        }
        // TODO: Walk instance method parameters and return type to find new types to add to imports
        m_methods[method_declaration->getParent()].push_back(method_declaration);
    } else if (method_declaration->isStatic()) {
        // TODO: Walk static method parameters and return type to find new types to add to imports
        m_methods[method_declaration->getParent()].push_back(method_declaration);
    }
}

void CXXClassListener::visitEnumeration(clang::EnumDecl const* enum_declaration)
{
    if (std::find(m_tag_decls.begin(), m_tag_decls.end(), enum_declaration) != m_tag_decls.end())
        return;

    m_tag_decls.push_back(enum_declaration);
}

}
