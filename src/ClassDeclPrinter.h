/*
 * Copyright (c) 2022, Andrew Kaster <akaster@serenityos.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <clang/ASTMatchers/ASTMatchers.h>
#include <clang/ASTMatchers/ASTMatchFinder.h>
#include <llvm-15/llvm/ADT/StringRef.h>
#include <llvm/ADT/StringRef.h>
#include <memory>

namespace jakt_bindgen {

class ClassDeclPrinter : public clang::ast_matchers::MatchFinder::MatchCallback {
public:
    ClassDeclPrinter(std::string Namespace, std::string Header);

    clang::ast_matchers::DeclarationMatcher& GetMatcher() { return *Matcher; }

    virtual void run(clang::ast_matchers::MatchFinder::MatchResult const& Result) override;
private:

    std::unique_ptr<clang::ast_matchers::DeclarationMatcher> Matcher;
};

}
