/*
 * Copyright (c) 2022, Andrew Kaster <akaster@serenityos.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include "CXXClassListener.h"
#include <clang/AST/Decl.h>
#include <clang/AST/DeclCXX.h>
#include <clang/AST/PrettyPrinter.h>
#include <clang/AST/Type.h>
#include <clang/ASTMatchers/ASTMatchFinder.h>
#include <clang/ASTMatchers/ASTMatchers.h>
#include <clang/Basic/Specifiers.h>
#include <llvm/Support/Casting.h>
#include <llvm/Support/Error.h>
#include <llvm/Support/ErrorHandling.h>
#include <vector>
#include <filesystem>

namespace jakt_bindgen {

using namespace clang::ast_matchers;

CXXClassListener::CXXClassListener(std::string Namespace, clang::ast_matchers::MatchFinder& Finder)
    : Namespace(std::move(Namespace))
    , Finder(Finder)
{
}

CXXClassListener::~CXXClassListener()
{
}

void CXXClassListener::registerMatches()
{
    Finder.addMatcher(traverse(clang::TK_IgnoreUnlessSpelledInSource,
        recordDecl(decl().bind("record"),
            hasParent(namespaceDecl(hasName(Namespace))),
            isExpansionInFileMatching(Header)))
        .bind("names"), this);
}

void CXXClassListener::run(MatchFinder::MatchResult const& Result) {
    if (clang::RecordDecl const* RD = Result.Nodes.getNodeAs<clang::RecordDecl>("names")) {
        if (RD->isClass())
            visitClass(llvm::cast<clang::CXXRecordDecl>(RD->getDefinition()));
    }
}

void CXXClassListener::resetForNextFile(std::string FileName)
{
    // Note: We assume all classes will be in Namespace
    Header = std::move(FileName);
    Records.clear();
    Imports.clear();

    registerMatches();
}

std::string CXXClassListener::importPath() const
{
    return (std::filesystem::path(Namespace) / Header).string();
}

void CXXClassListener::visitClass(clang::CXXRecordDecl const* class_definition)
{
    Records.push_back(class_definition);

    // Visit bases and add to import list
    for (auto const& base : class_definition->bases()) {
        if (base.isVirtual())
            llvm::report_fatal_error("ERROR: Virtual base class!\n", false);
        if (!(base.getAccessSpecifier() == clang::AccessSpecifier::AS_public))
            llvm::report_fatal_error("ERROR: Don't know how to handle non-public bases\n", false);

        clang::RecordType const* Ty = base.getType()->getAs<clang::RecordType>();
        clang::CXXRecordDecl const* base_record = llvm::cast_or_null<clang::CXXRecordDecl>(Ty->getDecl()->getDefinition());
        if (!base_record)
            llvm::report_fatal_error("ERROR: Base class unusable", false);

        // FIXME: Only do this if base_record and class_defintion are not from the same header
        Imports.push_back(base_record);
    }

    visitClassMethods(class_definition);
}

void CXXClassListener::visitClassMethods(clang::CXXRecordDecl const* class_definition)
{
    for (clang::CXXMethodDecl const* method : class_definition->methods()) {
        if (method->isInstance()) {
            if (method->getAccess() == clang::AccessSpecifier::AS_private)
                continue;
            // TODO: Walk instance methods and find new types to add to imports
        } else if (method->isStatic()) {
            // TODO: Walk static methods and find new types to add to imports
        }
    }
}

}
