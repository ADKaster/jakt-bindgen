/*
 * Copyright (c) 2022, Andrew Kaster <akaster@serenityos.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include "SourceFileHandler.h"

#include <clang/ASTMatchers/ASTMatchFinder.h>
#include <clang/ASTMatchers/ASTMatchers.h>
#include <clang/Tooling/CommonOptionsParser.h>
#include <clang/Tooling/Tooling.h>
#include <llvm/Support/CommandLine.h>
#include <llvm/Support/CommandLine.h>

#include <filesystem>

// Apply a custom category to all command-line options so that they are the
// only ones displayed.
static llvm::cl::OptionCategory MyToolCategory("jakt-bindgen options");

// CommonOptionsParser declares HelpMessage with a description of the common
// command-line options related to the compilation database and input files.
// It's nice to have this help message in all tools.
static llvm::cl::extrahelp CommonHelp(clang::tooling::CommonOptionsParser::HelpMessage);

// A help message for this specific tool can be added afterwards.
static llvm::cl::extrahelp MoreHelp("\nMore help text...\n");

static llvm::cl::opt<std::string> TargetNamespace("n", llvm::cl::desc("Specify namespace to import names from"),
                                                  llvm::cl::value_desc("namespace"),
                                                  llvm::cl::Required);

static llvm::cl::opt<std::string> BasePath("b", llvm::cl::desc("Specify base path to use to determine import paths"),
                                                llvm::cl::value_desc("base"),
                                                llvm::cl::Required);

int main(int argc, const char **argv) {

  auto destination_path = std::filesystem::current_path();

  auto ExpectedParser = clang::tooling::CommonOptionsParser::create(argc, argv, MyToolCategory);
  if (!ExpectedParser) {
    // Fail gracefully for unsupported options.
    llvm::errs() << ExpectedParser.takeError();
    return 1;
  }
  auto& OptionsParser = ExpectedParser.get();
  clang::tooling::ClangTool Tool(OptionsParser.getCompilations(),
                 OptionsParser.getSourcePathList());

  jakt_bindgen::SourceFileHandler handler(TargetNamespace, destination_path, std::filesystem::canonical(BasePath.c_str()));

  auto action = clang::tooling::newFrontendActionFactory(&handler.finder(), &handler);

  return Tool.run(action.get());
}
