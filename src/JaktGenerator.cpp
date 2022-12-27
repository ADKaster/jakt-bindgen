/*
 * Copyright (c) 2022, Andrew Kaster <akaster@serenityos.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#define DEBUG_TYPE "jakt-gen"

#include "JaktGenerator.h"
#include "CXXClassListener.h"
#include <clang/AST/Decl.h>
#include <clang/AST/DeclCXX.h>
#include <clang/Basic/Specifiers.h>
#include <llvm/Support/Debug.h>

namespace jakt_bindgen {

JaktGenerator::JaktGenerator(llvm::raw_ostream& Out, CXXClassListener const& class_information)
    : Out(Out)
    , class_information(class_information)
{
}

void JaktGenerator::generate(std::string const& header_path)
{
    printImportStatements();

    printImportExternBegin(header_path);

    for (clang::CXXRecordDecl const* klass : class_information.records())
        printClass(klass);

    printImportExternEnd();
}

void JaktGenerator::printImportStatements()
{
    for (auto const* klass : class_information.imports()) {
        Out << "import " <<  llvm::cast<clang::NamespaceDecl>(klass->getEnclosingNamespaceContext())->getQualifiedNameAsString();
        Out << " { " << klass->getNameAsString() << " }\n";
    }
}

void JaktGenerator::printImportExternBegin(std::string const& header)
{
    Out << "import extern \"" << header << "\" {\n";
}

void JaktGenerator::printImportExternEnd()
{
    Out << "} // import\n";
}

void JaktGenerator::printNamespaceBegin(clang::NamespaceDecl const* NS)
{
    Out << "namespace " << NS->getQualifiedNameAsString() << " {\n";
}

void JaktGenerator::printNamespaceEnd()
{
    Out << "} // namespace\n";
}

void JaktGenerator::printClass(clang::CXXRecordDecl const* class_definition)
{

    printNamespaceBegin(llvm::cast<clang::NamespaceDecl>(class_definition->getEnclosingNamespaceContext()));

    // class <name> : <base(s)>
    printClassDeclaration(class_definition);
    Out << " {\n";
    printClassMethods(class_definition);
    Out << "}\n";

    printNamespaceEnd();
}

void JaktGenerator::printClassDeclaration(clang::CXXRecordDecl const* class_definition)
{
    Out << "class " << class_definition->getName() << " ";

    bool first_base = true;
    for (auto const& base : class_definition->bases()) {
        if (base.isVirtual())
            llvm::report_fatal_error("ERROR: Virtual base class!\n", false);
        if (!(base.getAccessSpecifier() == clang::AccessSpecifier::AS_public))
            llvm::report_fatal_error("ERROR: Don't know how to handle non-public bases\n", false);

        if (first_base) {
            Out << ": ";
            first_base = false;
        } else {
            Out << ", ";
        }

        clang::RecordType const* Ty = base.getType()->getAs<clang::RecordType>();
        clang::CXXRecordDecl const* base_record = llvm::cast_or_null<clang::CXXRecordDecl>(Ty->getDecl()->getDefinition());
        if (!base_record)
            llvm::report_fatal_error("ERROR: Base class unusable", false);
        Out << base_record->getNameAsString();
    }
}

void JaktGenerator::printClassMethods(clang::CXXRecordDecl const* class_definition)
{
    for (clang::CXXMethodDecl const* method : class_information.methods_for(class_definition)) {
        bool const is_static = method->isStatic();
        bool const is_virtual = method->isVirtual();
        bool const is_protected = method->getAccess() == clang::AccessSpecifier::AS_protected;
        assert(!(is_static && is_virtual));

        Out << "    ";

        if (!is_static) {
            if (is_protected)
                Out << "protected ";
            else
                Out << "public ";

            if (is_virtual)
                Out << "virtual ";
        }

        Out << "function " << method->getName() << "(";

        if (!is_static)
            Out << "this, ";

        // FIXME: Add parameters

        // FIXME: Create a pretty printer for QualType that knows how to turn e.g. int --> c_int, * --> raw, etc.
        Out << ") -> " << method->getReturnType().getAsString() << "\n";
    }
}

}
