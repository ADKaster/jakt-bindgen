/*
 * Copyright (c) 2022, Andrew Kaster <akaster@serenityos.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include "EnumBits.h"
#include <clang/AST/Decl.h>
#include <clang/AST/DeclCXX.h>
#include <clang/AST/DeclTemplate.h>
#include <clang/AST/Type.h>
#include <clang/Frontend/CompilerInstance.h>
#include <clang/Tooling/Tooling.h>
#include <llvm/Support/raw_ostream.h>
#include <optional>

namespace jakt_bindgen {

class CXXClassListener;

class JaktGenerator : public clang::tooling::SourceFileCallbacks {
public:
    JaktGenerator(llvm::raw_ostream& out, CXXClassListener const& class_information);

    void generate(std::string const& header_path);

    enum class QualTypePrintFlags {
        PF_Nothing = 0,
        PF_IsReturnType = 1,
        PF_InFunctionThatMayThrow = 2,
    };

private:
    void printImportStatements();

    void printImportExternBegin(std::string const& header);
    void printImportExternEnd();

    void printNamespaceBegin(clang::NamespaceDecl const* ns);
    void printNamespaceEnd();

    void printTagDecl(clang::TagDecl const* tag_declaration);

    void printClass(clang::CXXRecordDecl const* class_definition);
    void printClassDeclaration(clang::CXXRecordDecl const* class_definition);
    void printClassMethods(clang::CXXRecordDecl const* class_definition);
    void printClassTemplateMethod(clang::CXXMethodDecl const* method_declaration, clang::FunctionTemplateDecl const* template_method);

    void printEnumeration(clang::EnumDecl const* enum_definition);

    void printParameter(clang::ParmVarDecl const* parameter, unsigned int parameter_index, bool is_last_parameter);
    std::string rewriteParameter(llvm::StringRef name, unsigned index, clang::QualType const& type);

    std::string rewriteQualTypeToJaktType(clang::QualType const& type, QualTypePrintFlags flags);

    void printQualType(clang::QualType const& type, QualTypePrintFlags flags)
    {
        m_out << rewriteQualTypeToJaktType(type, flags);
    }

    virtual bool handleBeginSource(clang::CompilerInstance& CI) override
    {
        if (!CI.hasASTContext())
            CI.createASTContext();

        m_context = &CI.getASTContext();
        return true;
    }

    void printIndentation()
    {
        for (auto i = 0U; i < m_indentation_level; ++i)
            m_out << "    ";
    }

    class IndentationIncreaser
    {
    public:
        IndentationIncreaser(uint32_t& i) : m_i(i) { ++m_i; }
        ~IndentationIncreaser() { --m_i; }
    private:
        uint32_t& m_i;
    };

    bool isErrorOr(clang::QualType const&) const;
    std::optional<clang::QualType> getTemplateParameterIfMatches(clang::QualType const&, llvm::StringRef template_name, unsigned index = 0) const;

    llvm::raw_ostream& m_out;
    CXXClassListener const& m_class_information;
    clang::PrintingPolicy m_printing_policy;
    uint32_t m_indentation_level { 0 };
    clang::ASTContext const* m_context { nullptr };
};

ENUM_BITWISE_OPERATORS(JaktGenerator::QualTypePrintFlags)

}
