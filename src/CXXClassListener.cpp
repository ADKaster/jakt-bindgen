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
#include <algorithm>
#include <vector>
#include <filesystem>

namespace jakt_bindgen {

using namespace clang::ast_matchers;

CXXClassListener::CXXClassListener(std::string Namespace, clang::ast_matchers::MatchFinder& Finder)
    : Namespace(std::move(Namespace))
    , Finder(Finder)
{
    registerMatches();
}

CXXClassListener::~CXXClassListener()
{
}

void CXXClassListener::registerMatches()
{
    Finder.addMatcher(traverse(clang::TK_IgnoreUnlessSpelledInSource,
        recordDecl(decl().bind("toplevel-name"),
            hasParent(namespaceDecl(hasName(Namespace))),
            isExpansionInMainFile(),
            forEachDescendant(cxxMethodDecl(unless(isPrivate())).bind("toplevel-method")))
        ), this);
}

void CXXClassListener::run(MatchFinder::MatchResult const& Result) {
    if (clang::RecordDecl const* RD = Result.Nodes.getNodeAs<clang::RecordDecl>("toplevel-name")) {
        if (RD->isClass())
            visitClass(llvm::cast<clang::CXXRecordDecl>(RD->getDefinition()), Result.SourceManager);
    }
    if (clang::CXXMethodDecl const* MD = Result.Nodes.getNodeAs<clang::CXXMethodDecl>("toplevel-method")) {
        visitClassMethod(MD);
    }
}

void CXXClassListener::resetForNextFile()
{
    Records.clear();
    Imports.clear();
}

void CXXClassListener::visitClass(clang::CXXRecordDecl const* class_definition, clang::SourceManager const* source_manager)
{
    if (std::find(Records.begin(), Records.end(), class_definition) != Records.end())
        return;

    Records.push_back(class_definition);

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

        Imports.push_back(base_record);
    }
}

void CXXClassListener::visitClassMethod(clang::CXXMethodDecl const* method_declaration)
{
    if (method_declaration->isInstance()) {
        if (llvm::isa<clang::CXXConstructorDecl>(method_declaration)
            || llvm::isa<clang::CXXDestructorDecl>(method_declaration)
            || llvm::isa<clang::CXXConversionDecl>(method_declaration)) {
            return;
        }
        // TODO: Walk instance method parameters and return type to find new types to add to imports
        Methods[method_declaration->getParent()].push_back(method_declaration);
    } else if (method_declaration->isStatic()) {
        // TODO: Walk static method parameters and return type to find new types to add to imports
        Methods[method_declaration->getParent()].push_back(method_declaration);
    }
}

}
