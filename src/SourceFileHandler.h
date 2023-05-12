/*
 * Copyright (c) 2022, Andrew Kaster <akaster@serenityos.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include "CXXClassListener.h"
#include <clang/ASTMatchers/ASTMatchFinder.h>
#include <clang/Tooling/Tooling.h>
#include <filesystem>
#include <llvm/Support/raw_ostream.h>

namespace jakt_bindgen {

class SourceFileHandler : public clang::tooling::SourceFileCallbacks {
public:
    SourceFileHandler(std::vector<std::string> namespaces, std::filesystem::path out_dir, std::vector<std::filesystem::path> base_dirs, std::vector<std::string>& source_paths);

    virtual bool handleBeginSource(clang::CompilerInstance&) override;
    virtual void handleEndSource() override;

    clang::ast_matchers::MatchFinder& finder() { return m_finder; }

private:
    std::filesystem::path m_current_filepath;
    std::filesystem::path m_out_dir;
    std::vector<std::filesystem::path> m_base_dirs;
    std::set<std::filesystem::path> m_seen_files;

    clang::ast_matchers::MatchFinder m_finder;
    CXXClassListener m_listener;
    clang::CompilerInstance* m_ci { nullptr };
};

}
