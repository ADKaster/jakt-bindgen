/*
 * Copyright (c) 2022, Andrew Kaster <akaster@serenityos.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include "CXXClassListener.h"
#include <clang/ASTMatchers/ASTMatchFinder.h>
#include <clang/Tooling/Tooling.h>
#include <llvm/Support/raw_ostream.h>
#include <filesystem>

namespace jakt_bindgen {

class SourceFileHandler : public clang::tooling::SourceFileCallbacks
{
public:
    SourceFileHandler(std::string Namespace, std::filesystem::path out_dir);

    virtual bool handleBeginSource(clang::CompilerInstance&) override;
    virtual void handleEndSource() override;

    clang::ast_matchers::MatchFinder& finder() { return Finder; }

private:
    std::filesystem::path current_filepath;
    std::filesystem::path out_dir;

    clang::ast_matchers::MatchFinder Finder;
    CXXClassListener listener;
};

}
