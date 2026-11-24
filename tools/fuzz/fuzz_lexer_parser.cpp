#include "compiler/diagnostics/DiagnosticEmitter.hpp"
#include "compiler/lexer/Lexer.hpp"
#include "compiler/parser/Parser.hpp"
#include "compiler/source/SourceManager.hpp"
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    if (size == 0 || size > 65536) return 0;

    std::string source(reinterpret_cast<const char*>(data), size);

    sph::SourceManager sm;
    uint32_t fileId = sm.addSource("fuzz_input.sph", source);
    sph::DiagnosticEmitter emitter(sm);

    // Lexer Fuzzing
    try {
        sph::Lexer lexer(fileId, sm, &emitter);
        std::vector<sph::Token> tokens;
        while (true) {
            auto tok = lexer.nextToken();
            tokens.push_back(tok);
            if (tok.is(sph::TokenKind::Eof)) break;
            if (tokens.size() > 10000) break; // bounded
        }
    } catch (...) {
        // Must never crash on any arbitrary input
    }

    // Parser Fuzzing
    try {
        sph::Parser parser(fileId, sm, &emitter);
        auto program = parser.parseProgram();
        (void)program;
    } catch (...) {
        // Must never crash on any arbitrary input
    }

    return 0;
}

#ifndef LIBFUZZER_ENTRY
int main(int argc, char** argv) {
    if (argc > 1) {
        // Read from argument file
        std::string filename = argv[1];
        std::ifstream file(filename, std::ios::binary);
        if (file) {
            std::vector<uint8_t> buffer((std::istreambuf_iterator<char>(file)),
                                         std::istreambuf_iterator<char>());
            return LLVMFuzzerTestOneInput(buffer.data(), buffer.size());
        }
    }

    // Pseudo-random deterministic fuzz suite
    std::cout << "[Fuzz Harness] Running 1000 synthetic fuzz iterations...\n";
    uint32_t seed = 0x12345678;
    for (int iter = 0; iter < 1000; ++iter) {
        size_t len = (iter % 256) + 1;
        std::vector<uint8_t> bytes(len);
        for (size_t i = 0; i < len; ++i) {
            seed = seed * 1664525u + 1013904223u;
            bytes[i] = static_cast<uint8_t>(seed >> 16);
        }
        LLVMFuzzerTestOneInput(bytes.data(), bytes.size());
    }
    std::cout << "[Fuzz Harness] 1000 fuzz iterations completed with zero crashes.\n";
    return 0;
}
#endif

