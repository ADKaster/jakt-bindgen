/*
 * Copyright (c) 2022, Andrew Kaster <akaster@serenityos.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#define DEBUG_TYPE "jakt-gen"
#include <llvm/Support/Debug.h>

#include "CXXClassListener.h"
#include "JaktGenerator.h"

#include <clang/AST/CXXInheritance.h>
#include <clang/AST/Decl.h>
#include <clang/AST/DeclCXX.h>
#include <clang/AST/DeclTemplate.h>
#include <clang/AST/PrettyPrinter.h>
#include <clang/AST/Type.h>
#include <clang/Basic/LangOptions.h>
#include <clang/Basic/Specifiers.h>

#include <string>
#include <string_view>

namespace jakt_bindgen {

JaktGenerator::JaktGenerator(llvm::raw_ostream& out, CXXClassListener const& class_information)
    : m_out(out)
    , m_class_information(class_information)
    , m_printing_policy(clang::LangOptions {})
{
    // FIXME: Get the language options from higher up in the stack. The SourceFileHandler can probably get one from the clang::CompilerInstance
    m_printing_policy.adjustForCPlusPlus();
}

void JaktGenerator::generate(std::string const& header_path)
{
    printImportStatements();

    printImportExternBegin(header_path);

    for (clang::CXXRecordDecl const* klass : m_class_information.records())
        printClass(klass);

    printImportExternEnd();
}

void JaktGenerator::printImportStatements()
{
    for (auto const* klass : m_class_information.imports()) {
        m_out << "import " << llvm::cast<clang::NamespaceDecl>(klass->getEnclosingNamespaceContext())->getQualifiedNameAsString();
        m_out << " { " << klass->getName() << " }\n";
    }
}

void JaktGenerator::printImportExternBegin(std::string const& header)
{
    m_out << "import extern \"" << header << "\" {\n";
}

void JaktGenerator::printImportExternEnd()
{
    m_out << "} // import\n";
}

void JaktGenerator::printNamespaceBegin(clang::NamespaceDecl const* ns)
{
    m_out << "namespace " << ns->getQualifiedNameAsString() << " {\n";
}

void JaktGenerator::printNamespaceEnd()
{
    m_out << "} // namespace\n";
}

void JaktGenerator::printClass(clang::CXXRecordDecl const* class_definition)
{

    printNamespaceBegin(llvm::cast<clang::NamespaceDecl>(class_definition->getEnclosingNamespaceContext()));

    // extern struct | class <name> : <base(s)>
    printClassDeclaration(class_definition);
    m_out << " {\n";
    printClassMethods(class_definition);
    m_out << "}\n";

    printNamespaceEnd();
}

static bool hasBaseNamed(clang::CXXRecordDecl const* class_definition, std::string_view base_name)
{
    clang::CXXBasePaths Paths;
    Paths.setOrigin(class_definition);

    return class_definition->lookupInBases([&base_name](clang::CXXBaseSpecifier const* base, clang::CXXBasePath&) -> bool {
        auto record_type = base->getType()->getAs<clang::RecordType>();
        if (!record_type)
            return false;

        auto name = llvm::cast<clang::CXXRecordDecl>(record_type->getDecl()->getDefinition())->getQualifiedNameAsString();
        return name == base_name;
    },
        Paths, true);
}

bool JaktGenerator::isErrorOr(clang::QualType const& type) const
{
    return getTemplateParameterIfMatches(type, "AK::ErrorOr").has_value();
}

std::optional<clang::QualType> JaktGenerator::getTemplateParameterIfMatches(clang::QualType const& type, llvm::StringRef template_name, unsigned int index) const
{
    assert(m_context != nullptr);
    auto desugared_type = type.getDesugaredType(*m_context);
    if (auto const* record_type = llvm::dyn_cast<clang::RecordType>(desugared_type)) {
        if (record_type->getAsRecordDecl()->getQualifiedNameAsString() == template_name) {
            if (auto const* t = llvm::dyn_cast<clang::ClassTemplateSpecializationDecl>(record_type->getAsRecordDecl()); t && t->getTemplateArgs().size() > index)
                return t->getTemplateArgs()[index].getAsType();
        }
    }

    return {};
}

void JaktGenerator::printClassDeclaration(clang::CXXRecordDecl const* class_definition)
{
    bool is_class = hasBaseNamed(class_definition, "AK::RefCountedBase");

    m_out << "extern " << (is_class ? "class " : "struct ") << class_definition->getName() << " ";

    bool first_base = true;
    for (auto const& base : class_definition->bases()) {
        if (base.isVirtual())
            llvm::report_fatal_error("ERROR: Don't know how to handle virtual bases", false);
        if (base.getAccessSpecifier() != clang::AccessSpecifier::AS_public)
            llvm::report_fatal_error("ERROR: Don't know how to handle non-public bases", false);

        if (first_base) {
            m_out << ": ";
            first_base = false;
        } else {
            m_out << ", ";
        }

        clang::RecordType const* Ty = base.getType()->getAs<clang::RecordType>();
        clang::CXXRecordDecl const* base_record = llvm::cast_or_null<clang::CXXRecordDecl>(Ty->getDecl()->getDefinition());
        if (!base_record)
            llvm::report_fatal_error("ERROR: Base class unusable", false);
        m_out << base_record->getName();
    }
}

void JaktGenerator::printClassMethods(clang::CXXRecordDecl const* class_definition)
{
    auto for_each_method = [&](auto&& f) {
        if (m_class_information.contains_methods_for(class_definition)) {
            for (auto const& method : m_class_information.methods_for(class_definition))
                f(method);
        } else {
            for (auto const& method : class_definition->methods())
                f(method);
        }
    };

    for_each_method([&](clang::CXXMethodDecl const* method) {
        if (method->isOverloadedOperator() || llvm::isa<clang::CXXDestructorDecl>(method))
            return;

        bool const is_constructor = llvm::isa<clang::CXXConstructorDecl>(method);
        bool const is_static = method->isStatic() || is_constructor;
        bool const is_virtual = method->isVirtual();
        bool const is_protected = method->getAccess() == clang::AccessSpecifier::AS_protected;
        assert(!(is_static && is_virtual));

        m_out << "    ";

        if (!is_static) {
            if (is_protected)
                m_out << "protected ";
            else
                m_out << "public ";

            if (is_virtual)
                m_out << "virtual ";
        }

        if (clang::FunctionTemplateDecl const* TD = method->getDescribedFunctionTemplate()) {
            printClassTemplateMethod(method, TD);
            return;
        }

        m_out << "function " << method->getDeclName() << "(";

        if (!is_static) {
            m_out << "this";
            if (method->getNumParams() > 0)
                m_out << ", ";
        }

        for (auto i = 0U; i < method->getNumParams(); ++i) {
            clang::ParmVarDecl const* param = method->parameters()[i];
            printParameter(param, i, i + 1 == method->getNumParams());
        }

        m_out << ") ";
        QualTypePrintFlags flags { QualTypePrintFlags::PF_IsReturnType };
        if (isErrorOr(method->getReturnType())) {
            m_out << "throws ";
            flags |= QualTypePrintFlags::PF_InFunctionThatMayThrow;
        }
        m_out << "-> ";
        if (is_constructor)
            m_out << class_definition->getDeclName();
        else
            printQualType(method->getReturnType(), flags);
        m_out << "\n";
    });

    // FIXME: When variadic generics are added to jakt, don't hardcode these special cases.
    // Derived from Core::Object? Add [[name="try_create"]] <name> create() throws overload for each constructor
    if (hasBaseNamed(class_definition, "Core::Object")) {
        assert(hasBaseNamed(class_definition, "AK::RefCountedBase"));
        for (clang::CXXConstructorDecl const* ctor : class_definition->ctors()) {
            m_out << "    [[name=\"try_create\"]] function create(";
            if (!ctor->isDefaultConstructor() && !ctor->isCopyOrMoveConstructor() && !ctor->isDeleted()) {
                for (auto i = 0U; i < ctor->getNumParams(); ++i) {
                    clang::ParmVarDecl const* param = ctor->parameters()[i];
                    printParameter(param, i, i + 1 == ctor->getNumParams());
                }
            }
            m_out << ") throws -> " << class_definition->getName() << "\n";
        }
    }
}

void JaktGenerator::printClassTemplateMethod(clang::CXXMethodDecl const* method_declaration, clang::FunctionTemplateDecl const*)
{
    m_out << "// TODO: Template method " << method_declaration->getName() << "\n";
    // FIXME: Actually print this bad boy out
}

void JaktGenerator::printParameter(clang::ParmVarDecl const* parameter, unsigned int parameter_index, bool is_last_parameter)
{
    m_out << rewriteParameter(parameter->getName(), parameter_index, parameter->getType());
    if (!is_last_parameter)
        m_out << ", ";
}

std::string JaktGenerator::rewriteParameter(llvm::StringRef name, unsigned int index, clang::QualType const& type)
{
    std::string result;

    if (!name.empty()) {
        result += name;
    } else {
        result += "anon _param_";
        result += std::to_string(index);
    }
    result += ": ";

    result += rewriteQualTypeToJaktType(type, QualTypePrintFlags::PF_Nothing);
    return result;
}

std::string JaktGenerator::rewriteQualTypeToJaktType(clang::QualType const& base_type, QualTypePrintFlags flags)
{
    auto type = base_type.getDesugaredType(*m_context);

    if (has_flag(flags, QualTypePrintFlags::PF_InFunctionThatMayThrow) && has_flag(flags, QualTypePrintFlags::PF_IsReturnType)) {
        auto result_type = getTemplateParameterIfMatches(type, "AK::ErrorOr");
        if (result_type.has_value())
            return rewriteQualTypeToJaktType(result_type.value(), flags);
    }

    if (auto inner_type = getTemplateParameterIfMatches(type, "AK::NonnullRefPtr"); inner_type.has_value())
        return rewriteQualTypeToJaktType(inner_type.value(), flags & ~QualTypePrintFlags::PF_InFunctionThatMayThrow);

    if (auto inner_type = getTemplateParameterIfMatches(type, "AK::Optional"); inner_type.has_value())
        return rewriteQualTypeToJaktType(inner_type.value(), flags & ~QualTypePrintFlags::PF_InFunctionThatMayThrow) + "?";

    if (auto inner_type = getTemplateParameterIfMatches(type, "AK::DynamicArray"); inner_type.has_value())
        return std::string("[") + rewriteQualTypeToJaktType(inner_type.value(), flags & ~QualTypePrintFlags::PF_InFunctionThatMayThrow) + "]";

    if (auto key_type = getTemplateParameterIfMatches(type, "Jakt::Dictionary"); key_type.has_value()) {
        auto value_type = getTemplateParameterIfMatches(type, "Jakt::Dictionary", 1);
        return std::string("[")
            + rewriteQualTypeToJaktType(key_type.value(), flags & ~QualTypePrintFlags::PF_InFunctionThatMayThrow)
            + std::string(":")
            + rewriteQualTypeToJaktType(value_type.value(), flags & ~QualTypePrintFlags::PF_InFunctionThatMayThrow)
            + "]";
    }

    if (auto inner_type = getTemplateParameterIfMatches(type, "AK::WeakPtr"); inner_type.has_value()) {
        return std::string("weak ")
            + rewriteQualTypeToJaktType(inner_type.value(), flags & ~QualTypePrintFlags::PF_InFunctionThatMayThrow)
            + "?";
    }

    if (auto inner_type = getTemplateParameterIfMatches(type, "AK::Function"); inner_type.has_value()) {
        std::string jakt_type = "function(";
        auto const* function_type = inner_type.value()->getAs<clang::FunctionProtoType>();
        if (!function_type)
            llvm::report_fatal_error("Function type is not a function as it ought to be", false);

        bool first = true;
        unsigned index = 0;
        for (auto const& param_type : function_type->param_types()) {
            if (first)
                first = false;
            else
                jakt_type += ", ";

            jakt_type += rewriteParameter("", index, param_type);
        }

        jakt_type += ")";

        QualTypePrintFlags print_flags = QualTypePrintFlags::PF_IsReturnType;
        if (isErrorOr(function_type->getReturnType())) {
            jakt_type += " throws";
            print_flags |= QualTypePrintFlags::PF_InFunctionThatMayThrow;
        }

        jakt_type += " -> ";
        jakt_type += rewriteQualTypeToJaktType(function_type->getReturnType(), print_flags);
        return jakt_type;
    }

    auto is_mutable = !type.isConstQualified();

    if (auto const* reference_type = type->getAs<clang::ReferenceType>()) {
        std::string prefix = has_flag(flags, QualTypePrintFlags::PF_IsReturnType)
            ? ""
            : is_mutable
            ? "&mut "
            : "& ";

        return prefix + rewriteQualTypeToJaktType(reference_type->getPointeeType(), flags & ~QualTypePrintFlags::PF_InFunctionThatMayThrow);
    }

    if (auto const* pointer_type = type->getAs<clang::PointerType>())
        return std::string("raw ") + rewriteQualTypeToJaktType(pointer_type->getPointeeType(), flags & ~QualTypePrintFlags::PF_InFunctionThatMayThrow);

    if (auto const* builtin_type = type->getAs<clang::BuiltinType>()) {
        switch (builtin_type->getKind()) {
        case clang::BuiltinType::Void:
            return "void";
        case clang::BuiltinType::Bool:
            return "bool";
        case clang::BuiltinType::Char_U:
        case clang::BuiltinType::Char_S:
            return "c_char";
        case clang::BuiltinType::SChar:
            return "i8";
        case clang::BuiltinType::UChar:
            return "u8";
        case clang::BuiltinType::WChar_U:
        case clang::BuiltinType::WChar_S:
            return "c_char"; // lol
        case clang::BuiltinType::Char8:
            return "i8";
        case clang::BuiltinType::Char16:
            return "i16";
        case clang::BuiltinType::Char32:
            return "i32";
        case clang::BuiltinType::UShort:
            return "i16";
        case clang::BuiltinType::UInt:
        case clang::BuiltinType::ULong:
        case clang::BuiltinType::ULongLong:
        case clang::BuiltinType::UInt128:
        case clang::BuiltinType::Int:
        case clang::BuiltinType::Long:
        case clang::BuiltinType::LongLong:
        case clang::BuiltinType::Int128:
            return "c_int";
        case clang::BuiltinType::Float:
            return "f32";
        case clang::BuiltinType::Double:
            return "f64";
        case clang::BuiltinType::LongDouble:
            return "f64";
        case clang::BuiltinType::NullPtr:
            return "raw void"; // hehehe
        default: {
            std::string error_string = "Don't know how to convert ";
            error_string += base_type.getAsString();
            error_string += " to a jakt type";
            llvm::report_fatal_error(error_string.c_str(), false);
        }
        }
    }

    if (auto* record_type = type->getAs<clang::RecordType>()) {
        if (auto* specialization = llvm::dyn_cast<clang::ClassTemplateSpecializationDecl>(record_type->getAsRecordDecl())) {
            // decl < param... >
            std::string result = specialization->getQualifiedNameAsString();
            result += "<";
            auto& args = specialization->getTemplateArgs();
            for (size_t i = 0; i < args.size(); ++i) {
                if (args[i].getKind() != clang::TemplateArgument::Type) {
                    llvm::errs() << "Saw an NTTP (of kind " << args[i].getKind() << "), can't do that yet :(\n";
                    break;
                }

                if (i != 0)
                    result += ", ";
                result += rewriteQualTypeToJaktType(args[i].getAsType(), QualTypePrintFlags::PF_Nothing);
            }

            result += ">";
            return result;
        }

        auto name = type->getAsCXXRecordDecl()->getQualifiedNameAsString();
        if (name == "AK::StringView")
            return "StringView";
        if (name == "AK::DeprecatedString")
            return "String";

        return type.withoutLocalFastQualifiers().getAsString(m_printing_policy);
    }

    if (auto* enum_type = type->getAs<clang::EnumType>()) {
        return type.withoutLocalFastQualifiers().getAsString(m_printing_policy);
    }

    std::string error_string = "Don't know how to convert ";
    error_string += base_type.getAsString();
    error_string += " to a jakt type";
    llvm::report_fatal_error(error_string.c_str(), false);
}

}
