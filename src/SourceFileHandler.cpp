/*
 * Copyright (c) 2022, Andrew Kaster <akaster@serenityos.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include "SourceFileHandler.h"
#include "JaktGenerator.h"
#include <algorithm>
#include <filesystem>
#include <llvm/Support/raw_ostream.h>
#include <system_error>

namespace jakt_bindgen {

SourceFileHandler::SourceFileHandler(std::vector<std::string> namespaces, std::filesystem::path out_dir, std::vector<std::filesystem::path> base_dirs, std::vector<std::string>& source_paths)
    : m_out_dir(std::move(out_dir))
    , m_base_dirs(std::move(base_dirs))
    , m_listener(std::move(namespaces), m_finder, source_paths, m_seen_files)
{
}

bool SourceFileHandler::handleBeginSource(clang::CompilerInstance& CI)
{
    if (!clang::tooling::SourceFileCallbacks::handleBeginSource(CI))
        return false;

    auto main_file_id = CI.getSourceManager().getMainFileID();
    auto maybe_path = CI.getSourceManager().getNonBuiltinFilenameForID(main_file_id);
    if (!maybe_path.has_value())
        return false;

    auto path = std::filesystem::canonical(maybe_path.value().str());
    auto specific_base_directory_it = std::find_if(m_base_dirs.begin(), m_base_dirs.end(), [&](auto const& item) -> bool {
        return std::mismatch(item.begin(), item.end(), path.begin(), path.end()).first == item.end();
    });
    if (specific_base_directory_it == m_base_dirs.end()) {
        llvm::errs() << "File " << path.string() << " is not in any of the specified base directories\n";
        return false;
    }

    m_current_filepath = path.lexically_relative(*specific_base_directory_it);
    llvm::outs() << "Processing " << m_current_filepath.string() << "\n";

    m_listener.resetForNextFile();

    m_ci = &CI;

    return true;
}

void SourceFileHandler::handleEndSource()
{
    std::string base_name = m_current_filepath.filename().replace_extension(".jakt");
    std::transform(base_name.begin(), base_name.end(), base_name.begin(),
        [](unsigned char c) { return std::tolower(c); });

    std::string new_filename = (m_out_dir / base_name).string();

    std::error_code os_errc = {};
    llvm::raw_fd_ostream os(new_filename, os_errc, llvm::sys::fs::CD_CreateAlways);
    if (os_errc) {
        llvm::errs() << "Can't open file " << new_filename << ": " << os_errc.message() << "\n";
        return;
    }

    if (m_listener.tag_decls().empty()) {
        llvm::errs() << "No classes found?\n";
        return;
    }

    JaktGenerator generator(os, m_listener);

    static_cast<clang::tooling::SourceFileCallbacks&>(generator).handleBeginSource(*m_ci);
    generator.generate(m_current_filepath.string());
    static_cast<clang::tooling::SourceFileCallbacks&>(generator).handleEndSource();
}

}
