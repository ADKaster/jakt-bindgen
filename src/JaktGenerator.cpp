/*
 * Copyright (c) 2022, Andrew Kaster <akaster@serenityos.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#define DEBUG_TYPE "jakt-gen"

#include "JaktGenerator.h"
#include "CXXClassListener.h"

#include <clang/AST/PrettyPrinter.h>
#include <clang/AST/Type.h>
#include <clang/Basic/LangOptions.h>
#include <clang/AST/CXXInheritance.h>
#include <clang/AST/Decl.h>
#include <clang/AST/DeclCXX.h>
#include <clang/AST/DeclTemplate.h>
#include <clang/Basic/Specifiers.h>
#include <llvm/Support/Debug.h>

namespace jakt_bindgen {

JaktGenerator::JaktGenerator(llvm::raw_ostream& Out, CXXClassListener const& class_information)
    : Out(Out)
    , class_information(class_information)
    , printing_policy(clang::LangOptions{})
{
    // FIXME: Get the language options from higher up in the stack. The SourceFileHandler can probably get one from the clang::CompilerInstance
    printing_policy.adjustForCPlusPlus();
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
        Out << " { " << klass->getName() << " }\n";
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

    // extern struct | class <name> : <base(s)>
    printClassDeclaration(class_definition);
    Out << " {\n";
    printClassMethods(class_definition);
    Out << "}\n";

    printNamespaceEnd();
}

static bool hasBaseNamed(clang::CXXRecordDecl const* class_definition, std::string_view base_name)
{
    clang::CXXBasePaths Paths;
    Paths.setOrigin(class_definition);

    return class_definition->lookupInBases([&base_name](clang::CXXBaseSpecifier const* base, clang::CXXBasePath&) -> bool {
        auto name = llvm::cast<clang::CXXRecordDecl>(base->getType()->getAs<clang::RecordType>()->getDecl()->getDefinition())->getQualifiedNameAsString();
        return name == base_name;
    }, Paths, true);
}

static bool isErrorOr(clang::QualType const&)
{
    // FIXME: How do we check if a type is a specialization of a specific named template?
    return false;
}

void JaktGenerator::printClassDeclaration(clang::CXXRecordDecl const* class_definition)
{
    bool is_class = hasBaseNamed(class_definition, "AK::RefCountedBase");

    Out << "extern " << (is_class ? "class " : "struct ") << class_definition->getName() << " ";

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
        Out << base_record->getName();
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

        if (clang::FunctionTemplateDecl const* TD = method->getDescribedFunctionTemplate()) {
            printClassTemplateMethod(method, TD);
            continue;
        }

        Out << "function " << method->getName() << "(";

        if (!is_static) {
            Out << "this";
            if (method->getNumParams() > 0)
                Out << ", ";
        }

        for (auto i = 0U; i < method->getNumParams(); ++i) {
            clang::ParmVarDecl const* param = method->parameters()[i];
            printParameter(param, i, i + 1 == method->getNumParams());
        }

        Out << ") ";
        if (isErrorOr(method->getReturnType()))
            Out << "throws ";
        Out << "-> ";
        printQualType(method->getReturnType(), true);
        Out << "\n";
    }

    // FIXME: When variadic generics are added to jakt, don't hardcode these special cases.
    // Derived from Core::Object? Add [[name="try_create"]] <name> create() throws overload for each constructor
    if (hasBaseNamed(class_definition, "Core::Object")) {
        assert(hasBaseNamed(class_definition, "AK::RefCountedBase"));
        for (clang::CXXConstructorDecl const* ctor : class_definition->ctors())
        {
            Out << "    [[name=\"try_create\"]] function create(";
            if (!ctor->isDefaultConstructor() && !ctor->isCopyOrMoveConstructor() && !ctor->isDeleted()) {
                for (auto i = 0U; i < ctor->getNumParams(); ++i) {
                    clang::ParmVarDecl const* param = ctor->parameters()[i];
                    printParameter(param, i, i + 1 == ctor->getNumParams());
                }
            }
            Out << ") throws -> " << class_definition->getName() << "\n";
        }
    }
}

void JaktGenerator::printClassTemplateMethod(clang::CXXMethodDecl const* method_declaration, clang::FunctionTemplateDecl const* template_method)
{
    Out << "// TODO: Template method " << method_declaration->getName() << "\n";
    // FIXME: Actually print this bad boy out
}

void JaktGenerator::printParameter(clang::ParmVarDecl const* parameter, unsigned int parameter_index, bool is_last_parameter)
{
    auto param_name = parameter->getName();
    if (!param_name.empty()) {
        Out << param_name << ": ";
    }
    else {
        // Use type name as parameter name
        Out << "anon _param_" << std::to_string(parameter_index) << ": ";
    }
    printQualType(parameter->getType(), false);
    if (!is_last_parameter) {
        Out << ", ";
    }
}

void JaktGenerator::printQualType(clang::QualType const& type, bool is_return_type)
{
    // FIXME: Create a pretty printer for QualType that knows how to turn e.g. int --> c_int, * --> raw, etc.
    // FIXME: Do extra conversions on return types (e.g. ErrorOr<T> --> T and function is now `throws`)
    Out << type.getAsString(printing_policy);
}

}
