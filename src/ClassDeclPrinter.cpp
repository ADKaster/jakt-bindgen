/*
 * Copyright (c) 2022, Andrew Kaster <akaster@serenityos.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include "ClassDeclPrinter.h"
#include <clang/AST/Decl.h>
#include <clang/AST/PrettyPrinter.h>
#include <clang/ASTMatchers/ASTMatchers.h>
#include <clang/ASTMatchers/ASTMatchersInternal.h>

namespace jakt_bindgen {

using namespace clang::ast_matchers;

ClassDeclPrinter::ClassDeclPrinter(std::string Namespace, std::string Header)
{
    Matcher = std::make_unique<DeclarationMatcher>(
        recordDecl(decl().bind("record"),
            hasParent(namespaceDecl(hasName(Namespace))),
            isExpansionInFileMatching(Header))
        .bind("names")
    );
}

void ClassDeclPrinter::run(MatchFinder::MatchResult const& Result) {
    if (clang::RecordDecl const* RD = Result.Nodes.getNodeAs<clang::RecordDecl>("names")) {
        auto str = RD->getUnderlyingDecl()->getNameAsString();
        auto ns = llvm::cast<clang::NamespaceDecl>(RD->getEnclosingNamespaceContext())->getQualifiedNameAsString();
        llvm::dbgs() << "Found RecordDecl " << ns << "::" << str << "\n";
    }
}

}
