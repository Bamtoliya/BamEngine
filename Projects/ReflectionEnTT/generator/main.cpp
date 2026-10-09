#pragma region clang includes
#include <clang/Basic/Diagnostic.h>
#include <clang/Basic/SourceManager.h>
#include <clang/Basic/MakeSupport.h>
#include <clang/Frontend/CompilerInstance.h>
#include <clang/Frontend/FrontendActions.h>
#include <clang/Frontend/Utils.h>
#include <clang/Lex/Lexer.h>
#include <clang/Lex/MacroArgs.h>
#include <clang/Lex/PPCallbacks.h>
#include <clang/Lex/Preprocessor.h>
#include <clang/Tooling/CommonOptionsParser.h>
#include <clang/Tooling/Tooling.h>
#include <clang/AST/ASTConsumer.h>
#include <clang/AST/ASTContext.h>
#include <clang/AST/DeclCXX.h>
#include <clang/AST/RecursiveASTVisitor.h>
#include <clang/AST/DeclFriend.h>
#pragma endregion

#pragma region llvm includes
#include <llvm/Support/Casting.h>
#include <llvm/Support/CommandLine.h>
#include <llvm/Support/Error.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/ADT/SmallString.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/Path.h>
#pragma endregion

#pragma region std includes
#include <optional>
#include <type_traits>
#include <memory>
#include <string>
#include <utility>
#include <vector>
#include <regex>
#include <exception>
#include <algorithm>
#pragma endregion

#pragma region metadata includes
#include "MetadataParser.h"
#include "ReflectionModel.h"
#include "ReflectionUnit.h"
#include "RegistrationEmitter.h"
#pragma endregion

namespace
{   
    llvm::cl::OptionCategory ReflectionOptions("Reflection generator options");
    llvm::cl::opt<std::string> OutputPath("output", llvm::cl::desc("Generated .gen.cpp output path"), llvm::cl::Required, llvm::cl::cat(ReflectionOptions));
    llvm::cl::opt<std::string> ModuleName("module", llvm::cl::desc("Module name used in Register_<module>"), llvm::cl::Required, llvm::cl::cat(ReflectionOptions));
    llvm::cl::list<std::string> HeaderPaths("header", llvm::cl::desc("Headers included by generated registration code"), llvm::cl::OneOrMore, llvm::cl::cat(ReflectionOptions));
    llvm::cl::opt<std::string> DepfilePath("depfile", llvm::cl::desc("Dependency file output path"), llvm::cl::init(""), llvm::cl::cat(ReflectionOptions));

    std::optional<std::string> GeneratedCode;

    class ReflectionDependencies final : public clang::DependencyCollector
    {
    public:
        std::string Error;

        bool needSystemDependencies() override
        {
            return true;
        }

        bool sawDependency(llvm::StringRef filename, bool, bool, bool, bool isMissing) override
        {
            if (filename.empty() || filename.front() == '<' || isMissing)
            {
                return false;
            }

            llvm::SmallString<256> absolutePath(filename);
            if (const auto error = llvm::sys::fs::make_absolute(absolutePath))
            {
                Error = error.message();
                return false;
            }

            std::string normalized = absolutePath.str().str();
            std::replace(normalized.begin(), normalized.end(), '\\', '/');

            // 절대 경로를 직접 등록하고 원래 상대 경로의 등록은 건너뜁니다.
            addDependency(normalized);
            return false;
        }
    };

    std::shared_ptr<ReflectionDependencies> CollectedDependencies;

    std::string QuoteDependencyPath(llvm::StringRef path)
    {
        llvm::SmallString<256> quoted;
        clang::quoteMakeTarget(path, quoted);
        return quoted.str().str();
    }

    bool SaveGeneratedFile(llvm::StringRef path, llvm::StringRef contents)
    {
        llvm::SmallString<256> directory(path);
        llvm::sys::path::remove_filename(directory);

        if (!directory.empty())
        {
            if (const auto error = llvm::sys::fs::create_directories(directory))
            {
                llvm::errs() << "Cannot create output directory: " << error.message() << '\n';
                return false;
            }
        }

        if (auto error = llvm::writeToOutput(
            path, [contents](llvm::raw_ostream& output) -> llvm::Error
            {
                output << contents;
                return llvm::Error::success();
            }))
        {
            llvm::errs() << "Cannot save " << path << ": "
                << llvm::toString(std::move(error)) << '\n';
            return false;
        }

        return true;
    }

    struct ReflectionMarker
    {
        std::string Kind;
        std::string File;
        unsigned Line = 0;
        unsigned Column = 0;

        clang::SourceLocation Location;
        clang::SourceLocation TargetLocation;

        bool Handled = false;

        std::vector<clang::Token> MetadataTokens;
        std::optional<reflection_entt_codegen::DeclarationModel> Model;
    };

    class ReflectionCallbacks final : public clang::PPCallbacks
    {
    public:
        ReflectionCallbacks(clang::Preprocessor& preprocessor, std::vector<ReflectionMarker>& markers)
            : m_preprocessor(preprocessor)
            , m_markers(markers)
        {
        }

        void MacroExpands(const clang::Token& macroNameToken, const clang::MacroDefinition&, clang::SourceRange range, const clang::MacroArgs* arguments) override
        {
            const clang::IdentifierInfo* identifier =
                macroNameToken.getIdentifierInfo();

            if (identifier == nullptr)
            {
                return;
            }

            const llvm::StringRef name = identifier->getName();

            if (name != "CLASS" &&
                name != "STRUCT" &&
                name != "ENUM" &&
                name != "FUNCTION" &&
                name != "PROPERTY")
            {
                return;
            }

            const clang::SourceManager& sourceManager =
                m_preprocessor.getSourceManager();

            const clang::SourceLocation location =
                sourceManager.getExpansionLoc(
                    macroNameToken.getLocation()
                );

            if (sourceManager.isInSystemHeader(location))
            {
                return;
            }

            const clang::PresumedLoc presumed =
                sourceManager.getPresumedLoc(location);

            if (!presumed.isValid())
            {
                return;
            }

            ReflectionMarker marker;
            marker.Kind = name.str();
            marker.File = presumed.getFilename();
            marker.Line = presumed.getLine();
            marker.Column = presumed.getColumn();
            marker.Location = location;

            const auto nextToken = clang::Lexer::findNextToken(
                sourceManager.getExpansionLoc(range.getEnd()),
                sourceManager,
                m_preprocessor.getLangOpts()
            );

            if (nextToken)
            {
                marker.TargetLocation = sourceManager.getExpansionLoc(
                    nextToken->getLocation()
                );
            }
            if (arguments != nullptr &&
                arguments->getNumMacroArguments() > 0)
            {
                // Clang의 콜백은 const 포인터를 전달하지만,
                // 인자 확장 API는 내부 캐시를 갱신하는 비const 함수입니다.
                auto* expandableArguments =
                    const_cast<clang::MacroArgs*>(arguments);

                const std::vector<clang::Token>& tokens =
                    expandableArguments->getPreExpArgument(
                        0,
                        m_preprocessor
                    );

                for (const clang::Token& token : tokens)
                {
                    if (token.is(clang::tok::eof))
                    {
                        break;
                    }

                    marker.MetadataTokens.push_back(token);
                }
            }

            m_markers.push_back(std::move(marker));
        }

    private:
        clang::Preprocessor& m_preprocessor;
        std::vector<ReflectionMarker>& m_markers;
    };

    class ReflectionVisitor final
        : public clang::RecursiveASTVisitor<ReflectionVisitor>
    {
    public:
        ReflectionVisitor(
            clang::ASTContext& context,
            std::vector<ReflectionMarker>& markers
        )
            : m_context(context)
            , m_markers(markers)
        {
        }

        bool VisitNamedDecl(clang::NamedDecl* declaration)
        {
            if (declaration->isImplicit())
            {
                return true;
            }

            const clang::SourceManager& sourceManager =
                m_context.getSourceManager();

            const clang::SourceLocation begin =
                sourceManager.getExpansionLoc(
                    declaration->getBeginLoc()
                );

            if (begin.isInvalid() ||
                sourceManager.isInSystemHeader(begin))
            {
                return true;
            }

            for (ReflectionMarker& marker : m_markers)
            {
                if (marker.Handled ||
                    marker.TargetLocation.isInvalid() ||
                    marker.TargetLocation != begin)
                {
                    continue;
                }

                marker.Handled = true;

                if (!MatchesKind(marker.Kind, declaration))
                {
                    clang::DiagnosticsEngine& diagnostics =
                        m_context.getDiagnostics();

                    const unsigned diagnosticID =
                        diagnostics.getCustomDiagID(
                            clang::DiagnosticsEngine::Error,
                            "%0 cannot annotate this declaration"
                        );

                    diagnostics.Report(
                        marker.Location,
                        diagnosticID
                    ) << marker.Kind;

                    continue;
                }

                marker.Model = MakeModel(declaration);
            }

            return true;
        }

    private:
        // MatchesKind()로 지원되는 선언임을 확인한 뒤 호출합니다.
        static reflection_entt_codegen::DeclarationModel MakeModel(
            const clang::NamedDecl* declaration
        )
        {
            using namespace reflection_entt_codegen;

            DeclarationModel model;
            model.Name = declaration->getNameAsString();
            model.QualifiedName =
                declaration->getQualifiedNameAsString();

            switch (declaration->getAccess())
            {
            case clang::AS_public:
                model.Access = AccessKind::Public;
                break;

            case clang::AS_protected:
                model.Access = AccessKind::Protected;
                break;

            case clang::AS_private:
                model.Access = AccessKind::Private;
                break;

            case clang::AS_none:
                model.Access = AccessKind::None;
                break;
            }

            if (const auto* record =
                llvm::dyn_cast<clang::CXXRecordDecl>(declaration))
            {
                TypeDetails details;
                details.IsStruct = record->isStruct();
                details.IsAbstract = record->isAbstract();
                details.IsTemplate = record->isDependentContext();

                for (const clang::FriendDecl* friendDeclaration :
                    record->friends())
                {
                    const clang::NamedDecl* friendTarget =
                        friendDeclaration->getFriendDecl();

                    if (friendTarget != nullptr &&
                        friendTarget->getQualifiedNameAsString() ==
                        "reflection_entt::detail::TypeRegistration")
                    {
                        details.HasRegistrationFriend = true;
                        break;
                    }
                }

                for (const clang::CXXBaseSpecifier& base : record->bases())
                {
                    if (base.getAccessSpecifier() == clang::AS_private || base.getAccessSpecifier() == clang::AS_protected)
                    {
                        details.HasNonPublicBase = true;
                    }
                    details.BaseTypes.push_back(
                        base.getType().getCanonicalType().getAsString()
                    );
                }

                model.Details = std::move(details);
            }
            else if (const auto* field =
                llvm::dyn_cast<clang::FieldDecl>(declaration))
            {
                PropertyDetails details;

                details.OwnerType =
                    field->getParent()->getQualifiedNameAsString();

                details.TypeName = field->getType().getAsString();

                details.CanonicalTypeName =
                    field->getType().getCanonicalType().getAsString();

                details.IsBitField = field->isBitField();
                details.IsReference = field->getType()->isReferenceType();
                details.IsVolatile = field->getType().isVolatileQualified();

                model.Details = std::move(details);
            }
            else if (const auto* function =
                llvm::dyn_cast<clang::FunctionDecl>(declaration))
            {
                using Status = reflection_entt_codegen::BindingStatus;

                FunctionDetails details;
                const clang::QualType returnType = function->getReturnType();

                details.ReturnType = returnType.getAsString();
                details.CanonicalReturnType = returnType.getCanonicalType().getAsString();
                details.Signature = function->getType().getCanonicalType().getAsString();

                if (const auto* method = llvm::dyn_cast<clang::CXXMethodDecl>(function))
                {
                    details.OwnerType = method->getParent()->getQualifiedNameAsString();
                    details.IsStaticMember = method->isStatic();
                    details.IsConstMember = method->isConst();
                }

                const auto* prototype =
                    function->getType()->getAs<clang::FunctionProtoType>();

                details.IsNoexcept = prototype != nullptr && prototype->isNothrow();

                if (llvm::isa<clang::CXXConstructorDecl>(function) ||
                    llvm::isa<clang::CXXDestructorDecl>(function))
                {
                    details.BindingStatus = Status::SpecialMemberFunction;
                }
                else if (prototype == nullptr || function->isDependentContext())
                {
                    details.BindingStatus = Status::UnsupportedPrototype;
                }
                else if (function->isDeleted())
                {
                    details.BindingStatus = Status::DeletedFunction;
                }
                else if (prototype->getMethodQuals().hasVolatile() ||
                    prototype->getMethodQuals().hasRestrict())
                {
                    details.BindingStatus = Status::UnsupportedQualifiers;
                }
                else if (prototype->getRefQualifier() != clang::RQ_None)
                {
                    details.BindingStatus = Status::RefQualifiedMember;
                }
                else if (prototype->isVariadic())
                {
                    details.BindingStatus = Status::VariadicFunction;
                }
                else
                {
                    details.BindingStatus = Status::Available;
                }

                for (const clang::ParmVarDecl* parameter : function->parameters())
                {
                    details.Parameters.push_back(ParameterDetails{
                        parameter->getNameAsString(),
                        parameter->getType().getAsString(),
                        parameter->getType().getCanonicalType().getAsString()
                        });
                }

                model.Details = std::move(details);
            }
            else
            {
                const auto* enumDeclaration =
                    llvm::cast<clang::EnumDecl>(declaration);

                const clang::QualType underlyingType =
                    enumDeclaration->getIntegerType();

                EnumDetails details;
                details.IsScoped = enumDeclaration->isScoped();
                details.IsUnsigned = underlyingType->isUnsignedIntegerType();
                details.UnderlyingType = underlyingType.getAsString();

                for (const clang::EnumConstantDecl* entry :
                    enumDeclaration->enumerators())
                {
                    llvm::SmallString<32> value;
                    entry->getInitVal().toString(value, 10);

                    details.Entries.push_back(
                        EnumEntry{
                            entry->getNameAsString(),
                            value.str().str()
                        }
                    );
                }

                model.Details = std::move(details);
            }

            return model;
        }

        static bool MatchesKind(
            const std::string& kind,
            const clang::NamedDecl* declaration
        )
        {
            if (kind == "CLASS" || kind == "STRUCT")
            {
                const auto* record =
                    llvm::dyn_cast<clang::CXXRecordDecl>(
                        declaration
                    );

                if (record == nullptr ||
                    !record->isThisDeclarationADefinition())
                {
                    return false;
                }

                return kind == "CLASS"
                    ? record->isClass()
                    : record->isStruct();
            }

            if (kind == "ENUM")
            {
                const auto* enumDeclaration =
                    llvm::dyn_cast<clang::EnumDecl>(
                        declaration
                    );

                return enumDeclaration != nullptr &&
                    enumDeclaration->isThisDeclarationADefinition();
            }

            if (kind == "PROPERTY")
            {
                return llvm::isa<clang::FieldDecl>(declaration);
            }

            if (kind == "FUNCTION")
            {
                return llvm::isa<clang::FunctionDecl>(declaration);
            }

            return false;
        }

        clang::ASTContext& m_context;
        std::vector<ReflectionMarker>& m_markers;
    };

    const char* GetAccessName(
        reflection_entt_codegen::AccessKind access
    )
    {
        using reflection_entt_codegen::AccessKind;

        switch (access)
        {
        case AccessKind::Public:
            return "public";

        case AccessKind::Protected:
            return "protected";

        case AccessKind::Private:
            return "private";

        case AccessKind::None:
            return "none";
        }

        return "unknown";
    }

    void PrintModelDetails(
        const reflection_entt_codegen::DeclarationModel& model
    )
    {
        using namespace reflection_entt_codegen;

        std::visit(
            [](const auto& details)
            {
                using T = std::remove_cvref_t<decltype(details)>;

                if constexpr (std::is_same_v<T, TypeDetails>)
                {
                    llvm::outs()
                        << "  Abstract: "
                        << (details.IsAbstract ? "true" : "false")
                        << '\n';

                    llvm::outs()
                        << "  Registration friend: "
                        << (details.HasRegistrationFriend ? "true" : "false")
                        << '\n';

                    for (const std::string& base : details.BaseTypes)
                    {
                        llvm::outs() << "  Base: " << base << '\n';
                    }
                }
                else if constexpr (std::is_same_v<T, PropertyDetails>)
                {
                    llvm::outs()
                        << "  Owner: " << details.OwnerType << '\n'
                        << "  Type: " << details.TypeName << '\n'
                        << "  Canonical type: "
                        << details.CanonicalTypeName << '\n'
                        << "  Bit-field: "
                        << (details.IsBitField ? "true" : "false")
                        << '\n';
                }
                else if constexpr (std::is_same_v<T, FunctionDetails>)
                {
                    llvm::outs()
                        << "  Return: " << details.ReturnType << '\n'
                        << "  Signature: " << details.Signature << '\n'
                        << "  Static member: "
                        << (details.IsStaticMember ? "true" : "false")
                        << '\n'
                        << "  Const member: "
                        << (details.IsConstMember ? "true" : "false")
                        << '\n';

                    for (const ParameterDetails& parameter :
                        details.Parameters)
                    {
                        llvm::outs()
                            << "  Parameter: "
                            << parameter.TypeName << ' '
                            << parameter.Name << '\n';
                    }
                }
                else if constexpr (std::is_same_v<T, EnumDetails>)
                {
                    llvm::outs()
                        << "  Underlying type: "
                        << details.UnderlyingType << '\n'
                        << "  Scoped: "
                        << (details.IsScoped ? "true" : "false")
                        << '\n';

                    for (const EnumEntry& entry : details.Entries)
                    {
                        llvm::outs()
                            << "  Enumerator: "
                            << entry.Name << " = "
                            << entry.DecimalValue << '\n';
                    }
                }
            },
            model.Details
        );
    }

    class ReflectionConsumer final : public clang::ASTConsumer
    {
    public:
        explicit ReflectionConsumer(
            std::vector<ReflectionMarker>& markers
        )
            : m_markers(markers)
        {
        }

        void HandleTranslationUnit(
            clang::ASTContext& context
        ) override
        {
            if (context.getDiagnostics().hasErrorOccurred())
            {
                return;
            }

            ReflectionVisitor visitor(context, m_markers);
            visitor.TraverseDecl(context.getTranslationUnitDecl());

            for (const ReflectionMarker& marker : m_markers)
            {
                if (marker.Handled)
                {
                    continue;
                }

                clang::DiagnosticsEngine& diagnostics =
                    context.getDiagnostics();

                const unsigned diagnosticID =
                    diagnostics.getCustomDiagID(
                        clang::DiagnosticsEngine::Error,
                        "%0 is not immediately followed by "
                        "a supported declaration"
                    );

                diagnostics.Report(
                    marker.Location,
                    diagnosticID
                ) << marker.Kind;
            }

            if (context.getDiagnostics().hasErrorOccurred())
            {
                return;
            }

            for (ReflectionMarker& marker : m_markers)
            {
                auto parsed = reflection_entt_codegen::ParseMetadata(
                    marker.MetadataTokens,
                    context
                );

                if (!parsed.Succeeded())
                {
                    clang::DiagnosticsEngine& diagnostics =
                        context.getDiagnostics();

                    const unsigned diagnosticID =
                        diagnostics.getCustomDiagID(
                            clang::DiagnosticsEngine::Error,
                            "Invalid metadata: %0"
                        );

                    const clang::SourceLocation errorLocation =
                        parsed.ErrorLocation.isValid()
                        ? parsed.ErrorLocation
                        : marker.Location;

                    diagnostics.Report(
                        errorLocation,
                        diagnosticID
                    ) << parsed.Error;

                    continue;
                }

                marker.Model->Metadata = std::move(parsed.Entries);
            }

            if (context.getDiagnostics().hasErrorOccurred())
            {
                return;
            }

            std::vector<reflection_entt_codegen::DeclarationModel> declarations;
            declarations.reserve(m_markers.size());

            for (const ReflectionMarker& marker : m_markers)
            {
                declarations.push_back(*marker.Model);
            }

            auto grouped = reflection_entt_codegen::BuildReflectionUnit(
                declarations
            );

            if (!grouped.Succeeded())
            {
                clang::DiagnosticsEngine& diagnostics =
                    context.getDiagnostics();

                const unsigned diagnosticID =
                    diagnostics.getCustomDiagID(
                        clang::DiagnosticsEngine::Error,
                        "Invalid reflection structure: %0"
                    );

                diagnostics.Report(
                    m_markers[grouped.DeclarationIndex].Location,
                    diagnosticID
                ) << grouped.Error;

                return;
            }

            llvm::SmallString<256> generatedHeaderName(llvm::sys::path::filename(OutputPath.getValue()));
            llvm::sys::path::replace_extension(generatedHeaderName, ".h");

            const std::vector<std::string> headers(HeaderPaths.begin(), HeaderPaths.end());
            try
            {
                GeneratedCode = reflection_entt_codegen::EmitRegistrationCpp(grouped.Unit, ModuleName.getValue(),
                    headers, generatedHeaderName.str().str());
            }
            catch (const std::exception& error)
            {
                const auto diagnosticID = context.getDiagnostics().getCustomDiagID(
                    clang::DiagnosticsEngine::Error, "Cannot emit EnTT registration: %0");
                context.getDiagnostics().Report(diagnosticID) << error.what();
                return;
            }

            for (const ReflectionMarker& marker : m_markers)
            {
                const auto& model = *marker.Model;

                llvm::outs()
                    << marker.Kind << " -> "
                    << model.QualifiedName << '\n';

                PrintModelDetails(model);

                llvm::outs()
                    << "  Access: "
                    << GetAccessName(model.Access)
                    << '\n';

                for (const reflection_entt::MetadataEntry& entry : model.Metadata)
                {
                    llvm::outs() << "  " << entry.Key << " = ";

                    std::visit(
                        [](const auto& value)
                        {
                            using T = std::remove_cvref_t<decltype(value)>;

                            if constexpr (std::is_same_v<T, std::string>)
                            {
                                llvm::outs() << "string: \"";
                                llvm::outs().write_escaped(value);
                                llvm::outs() << '"';
                            }
                            else if constexpr (std::is_same_v<T, bool>)
                            {
                                llvm::outs()
                                    << "bool: "
                                    << (value ? "true" : "false");
                            }
                            else if constexpr (std::is_same_v<T, double>)
                            {
                                llvm::outs() << "double: " << value;
                            }
                            else if constexpr (std::is_same_v<T, std::int64_t>)
                            {
                                llvm::outs() << "int64: " << value;
                            }
                            else
                            {
                                llvm::outs() << "uint64: " << value;
                            }
                        },
                        entry.Value
                    );

                    llvm::outs() << '\n';
                }
            }

            llvm::outs()
                << "Matched markers: "
                << m_markers.size() << '\n';

            llvm::outs() << "\nGrouped types:\n";

            for (const reflection_entt_codegen::ReflectedType& type :
                grouped.Unit.Types)
            {
                llvm::outs()
                    << type.Declaration.QualifiedName << '\n';

                for (const auto& property : type.Properties)
                {
                    llvm::outs()
                        << "  PROPERTY: " << property.Name
                        << " [" << GetAccessName(property.Access)
                        << "]\n";
                }

                for (const auto& function : type.Functions)
                {
                    const auto& details =
                        std::get<reflection_entt_codegen::FunctionDetails>(
                            function.Details
                        );

                    llvm::outs()
                        << "  FUNCTION: " << function.Name
                        << " [" << GetAccessName(function.Access)
                        << "] " << details.Signature << '\n';
                }
            }

            llvm::outs()
                << "Free functions: "
                << grouped.Unit.FreeFunctions.size() << '\n'
                << "Enums: "
                << grouped.Unit.Enums.size() << '\n';
        }

    private:
        std::vector<ReflectionMarker>& m_markers;
    };

    class ReflectionCollectAction final
        : public clang::ASTFrontendAction
    {
    protected:
        std::unique_ptr<clang::ASTConsumer> CreateASTConsumer(
            clang::CompilerInstance& compiler, llvm::StringRef inputFile) override
        {
            m_markers.clear();
            clang::Preprocessor& preprocessor = compiler.getPreprocessor();

            if (!DepfilePath.getValue().empty())
            {
                CollectedDependencies = std::make_shared<ReflectionDependencies>();
                CollectedDependencies->attachToPreprocessor(preprocessor);
                CollectedDependencies->maybeAddDependency(
                    inputFile, false, false, false, false);
            }

            preprocessor.addPPCallbacks(
                std::make_unique<ReflectionCallbacks>(preprocessor, m_markers));

            return std::make_unique<ReflectionConsumer>(m_markers);
        }

    private:
        std::vector<ReflectionMarker> m_markers;
    };
}

int main(int argc, const char** argv)
{
    auto options = clang::tooling::CommonOptionsParser::create(
        argc,
        argv,
        ReflectionOptions
    );

    if (!options)
    {
        llvm::errs()
            << llvm::toString(options.takeError()) << '\n';

        return 1;
    }

    if (options->getSourcePathList().size() != 1)
    {
        llvm::errs() << "Exactly one input file is supported.\n";
        return 1;
    }

    const std::string& module = ModuleName.getValue();

    if (!std::regex_match(
        module,
        std::regex{ "[A-Za-z][A-Za-z0-9_]*" }
    ) || module.find("__") != std::string::npos)
    {
        llvm::errs() << "Invalid module name.\n";
        return 1;
    }

    for (const std::string& header : HeaderPaths)
    {
        if (header.empty() || header.find_first_of("\"\r\n") != std::string::npos)
        {
            llvm::errs() << "Invalid header path: " << header << '\n';
            return 1;
        }
    }

    if (OutputPath.getValue().find_first_of("\"\r\n") != std::string::npos)
    {
        llvm::errs() << "Output path must not contain quotes or line breaks.\n";
        return 1;
    }

    clang::tooling::ClangTool tool(
        options->getCompilations(),
        options->getSourcePathList()
    );

    const int result = tool.run(
        clang::tooling::newFrontendActionFactory<
        ReflectionCollectAction
        >().get()
    );

    if (result != 0)
    {
        llvm::errs()
            << "Reflection generation failed. Output was not updated.\n";

        return result;
    }

    if (!GeneratedCode)
    {
        llvm::errs() << "No registration code was produced.\n";
        return 1;
    }

    if (!DepfilePath.getValue().empty())
    {
        if (!CollectedDependencies || !CollectedDependencies->Error.empty())
        {
            llvm::errs() << "Cannot collect reflection dependencies.\n";
            if (CollectedDependencies)
            {
                llvm::errs() << CollectedDependencies->Error << '\n';
            }
            return 1;
        }

        llvm::SmallString<256> targetPath(OutputPath.getValue());
        if (const auto error = llvm::sys::fs::make_absolute(targetPath))
        {
            llvm::errs() << "Cannot resolve output path: " << error.message() << '\n';
            return 1;
        }

        std::string target = targetPath.str().str();
        std::replace(target.begin(), target.end(), '\\', '/');

        std::string dependencies = QuoteDependencyPath(target) + ":";
        for (const std::string& path : CollectedDependencies->getDependencies())
        {
            dependencies += " \\\n  " + QuoteDependencyPath(path);
        }
        dependencies += '\n';

        if (!SaveGeneratedFile(DepfilePath.getValue(), dependencies))
        {
            return 1;
        }

        llvm::outs() << "Dependencies: " << DepfilePath.getValue() << '\n';
    }

    llvm::SmallString<256> generatedHeaderPath(OutputPath.getValue());
    llvm::sys::path::replace_extension(generatedHeaderPath, ".h");

    const std::string generatedHeader = reflection_entt_codegen::EmitRegistrationHeader(module);
    if (!SaveGeneratedFile(generatedHeaderPath, generatedHeader))
    {
        return 1;
    }

    if (!SaveGeneratedFile(OutputPath.getValue(), *GeneratedCode))
    {
        return 1;
    }

    llvm::outs() << "Generated header: " << generatedHeaderPath << '\n';
    llvm::outs() << "Generated: " << OutputPath.getValue() << '\n';
    return 0;
}