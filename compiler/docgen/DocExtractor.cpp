#include "compiler/docgen/DocExtractor.hpp"
#include <algorithm>
#include <cctype>

namespace sph {

namespace {

std::string trimView(std::string_view s) {
    size_t start = 0;
    while (start < s.size() && std::isspace(static_cast<unsigned char>(s[start]))) {
        start++;
    }
    size_t end = s.size();
    while (end > start && std::isspace(static_cast<unsigned char>(s[end - 1]))) {
        end--;
    }
    return std::string(s.substr(start, end - start));
}

} // namespace

DocComment DocExtractor::findDocCommentAbove(uint32_t fileId, uint32_t line) const {
    if (line <= 1) return {};

    uint32_t cur = line - 1;
    // Skip empty lines (at most 1)
    std::string_view curLine = m_sourceManager.getLineContent(fileId, cur);
    if (trimView(curLine).empty() && cur > 1) {
        cur--;
        curLine = m_sourceManager.getLineContent(fileId, cur);
    }

    std::string trimmed = trimView(curLine);
    if (trimmed.empty()) return {};

    // Check for block comment ending with */
    if (trimmed.size() >= 2 && trimmed.substr(trimmed.size() - 2) == "*/") {
        uint32_t endLine = cur;
        uint32_t startLine = cur;
        bool foundStart = false;

        while (cur >= 1) {
            std::string_view l = m_sourceManager.getLineContent(fileId, cur);
            std::string t = trimView(l);
            if (t.rfind("/**", 0) == 0 || t.rfind("/*", 0) == 0) {
                startLine = cur;
                foundStart = true;
                break;
            }
            if (cur == 1) break;
            cur--;
        }

        if (foundStart) {
            std::string block;
            for (uint32_t l = startLine; l <= endLine; ++l) {
                block += std::string(m_sourceManager.getLineContent(fileId, l));
                block += "\n";
            }
            return DocCommentParser::parse(block);
        }
    }

    // Check for triple slash comment ///
    if (trimmed.rfind("///", 0) == 0) {
        uint32_t endLine = cur;
        uint32_t startLine = cur;
        while (cur >= 1) {
            std::string_view l = m_sourceManager.getLineContent(fileId, cur);
            std::string t = trimView(l);
            if (t.rfind("///", 0) == 0) {
                startLine = cur;
                if (cur == 1) break;
                cur--;
            } else {
                break;
            }
        }

        std::string lines;
        for (uint32_t l = startLine; l <= endLine; ++l) {
            lines += std::string(m_sourceManager.getLineContent(fileId, l));
            lines += "\n";
        }
        return DocCommentParser::parse(lines);
    }

    return {};
}

DocFunction DocExtractor::extractFunction(uint32_t fileId, const FunctionDecl& fn) const {
    DocFunction df;
    df.name = fn.getName();
    df.returnType = fn.getReturnType();
    df.typeParams = fn.getTypeParams();
    df.isAsync = fn.isAsync();
    df.isExtern = fn.isExtern();
    df.abi = fn.getAbi();
    df.line = fn.getSpan().start.line;
    df.doc = findDocCommentAbove(fileId, df.line);

    for (const auto& p : fn.getParams()) {
        DocParamInfo pInfo;
        pInfo.name = p.name;
        pInfo.type = p.type;
        pInfo.isByRef = p.isByRef;

        // Match description from @param tag in doc
        for (const auto& tag : df.doc.tags) {
            if (tag.tag == "param" && tag.target == p.name) {
                pInfo.description = tag.description;
                break;
            }
        }
        df.params.push_back(std::move(pInfo));
    }

    return df;
}

DocStruct DocExtractor::extractStruct(uint32_t fileId, const StructDecl& st) const {
    DocStruct ds;
    ds.name = st.getName();
    ds.typeParams = st.getTypeParams();
    ds.line = st.getSpan().start.line;
    ds.doc = findDocCommentAbove(fileId, ds.line);

    for (const auto& f : st.getFields()) {
        DocFieldInfo fi;
        fi.name = f.name;
        fi.type = f.type;
        fi.doc = findDocCommentAbove(fileId, f.span.start.line);
        ds.fields.push_back(std::move(fi));
    }

    return ds;
}

DocClass DocExtractor::extractClass(uint32_t fileId, const ClassDecl& cl) const {
    DocClass dc;
    dc.name = cl.getName();
    dc.implementsInterfaces = cl.getImplements();
    dc.line = cl.getSpan().start.line;
    dc.doc = findDocCommentAbove(fileId, dc.line);

    for (const auto& f : cl.getFields()) {
        DocFieldInfo fi;
        fi.name = f.name;
        fi.type = f.type;
        fi.doc = findDocCommentAbove(fileId, f.span.start.line);
        dc.fields.push_back(std::move(fi));
    }

    for (const auto& m : cl.getMethods()) {
        if (m) {
            dc.methods.push_back(extractFunction(fileId, *m));
        }
    }

    return dc;
}

DocInterface DocExtractor::extractInterface(uint32_t fileId, const InterfaceDecl& in) const {
    DocInterface di;
    di.name = in.getName();
    di.line = in.getSpan().start.line;
    di.doc = findDocCommentAbove(fileId, di.line);

    for (const auto& sig : in.getMethods()) {
        DocFunction df;
        df.name = sig.name;
        df.returnType = sig.returnType;
        df.line = sig.span.start.line;
        df.doc = findDocCommentAbove(fileId, df.line);

        for (const auto& p : sig.params) {
            DocParamInfo pInfo;
            pInfo.name = p.name;
            pInfo.type = p.type;
            pInfo.isByRef = p.isByRef;
            for (const auto& tag : df.doc.tags) {
                if (tag.tag == "param" && tag.target == p.name) {
                    pInfo.description = tag.description;
                    break;
                }
            }
            df.params.push_back(std::move(pInfo));
        }

        di.methods.push_back(std::move(df));
    }

    return di;
}

DocEnum DocExtractor::extractEnum(uint32_t fileId, const EnumDecl& en) const {
    DocEnum de;
    de.name = en.getName();
    de.line = en.getSpan().start.line;
    de.doc = findDocCommentAbove(fileId, de.line);

    for (const auto& v : en.getVariants()) {
        DocEnumVariantInfo vi;
        vi.name = v.name;
        if (v.explicitValue) {
            vi.explicitValue = std::to_string(*v.explicitValue);
        }
        vi.doc = findDocCommentAbove(fileId, v.span.start.line);
        de.variants.push_back(std::move(vi));
    }

    return de;
}

DocModule DocExtractor::extractModule(uint32_t fileId, const ProgramAST& program) {
    DocModule module;
    const SourceFile* sf = m_sourceManager.getFile(fileId);
    if (sf) {
        module.sourceFile = sf->getFilename();
        std::filesystem::path p(sf->getFilename());
        module.name = p.stem().string();
    }

    // Module-level doc comment (line 1 or directly below <?php)
    module.moduleDoc = findDocCommentAbove(fileId, 2);

    for (const auto& fn : program.getFunctions()) {
        if (fn) module.functions.push_back(extractFunction(fileId, *fn));
    }
    for (const auto& st : program.getStructs()) {
        if (st) module.structs.push_back(extractStruct(fileId, *st));
    }
    for (const auto& cl : program.getClasses()) {
        if (cl) module.classes.push_back(extractClass(fileId, *cl));
    }
    for (const auto& in : program.getInterfaces()) {
        if (in) module.interfaces.push_back(extractInterface(fileId, *in));
    }
    for (const auto& en : program.getEnums()) {
        if (en) module.enums.push_back(extractEnum(fileId, *en));
    }

    return module;
}

} // namespace sph
