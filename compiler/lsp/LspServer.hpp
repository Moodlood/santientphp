#pragma once

#include "Json.hpp"
#include "compiler/diagnostics/DiagnosticEmitter.hpp"
#include "compiler/parser/Parser.hpp"
#include "compiler/semantic/SemanticAnalyzer.hpp"
#include "compiler/source/SourceManager.hpp"
#include <iosfwd>
#include <map>
#include <string>
#include <vector>

namespace sph::lsp {

struct DocumentState {
    std::string uri;
    std::string text;
    int version = 0;
};

class LspServer {
public:
    LspServer();
    ~LspServer() = default;

    // Runs server loop listening on input stream and replying on output stream
    int run(std::istream& in, std::ostream& out);

    // Process a single JSON-RPC raw payload and returns response (or empty if notification)
    std::string handleMessage(const std::string& rawJson, std::ostream* out = nullptr);

    // Document state query
    [[nodiscard]] const std::map<std::string, DocumentState>& getDocuments() const noexcept { return m_documents; }
    [[nodiscard]] bool isInitialized() const noexcept { return m_initialized; }
    [[nodiscard]] bool isShutdown() const noexcept { return m_shutdown; }
    [[nodiscard]] bool isExitRequested() const noexcept { return m_exitRequested; }

private:
    bool m_initialized = false;
    bool m_shutdown = false;
    bool m_exitRequested = false;
    std::map<std::string, DocumentState> m_documents;

    // Dispatchers
    JsonValue handleInitialize(const JsonValue& id, const JsonValue& params);
    void handleInitialized(const JsonValue& params);
    JsonValue handleShutdown(const JsonValue& id);
    void handleExit();
    void handleDidOpen(const JsonValue& params, std::ostream* out);
    void handleDidChange(const JsonValue& params, std::ostream* out);
    void handleDidClose(const JsonValue& params, std::ostream* out);
    JsonValue handleHover(const JsonValue& id, const JsonValue& params);
    JsonValue handleCompletion(const JsonValue& id, const JsonValue& params);
    JsonValue handleDefinition(const JsonValue& id, const JsonValue& params);
    JsonValue handleDocumentSymbol(const JsonValue& id, const JsonValue& params);

    // Diagnostics publication
    void publishDiagnostics(const std::string& uri, const std::string& text, std::ostream* out);

    // Helpers
    static void sendFramedMessage(std::ostream& out, const std::string& payload);
    static std::string formatResponse(const JsonValue& id, const JsonValue& result);
    static std::string formatError(const JsonValue& id, int code, const std::string& message);
    static std::string formatNotification(const std::string& method, const JsonValue& params);

    static std::string getWordAtPosition(const std::string& text, int line, int character);
    static std::string getLine(const std::string& text, int line);
};

} // namespace sph::lsp

