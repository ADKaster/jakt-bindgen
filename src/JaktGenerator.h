/*
 * Copyright (c) 2022, Andrew Kaster <akaster@serenityos.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include "CXXClassListener.h"
#include <clang/Frontend/PrecompiledPreamble.h>
#include <clang/Tooling/Tooling.h>
#include <llvm/Support/raw_ostream.h>
#include <filesystem>

namespace jakt_bindgen {

class JaktGenerator : public clang::tooling::SourceFileCallbacks
{
public:
    JaktGenerator(llvm::raw_ostream& Out, CXXClassListener& class_information);

    // FIXME: put the source file callbacks in another class that owns the listener and the generator
    virtual bool handleBeginSource(clang::CompilerInstance&) override;
    virtual void handleEndSource() override;

private:
    void generate();

    void printImportStatements();

    void printImportExternBegin(std::string const& header);
    void printImportExternEnd();

    void printNamespaceBegin(clang::NamespaceDecl const* NS);
    void printNamespaceEnd();

    void printClass(clang::CXXRecordDecl const* class_definition);
    void printClassDeclaration(clang::CXXRecordDecl const* class_definition);
    void printClassMethods(clang::CXXRecordDecl const* class_definition);

    llvm::raw_ostream& Out;
    CXXClassListener& class_information;
};

}
