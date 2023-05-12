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

#include <filesystem>

// Apply a custom category to all command-line options so that they are the
// only ones displayed.
static llvm::cl::OptionCategory s_tool_category("jakt-bindgen options");

// CommonOptionsParser declares HelpMessage with a description of the common
// command-line options related to the compilation database and input files.
// It's nice to have this help message in all tools.
static llvm::cl::extrahelp s_common_help(clang::tooling::CommonOptionsParser::HelpMessage);

// A help message for this specific tool can be added afterwards.
static llvm::cl::extrahelp s_more_help("\nMore help text...\n");

static llvm::cl::list<std::string> s_target_namespaces("n", llvm::cl::desc("Specify namespaces to import names from"),
    llvm::cl::value_desc("namespace"),
    llvm::cl::OneOrMore);

static llvm::cl::list<std::string> s_base_paths("b", llvm::cl::desc("Specify base paths to use to determine import paths"),
    llvm::cl::value_desc("base"),
    llvm::cl::OneOrMore);

int main(int argc, char const** argv)
{
    auto destination_path = std::filesystem::current_path();

    auto expected_parser = clang::tooling::CommonOptionsParser::create(argc, argv, s_tool_category);
    if (!expected_parser) {
        // Fail gracefully for unsupported options.
        llvm::errs() << expected_parser.takeError();
        return 1;
    }
    auto& options_parser = expected_parser.get();

    std::vector<std::string> extra_source_paths;
    std::vector<std::filesystem::path> base_paths;
    for (auto& base_path : s_base_paths)
        base_paths.push_back(std::filesystem::canonical(base_path));
    jakt_bindgen::SourceFileHandler handler(s_target_namespaces, destination_path, std::move(base_paths), extra_source_paths);

    auto action = clang::tooling::newFrontendActionFactory(&handler.finder(), &handler);

    int ret = 0;
    do {
        auto const source_paths = extra_source_paths.empty() ? options_parser.getSourcePathList() : extra_source_paths;
        extra_source_paths.clear();

        clang::tooling::ClangTool tool(options_parser.getCompilations(), source_paths);
        if (auto rv = tool.run(action.get()); rv != 0)
            ret = rv;
    } while (!extra_source_paths.empty());

    return ret;
}
