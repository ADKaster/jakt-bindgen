/*
 * Copyright (c) 2022, Andrew Kaster <akaster@serenityos.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <clang/AST/Decl.h>
#include <clang/AST/DeclCXX.h>
#include <clang/AST/DeclTemplate.h>
#include <clang/AST/Type.h>
#include <clang/Frontend/CompilerInstance.h>
#include <clang/Tooling/Tooling.h>
#include <llvm/Support/raw_ostream.h>

namespace jakt_bindgen {

class CXXClassListener;

class JaktGenerator : public clang::tooling::SourceFileCallbacks {
public:
    JaktGenerator(llvm::raw_ostream& out, CXXClassListener const& class_information);

    void generate(std::string const& header_path);

private:
    void printImportStatements();

    void printImportExternBegin(std::string const& header);
    void printImportExternEnd();

    void printNamespaceBegin(clang::NamespaceDecl const* ns);
    void printNamespaceEnd();

    void printClass(clang::CXXRecordDecl const* class_definition);
    void printClassDeclaration(clang::CXXRecordDecl const* class_definition);
    void printClassMethods(clang::CXXRecordDecl const* class_definition);
    void printClassTemplateMethod(clang::CXXMethodDecl const* method_declaration, clang::FunctionTemplateDecl const* template_method);

    void printParameter(clang::ParmVarDecl const* parameter, unsigned int parameter_index, bool is_last_parameter);
    void printQualType(clang::QualType const& type, bool is_return_type);

    llvm::raw_ostream& m_out;
    CXXClassListener const& m_class_information;
    clang::PrintingPolicy m_printing_policy;
};

}
