/*
 * Copyright (c) 2022, Andrew Kaster <akaster@serenityos.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include "ClassDeclPrinter.h"

#include <clang/ASTMatchers/ASTMatchFinder.h>
#include <clang/ASTMatchers/ASTMatchers.h>
#include <clang/Tooling/CommonOptionsParser.h>
#include <clang/Tooling/Tooling.h>
#include <llvm/Support/CommandLine.h>

// Apply a custom category to all command-line options so that they are the
// only ones displayed.
static llvm::cl::OptionCategory MyToolCategory("jakt-bindgen options");

// CommonOptionsParser declares HelpMessage with a description of the common
// command-line options related to the compilation database and input files.
// It's nice to have this help message in all tools.
static llvm::cl::extrahelp CommonHelp(clang::tooling::CommonOptionsParser::HelpMessage);

// A help message for this specific tool can be added afterwards.
static llvm::cl::extrahelp MoreHelp("\nMore help text...\n");

int main(int argc, const char **argv) {
  auto ExpectedParser = clang::tooling::CommonOptionsParser::create(argc, argv, MyToolCategory);
  if (!ExpectedParser) {
    // Fail gracefully for unsupported options.
    llvm::errs() << ExpectedParser.takeError();
    return 1;
  }
  auto& OptionsParser = ExpectedParser.get();
  clang::tooling::ClangTool Tool(OptionsParser.getCompilations(),
                 OptionsParser.getSourcePathList());

  jakt_bindgen::ClassDeclPrinter Printer("GUI", "Widget.h");
  clang::ast_matchers::MatchFinder Finder;
  Finder.addMatcher(Printer.GetMatcher(), &Printer);
            
  return Tool.run(clang::tooling::newFrontendActionFactory(&Finder).get());
}
