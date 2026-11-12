#include "LspServer.hpp"
#include <cctype>
#include <iostream>
#include <sstream>

namespace sph::lsp {

LspServer::LspServer() = default;

int LspServer::run(std::istream& in, std::ostream& out) {
    while (in && !m_exitRequested) {
        std::string line;
        size_t contentLength = 0;

        while (std::getline(in, line)) {
            if (!line.empty() && line.back() == '\r') {
                line.pop_back();
            }
            if (line.empty()) {
                break;
            }
            if (line.rfind("Content-Length:", 0) == 0) {
                std::string lenStr = line.substr(15);
                while (!lenStr.empty() && (lenStr.front() == ' ' || lenStr.front() == '\t')) {
                    lenStr.erase(0, 1);
                }
                try {
                    contentLength = std::stoul(lenStr);
                } catch (...) {
                    contentLength = 0;
                }
            }
        }

        if (!in || contentLength == 0) {
            break;
        }

        std::string payload(contentLength, '\0');
        in.read(&payload[0], contentLength);
        if (!in) {
            break;
        }

        std::string response = handleMessage(payload, &out);
        if (!response.empty()) {
            sendFramedMessage(out, response);
        }

        if (m_exitRequested) {
            break;
        }
    }

    return m_shutdown ? 0 : 1;
}

std::string LspServer::handleMessage(const std::string& rawJson, std::ostream* out) {
    JsonValue msg = JsonValue::parse(rawJson);
    if (!msg.isObject()) {
        return "";
    }

    std::string method = msg["method"].asString();
    JsonValue id = msg["id"];
    JsonValue params = msg["params"];

    if (method == "initialize") {
        return formatResponse(id, handleInitialize(id, params));
    }
    if (method == "initialized") {
        handleInitialized(params);
        return "";
    }
    if (method == "shutdown") {
        return formatResponse(id, handleShutdown(id));
    }
    if (method == "exit") {
        handleExit();
        return "";
    }
    if (method == "textDocument/didOpen") {
        handleDidOpen(params, out);
        return "";
    }
    if (method == "textDocument/didChange") {
        handleDidChange(params, out);
        return "";
    }
    if (method == "textDocument/didClose") {
        handleDidClose(params, out);
        return "";
    }
    if (method == "textDocument/hover") {
        return formatResponse(id, handleHover(id, params));
    }
    if (method == "textDocument/completion") {
        return formatResponse(id, handleCompletion(id, params));
    }
    if (method == "textDocument/definition") {
        return formatResponse(id, handleDefinition(id, params));
    }
    if (method == "textDocument/documentSymbol") {
        return formatResponse(id, handleDocumentSymbol(id, params));
    }

    // Unhandled request with ID -> MethodNotFound
    if (!id.isNull()) {
        return formatError(id, -32601, "Method not found: " + method);
    }

    return "";
}

JsonValue LspServer::handleInitialize(const JsonValue& /*id*/, const JsonValue& /*params*/) {
    m_initialized = true;

    JsonValue::Array triggerChars = {
        JsonValue("."), JsonValue(":"), JsonValue(">"), JsonValue("$")
    };

    JsonValue capabilities = JsonValue::Object{
        {"textDocumentSync", JsonValue(1)}, // Full sync
        {"hoverProvider", JsonValue(true)},
        {"completionProvider", JsonValue::Object{
            {"resolveProvider", JsonValue(false)},
            {"triggerCharacters", JsonValue(triggerChars)}
        }},
        {"definitionProvider", JsonValue(true)},
        {"documentSymbolProvider", JsonValue(true)}
    };

    JsonValue serverInfo = JsonValue::Object{
        {"name", JsonValue("sphc-lsp")},
        {"version", JsonValue("0.1.0")}
    };

    return JsonValue::Object{
        {"capabilities", std::move(capabilities)},
        {"serverInfo", std::move(serverInfo)}
    };
}

void LspServer::handleInitialized(const JsonValue& /*params*/) {
    m_initialized = true;
}

JsonValue LspServer::handleShutdown(const JsonValue& /*id*/) {
    m_shutdown = true;
    return JsonValue(); // null
}

void LspServer::handleExit() {
    m_exitRequested = true;
}

void LspServer::handleDidOpen(const JsonValue& params, std::ostream* out) {
    const auto& doc = params["textDocument"];
    std::string uri = doc["uri"].asString();
    std::string text = doc["text"].asString();
    int version = static_cast<int>(doc["version"].asInt(1));

    m_documents[uri] = DocumentState{uri, text, version};
    publishDiagnostics(uri, text, out);
}

void LspServer::handleDidChange(const JsonValue& params, std::ostream* out) {
    const auto& doc = params["textDocument"];
    std::string uri = doc["uri"].asString();
    int version = static_cast<int>(doc["version"].asInt(0));

    const auto& changes = params["contentChanges"].asArray();
    if (!changes.empty()) {
        // Full text synchronization
        std::string newText = changes.back()["text"].asString();
        m_documents[uri] = DocumentState{uri, newText, version};
        publishDiagnostics(uri, newText, out);
    }
}

void LspServer::handleDidClose(const JsonValue& params, std::ostream* out) {
    const auto& doc = params["textDocument"];
    std::string uri = doc["uri"].asString();
    m_documents.erase(uri);

    // Clear diagnostics on client
    if (out) {
        JsonValue emptyDiag = JsonValue::Object{
            {"uri", JsonValue(uri)},
            {"diagnostics", JsonValue(JsonValue::Array{})}
        };
        sendFramedMessage(*out, formatNotification("textDocument/publishDiagnostics", emptyDiag));
    }
}

void LspServer::publishDiagnostics(const std::string& uri, const std::string& text, std::ostream* out) {
    if (!out) return;

    SourceManager sm;
    uint32_t fileId = sm.addSource(uri, text);
    DiagnosticEmitter emitter(sm, nullptr); // Do not output to std::cerr, capture into diagnostics vector

    Parser parser(fileId, sm, &emitter);
    auto program = parser.parseProgram();

    if (program) {
        SemanticAnalyzer analyzer(emitter);
        analyzer.analyze(*program);
    }

    JsonValue::Array diagArray;
    for (const auto& diag : emitter.getDiagnostics()) {
        int startLine = diag.span.start.line > 0 ? static_cast<int>(diag.span.start.line - 1) : 0;
        int startCol = diag.span.start.column > 0 ? static_cast<int>(diag.span.start.column - 1) : 0;
        int endLine = diag.span.end.line > 0 ? static_cast<int>(diag.span.end.line - 1) : startLine;
        int endCol = diag.span.end.column > 0 ? static_cast<int>(diag.span.end.column - 1) : (startCol + 1);

        if (endCol <= startCol && endLine == startLine) {
            endCol = startCol + 1;
        }

        int severity = 1; // 1: Error, 2: Warning, 3: Information
        if (diag.severity == DiagnosticSeverity::Warning) {
            severity = 2;
        } else if (diag.severity == DiagnosticSeverity::Note) {
            severity = 3;
        }

        JsonValue d = JsonValue::Object{
            {"range", JsonValue::Object{
                {"start", JsonValue::Object{{"line", JsonValue(startLine)}, {"character", JsonValue(startCol)}}},
                {"end", JsonValue::Object{{"line", JsonValue(endLine)}, {"character", JsonValue(endCol)}}}
            }},
            {"severity", JsonValue(severity)},
            {"source", JsonValue("sphc")},
            {"message", JsonValue(diag.message)}
        };
        diagArray.push_back(std::move(d));
    }

    JsonValue notificationParams = JsonValue::Object{
        {"uri", JsonValue(uri)},
        {"diagnostics", JsonValue(std::move(diagArray))}
    };

    sendFramedMessage(*out, formatNotification("textDocument/publishDiagnostics", notificationParams));
}

std::string LspServer::getWordAtPosition(const std::string& text, int line, int character) {
    std::string lineStr = getLine(text, line);
    if (lineStr.empty() || character < 0 || character >= static_cast<int>(lineStr.size())) {
        return "";
    }

    int start = character;
    while (start > 0 && (std::isalnum(static_cast<unsigned char>(lineStr[start - 1])) || lineStr[start - 1] == '_' || lineStr[start - 1] == '$')) {
        --start;
    }

    int end = character;
    while (end < static_cast<int>(lineStr.size()) &&
           (std::isalnum(static_cast<unsigned char>(lineStr[end])) || lineStr[end] == '_' || lineStr[end] == '$')) {
        ++end;
    }

    if (start >= end) return "";
    return lineStr.substr(start, end - start);
}

std::string LspServer::getLine(const std::string& text, int targetLine) {
    int currentLine = 0;
    size_t start = 0;
    while (start < text.size()) {
        size_t end = text.find('\n', start);
        if (end == std::string::npos) {
            end = text.size();
        }
        if (currentLine == targetLine) {
            std::string l = text.substr(start, end - start);
            if (!l.empty() && l.back() == '\r') l.pop_back();
            return l;
        }
        currentLine++;
        start = end + 1;
    }
    return "";
}

JsonValue LspServer::handleHover(const JsonValue& /*id*/, const JsonValue& params) {
    std::string uri = params["textDocument"]["uri"].asString();
    int line = static_cast<int>(params["position"]["line"].asInt(0));
    int character = static_cast<int>(params["position"]["character"].asInt(0));

    auto it = m_documents.find(uri);
    if (it == m_documents.end()) {
        return JsonValue(); // null
    }

    std::string word = getWordAtPosition(it->second.text, line, character);
    if (word.empty()) {
        return JsonValue();
    }

    std::string markdown;

    // 1. Language keywords & syntax
    if (word == "fn") {
        markdown = "```santientphp\nfn <name>(<params>): <type>\n```\nDeclares a function in SantientPHP.";
    } else if (word == "let") {
        markdown = "```santientphp\nlet <name>: <type> = <value>;\n```\nDeclares an immutable variable.";
    } else if (word == "mut") {
        markdown = "```santientphp\nlet mut <name>: <type> = <value>;\n```\nDeclares a mutable variable.";
    } else if (word == "async") {
        markdown = "```santientphp\nasync fn <name>(<params>): Promise<T>\n```\nDeclares an asynchronous function executing as a cooperative coroutine.";
    } else if (word == "await") {
        markdown = "```santientphp\nawait <promise_expr>\n```\nSuspends coroutine execution until the target Promise is resolved.";
    } else if (word == "dynamic") {
        markdown = "```santientphp\ndynamic\n```\nPHP compatibility dynamic value type supporting loose coercion, dynamic properties, and associative arrays.";
    } else if (word == "class") {
        markdown = "```santientphp\nclass <Name> [implements <Interface>]\n```\nDeclares an object-oriented class.";
    } else if (word == "interface") {
        markdown = "```santientphp\ninterface <Name>\n```\nDeclares an interface contract.";
    } else if (word == "struct") {
        markdown = "```santientphp\nstruct <Name>\n```\nDeclares a memory-layout value-type struct.";
    } else if (word == "enum") {
        markdown = "```santientphp\nenum <Name>\n```\nDeclares an enumerated type.";
    } else if (word == "foreach") {
        markdown = "```santientphp\nforeach ($arr as $key => $val)\n```\nPHP-style associative and sequential collection iteration.";
    } else if (word == "match") {
        markdown = "```santientphp\nmatch (<expr>) { <pattern> => <result>, ... }\n```\nExhaustive pattern matching expression.";
    } else if (word == "f32x4" || word == "i32x4" || word == "f64x2" || word == "i64x2") {
        markdown = "```santientphp\n" + word + "\n```\n128-bit SIMD vector type accelerated with SSE/AVX CPU instructions.";
    } else if (word == "AtomicI32" || word == "AtomicI64" || word == "AtomicBool") {
        markdown = "```santientphp\n" + word + "\n```\nThread-safe hardware atomic concurrency primitive.";
    } else if (word == "println" || word == "print") {
        markdown = "```santientphp\nfn " + word + "(val: any): void\n```\nOutputs formatted text to standard output.";
    } else if (word == "echo") {
        markdown = "```santientphp\necho <expr>, ...;\n```\nPHP-compatible output statement.";
    } else if (word == "var_dump") {
        markdown = "```santientphp\nfn var_dump(val: any): void\n```\nPHP-compatible detailed value inspection and debugging dump.";
    } else {
        // 2. Check AST for declared symbols
        SourceManager sm;
        uint32_t fileId = sm.addSource(uri, it->second.text);
        DiagnosticEmitter emitter(sm, nullptr);
        Parser parser(fileId, sm, &emitter);
        auto program = parser.parseProgram();

        if (program) {
            for (const auto& fn : program->getFunctions()) {
                if (fn->getName() == word) {
                    std::ostringstream sig;
                    sig << "```santientphp\n";
                    if (fn->isAsync()) sig << "async ";
                    sig << "fn " << fn->getName() << "(";
                    const auto& params = fn->getParams();
                    for (size_t p = 0; p < params.size(); ++p) {
                        if (p > 0) sig << ", ";
                        if (params[p].isByRef) sig << "&";
                        sig << params[p].name << ": " << params[p].type;
                    }
                    sig << "): " << fn->getReturnType() << "\n```";
                    markdown = sig.str();
                    break;
                }
            }
            if (markdown.empty()) {
                for (const auto& st : program->getStructs()) {
                    if (st->getName() == word) {
                        std::ostringstream sig;
                        sig << "```santientphp\nstruct " << st->getName() << " {\n";
                        for (const auto& f : st->getFields()) {
                            sig << "    " << f.name << ": " << f.type << ",\n";
                        }
                        sig << "}\n```";
                        markdown = sig.str();
                        break;
                    }
                }
            }
            if (markdown.empty()) {
                for (const auto& cl : program->getClasses()) {
                    if (cl->getName() == word) {
                        markdown = "```santientphp\nclass " + cl->getName() + "\n```";
                        break;
                    }
                }
            }
            if (markdown.empty()) {
                for (const auto& en : program->getEnums()) {
                    if (en->getName() == word) {
                        markdown = "```santientphp\nenum " + en->getName() + "\n```";
                        break;
                    }
                }
            }
        }
    }

    if (markdown.empty()) {
        return JsonValue(); // null
    }

    return JsonValue::Object{
        {"contents", JsonValue::Object{
            {"kind", JsonValue("markdown")},
            {"value", JsonValue(markdown)}
        }}
    };
}

JsonValue LspServer::handleCompletion(const JsonValue& /*id*/, const JsonValue& params) {
    std::string uri = params["textDocument"]["uri"].asString();

    JsonValue::Array items;

    // Helper lambda to add completion
    auto addItem = [&](const std::string& label, int kind, const std::string& detail, const std::string& doc = "") {
        JsonValue::Object it{
            {"label", JsonValue(label)},
            {"kind", JsonValue(kind)},
            {"detail", JsonValue(detail)}
        };
        if (!doc.empty()) {
            it["documentation"] = JsonValue(doc);
        }
        items.push_back(JsonValue(std::move(it)));
    };

    // 1. Language Keywords (Kind 14: Keyword)
    const std::vector<std::pair<std::string, std::string>> keywords = {
        {"fn", "Function declaration"},
        {"let", "Immutable variable declaration"},
        {"mut", "Mutable variable modifier"},
        {"class", "Class declaration"},
        {"interface", "Interface contract declaration"},
        {"struct", "Value-type struct declaration"},
        {"enum", "Enumerated type declaration"},
        {"return", "Return statement"},
        {"if", "Conditional branch"},
        {"else", "Alternative conditional branch"},
        {"while", "While loop"},
        {"for", "For loop"},
        {"foreach", "PHP collection loop"},
        {"match", "Pattern matching expression"},
        {"try", "Exception try block"},
        {"catch", "Exception handler block"},
        {"finally", "Exception cleanup block"},
        {"throw", "Exception throw statement"},
        {"new", "Object instantiation"},
        {"async", "Asynchronous coroutine declaration"},
        {"await", "Coroutine suspension operator"},
        {"unsafe", "Unsafe systems block"},
        {"pub", "Public visibility modifier"},
        {"use", "Namespace / trait import"},
        {"const", "Constant declaration"}
    };

    for (const auto& [kw, det] : keywords) {
        addItem(kw, 14, det);
    }

    // 2. Types (Kind 7: Class / Type)
    const std::vector<std::string> types = {
        "i32", "i64", "u32", "u64", "f32", "f64", "bool", "string", "void", "dynamic",
        "f32x4", "i32x4", "f64x2", "i64x2",
        "AtomicI32", "AtomicI64", "AtomicBool",
        "Vec", "Rc", "Box", "Arena", "Promise"
    };

    for (const auto& ty : types) {
        addItem(ty, 7, "Type " + ty);
    }

    // 3. Builtins (Kind 3: Function)
    const std::vector<std::pair<std::string, std::string>> builtins = {
        {"println", "fn println(val: any): void"},
        {"print", "fn print(val: any): void"},
        {"echo", "echo statement"},
        {"var_dump", "fn var_dump(val: any): void"},
        {"count", "fn count(arr: dynamic): i32"},
        {"array_push", "fn array_push(&arr: dynamic, val: any): void"},
        {"array_pop", "fn array_pop(&arr: dynamic): dynamic"},
        {"explode", "fn explode(delim: string, str: string): dynamic"},
        {"implode", "fn implode(glue: string, pieces: dynamic): string"},
        {"strlen", "fn strlen(s: string): i32"}
    };

    for (const auto& [b, det] : builtins) {
        addItem(b, 3, det);
    }

    // 4. In-scope AST symbols from document
    auto it = m_documents.find(uri);
    if (it != m_documents.end()) {
        SourceManager sm;
        uint32_t fileId = sm.addSource(uri, it->second.text);
        DiagnosticEmitter emitter(sm, nullptr);
        Parser parser(fileId, sm, &emitter);
        auto program = parser.parseProgram();
        if (program) {
            for (const auto& fn : program->getFunctions()) {
                addItem(fn->getName(), 3, "fn " + fn->getName() + "(...): " + fn->getReturnType());
            }
            for (const auto& st : program->getStructs()) {
                addItem(st->getName(), 22, "struct " + st->getName());
            }
            for (const auto& cl : program->getClasses()) {
                addItem(cl->getName(), 7, "class " + cl->getName());
            }
            for (const auto& en : program->getEnums()) {
                addItem(en->getName(), 13, "enum " + en->getName());
            }
        }
    }

    return JsonValue(std::move(items));
}

JsonValue LspServer::handleDefinition(const JsonValue& /*id*/, const JsonValue& params) {
    std::string uri = params["textDocument"]["uri"].asString();
    int line = static_cast<int>(params["position"]["line"].asInt(0));
    int character = static_cast<int>(params["position"]["character"].asInt(0));

    auto it = m_documents.find(uri);
    if (it == m_documents.end()) {
        return JsonValue(); // null
    }

    std::string word = getWordAtPosition(it->second.text, line, character);
    if (word.empty()) {
        return JsonValue();
    }

    SourceManager sm;
    uint32_t fileId = sm.addSource(uri, it->second.text);
    DiagnosticEmitter emitter(sm, nullptr);
    Parser parser(fileId, sm, &emitter);
    auto program = parser.parseProgram();

    if (!program) {
        return JsonValue();
    }

    // Check functions
    for (const auto& fn : program->getFunctions()) {
        if (fn->getName() == word) {
            int defLine = fn->getSpan().start.line > 0 ? static_cast<int>(fn->getSpan().start.line - 1) : 0;
            int defCol = fn->getSpan().start.column > 0 ? static_cast<int>(fn->getSpan().start.column - 1) : 0;
            return JsonValue::Object{
                {"uri", JsonValue(uri)},
                {"range", JsonValue::Object{
                    {"start", JsonValue::Object{{"line", JsonValue(defLine)}, {"character", JsonValue(defCol)}}},
                    {"end", JsonValue::Object{{"line", JsonValue(defLine)}, {"character", JsonValue(defCol + static_cast<int>(word.size()))}}}
                }}
            };
        }
    }

    // Check structs
    for (const auto& st : program->getStructs()) {
        if (st->getName() == word) {
            int defLine = st->getSpan().start.line > 0 ? static_cast<int>(st->getSpan().start.line - 1) : 0;
            int defCol = st->getSpan().start.column > 0 ? static_cast<int>(st->getSpan().start.column - 1) : 0;
            return JsonValue::Object{
                {"uri", JsonValue(uri)},
                {"range", JsonValue::Object{
                    {"start", JsonValue::Object{{"line", JsonValue(defLine)}, {"character", JsonValue(defCol)}}},
                    {"end", JsonValue::Object{{"line", JsonValue(defLine)}, {"character", JsonValue(defCol + static_cast<int>(word.size()))}}}
                }}
            };
        }
    }

    // Check classes
    for (const auto& cl : program->getClasses()) {
        if (cl->getName() == word) {
            int defLine = cl->getSpan().start.line > 0 ? static_cast<int>(cl->getSpan().start.line - 1) : 0;
            int defCol = cl->getSpan().start.column > 0 ? static_cast<int>(cl->getSpan().start.column - 1) : 0;
            return JsonValue::Object{
                {"uri", JsonValue(uri)},
                {"range", JsonValue::Object{
                    {"start", JsonValue::Object{{"line", JsonValue(defLine)}, {"character", JsonValue(defCol)}}},
                    {"end", JsonValue::Object{{"line", JsonValue(defLine)}, {"character", JsonValue(defCol + static_cast<int>(word.size()))}}}
                }}
            };
        }
    }

    // Check enums
    for (const auto& en : program->getEnums()) {
        if (en->getName() == word) {
            int defLine = en->getSpan().start.line > 0 ? static_cast<int>(en->getSpan().start.line - 1) : 0;
            int defCol = en->getSpan().start.column > 0 ? static_cast<int>(en->getSpan().start.column - 1) : 0;
            return JsonValue::Object{
                {"uri", JsonValue(uri)},
                {"range", JsonValue::Object{
                    {"start", JsonValue::Object{{"line", JsonValue(defLine)}, {"character", JsonValue(defCol)}}},
                    {"end", JsonValue::Object{{"line", JsonValue(defLine)}, {"character", JsonValue(defCol + static_cast<int>(word.size()))}}}
                }}
            };
        }
    }

    return JsonValue(); // null
}

JsonValue LspServer::handleDocumentSymbol(const JsonValue& /*id*/, const JsonValue& params) {
    std::string uri = params["textDocument"]["uri"].asString();

    auto it = m_documents.find(uri);
    if (it == m_documents.end()) {
        return JsonValue(JsonValue::Array{});
    }

    SourceManager sm;
    uint32_t fileId = sm.addSource(uri, it->second.text);
    DiagnosticEmitter emitter(sm, nullptr);
    Parser parser(fileId, sm, &emitter);
    auto program = parser.parseProgram();

    if (!program) {
        return JsonValue(JsonValue::Array{});
    }

    JsonValue::Array symbols;

    // Helper to build range
    auto makeRange = [](const SourceSpan& span, const std::string& name) {
        int l = span.start.line > 0 ? static_cast<int>(span.start.line - 1) : 0;
        int c = span.start.column > 0 ? static_cast<int>(span.start.column - 1) : 0;
        return JsonValue::Object{
            {"start", JsonValue::Object{{"line", JsonValue(l)}, {"character", JsonValue(c)}}},
            {"end", JsonValue::Object{{"line", JsonValue(l)}, {"character", JsonValue(c + static_cast<int>(name.size()))}}}
        };
    };

    // Functions (Kind 12: Function)
    for (const auto& fn : program->getFunctions()) {
        JsonValue rng = makeRange(fn->getSpan(), fn->getName());
        symbols.push_back(JsonValue::Object{
            {"name", JsonValue(fn->getName())},
            {"kind", JsonValue(12)},
            {"range", rng},
            {"selectionRange", rng}
        });
    }

    // Structs (Kind 23: Struct)
    for (const auto& st : program->getStructs()) {
        JsonValue rng = makeRange(st->getSpan(), st->getName());
        symbols.push_back(JsonValue::Object{
            {"name", JsonValue(st->getName())},
            {"kind", JsonValue(23)},
            {"range", rng},
            {"selectionRange", rng}
        });
    }

    // Classes (Kind 5: Class)
    for (const auto& cl : program->getClasses()) {
        JsonValue rng = makeRange(cl->getSpan(), cl->getName());
        symbols.push_back(JsonValue::Object{
            {"name", JsonValue(cl->getName())},
            {"kind", JsonValue(5)},
            {"range", rng},
            {"selectionRange", rng}
        });
    }

    // Interfaces (Kind 11: Interface)
    for (const auto& iface : program->getInterfaces()) {
        JsonValue rng = makeRange(iface->getSpan(), iface->getName());
        symbols.push_back(JsonValue::Object{
            {"name", JsonValue(iface->getName())},
            {"kind", JsonValue(11)},
            {"range", rng},
            {"selectionRange", rng}
        });
    }

    // Enums (Kind 10: Enum)
    for (const auto& en : program->getEnums()) {
        JsonValue rng = makeRange(en->getSpan(), en->getName());
        symbols.push_back(JsonValue::Object{
            {"name", JsonValue(en->getName())},
            {"kind", JsonValue(10)},
            {"range", rng},
            {"selectionRange", rng}
        });
    }

    return JsonValue(std::move(symbols));
}

void LspServer::sendFramedMessage(std::ostream& out, const std::string& payload) {
    out << "Content-Length: " << payload.size() << "\r\n\r\n" << payload;
    out.flush();
}

std::string LspServer::formatResponse(const JsonValue& id, const JsonValue& result) {
    JsonValue res = JsonValue::Object{
        {"jsonrpc", JsonValue("2.0")},
        {"id", id},
        {"result", result}
    };
    return res.serialize();
}

std::string LspServer::formatError(const JsonValue& id, int code, const std::string& message) {
    JsonValue res = JsonValue::Object{
        {"jsonrpc", JsonValue("2.0")},
        {"id", id},
        {"error", JsonValue::Object{
            {"code", JsonValue(code)},
            {"message", JsonValue(message)}
        }}
    };
    return res.serialize();
}

std::string LspServer::formatNotification(const std::string& method, const JsonValue& params) {
    JsonValue notif = JsonValue::Object{
        {"jsonrpc", JsonValue("2.0")},
        {"method", JsonValue(method)},
        {"params", params}
    };
    return notif.serialize();
}

} // namespace sph::lsp

