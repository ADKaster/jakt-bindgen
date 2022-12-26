/*
 * Copyright (c) 2022, Andrew Kaster <akaster@serenityos.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include "JaktGenerator.h"
#include <clang/AST/Decl.h>
#include <clang/AST/DeclCXX.h>
#include <clang/Frontend/CompilerInstance.h>

namespace jakt_bindgen {

JaktGenerator::JaktGenerator(llvm::raw_ostream& Out, CXXClassListener& class_information)
    : Out(Out)
    , class_information(class_information)
{
}

bool JaktGenerator::handleBeginSource(clang::CompilerInstance& CI)
{
    if (!clang::tooling::SourceFileCallbacks::handleBeginSource(CI))
        return false;

    auto main_file_id = CI.getSourceManager().getMainFileID();
    auto maybe_path = CI.getSourceManager().getNonBuiltinFilenameForID(main_file_id);
    if (!maybe_path.has_value())
        return false;
    
    std::filesystem::path main_file_path(maybe_path.value().str());
    class_information.resetForNextFile(main_file_path.filename());

    return true;
}

void JaktGenerator::handleEndSource()
{
    generate();
    clang::tooling::SourceFileCallbacks::handleEndSource();
}

void JaktGenerator::generate()
{
    printImportStatements();

    printImportExternBegin(class_information.importPath());

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
    for (clang::CXXMethodDecl const* method : class_definition->methods()) {
        if (method->isInstance()) {
            if (method->getAccess() == clang::AccessSpecifier::AS_private)
                continue;
            std::string access = (method->getAccess() == clang::AS_public) ? std::string("public") : (method->getAccess() == clang::AS_protected) ? std::string("protected") : "unknown??";
            llvm::outs() << "\t" << access << " Instance method: " << method->getNameAsString() << "\n";
        } else if (method->isStatic()) {
            llvm::outs() << "\tStatic method: " << method->getNameAsString() << "\n";
        }
        else {
            llvm::outs() << "vas ist das? " << method->getNameAsString() << "\n";
        }
    }
}


}
