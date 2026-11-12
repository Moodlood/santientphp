#include "Parser.hpp"
#include <stdexcept>

namespace sph {

namespace {

int64_t parseInteger(const std::string& text) {
    if (text.size() > 2 && text[0] == '0' && (text[1] == 'b' || text[1] == 'B')) {
        int64_t val = 0;
        for (size_t i = 2; i < text.size(); ++i) {
            val = (val << 1) | (text[i] - '0');
        }
        return val;
    }
    return std::stoll(text, nullptr, 0);
}

double parseFloat(const std::string& text) {
    return std::stod(text);
}

} // namespace

Parser::Parser(uint32_t fileId, SourceManager& sourceManager, DiagnosticEmitter* diagnostics,
               std::shared_ptr<std::unordered_set<std::string>> visitedFiles)
    : m_fileId(fileId), m_sourceManager(sourceManager), m_diagnostics(diagnostics),
      m_visitedFiles(std::move(visitedFiles)) {
    if (!m_visitedFiles) {
        m_visitedFiles = std::make_shared<std::unordered_set<std::string>>();
    }
    const auto* file = m_sourceManager.getFile(m_fileId);
    if (file) {
        m_visitedFiles->insert(file->getFilename());
    }
    Lexer lexer(m_fileId, m_sourceManager, m_diagnostics);
    m_tokens = lexer.tokenizeAll();
}

bool Parser::isAtEnd() const noexcept {
    return peek().is(TokenKind::EndOfFile);
}

const Token& Parser::peek() const noexcept {
    if (m_current >= m_tokens.size()) {
        return m_tokens.back();
    }
    return m_tokens[m_current];
}

const Token& Parser::peekNext() const noexcept {
    if (m_current + 1 < m_tokens.size()) {
        return m_tokens[m_current + 1];
    }
    return m_tokens.back();
}

const Token& Parser::previous() const noexcept {
    if (m_current == 0) {
        return m_tokens[0];
    }
    return m_tokens[m_current - 1];
}

const Token& Parser::advance() noexcept {
    if (!isAtEnd()) {
        ++m_current;
    }
    return previous();
}

bool Parser::check(TokenKind kind) const noexcept {
    if (isAtEnd()) return kind == TokenKind::EndOfFile;
    return peek().is(kind);
}

bool Parser::match(TokenKind kind) noexcept {
    if (check(kind)) {
        advance();
        return true;
    }
    return false;
}

const Token* Parser::expect(TokenKind kind, const std::string& errMsg) {
    if (check(kind)) {
        return &advance();
    }
    error(peek().span, errMsg);
    return nullptr;
}

void Parser::error(SourceSpan span, std::string message) {
    if (m_diagnostics) {
        m_diagnostics->emit(Diagnostic::makeError(span, std::move(message)));
    }
}

void Parser::synchronize() {
    advance();

    while (!isAtEnd()) {
        if (previous().is(TokenKind::Semicolon)) {
            return;
        }

        switch (peek().kind) {
            case TokenKind::KwImport:
            case TokenKind::KwFn:
            case TokenKind::KwLet:
            case TokenKind::KwMut:
            case TokenKind::KwIf:
            case TokenKind::KwWhile:
            case TokenKind::KwReturn:
            case TokenKind::KwInterface:
            case TokenKind::KwClass:
            case TokenKind::RBrace:
                return;
            default:
                break;
        }

        advance();
    }
}

std::unique_ptr<ImportDecl> Parser::parseImport() {
    const Token* impTok = expect(TokenKind::KwImport, "expected 'import'");
    if (!impTok) return nullptr;
    SourceLocation startLoc = impTok->span.start;

    const Token* pathTok = expect(TokenKind::StringLiteral, "expected string literal after 'import'");
    if (!pathTok) return nullptr;
    std::string importPath = pathTok->text;

    const Token* semi = expect(TokenKind::Semicolon, "expected ';' after import path");
    SourceLocation endLoc = semi ? semi->span.end : pathTok->span.end;

    return std::make_unique<ImportDecl>(std::move(importPath), SourceSpan(startLoc, endLoc));
}

std::unique_ptr<ProgramAST> Parser::parseProgram() {
    SourceLocation startLoc = peek().span.start;

    // Check PHP opening tag <?php
    if (!match(TokenKind::PhpOpenTag)) {
        error(peek().span, "expected '<?php' at beginning of file");
    }

    std::vector<std::unique_ptr<ImportDecl>> imports;
    std::vector<std::unique_ptr<StructDecl>> structs;
    std::vector<std::unique_ptr<EnumDecl>> enums;
    std::vector<std::unique_ptr<InterfaceDecl>> interfaces;
    std::vector<std::unique_ptr<ClassDecl>> classes;
    std::vector<std::unique_ptr<FunctionDecl>> functions;

    while (!isAtEnd()) {
        if (check(TokenKind::KwImport)) {
            auto imp = parseImport();
            if (imp) {
                std::string importPath = imp->getPath();
                imports.push_back(std::move(imp));

                // Resolve file
                std::optional<uint32_t> targetFileId;
                const auto* currentFile = m_sourceManager.getFile(m_fileId);
                std::string currentFilename = currentFile ? currentFile->getFilename() : "";

                // First check in-memory files (for testing)
                targetFileId = m_sourceManager.findFileIdByName(importPath);

                std::string canonicalKey;
                if (!targetFileId) {
                    std::filesystem::path resolvedPath = importPath;
                    if (!currentFilename.empty()) {
                        std::filesystem::path currentPath(currentFilename);
                        resolvedPath = currentPath.parent_path() / importPath;
                    }
                    if (std::filesystem::exists(resolvedPath)) {
                        targetFileId = m_sourceManager.loadFile(resolvedPath);
                        canonicalKey = std::filesystem::weakly_canonical(resolvedPath).string();
                    } else if (std::filesystem::exists(importPath)) {
                        targetFileId = m_sourceManager.loadFile(importPath);
                        canonicalKey = std::filesystem::weakly_canonical(importPath).string();
                    }
                } else {
                    canonicalKey = importPath;
                }

                if (!targetFileId) {
                    error(previous().span, "could not open imported file '" + importPath + "'");
                } else if (!m_visitedFiles->count(canonicalKey)) {
                    m_visitedFiles->insert(canonicalKey);
                    Parser subParser(*targetFileId, m_sourceManager, m_diagnostics, m_visitedFiles);
                    auto importedProg = subParser.parseProgram();
                    if (importedProg) {
                        for (auto& s : importedProg->getStructs()) structs.push_back(std::move(s));
                        for (auto& e : importedProg->getEnums()) enums.push_back(std::move(e));
                        for (auto& iface : importedProg->getInterfaces()) interfaces.push_back(std::move(iface));
                        for (auto& c : importedProg->getClasses()) classes.push_back(std::move(c));
                        for (auto& f : importedProg->getFunctions()) functions.push_back(std::move(f));
                        for (auto& i : importedProg->getImports()) imports.push_back(std::move(i));
                    }
                }
            } else {
                synchronize();
            }
        } else if (check(TokenKind::KwAsync) || check(TokenKind::KwFn) || check(TokenKind::KwFunction)) {
            auto fn = parseFunction();
            if (fn) {
                functions.push_back(std::move(fn));
            } else {
                synchronize();
            }
        } else if (check(TokenKind::KwExtern)) {
            auto extFn = parseExternFunction();
            if (extFn) {
                functions.push_back(std::move(extFn));
            } else {
                synchronize();
            }
        } else if (check(TokenKind::Hash) || check(TokenKind::KwStruct)) {
            StructAttribute attr = parseStructAttributes();
            auto st = parseStruct(attr);
            if (st) {
                structs.push_back(std::move(st));
            } else {
                synchronize();
            }
        } else if (check(TokenKind::KwEnum)) {
            auto en = parseEnum();
            if (en) {
                enums.push_back(std::move(en));
            } else {
                synchronize();
            }
        } else if (check(TokenKind::KwInterface)) {
            auto iface = parseInterface();
            if (iface) {
                interfaces.push_back(std::move(iface));
            } else {
                synchronize();
            }
        } else if (check(TokenKind::KwClass)) {
            auto cl = parseClass();
            if (cl) {
                classes.push_back(std::move(cl));
            } else {
                synchronize();
            }
        } else {
            error(peek().span, "expected top-level declaration ('import', 'fn', 'struct', 'enum', 'interface', or 'class')");
            synchronize();
        }
    }

    SourceLocation endLoc = peek().span.end;
    return std::make_unique<ProgramAST>(std::move(imports), std::move(structs), std::move(enums), std::move(interfaces), std::move(classes), std::move(functions), SourceSpan(startLoc, endLoc));
}

std::unique_ptr<EnumDecl> Parser::parseEnum() {
    const Token* enumToken = expect(TokenKind::KwEnum, "expected 'enum'");
    if (!enumToken) return nullptr;
    SourceLocation startLoc = enumToken->span.start;

    const Token* nameTok = expect(TokenKind::Identifier, "expected enum name after 'enum'");
    if (!nameTok) return nullptr;
    std::string enumName = nameTok->text;

    if (!expect(TokenKind::LBrace, "expected '{' after enum name")) return nullptr;

    std::vector<EnumVariant> variants;
    while (!check(TokenKind::RBrace) && !isAtEnd()) {
        const Token* variantNameTok = expect(TokenKind::Identifier, "expected enum variant name");
        if (!variantNameTok) return nullptr;

        std::optional<int64_t> explicitVal;
        if (match(TokenKind::Equal)) {
            bool negative = match(TokenKind::Minus);
            const Token* valTok = expect(TokenKind::IntLiteral, "expected integer value after '=' in enum variant");
            if (!valTok) return nullptr;
            try {
                int64_t v = std::stoll(valTok->text);
                if (negative) v = -v;
                explicitVal = v;
            } catch (...) {
                error(valTok->span, "invalid integer literal in enum variant");
                return nullptr;
            }
        }

        SourceSpan variantSpan(variantNameTok->span.start, previous().span.end);
        variants.push_back(EnumVariant{variantNameTok->text, explicitVal, variantSpan});

        if (!match(TokenKind::Comma)) {
            break;
        }
    }

    const Token* rbrace = expect(TokenKind::RBrace, "expected '}' after enum body");
    SourceLocation endLoc = rbrace ? rbrace->span.end : (variants.empty() ? nameTok->span.end : variants.back().span.end);
    return std::make_unique<EnumDecl>(std::move(enumName), std::move(variants), SourceSpan(startLoc, endLoc));
}

StructAttribute Parser::parseStructAttributes() {
    StructAttribute attr;
    while (match(TokenKind::Hash)) {
        if (!expect(TokenKind::LBracket, "expected '[' after '#'")) return attr;
        const Token* idTok = expect(TokenKind::Identifier, "expected attribute name (e.g. 'repr' or 'align')");
        if (!idTok) return attr;

        if (idTok->text == "repr") {
            if (!expect(TokenKind::LParen, "expected '(' after 'repr'")) return attr;
            const Token* kindTok = expect(TokenKind::Identifier, "expected 'packed' or 'C' in 'repr(...)'");
            if (!kindTok) return attr;
            if (kindTok->text == "packed") {
                attr.layout = StructLayout::Packed;
            } else if (kindTok->text == "C") {
                attr.layout = StructLayout::C;
            } else {
                error(kindTok->span, "unknown repr layout '" + kindTok->text + "'; expected 'packed' or 'C'");
            }
            expect(TokenKind::RParen, "expected ')' after repr modifier");
        } else if (idTok->text == "align") {
            if (!expect(TokenKind::LParen, "expected '(' after 'align'")) return attr;
            const Token* numTok = expect(TokenKind::IntLiteral, "expected integer alignment in 'align(...)'");
            if (!numTok) return attr;
            try {
                attr.explicitAlignment = std::stoull(numTok->text);
            } catch (...) {
                error(numTok->span, "invalid alignment literal");
            }
            expect(TokenKind::RParen, "expected ')' after align modifier");
        } else {
            error(idTok->span, "unknown attribute '" + idTok->text + "'");
        }

        expect(TokenKind::RBracket, "expected ']' after attribute");
    }
    return attr;
}

std::unique_ptr<StructDecl> Parser::parseStruct(StructAttribute attr) {
    if (check(TokenKind::Hash)) {
        attr = parseStructAttributes();
    }
    const Token* structToken = expect(TokenKind::KwStruct, "expected 'struct'");
    if (!structToken) return nullptr;
    SourceLocation startLoc = structToken->span.start;

    const Token* nameTok = expect(TokenKind::Identifier, "expected struct name after 'struct'");
    if (!nameTok) return nullptr;
    std::string structName = nameTok->text;

    std::vector<std::string> typeParams;
    if (check(TokenKind::Less)) {
        typeParams = parseTypeParameterList();
    }

    if (!expect(TokenKind::LBrace, "expected '{' after struct name")) return nullptr;

    std::vector<StructField> fields;
    while (!check(TokenKind::RBrace) && !isAtEnd()) {
        const Token* fieldNameTok = expect(TokenKind::Identifier, "expected field name");
        if (!fieldNameTok) return nullptr;

        if (!expect(TokenKind::Colon, "expected ':' after field name")) return nullptr;
        std::string fieldType = parseType();
        if (fieldType.empty()) return nullptr;

        SourceSpan fieldSpan(fieldNameTok->span.start, previous().span.end);
        fields.push_back(StructField{fieldNameTok->text, std::move(fieldType), fieldSpan});

        if (!match(TokenKind::Comma)) {
            match(TokenKind::Semicolon);
        }
    }

    const Token* rbrace = expect(TokenKind::RBrace, "expected '}' after struct body");
    SourceLocation endLoc = rbrace ? rbrace->span.end : (fields.empty() ? nameTok->span.end : fields.back().span.end);
    return std::make_unique<StructDecl>(std::move(structName), std::move(typeParams), std::move(fields), attr, SourceSpan(startLoc, endLoc));
}

std::unique_ptr<InterfaceDecl> Parser::parseInterface() {
    const Token* interfaceToken = expect(TokenKind::KwInterface, "expected 'interface'");
    if (!interfaceToken) return nullptr;
    SourceLocation startLoc = interfaceToken->span.start;

    const Token* nameTok = expect(TokenKind::Identifier, "expected interface name after 'interface'");
    if (!nameTok) return nullptr;
    std::string interfaceName = nameTok->text;

    if (!expect(TokenKind::LBrace, "expected '{' after interface name")) return nullptr;

    std::vector<MethodSignature> methods;
    while (!check(TokenKind::RBrace) && !isAtEnd()) {
        const Token* fnToken = expect(TokenKind::KwFn, "expected 'fn' in interface body");
        if (!fnToken) return nullptr;
        SourceLocation sigStart = fnToken->span.start;

        const Token* methodTok = expect(TokenKind::Identifier, "expected method name after 'fn'");
        if (!methodTok) return nullptr;
        std::string methodName = methodTok->text;

        if (!expect(TokenKind::LParen, "expected '(' after method name")) return nullptr;
        auto params = parseParameterList();
        if (!expect(TokenKind::RParen, "expected ')' after parameters")) return nullptr;

        std::string returnType = "void";
        if (match(TokenKind::Colon)) {
            returnType = parseType();
            if (returnType.empty()) return nullptr;
        }

        const Token* semi = expect(TokenKind::Semicolon, "expected ';' after interface method signature");
        SourceLocation sigEnd = semi ? semi->span.end : previous().span.end;

        methods.push_back(MethodSignature{std::move(methodName), std::move(params), std::move(returnType), SourceSpan(sigStart, sigEnd)});
    }

    const Token* rbrace = expect(TokenKind::RBrace, "expected '}' after interface body");
    SourceLocation endLoc = rbrace ? rbrace->span.end : (methods.empty() ? nameTok->span.end : methods.back().span.end);
    return std::make_unique<InterfaceDecl>(std::move(interfaceName), std::move(methods), SourceSpan(startLoc, endLoc));
}

std::unique_ptr<ClassDecl> Parser::parseClass() {
    const Token* classToken = expect(TokenKind::KwClass, "expected 'class'");
    if (!classToken) return nullptr;
    SourceLocation startLoc = classToken->span.start;

    const Token* nameTok = expect(TokenKind::Identifier, "expected class name after 'class'");
    if (!nameTok) return nullptr;
    std::string className = nameTok->text;

    std::vector<std::string> implementsInterfaces;
    if (match(TokenKind::KwImplements)) {
        do {
            const Token* ifaceTok = expect(TokenKind::Identifier, "expected interface name after 'implements'");
            if (!ifaceTok) return nullptr;
            implementsInterfaces.push_back(ifaceTok->text);
        } while (match(TokenKind::Comma));
    }

    if (!expect(TokenKind::LBrace, "expected '{' after class name")) return nullptr;

    std::vector<ClassField> fields;
    std::vector<std::unique_ptr<FunctionDecl>> methods;

    while (!check(TokenKind::RBrace) && !isAtEnd()) {
        match(TokenKind::KwPublic) || match(TokenKind::KwPrivate) || match(TokenKind::KwProtected);
        if (check(TokenKind::KwAsync) || check(TokenKind::KwFn) || check(TokenKind::KwFunction)) {
            auto method = parseFunction();
            if (!method) return nullptr;
            methods.push_back(std::move(method));
        } else if (check(TokenKind::Identifier)) {
            const Token* fieldNameTok = &advance();
            std::string fieldName = fieldNameTok->text;
            if (!fieldName.empty() && fieldName[0] == '$') {
                fieldName = fieldName.substr(1);
            }
            std::string fieldType = "dynamic";
            if (match(TokenKind::Colon)) {
                fieldType = parseType();
                if (fieldType.empty()) return nullptr;
            } else if (check(TokenKind::Identifier) && !peek().text.empty() && peek().text[0] == '$') {
                // e.g. "string $name;" where fieldNameTok was type "string"
                fieldType = fieldName;
                const Token* actualNameTok = &advance();
                fieldName = actualNameTok->text.substr(1);
            }

            SourceSpan fieldSpan(fieldNameTok->span.start, previous().span.end);
            fields.push_back(ClassField{std::move(fieldName), std::move(fieldType), fieldSpan});

            if (!match(TokenKind::Comma)) {
                match(TokenKind::Semicolon);
            }
        } else {
            error(peek().span, "expected field or method declaration in class body");
            return nullptr;
        }
    }

    const Token* rbrace = expect(TokenKind::RBrace, "expected '}' after class body");
    SourceLocation endLoc = rbrace ? rbrace->span.end : (methods.empty() && fields.empty() ? nameTok->span.end : previous().span.end);
    return std::make_unique<ClassDecl>(std::move(className), std::move(implementsInterfaces), std::move(fields), std::move(methods), SourceSpan(startLoc, endLoc));
}

std::unique_ptr<FunctionDecl> Parser::parseFunction() {
    bool isAsync = false;
    SourceLocation startLoc = peek().span.start;
    if (match(TokenKind::KwAsync)) {
        isAsync = true;
    }

    const Token* fnToken = nullptr;
    if (check(TokenKind::KwFn) || check(TokenKind::KwFunction)) {
        fnToken = &advance();
        if (!isAsync) {
            startLoc = fnToken->span.start;
        }
    } else {
        fnToken = expect(TokenKind::KwFn, "expected 'fn' or 'function'");
        if (!fnToken) return nullptr;
    }

    const Token* nameTok = expect(TokenKind::Identifier, "expected function name after 'fn' or 'function'");
    if (!nameTok) return nullptr;
    std::string fnName = nameTok->text;

    std::vector<std::string> typeParams;
    if (check(TokenKind::Less)) {
        typeParams = parseTypeParameterList();
    }

    if (!expect(TokenKind::LParen, "expected '(' after function name")) return nullptr;
    auto params = parseParameterList();
    if (!expect(TokenKind::RParen, "expected ')' after parameters")) return nullptr;

    std::string returnType = isAsync ? "Promise" : ((fnToken->is(TokenKind::KwFunction) && fnName != "__construct") ? "dynamic" : "void");
    if (match(TokenKind::Colon)) {
        returnType = parseType();
        if (returnType.empty()) return nullptr;
    }

    if (!check(TokenKind::LBrace)) {
        error(peek().span, "expected '{' for function body");
        return nullptr;
    }

    auto body = parseBlock();
    if (!body) return nullptr;

    SourceSpan fnSpan(startLoc, body->getSpan().end);
    auto fnDecl = std::make_unique<FunctionDecl>(std::move(fnName), std::move(typeParams), std::move(params), std::move(returnType),
                                                 std::move(body), fnSpan);
    fnDecl->setAsync(isAsync);
    return fnDecl;
}

std::unique_ptr<FunctionDecl> Parser::parseExternFunction() {
    const Token* externToken = expect(TokenKind::KwExtern, "expected 'extern'");
    if (!externToken) return nullptr;
    SourceLocation startLoc = externToken->span.start;

    std::string abi = "C";
    if (check(TokenKind::StringLiteral)) {
        abi = advance().text;
    }

    const Token* fnToken = expect(TokenKind::KwFn, "expected 'fn' after 'extern'");
    if (!fnToken) return nullptr;

    const Token* nameTok = expect(TokenKind::Identifier, "expected function name after 'fn'");
    if (!nameTok) return nullptr;
    std::string fnName = nameTok->text;

    if (!expect(TokenKind::LParen, "expected '(' after function name")) return nullptr;
    auto params = parseParameterList();
    if (!expect(TokenKind::RParen, "expected ')' after parameters")) return nullptr;

    std::string returnType = "void";
    if (match(TokenKind::Colon)) {
        returnType = parseType();
        if (returnType.empty()) return nullptr;
    }

    const Token* semi = expect(TokenKind::Semicolon, "expected ';' after extern function declaration");
    SourceLocation endLoc = semi ? semi->span.end : previous().span.end;

    SourceSpan fnSpan(startLoc, endLoc);
    return std::make_unique<FunctionDecl>(std::move(fnName), std::move(abi), std::move(params), std::move(returnType), fnSpan);
}

std::vector<Parameter> Parser::parseParameterList() {
    std::vector<Parameter> params;
    if (check(TokenKind::RParen)) {
        return params;
    }

    do {
        bool isByRef = false;
        SourceLocation startLoc = peek().span.start;
        if (match(TokenKind::Amp)) {
            isByRef = true;
        }

        std::string paramName;
        std::string typeStr = "dynamic";

        if (check(TokenKind::KwI32) || check(TokenKind::KwI64) || check(TokenKind::KwF32) ||
            check(TokenKind::KwF64) || check(TokenKind::KwBool) || check(TokenKind::KwString)) {
            typeStr = parseType();
            if (match(TokenKind::Amp)) {
                isByRef = true;
            }
            const Token* idTok = expect(TokenKind::Identifier, "expected parameter name after type");
            if (!idTok) return params;
            paramName = idTok->text;
        } else {
            const Token* idTok = expect(TokenKind::Identifier, "expected parameter name");
            if (!idTok) return params;

            paramName = idTok->text;
            if (match(TokenKind::Colon)) {
                typeStr = parseType();
                if (typeStr.empty()) return params;
            } else if (match(TokenKind::Amp)) {
                isByRef = true;
                typeStr = paramName;
                const Token* actualNameTok = expect(TokenKind::Identifier, "expected parameter name after '&'");
                if (!actualNameTok) return params;
                paramName = actualNameTok->text;
            } else if (check(TokenKind::Identifier) && !peek().text.empty() && peek().text[0] == '$') {
                typeStr = paramName;
                const Token* actualNameTok = &advance();
                paramName = actualNameTok->text;
            }
        }

        SourceSpan paramSpan(startLoc, previous().span.end);
        params.push_back(Parameter{std::move(paramName), std::move(typeStr), paramSpan, isByRef});
    } while (match(TokenKind::Comma));

    return params;
}

std::vector<std::string> Parser::parseTypeParameterList() {
    std::vector<std::string> params;
    if (!match(TokenKind::Less)) {
        return params;
    }
    do {
        const Token* idTok = expect(TokenKind::Identifier, "expected type parameter name");
        if (!idTok) return params;
        params.push_back(idTok->text);
    } while (match(TokenKind::Comma));

    expect(TokenKind::Greater, "expected '>' after type parameters");
    return params;
}

std::vector<std::string> Parser::parseTypeArgumentList() {
    std::vector<std::string> args;
    if (!match(TokenKind::Less)) {
        return args;
    }
    do {
        std::string t = parseType();
        if (t.empty()) return args;
        args.push_back(std::move(t));
    } while (match(TokenKind::Comma));

    expect(TokenKind::Greater, "expected '>' after type arguments");
    return args;
}

bool Parser::isGenericCallOrStructInit() const noexcept {
    if (m_current >= m_tokens.size() || m_tokens[m_current].kind != TokenKind::Less) {
        return false;
    }
    int depth = 0;
    size_t i = m_current;
    while (i < m_tokens.size()) {
        if (m_tokens[i].kind == TokenKind::Less) {
            depth++;
        } else if (m_tokens[i].kind == TokenKind::Greater) {
            depth--;
            if (depth == 0) {
                if (i + 1 < m_tokens.size()) {
                    TokenKind nextKind = m_tokens[i + 1].kind;
                    if (nextKind == TokenKind::LParen || nextKind == TokenKind::LBrace) {
                        return true;
                    }
                }
                return false;
            }
        } else if (m_tokens[i].kind == TokenKind::Semicolon || 
                   m_tokens[i].kind == TokenKind::Equal ||
                   m_tokens[i].kind == TokenKind::EndOfFile) {
            return false;
        }
        i++;
    }
    return false;
}

std::string Parser::parseType() {
    if (match(TokenKind::Star)) {
        std::string elemType = parseType();
        if (elemType.empty()) return "";
        return "*" + elemType;
    }
    if (match(TokenKind::Amp)) {
        if (!expect(TokenKind::LBracket, "expected '[' after '&' in slice type")) return "";
        std::string elemType = parseType();
        if (elemType.empty()) return "";
        if (!expect(TokenKind::RBracket, "expected ']' after slice type")) return "";
        return "&[" + elemType + "]";
    }
    if (match(TokenKind::LBracket)) {
        std::string elemType = parseType();
        if (elemType.empty()) return "";
        if (!expect(TokenKind::Semicolon, "expected ';' in array type")) return "";
        const Token* sizeTok = expect(TokenKind::IntLiteral, "expected array size in array type");
        if (!sizeTok) return "";
        if (!expect(TokenKind::RBracket, "expected ']' after array type")) return "";
        return "[" + elemType + "; " + sizeTok->text + "]";
    }
    if (match(TokenKind::KwI32))    return "i32";
    if (match(TokenKind::KwI64))    return "i64";
    if (match(TokenKind::KwF32))    return "f32";
    if (match(TokenKind::KwF64))    return "f64";
    if (match(TokenKind::KwBool))   return "bool";
    if (match(TokenKind::KwString)) return "string";
    if (match(TokenKind::KwVoid))   return "void";
    if (match(TokenKind::KwRc)) {
        if (!expect(TokenKind::Less, "expected '<' after 'Rc'")) return "";
        std::string elemType = parseType();
        if (elemType.empty()) return "";
        if (!expect(TokenKind::Greater, "expected '>' after Rc type argument")) return "";
        return "Rc<" + elemType + ">";
    }
    if (match(TokenKind::KwBox)) {
        if (!expect(TokenKind::Less, "expected '<' after 'Box'")) return "";
        std::string elemType = parseType();
        if (elemType.empty()) return "";
        if (!expect(TokenKind::Greater, "expected '>' after Box type argument")) return "";
        return "Box<" + elemType + ">";
    }
    if (match(TokenKind::KwArena)) {
        return "Arena";
    }
    if (match(TokenKind::KwVec)) {
        if (!expect(TokenKind::Less, "expected '<' after 'Vec'")) return "";
        std::string elemType = parseType();
        if (elemType.empty()) return "";
        if (!expect(TokenKind::Greater, "expected '>' after Vec type argument")) return "";
        return "Vec<" + elemType + ">";
    }
    if (match(TokenKind::Identifier)) {
        std::string name = previous().text;
        if (check(TokenKind::Less)) {
            auto args = parseTypeArgumentList();
            std::string full = name + "<";
            for (size_t i = 0; i < args.size(); ++i) {
                if (i > 0) full += ", ";
                full += args[i];
            }
            full += ">";
            return full;
        }
        return name;
    }

    error(peek().span, "expected type name (e.g. i32, i64, f64, bool, string, void)");
    return "";
}

std::unique_ptr<BlockStmt> Parser::parseBlock() {
    const Token* lbrace = expect(TokenKind::LBrace, "expected '{'");
    if (!lbrace) return nullptr;
    SourceLocation startLoc = lbrace->span.start;

    std::vector<std::unique_ptr<StmtAST>> stmts;
    while (!check(TokenKind::RBrace) && !isAtEnd()) {
        auto stmt = parseStatement();
        if (stmt) {
            stmts.push_back(std::move(stmt));
        } else {
            synchronize();
        }
    }

    const Token* rbrace = expect(TokenKind::RBrace, "expected '}' to close block");
    SourceLocation endLoc = rbrace ? rbrace->span.end : peek().span.end;

    return std::make_unique<BlockStmt>(std::move(stmts), SourceSpan(startLoc, endLoc));
}

std::unique_ptr<StmtAST> Parser::parseStatement() {
    if (match(TokenKind::KwUnsafe)) {
        SourceLocation startLoc = previous().span.start;
        auto block = parseBlock();
        if (!block) return nullptr;
        SourceSpan span(startLoc, block->getSpan().end);
        return std::make_unique<UnsafeBlockStmt>(std::move(block), span);
    }
    if (match(TokenKind::KwLet) || match(TokenKind::KwMut)) {
        return parseVarDecl();
    }
    if (match(TokenKind::KwReturn)) {
        return parseReturn();
    }
    if (match(TokenKind::KwIf)) {
        return parseIf();
    }
    if (match(TokenKind::KwWhile)) {
        return parseWhile();
    }
    if (match(TokenKind::KwForeach)) {
        return parseForeach();
    }
    if (match(TokenKind::KwBreak)) {
        SourceSpan span = previous().span;
        expect(TokenKind::Semicolon, "expected ';' after 'break'");
        return std::make_unique<BreakStmt>(span);
    }
    if (match(TokenKind::KwContinue)) {
        SourceSpan span = previous().span;
        expect(TokenKind::Semicolon, "expected ';' after 'continue'");
        return std::make_unique<ContinueStmt>(span);
    }
    if (match(TokenKind::KwEcho)) {
        return parseEcho();
    }
    if (match(TokenKind::KwTry)) {
        return parseTry();
    }
    if (match(TokenKind::KwThrow)) {
        return parseThrow();
    }
    if (check(TokenKind::LBrace)) {
        return parseBlock();
    }
    return parseExprStatement();
}

std::unique_ptr<VarDeclStmt> Parser::parseVarDecl() {
    bool isMut = previous().is(TokenKind::KwMut);
    SourceLocation startLoc = previous().span.start;

    if (!isMut && match(TokenKind::KwMut)) {
        isMut = true;
    }

    const Token* idTok = expect(TokenKind::Identifier, "expected variable name after 'let' or 'mut'");
    if (!idTok) return nullptr;
    std::string name = idTok->text;

    std::optional<std::string> typeAnnotation;
    if (match(TokenKind::Colon)) {
        std::string type = parseType();
        if (type.empty()) return nullptr;
        typeAnnotation = std::move(type);
    }

    if (!expect(TokenKind::Equal, "expected '=' in variable declaration")) return nullptr;

    auto initExpr = parseExpression();
    if (!initExpr) return nullptr;

    const Token* semi = expect(TokenKind::Semicolon, "expected ';' after variable declaration");
    SourceLocation endLoc = semi ? semi->span.end : initExpr->getSpan().end;

    return std::make_unique<VarDeclStmt>(isMut, std::move(name), std::move(typeAnnotation),
                                         std::move(initExpr), SourceSpan(startLoc, endLoc));
}

std::unique_ptr<ReturnStmt> Parser::parseReturn() {
    SourceLocation startLoc = previous().span.start;

    if (match(TokenKind::Semicolon)) {
        return std::make_unique<ReturnStmt>(nullptr, SourceSpan(startLoc, previous().span.end));
    }

    auto value = parseExpression();
    if (!value) return nullptr;

    const Token* semi = expect(TokenKind::Semicolon, "expected ';' after return value");
    SourceLocation endLoc = semi ? semi->span.end : value->getSpan().end;

    return std::make_unique<ReturnStmt>(std::move(value), SourceSpan(startLoc, endLoc));
}

std::unique_ptr<IfStmt> Parser::parseIf() {
    SourceLocation startLoc = previous().span.start;

    if (!expect(TokenKind::LParen, "expected '(' after 'if'")) return nullptr;
    auto cond = parseExpression();
    if (!cond) return nullptr;
    if (!expect(TokenKind::RParen, "expected ')' after if condition")) return nullptr;

    auto thenBlock = parseBlock();
    if (!thenBlock) return nullptr;

    std::unique_ptr<StmtAST> elseBranch = nullptr;
    if (match(TokenKind::KwElse)) {
        if (match(TokenKind::KwIf)) {
            elseBranch = parseIf();
        } else {
            elseBranch = parseBlock();
        }
        if (!elseBranch) return nullptr;
    }

    SourceLocation endLoc = elseBranch ? elseBranch->getSpan().end : thenBlock->getSpan().end;
    return std::make_unique<IfStmt>(std::move(cond), std::move(thenBlock), std::move(elseBranch),
                                   SourceSpan(startLoc, endLoc));
}

std::unique_ptr<WhileStmt> Parser::parseWhile() {
    SourceLocation startLoc = previous().span.start;

    if (!expect(TokenKind::LParen, "expected '(' after 'while'")) return nullptr;
    auto cond = parseExpression();
    if (!cond) return nullptr;
    if (!expect(TokenKind::RParen, "expected ')' after while condition")) return nullptr;

    auto body = parseBlock();
    if (!body) return nullptr;

    SourceSpan span(startLoc, body->getSpan().end);
    return std::make_unique<WhileStmt>(std::move(cond), std::move(body), span);
}

std::unique_ptr<StmtAST> Parser::parseForeach() {
    SourceLocation startLoc = previous().span.start;

    if (!expect(TokenKind::LParen, "expected '(' after 'foreach'")) return nullptr;
    m_allowAs = false;
    auto collection = parseExpression();
    m_allowAs = true;
    if (!collection) return nullptr;

    if (!expect(TokenKind::KwAs, "expected 'as' in foreach statement")) return nullptr;

    const Token* firstVar = expect(TokenKind::Identifier, "expected variable name after 'as'");
    if (!firstVar) return nullptr;
    std::string firstVarName = firstVar->text;

    std::optional<std::string> keyVar = std::nullopt;
    std::string valVar;

    if (match(TokenKind::DoubleArrow)) {
        keyVar = firstVarName;
        const Token* secondVar = expect(TokenKind::Identifier, "expected value variable name after '=>'");
        if (!secondVar) return nullptr;
        valVar = secondVar->text;
    } else {
        valVar = firstVarName;
    }

    if (!expect(TokenKind::RParen, "expected ')' after foreach variables")) return nullptr;

    auto body = parseBlock();
    if (!body) return nullptr;

    SourceSpan span(startLoc, body->getSpan().end);
    return std::make_unique<ForeachStmt>(std::move(collection), std::move(keyVar), std::move(valVar), std::move(body), span);
}

std::unique_ptr<EchoStmt> Parser::parseEcho() {
    SourceLocation startLoc = previous().span.start;
    std::vector<std::unique_ptr<ExprAST>> exprs;

    do {
        auto expr = parseExpression();
        if (!expr) return nullptr;
        exprs.push_back(std::move(expr));
    } while (match(TokenKind::Comma));

    const Token* semi = expect(TokenKind::Semicolon, "expected ';' after echo statement");
    SourceLocation endLoc = semi ? semi->span.end : exprs.back()->getSpan().end;

    return std::make_unique<EchoStmt>(std::move(exprs), SourceSpan(startLoc, endLoc));
}

std::unique_ptr<ThrowStmt> Parser::parseThrow() {
    SourceLocation startLoc = previous().span.start;
    auto expr = parseExpression();
    if (!expr) return nullptr;
    const Token* semi = expect(TokenKind::Semicolon, "expected ';' after throw statement");
    SourceLocation endLoc = semi ? semi->span.end : expr->getSpan().end;
    return std::make_unique<ThrowStmt>(std::move(expr), SourceSpan(startLoc, endLoc));
}

std::unique_ptr<TryStmt> Parser::parseTry() {
    SourceLocation startLoc = previous().span.start;
    auto tryBlock = parseBlock();
    if (!tryBlock) return nullptr;

    std::vector<CatchClause> catchClauses;
    while (match(TokenKind::KwCatch)) {
        SourceLocation catchStartLoc = previous().span.start;
        if (!expect(TokenKind::LParen, "expected '(' after 'catch'")) return nullptr;

        std::string catchType = "";
        std::string varName = "";

        const Token* firstTok = expect(TokenKind::Identifier, "expected type or variable in catch clause");
        if (!firstTok) return nullptr;

        if (!firstTok->text.empty() && firstTok->text[0] == '$') {
            varName = firstTok->text;
        } else {
            catchType = firstTok->text;
            if (check(TokenKind::Identifier)) {
                varName = advance().text;
            }
        }

        if (!expect(TokenKind::RParen, "expected ')' after catch clause parameter")) return nullptr;

        auto catchBody = parseBlock();
        if (!catchBody) return nullptr;

        SourceSpan catchSpan(catchStartLoc, catchBody->getSpan().end);
        catchClauses.push_back(CatchClause{std::move(catchType), std::move(varName), std::move(catchBody), catchSpan});
    }

    std::unique_ptr<BlockStmt> finallyBlock = nullptr;
    if (match(TokenKind::KwFinally)) {
        finallyBlock = parseBlock();
        if (!finallyBlock) return nullptr;
    }

    if (catchClauses.empty() && !finallyBlock) {
        error(tryBlock->getSpan(), "try statement must have at least one catch clause or a finally block");
        return nullptr;
    }

    SourceLocation endLoc = finallyBlock ? finallyBlock->getSpan().end
                          : (!catchClauses.empty() ? catchClauses.back().body->getSpan().end : tryBlock->getSpan().end);

    return std::make_unique<TryStmt>(std::move(tryBlock), std::move(catchClauses), std::move(finallyBlock), SourceSpan(startLoc, endLoc));
}

std::unique_ptr<ExprStmt> Parser::parseExprStatement() {
    auto expr = parseExpression();
    if (!expr) return nullptr;

    const Token* semi = expect(TokenKind::Semicolon, "expected ';' after expression");
    SourceLocation endLoc = semi ? semi->span.end : expr->getSpan().end;

    return std::make_unique<ExprStmt>(std::move(expr), SourceSpan(expr->getSpan().start, endLoc));
}

std::unique_ptr<ExprAST> Parser::parseExpression() {
    return parseAssignment();
}

std::unique_ptr<ExprAST> Parser::parseAssignment() {
    auto expr = parseLogicalOr();
    if (!expr) return nullptr;

    if (match(TokenKind::Equal)) {
        auto value = parseAssignment();
        if (!value) return nullptr;

        bool isDeref = false;
        if (auto* u = dynamic_cast<UnaryExpr*>(expr.get())) {
            isDeref = (u->getOp() == UnaryOp::Deref);
        }

        if (dynamic_cast<VariableExpr*>(expr.get()) ||
            dynamic_cast<MemberAccessExpr*>(expr.get()) ||
            dynamic_cast<IndexExpr*>(expr.get()) ||
            isDeref) {
            SourceSpan span = expr->getSpan().merge(value->getSpan());
            return std::make_unique<AssignExpr>(std::move(expr), std::move(value), span);
        }

        error(expr->getSpan(), "invalid assignment target; expected variable, struct member, array index, or pointer dereference");
        return nullptr;
    }

    return expr;
}

std::unique_ptr<ExprAST> Parser::parseLogicalOr() {
    auto expr = parseLogicalAnd();
    if (!expr) return nullptr;

    while (match(TokenKind::PipePipe)) {
        auto rhs = parseLogicalAnd();
        if (!rhs) return nullptr;
        SourceSpan span = expr->getSpan().merge(rhs->getSpan());
        expr = std::make_unique<BinaryExpr>(BinaryOp::LogicalOr, std::move(expr), std::move(rhs), span);
    }

    return expr;
}

std::unique_ptr<ExprAST> Parser::parseLogicalAnd() {
    auto expr = parseBitwiseOr();
    if (!expr) return nullptr;

    while (match(TokenKind::AmpAmp)) {
        auto rhs = parseBitwiseOr();
        if (!rhs) return nullptr;
        SourceSpan span = expr->getSpan().merge(rhs->getSpan());
        expr = std::make_unique<BinaryExpr>(BinaryOp::LogicalAnd, std::move(expr), std::move(rhs), span);
    }

    return expr;
}

std::unique_ptr<ExprAST> Parser::parseBitwiseOr() {
    auto expr = parseBitwiseXor();
    if (!expr) return nullptr;

    while (match(TokenKind::Pipe)) {
        auto rhs = parseBitwiseXor();
        if (!rhs) return nullptr;
        SourceSpan span = expr->getSpan().merge(rhs->getSpan());
        expr = std::make_unique<BinaryExpr>(BinaryOp::BitwiseOr, std::move(expr), std::move(rhs), span);
    }

    return expr;
}

std::unique_ptr<ExprAST> Parser::parseBitwiseXor() {
    auto expr = parseBitwiseAnd();
    if (!expr) return nullptr;

    while (match(TokenKind::Caret)) {
        auto rhs = parseBitwiseAnd();
        if (!rhs) return nullptr;
        SourceSpan span = expr->getSpan().merge(rhs->getSpan());
        expr = std::make_unique<BinaryExpr>(BinaryOp::BitwiseXor, std::move(expr), std::move(rhs), span);
    }

    return expr;
}

std::unique_ptr<ExprAST> Parser::parseBitwiseAnd() {
    auto expr = parseEquality();
    if (!expr) return nullptr;

    while (match(TokenKind::Amp)) {
        auto rhs = parseEquality();
        if (!rhs) return nullptr;
        SourceSpan span = expr->getSpan().merge(rhs->getSpan());
        expr = std::make_unique<BinaryExpr>(BinaryOp::BitwiseAnd, std::move(expr), std::move(rhs), span);
    }

    return expr;
}

std::unique_ptr<ExprAST> Parser::parseEquality() {
    auto expr = parseRelational();
    if (!expr) return nullptr;

    while (match(TokenKind::EqualEqual) || match(TokenKind::ExclaimEqual)) {
        BinaryOp op = (previous().kind == TokenKind::EqualEqual) ? BinaryOp::Equal : BinaryOp::NotEqual;
        auto rhs = parseRelational();
        if (!rhs) return nullptr;
        SourceSpan span = expr->getSpan().merge(rhs->getSpan());
        expr = std::make_unique<BinaryExpr>(op, std::move(expr), std::move(rhs), span);
    }

    return expr;
}

std::unique_ptr<ExprAST> Parser::parseRelational() {
    auto expr = parseShift();
    if (!expr) return nullptr;

    while (match(TokenKind::Less) || match(TokenKind::LessEqual) ||
           match(TokenKind::Greater) || match(TokenKind::GreaterEqual)) {
        BinaryOp op;
        switch (previous().kind) {
            case TokenKind::Less:         op = BinaryOp::Less; break;
            case TokenKind::LessEqual:    op = BinaryOp::LessEqual; break;
            case TokenKind::Greater:      op = BinaryOp::Greater; break;
            case TokenKind::GreaterEqual: op = BinaryOp::GreaterEqual; break;
            default:                      op = BinaryOp::Less; break;
        }

        auto rhs = parseShift();
        if (!rhs) return nullptr;
        SourceSpan span = expr->getSpan().merge(rhs->getSpan());
        expr = std::make_unique<BinaryExpr>(op, std::move(expr), std::move(rhs), span);
    }

    return expr;
}

std::unique_ptr<ExprAST> Parser::parseShift() {
    auto expr = parseAdditive();
    if (!expr) return nullptr;

    while (match(TokenKind::LessLess) || match(TokenKind::GreaterGreater)) {
        BinaryOp op = (previous().kind == TokenKind::LessLess) ? BinaryOp::ShiftLeft : BinaryOp::ShiftRight;
        auto rhs = parseAdditive();
        if (!rhs) return nullptr;
        SourceSpan span = expr->getSpan().merge(rhs->getSpan());
        expr = std::make_unique<BinaryExpr>(op, std::move(expr), std::move(rhs), span);
    }

    return expr;
}

std::unique_ptr<ExprAST> Parser::parseAdditive() {
    auto expr = parseMultiplicative();
    if (!expr) return nullptr;

    while (match(TokenKind::Plus) || match(TokenKind::Minus) || match(TokenKind::Dot)) {
        BinaryOp op;
        if (previous().kind == TokenKind::Plus) op = BinaryOp::Add;
        else if (previous().kind == TokenKind::Minus) op = BinaryOp::Sub;
        else op = BinaryOp::Concat;

        auto rhs = parseMultiplicative();
        if (!rhs) return nullptr;
        SourceSpan span = expr->getSpan().merge(rhs->getSpan());
        expr = std::make_unique<BinaryExpr>(op, std::move(expr), std::move(rhs), span);
    }

    return expr;
}

std::unique_ptr<ExprAST> Parser::parseMultiplicative() {
    auto expr = parseCast();
    if (!expr) return nullptr;

    while (match(TokenKind::Star) || match(TokenKind::Slash) || match(TokenKind::Percent)) {
        BinaryOp op;
        switch (previous().kind) {
            case TokenKind::Star:    op = BinaryOp::Mul; break;
            case TokenKind::Slash:   op = BinaryOp::Div; break;
            case TokenKind::Percent: op = BinaryOp::Mod; break;
            default:                 op = BinaryOp::Mul; break;
        }

        auto rhs = parseCast();
        if (!rhs) return nullptr;
        SourceSpan span = expr->getSpan().merge(rhs->getSpan());
        expr = std::make_unique<BinaryExpr>(op, std::move(expr), std::move(rhs), span);
    }

    return expr;
}

std::unique_ptr<ExprAST> Parser::parseCast() {
    auto expr = parseUnary();
    if (!expr) return nullptr;

    while (m_allowAs && match(TokenKind::KwAs)) {
        std::string targetType = parseType();
        if (targetType.empty()) return nullptr;
        SourceLocation endLoc = previous().span.end;
        SourceSpan span(expr->getSpan().start, endLoc);
        expr = std::make_unique<CastExpr>(std::move(expr), std::move(targetType), span);
    }

    return expr;
}

std::unique_ptr<ExprAST> Parser::parseUnary() {
    if (match(TokenKind::KwAwait)) {
        SourceLocation startLoc = previous().span.start;
        auto operand = parseUnary();
        if (!operand) return nullptr;
        SourceSpan span(startLoc, operand->getSpan().end);
        return std::make_unique<AwaitExpr>(std::move(operand), span);
    }

    if (match(TokenKind::Minus) || match(TokenKind::Exclaim) || match(TokenKind::Tilde) ||
        match(TokenKind::Amp) || match(TokenKind::Star)) {
        UnaryOp op;
        TokenKind k = previous().kind;
        if (k == TokenKind::Minus) op = UnaryOp::Negate;
        else if (k == TokenKind::Exclaim) op = UnaryOp::LogicalNot;
        else if (k == TokenKind::Tilde) op = UnaryOp::BitwiseNot;
        else if (k == TokenKind::Amp) op = UnaryOp::AddressOf;
        else op = UnaryOp::Deref;

        SourceLocation startLoc = previous().span.start;
        auto operand = parseUnary();
        if (!operand) return nullptr;
        SourceSpan span(startLoc, operand->getSpan().end);
        return std::make_unique<UnaryExpr>(op, std::move(operand), span);
    }

    return parsePostfix();
}

std::unique_ptr<ExprAST> Parser::parsePostfix() {
    auto expr = parsePrimary();
    if (!expr) return nullptr;

    while (true) {
        if (check(TokenKind::Dot)) {
            bool isMember = false;
            if (m_current + 1 < m_tokens.size()) {
                const Token& nextTok = m_tokens[m_current + 1];
                if (nextTok.kind == TokenKind::Identifier && !nextTok.text.empty() && nextTok.text[0] != '$') {
                    if (!dynamic_cast<StringLiteralExpr*>(expr.get()) &&
                        !dynamic_cast<IntegerLiteralExpr*>(expr.get()) &&
                        !dynamic_cast<FloatLiteralExpr*>(expr.get()) &&
                        !dynamic_cast<BoolLiteralExpr*>(expr.get()) &&
                        !dynamic_cast<ArrayLiteralExpr*>(expr.get())) {
                        isMember = true;
                    }
                }
            }

            if (!isMember) {
                break;
            }

            advance(); // consume '.'
            const Token* memberTok = expect(TokenKind::Identifier, "expected member name after '.'");
            if (!memberTok) return nullptr;
            if (match(TokenKind::LParen)) {
                // Method call: expr.method(args...)
                std::vector<std::unique_ptr<ExprAST>> args;
                if (!check(TokenKind::RParen)) {
                    do {
                        auto arg = parseExpression();
                        if (!arg) return nullptr;
                        args.push_back(std::move(arg));
                    } while (match(TokenKind::Comma));
                }
                const Token* rparen = expect(TokenKind::RParen, "expected ')' after arguments");
                SourceLocation endLoc = rparen ? rparen->span.end : (args.empty() ? memberTok->span.end : args.back()->getSpan().end);
                SourceSpan span(expr->getSpan().start, endLoc);
                expr = std::make_unique<MethodCallExpr>(std::move(expr), memberTok->text, std::move(args), span);
            } else {
                SourceSpan span(expr->getSpan().start, memberTok->span.end);
                expr = std::make_unique<MemberAccessExpr>(std::move(expr), memberTok->text, span);
            }
        } else if (check(TokenKind::Arrow)) {
            advance(); // consume '->'
            const Token* memberTok = expect(TokenKind::Identifier, "expected property or method name after '->'");
            if (!memberTok) return nullptr;
            if (match(TokenKind::LParen)) {
                // Method call: expr->method(args...)
                std::vector<std::unique_ptr<ExprAST>> args;
                if (!check(TokenKind::RParen)) {
                    do {
                        auto arg = parseExpression();
                        if (!arg) return nullptr;
                        args.push_back(std::move(arg));
                    } while (match(TokenKind::Comma));
                }
                const Token* rparen = expect(TokenKind::RParen, "expected ')' after arguments");
                SourceLocation endLoc = rparen ? rparen->span.end : (args.empty() ? memberTok->span.end : args.back()->getSpan().end);
                SourceSpan span(expr->getSpan().start, endLoc);
                expr = std::make_unique<MethodCallExpr>(std::move(expr), memberTok->text, std::move(args), span);
            } else {
                SourceSpan span(expr->getSpan().start, memberTok->span.end);
                expr = std::make_unique<MemberAccessExpr>(std::move(expr), memberTok->text, span);
            }
        } else if (match(TokenKind::LBracket)) {
            if (match(TokenKind::RBracket)) {
                // Empty index: arr[] (used for array append in PHP)
                SourceLocation endLoc = previous().span.end;
                SourceSpan span(expr->getSpan().start, endLoc);
                expr = std::make_unique<IndexExpr>(std::move(expr), nullptr, span);
            } else if (match(TokenKind::DotDot)) {
                // Slicing: arr[..] or arr[..end]
                SourceLocation dotdotLoc = previous().span.start;
                std::unique_ptr<ExprAST> endExpr = nullptr;
                if (!check(TokenKind::RBracket)) {
                    endExpr = parseExpression();
                    if (!endExpr) return nullptr;
                }
                const Token* rbracket = expect(TokenKind::RBracket, "expected ']' after slice range");
                SourceLocation endLoc = rbracket ? rbracket->span.end : (endExpr ? endExpr->getSpan().end : dotdotLoc);
                auto rangeSpan = SourceSpan(dotdotLoc, endLoc);
                auto rangeExpr = std::make_unique<RangeExpr>(nullptr, std::move(endExpr), rangeSpan);
                SourceSpan span(expr->getSpan().start, endLoc);
                expr = std::make_unique<SliceExpr>(std::move(expr), std::move(rangeExpr), span);
            } else {
                auto firstExpr = parseExpression();
                if (!firstExpr) return nullptr;
                if (match(TokenKind::DotDot)) {
                    // Slicing: arr[start..] or arr[start..end]
                    SourceLocation startLoc = firstExpr->getSpan().start;
                    std::unique_ptr<ExprAST> endExpr = nullptr;
                    if (!check(TokenKind::RBracket)) {
                        endExpr = parseExpression();
                        if (!endExpr) return nullptr;
                    }
                    const Token* rbracket = expect(TokenKind::RBracket, "expected ']' after slice range");
                    SourceLocation endLoc = rbracket ? rbracket->span.end : (endExpr ? endExpr->getSpan().end : previous().span.end);
                    auto rangeSpan = SourceSpan(startLoc, endLoc);
                    auto rangeExpr = std::make_unique<RangeExpr>(std::move(firstExpr), std::move(endExpr), rangeSpan);
                    SourceSpan span(expr->getSpan().start, endLoc);
                    expr = std::make_unique<SliceExpr>(std::move(expr), std::move(rangeExpr), span);
                } else {
                    // Normal indexing: arr[index]
                    const Token* rbracket = expect(TokenKind::RBracket, "expected ']' after array index");
                    SourceLocation endLoc = rbracket ? rbracket->span.end : firstExpr->getSpan().end;
                    SourceSpan span(expr->getSpan().start, endLoc);
                    expr = std::make_unique<IndexExpr>(std::move(expr), std::move(firstExpr), span);
                }
            }
        } else {
            break;
        }
    }

    return expr;
}

std::unique_ptr<ExprAST> Parser::parsePrimary() {
    if (match(TokenKind::LBracket)) {
        SourceLocation startLoc = previous().span.start;
        std::vector<ArrayLiteralExpr::Element> elements;
        if (!check(TokenKind::RBracket)) {
            while (!check(TokenKind::RBracket) && !isAtEnd()) {
                auto first = parseExpression();
                if (!first) return nullptr;
                if (match(TokenKind::DoubleArrow)) {
                    auto val = parseExpression();
                    if (!val) return nullptr;
                    elements.push_back({std::move(first), std::move(val)});
                } else {
                    elements.push_back({nullptr, std::move(first)});
                }
                if (!match(TokenKind::Comma)) break;
            }
        }
        const Token* rbracket = expect(TokenKind::RBracket, "expected ']' after array elements");
        SourceLocation endLoc = rbracket ? rbracket->span.end : (elements.empty() ? startLoc : elements.back().value->getSpan().end);
        return std::make_unique<ArrayLiteralExpr>(std::move(elements), SourceSpan(startLoc, endLoc));
    }

    if (match(TokenKind::IntLiteral)) {
        const Token& tok = previous();
        int64_t val = parseInteger(tok.text);
        return std::make_unique<IntegerLiteralExpr>(val, tok.text, tok.span);
    }

    if (match(TokenKind::FloatLiteral)) {
        const Token& tok = previous();
        double val = parseFloat(tok.text);
        return std::make_unique<FloatLiteralExpr>(val, tok.text, tok.span);
    }

    if (match(TokenKind::StringLiteral)) {
        const Token& tok = previous();
        return std::make_unique<StringLiteralExpr>(tok.text, tok.span);
    }

    if (match(TokenKind::KwTrue)) {
        return std::make_unique<BoolLiteralExpr>(true, previous().span);
    }

    if (match(TokenKind::KwFalse)) {
        return std::make_unique<BoolLiteralExpr>(false, previous().span);
    }

    if (match(TokenKind::KwThis)) {
        return std::make_unique<ThisExpr>(previous().span);
    }

    if (match(TokenKind::KwNew)) {
        SourceLocation startLoc = previous().span.start;
        const Token* classTok = expect(TokenKind::Identifier, "expected class name after 'new'");
        if (!classTok) return nullptr;
        std::string className = classTok->text;
        if (check(TokenKind::Less)) {
            auto typeArgs = parseTypeArgumentList();
            className += "<";
            for (size_t i = 0; i < typeArgs.size(); ++i) {
                if (i > 0) className += ", ";
                className += typeArgs[i];
            }
            className += ">";
        }
        std::vector<std::unique_ptr<ExprAST>> args;
        SourceLocation endLoc = previous().span.end;
        if (match(TokenKind::LParen)) {
            if (!check(TokenKind::RParen)) {
                do {
                    auto arg = parseExpression();
                    if (!arg) return nullptr;
                    args.push_back(std::move(arg));
                } while (match(TokenKind::Comma));
            }
            const Token* rparen = expect(TokenKind::RParen, "expected ')' after class constructor arguments");
            if (rparen) endLoc = rparen->span.end;
        }
        return std::make_unique<NewExpr>(std::move(className), std::move(args), SourceSpan(startLoc, endLoc));
    }

    if (match(TokenKind::LParen)) {
        SourceLocation startLoc = previous().span.start;
        auto expr = parseExpression();
        if (!expr) return nullptr;
        const Token* rparen = expect(TokenKind::RParen, "expected ')' after expression");
        SourceLocation endLoc = rparen ? rparen->span.end : expr->getSpan().end;
        expr->setSpan(SourceSpan(startLoc, endLoc));
        return expr;
    }

    if (match(TokenKind::KwRc)) {
        SourceLocation startLoc = previous().span.start;
        if (!expect(TokenKind::ColonColon, "expected '::' after 'Rc'")) return nullptr;
        const Token* memberTok = nullptr;
        if (check(TokenKind::KwNew) || check(TokenKind::Identifier)) {
            memberTok = &advance();
        } else {
            error(peek().span, "expected 'new' or 'count' after 'Rc::'");
            return nullptr;
        }

        if (memberTok->text == "new") {
            if (!expect(TokenKind::LParen, "expected '(' after 'Rc::new'")) return nullptr;
            auto val = parseExpression();
            if (!val) return nullptr;
            const Token* rparen = expect(TokenKind::RParen, "expected ')' after Rc::new argument");
            SourceLocation endLoc = rparen ? rparen->span.end : val->getSpan().end;
            return std::make_unique<RcNewExpr>(std::move(val), SourceSpan(startLoc, endLoc));
        } else if (memberTok->text == "count") {
            if (!expect(TokenKind::LParen, "expected '(' after 'Rc::count'")) return nullptr;
            auto ptr = parseExpression();
            if (!ptr) return nullptr;
            const Token* rparen = expect(TokenKind::RParen, "expected ')' after Rc::count argument");
            SourceLocation endLoc = rparen ? rparen->span.end : ptr->getSpan().end;
            return std::make_unique<RcCountExpr>(std::move(ptr), SourceSpan(startLoc, endLoc));
        } else {
            error(memberTok->span, "unknown Rc function '" + memberTok->text + "', expected 'new' or 'count'");
            return nullptr;
        }
    }

    if (match(TokenKind::KwBox)) {
        SourceLocation startLoc = previous().span.start;
        if (!expect(TokenKind::ColonColon, "expected '::' after 'Box'")) return nullptr;
        const Token* memberTok = nullptr;
        if (check(TokenKind::KwNew) || check(TokenKind::Identifier)) {
            memberTok = &advance();
        } else {
            error(peek().span, "expected 'new' after 'Box::'");
            return nullptr;
        }

        if (memberTok->text == "new") {
            if (!expect(TokenKind::LParen, "expected '(' after 'Box::new'")) return nullptr;
            auto val = parseExpression();
            if (!val) return nullptr;
            const Token* rparen = expect(TokenKind::RParen, "expected ')' after Box::new argument");
            SourceLocation endLoc = rparen ? rparen->span.end : val->getSpan().end;
            return std::make_unique<BoxNewExpr>(std::move(val), SourceSpan(startLoc, endLoc));
        } else {
            error(memberTok->span, "unknown Box function '" + memberTok->text + "', expected 'new'");
            return nullptr;
        }
    }

    if (match(TokenKind::KwArena)) {
        SourceLocation startLoc = previous().span.start;
        if (!expect(TokenKind::ColonColon, "expected '::' after 'Arena'")) return nullptr;
        const Token* memberTok = nullptr;
        if (check(TokenKind::KwNew) || check(TokenKind::Identifier)) {
            memberTok = &advance();
        } else {
            error(peek().span, "expected 'new', 'alloc', 'reset', or 'destroy' after 'Arena::'");
            return nullptr;
        }

        if (memberTok->text == "new") {
            if (!expect(TokenKind::LParen, "expected '(' after 'Arena::new'")) return nullptr;
            std::unique_ptr<ExprAST> cap = nullptr;
            if (!check(TokenKind::RParen)) {
                cap = parseExpression();
                if (!cap) return nullptr;
            }
            const Token* rparen = expect(TokenKind::RParen, "expected ')' after Arena::new");
            SourceLocation endLoc = rparen ? rparen->span.end : (cap ? cap->getSpan().end : startLoc);
            return std::make_unique<ArenaNewExpr>(std::move(cap), SourceSpan(startLoc, endLoc));
        } else if (memberTok->text == "alloc") {
            if (!expect(TokenKind::LParen, "expected '(' after 'Arena::alloc'")) return nullptr;
            auto arena = parseExpression();
            if (!arena) return nullptr;
            if (!expect(TokenKind::Comma, "expected ',' between arena and value in 'Arena::alloc'")) return nullptr;
            auto val = parseExpression();
            if (!val) return nullptr;
            const Token* rparen = expect(TokenKind::RParen, "expected ')' after Arena::alloc arguments");
            SourceLocation endLoc = rparen ? rparen->span.end : val->getSpan().end;
            return std::make_unique<ArenaAllocExpr>(std::move(arena), std::move(val), SourceSpan(startLoc, endLoc));
        } else if (memberTok->text == "reset") {
            if (!expect(TokenKind::LParen, "expected '(' after 'Arena::reset'")) return nullptr;
            auto arena = parseExpression();
            if (!arena) return nullptr;
            const Token* rparen = expect(TokenKind::RParen, "expected ')' after Arena::reset argument");
            SourceLocation endLoc = rparen ? rparen->span.end : arena->getSpan().end;
            return std::make_unique<ArenaResetExpr>(std::move(arena), SourceSpan(startLoc, endLoc));
        } else if (memberTok->text == "destroy") {
            if (!expect(TokenKind::LParen, "expected '(' after 'Arena::destroy'")) return nullptr;
            auto arena = parseExpression();
            if (!arena) return nullptr;
            const Token* rparen = expect(TokenKind::RParen, "expected ')' after Arena::destroy argument");
            SourceLocation endLoc = rparen ? rparen->span.end : arena->getSpan().end;
            return std::make_unique<ArenaDestroyExpr>(std::move(arena), SourceSpan(startLoc, endLoc));
        } else {
            error(memberTok->span, "unknown Arena function '" + memberTok->text + "', expected 'new', 'alloc', 'reset', or 'destroy'");
            return nullptr;
        }
    }

    if (match(TokenKind::KwVec)) {
        SourceLocation startLoc = previous().span.start;
        std::string elemType = "";
        if (match(TokenKind::Less)) {
            elemType = parseType();
            if (!expect(TokenKind::Greater, "expected '>' after Vec type parameter")) return nullptr;
        }
        if (!expect(TokenKind::ColonColon, "expected '::' after 'Vec'")) return nullptr;
        const Token* memberTok = nullptr;
        if (check(TokenKind::KwNew) || check(TokenKind::Identifier)) {
            memberTok = &advance();
        } else {
            error(peek().span, "expected 'new' or 'with_capacity' after 'Vec::'");
            return nullptr;
        }

        if (memberTok->text == "new") {
            if (!expect(TokenKind::LParen, "expected '(' after 'Vec::new'")) return nullptr;
            const Token* rparen = expect(TokenKind::RParen, "expected ')' after 'Vec::new'");
            SourceLocation endLoc = rparen ? rparen->span.end : memberTok->span.end;
            return std::make_unique<VecNewExpr>(std::move(elemType), nullptr, SourceSpan(startLoc, endLoc));
        } else if (memberTok->text == "with_capacity") {
            if (!expect(TokenKind::LParen, "expected '(' after 'Vec::with_capacity'")) return nullptr;
            auto cap = parseExpression();
            if (!cap) return nullptr;
            const Token* rparen = expect(TokenKind::RParen, "expected ')' after Vec::with_capacity argument");
            SourceLocation endLoc = rparen ? rparen->span.end : cap->getSpan().end;
            return std::make_unique<VecNewExpr>(std::move(elemType), std::move(cap), SourceSpan(startLoc, endLoc));
        } else {
            error(memberTok->span, "unknown Vec function '" + memberTok->text + "', expected 'new' or 'with_capacity'");
            return nullptr;
        }
    }

    if (match(TokenKind::Identifier)) {
        const Token idTok = previous();

        std::vector<std::string> typeArgs;
        if (peek().is(TokenKind::Less) && isGenericCallOrStructInit()) {
            typeArgs = parseTypeArgumentList();
        }

        // Check if function call
        if (match(TokenKind::LParen)) {
            std::vector<std::unique_ptr<ExprAST>> args;
            if (!check(TokenKind::RParen)) {
                do {
                    auto arg = parseExpression();
                    if (!arg) return nullptr;
                    args.push_back(std::move(arg));
                } while (match(TokenKind::Comma));
            }

            const Token* rparen = expect(TokenKind::RParen, "expected ')' after arguments");
            SourceLocation endLoc = rparen ? rparen->span.end : (args.empty() ? idTok.span.end : args.back()->getSpan().end);
            return std::make_unique<CallExpr>(idTok.text, std::move(typeArgs), std::move(args), SourceSpan(idTok.span.start, endLoc));
        }

        // Check if struct initialization: e.g. Point { x: 1, y: 2 } or Pair<i32, i32> { first: 10, second: 20 }
        if (match(TokenKind::LBrace)) {
            std::vector<std::pair<std::string, std::unique_ptr<ExprAST>>> fields;
            if (!check(TokenKind::RBrace)) {
                do {
                    const Token* fieldNameTok = expect(TokenKind::Identifier, "expected field name in struct initialization");
                    if (!fieldNameTok) return nullptr;
                    if (!expect(TokenKind::Colon, "expected ':' after field name")) return nullptr;
                    auto fieldVal = parseExpression();
                    if (!fieldVal) return nullptr;
                    fields.emplace_back(fieldNameTok->text, std::move(fieldVal));
                    if (!match(TokenKind::Comma)) {
                        match(TokenKind::Semicolon);
                    }
                } while (!check(TokenKind::RBrace) && !isAtEnd());
            }

            const Token* rbrace = expect(TokenKind::RBrace, "expected '}' after struct initialization");
            SourceLocation endLoc = rbrace ? rbrace->span.end : (fields.empty() ? idTok.span.end : fields.back().second->getSpan().end);
            return std::make_unique<StructInitExpr>(idTok.text, std::move(typeArgs), std::move(fields), SourceSpan(idTok.span.start, endLoc));
        }

        // Check if scoped access or static call: e.g. Status::Active or File::read(...)
        if (match(TokenKind::ColonColon)) {
            const Token* memberTok = expect(TokenKind::Identifier, "expected member name after '::'");
            if (!memberTok) return nullptr;

            std::vector<std::string> memberTypeArgs;
            if (peek().is(TokenKind::Less) && isGenericCallOrStructInit()) {
                memberTypeArgs = parseTypeArgumentList();
            }

            if (match(TokenKind::LParen)) {
                std::vector<std::unique_ptr<ExprAST>> args;
                if (!check(TokenKind::RParen)) {
                    do {
                        auto arg = parseExpression();
                        if (!arg) return nullptr;
                        args.push_back(std::move(arg));
                    } while (match(TokenKind::Comma));
                }
                const Token* rparen = expect(TokenKind::RParen, "expected ')' after arguments");
                SourceLocation endLoc = rparen ? rparen->span.end : (args.empty() ? memberTok->span.end : args.back()->getSpan().end);
                return std::make_unique<CallExpr>(idTok.text + "::" + memberTok->text, std::move(memberTypeArgs), std::move(args), SourceSpan(idTok.span.start, endLoc));
            }

            SourceLocation endLoc = memberTok->span.end;
            return std::make_unique<ScopedAccessExpr>(idTok.text, memberTok->text, SourceSpan(idTok.span.start, endLoc));
        }

        return std::make_unique<VariableExpr>(idTok.text, idTok.span);
    }

    error(peek().span, "expected expression");
    return nullptr;
}

} // namespace sph
