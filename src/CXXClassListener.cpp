/*
 * Copyright (c) 2022, Andrew Kaster <akaster@serenityos.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include "CXXClassListener.h"
#include <algorithm>
#include <clang/AST/Decl.h>
#include <clang/AST/DeclCXX.h>
#include <clang/AST/PrettyPrinter.h>
#include <clang/AST/Type.h>
#include <clang/ASTMatchers/ASTMatchFinder.h>
#include <clang/ASTMatchers/ASTMatchers.h>
#include <clang/Basic/Specifiers.h>
#include <filesystem>
#include <llvm/Support/Casting.h>
#include <llvm/Support/Error.h>
#include <llvm/Support/ErrorHandling.h>
#include <vector>

namespace jakt_bindgen {

using namespace clang::ast_matchers;

CXXClassListener::CXXClassListener(std::vector<std::string> namespaces, clang::ast_matchers::MatchFinder& finder, std::vector<std::string>& source_paths, std::set<std::filesystem::path>& seen_files)
    : m_namespaces(std::move(namespaces))
    , m_finder(finder)
    , m_source_paths(source_paths)
    , m_seen_files(seen_files)
{
    registerMatches();
}

CXXClassListener::~CXXClassListener()
{
}

inline internal::Matcher<clang::NamedDecl> hasAnyNameOf(std::vector<std::string> Names)
{
    return internal::Matcher<clang::NamedDecl>(
        new internal::HasNameMatcher(std::move(Names)));
}

void CXXClassListener::registerMatches()
{
    m_finder.addMatcher(traverse(clang::TK_IgnoreUnlessSpelledInSource,
                            recordDecl(decl().bind("toplevel-name"),
                                anyOf(hasParent(namespaceDecl(hasAnyNameOf(m_namespaces))), hasParent(classTemplateDecl(hasParent(namespaceDecl(hasAnyNameOf(m_namespaces)))))),
                                isExpansionInMainFile(),
                                forEachDescendant(cxxMethodDecl(unless(isPrivate())).bind("toplevel-method")))),
        this);

    // Note: Matches *namespace scope* enums.
    //       Nested class enums are handled separately.
    m_finder.addMatcher(traverse(clang::TK_IgnoreUnlessSpelledInSource,
                            enumDecl(decl().bind("toplevel-enum"),
                                anyOf(hasParent(namespaceDecl(hasAnyNameOf(m_namespaces))), hasParent(classTemplateDecl(hasParent(namespaceDecl(hasAnyNameOf(m_namespaces)))))),
                                isExpansionInMainFile())),
        this);
}

void CXXClassListener::run(MatchFinder::MatchResult const& Result)
{
    if (clang::RecordDecl const* RD = Result.Nodes.getNodeAs<clang::RecordDecl>("toplevel-name")) {
        if (RD->isClass())
            visitClass(llvm::cast<clang::CXXRecordDecl>(RD->getDefinition()), Result.SourceManager);
    }
    if (clang::CXXMethodDecl const* MD = Result.Nodes.getNodeAs<clang::CXXMethodDecl>("toplevel-method")) {
        visitClassMethod(MD, Result.SourceManager);
    }
    if (clang::EnumDecl const* ED = Result.Nodes.getNodeAs<clang::EnumDecl>("toplevel-enum")) {
        visitEnumeration(ED, Result.SourceManager);
    }
}

void CXXClassListener::resetForNextFile()
{
    m_tag_decls.clear();
    m_imports.clear();
}

void CXXClassListener::visitClass(clang::CXXRecordDecl const* class_definition, clang::SourceManager const* source_manager)
{
    if (std::find(m_tag_decls.begin(), m_tag_decls.end(), class_definition) != m_tag_decls.end())
        return;

    m_tag_decls.push_back(class_definition);

    // Visit bases and add to import list
    if (class_definition->hasDefinition()) {
        for (clang::CXXBaseSpecifier const& base : class_definition->bases()) {
            if (base.isVirtual()) {
                llvm::errs() << "ERROR: Virtual base class!\n";
                continue;
            }
            if (base.getAccessSpecifier() != clang::AccessSpecifier::AS_public) {
                llvm::errs() << "ERROR: Don't know how to handle non-public bases\n";
                continue;
            }

            clang::RecordType const* Ty = base.getType()->getAs<clang::RecordType>();
            if (!Ty)
                continue;

            clang::CXXRecordDecl const* base_record = llvm::cast_or_null<clang::CXXRecordDecl>(Ty->getDecl()->getDefinition());
            if (!base_record) {
                llvm::errs() << "ERROR: Base class unusable\n";
                continue;
            }

            if (source_manager->isInMainFile(source_manager->getExpansionLoc(base_record->getBeginLoc()))) {
                continue;
            }
            auto base_class_name = base_record->getQualifiedNameAsString();
            if (base_class_name == "AK::RefCounted" || base_class_name == "AK::Weakable")
                continue;

            m_imports.push_back({ base_record, source_manager->getFilename(base_record->getBeginLoc()).str() });
        }
    }
}

void CXXClassListener::visitClassMethod(clang::CXXMethodDecl const* method_declaration, clang::SourceManager const* source_manager)
{
    for (auto& param : method_declaration->parameters()) {
        if (param->getType()->containsUnexpandedParameterPack() || llvm::isa<clang::PackExpansionType>(param->getType())) {
            llvm::errs() << "Note: Skipping method, don't know how to handle parameter packs\n";
            return;
        }
    }

    if (method_declaration->isInstance()) {
        if (llvm::isa<clang::CXXDestructorDecl>(method_declaration)
            || llvm::isa<clang::CXXConversionDecl>(method_declaration)
            || method_declaration->isOverloadedOperator()) {
            return;
        }
        if (clang::CXXConstructorDecl const* ctor = llvm::dyn_cast<clang::CXXConstructorDecl>(method_declaration)) {
            // Allow public default constructors to pass through, along with ctors with parameters
            if (ctor->isCopyOrMoveConstructor() || ctor->isDeleted())
                return;
        }
        // TODO: Walk instance method parameters and return type to find new types to add to imports
        m_methods[method_declaration->getParent()].push_back(method_declaration);
    } else if (method_declaration->isStatic()) {
        // TODO: Walk static method parameters and return type to find new types to add to imports
        m_methods[method_declaration->getParent()].push_back(method_declaration);
    }

    std::function<void(clang::CXXRecordDecl const*)> add_type = [&](auto decl) {
        if (!decl)
            return;

        auto path = std::filesystem::weakly_canonical(source_manager->getFilename(decl->getBeginLoc()).str());
        if (m_seen_files.contains(path))
            return;

        llvm::errs() << "Adding " << path << " to source paths\n";
        m_seen_files.insert(path);
        m_source_paths.push_back(path);

        m_imports.push_back({ decl, path });

        if (auto template_decl = dynamic_cast<clang::ClassTemplatePartialSpecializationDecl const*>(decl)) {
            for (auto arg : template_decl->getTemplateArgs().asArray()) {
                if (arg.getKind() != clang::TemplateArgument::ArgKind::Type)
                    continue;

                if (auto record_decl = arg.getAsType()->getAsCXXRecordDecl())
                    add_type(record_decl);
            }
        }
    };

    for (auto& param : method_declaration->parameters()) {
        if (m_methods.find(param->getType()->getAsCXXRecordDecl()) != m_methods.end())
            continue;

        if (auto decl = param->getType()->getAsCXXRecordDecl())
            add_type(decl);
        else if (param->getType()->isReferenceType())
            add_type(param->getType()->getPointeeCXXRecordDecl());
    }
}

void CXXClassListener::visitEnumeration(clang::EnumDecl const* enum_declaration, clang::SourceManager const*)
{
    if (std::find(m_tag_decls.begin(), m_tag_decls.end(), enum_declaration) != m_tag_decls.end())
        return;

    m_tag_decls.push_back(enum_declaration);
}

}
