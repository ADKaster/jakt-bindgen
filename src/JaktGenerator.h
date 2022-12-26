/*
 * Copyright (c) 2022, Andrew Kaster <akaster@serenityos.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <clang/Frontend/CompilerInstance.h>
#include <clang/Tooling/Tooling.h>
#include <llvm/Support/raw_ostream.h>

namespace jakt_bindgen {

class CXXClassListener;

class JaktGenerator : public clang::tooling::SourceFileCallbacks
{
public:
    JaktGenerator(llvm::raw_ostream& Out, CXXClassListener const& class_information);

    void generate();

private:

    void printImportStatements();

    void printImportExternBegin(std::string const& header);
    void printImportExternEnd();

    void printNamespaceBegin(clang::NamespaceDecl const* NS);
    void printNamespaceEnd();

    void printClass(clang::CXXRecordDecl const* class_definition);
    void printClassDeclaration(clang::CXXRecordDecl const* class_definition);
    void printClassMethods(clang::CXXRecordDecl const* class_definition);

    llvm::raw_ostream& Out;
    CXXClassListener const& class_information;
};

}
