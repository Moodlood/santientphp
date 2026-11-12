#include "compiler/docgen/DocGenerator.hpp"
#include <fstream>
#include <sstream>

namespace sph {

std::string DocGenerator::escapeHtml(std::string_view text) const {
    std::string out;
    out.reserve(text.size());
    for (char c : text) {
        switch (c) {
            case '&': out += "&amp;"; break;
            case '<': out += "&lt;"; break;
            case '>': out += "&gt;"; break;
            case '"': out += "&quot;"; break;
            case '\'': out += "&#39;"; break;
            default: out += c; break;
        }
    }
    return out;
}

void DocGenerator::renderFunctionMd(std::string& out, const DocFunction& fn, int headerLevel) const {
    std::string prefix(headerLevel, '#');
    out += prefix + " `" + fn.name + "`\n\n";

    // Signature
    out += "```sph\n";
    if (fn.isAsync) out += "async ";
    if (fn.isExtern) out += "extern \"" + fn.abi + "\" ";
    out += "fn " + fn.name;
    if (!fn.typeParams.empty()) {
        out += "<";
        for (size_t i = 0; i < fn.typeParams.size(); ++i) {
            if (i > 0) out += ", ";
            out += fn.typeParams[i];
        }
        out += ">";
    }
    out += "(";
    for (size_t i = 0; i < fn.params.size(); ++i) {
        if (i > 0) out += ", ";
        if (fn.params[i].isByRef) out += "&";
        out += fn.params[i].name + ": " + fn.params[i].type;
    }
    out += ")";
    if (!fn.returnType.empty() && fn.returnType != "void") {
        out += ": " + fn.returnType;
    }
    out += "\n```\n\n";

    if (!fn.doc.brief.empty()) {
        out += fn.doc.brief + "\n\n";
    }
    if (!fn.doc.detailed.empty()) {
        out += fn.doc.detailed + "\n\n";
    }

    if (!fn.params.empty()) {
        out += "**Parameters:**\n\n";
        out += "| Name | Type | Description |\n";
        out += "| :--- | :--- | :---------- |\n";
        for (const auto& p : fn.params) {
            out += "| `" + p.name + "` | `" + p.type + "` | " + (p.description.empty() ? "-" : p.description) + " |\n";
        }
        out += "\n";
    }

    const auto* retTag = fn.doc.findTag("return");
    if (retTag || (!fn.returnType.empty() && fn.returnType != "void")) {
        out += "**Returns:** `" + fn.returnType + "`";
        if (retTag && !retTag->description.empty()) {
            out += " — " + retTag->description;
        }
        out += "\n\n";
    }

    auto throwsTags = fn.doc.getTags("throws");
    if (!throwsTags.empty()) {
        out += "**Throws:**\n\n";
        for (const auto& t : throwsTags) {
            out += "- `" + t.target + "`: " + t.description + "\n";
        }
        out += "\n";
    }
}

void DocGenerator::renderStructMd(std::string& out, const DocStruct& st) const {
    out += "### Struct `" + st.name + "`\n\n";
    out += "```sph\nstruct " + st.name;
    if (!st.typeParams.empty()) {
        out += "<";
        for (size_t i = 0; i < st.typeParams.size(); ++i) {
            if (i > 0) out += ", ";
            out += st.typeParams[i];
        }
        out += ">";
    }
    out += "\n```\n\n";

    if (!st.doc.brief.empty()) out += st.doc.brief + "\n\n";
    if (!st.doc.detailed.empty()) out += st.doc.detailed + "\n\n";

    if (!st.fields.empty()) {
        out += "**Fields:**\n\n";
        out += "| Field | Type | Description |\n";
        out += "| :---- | :--- | :---------- |\n";
        for (const auto& f : st.fields) {
            out += "| `" + f.name + "` | `" + f.type + "` | " + (f.doc.brief.empty() ? "-" : f.doc.brief) + " |\n";
        }
        out += "\n";
    }
}

void DocGenerator::renderClassMd(std::string& out, const DocClass& cl) const {
    out += "### Class `" + cl.name + "`\n\n";
    if (!cl.implementsInterfaces.empty()) {
        out += "*Implements:* ";
        for (size_t i = 0; i < cl.implementsInterfaces.size(); ++i) {
            if (i > 0) out += ", ";
            out += "`" + cl.implementsInterfaces[i] + "`";
        }
        out += "\n\n";
    }

    if (!cl.doc.brief.empty()) out += cl.doc.brief + "\n\n";
    if (!cl.doc.detailed.empty()) out += cl.doc.detailed + "\n\n";

    if (!cl.fields.empty()) {
        out += "**Properties:**\n\n";
        out += "| Property | Type | Description |\n";
        out += "| :------- | :--- | :---------- |\n";
        for (const auto& f : cl.fields) {
            out += "| `" + f.name + "` | `" + f.type + "` | " + (f.doc.brief.empty() ? "-" : f.doc.brief) + " |\n";
        }
        out += "\n";
    }

    if (!cl.methods.empty()) {
        out += "#### Methods\n\n";
        for (const auto& m : cl.methods) {
            renderFunctionMd(out, m, 5);
        }
    }
}

void DocGenerator::renderInterfaceMd(std::string& out, const DocInterface& in) const {
    out += "### Interface `" + in.name + "`\n\n";
    if (!in.doc.brief.empty()) out += in.doc.brief + "\n\n";

    if (!in.methods.empty()) {
        out += "**Methods:**\n\n";
        for (const auto& m : in.methods) {
            renderFunctionMd(out, m, 4);
        }
    }
}

void DocGenerator::renderEnumMd(std::string& out, const DocEnum& en) const {
    out += "### Enum `" + en.name + "`\n\n";
    if (!en.doc.brief.empty()) out += en.doc.brief + "\n\n";

    if (!en.variants.empty()) {
        out += "**Variants:**\n\n";
        out += "| Variant | Value | Description |\n";
        out += "| :------ | :---- | :---------- |\n";
        for (const auto& v : en.variants) {
            out += "| `" + v.name + "` | " + (v.explicitValue.empty() ? "-" : "`" + v.explicitValue + "`") + " | " + (v.doc.brief.empty() ? "-" : v.doc.brief) + " |\n";
        }
        out += "\n";
    }
}

std::string DocGenerator::generateMarkdown(const DocModule& module) const {
    std::string out;
    out += "# Module `" + module.name + "`\n\n";
    if (!module.sourceFile.empty()) {
        out += "*Source:* `" + module.sourceFile + "`\n\n";
    }
    if (!module.moduleDoc.brief.empty()) {
        out += module.moduleDoc.brief + "\n\n";
    }

    if (!module.functions.empty()) {
        out += "## Functions\n\n";
        for (const auto& fn : module.functions) {
            renderFunctionMd(out, fn, 3);
        }
    }

    if (!module.structs.empty()) {
        out += "## Structs\n\n";
        for (const auto& st : module.structs) {
            renderStructMd(out, st);
        }
    }

    if (!module.classes.empty()) {
        out += "## Classes\n\n";
        for (const auto& cl : module.classes) {
            renderClassMd(out, cl);
        }
    }

    if (!module.interfaces.empty()) {
        out += "## Interfaces\n\n";
        for (const auto& in : module.interfaces) {
            renderInterfaceMd(out, in);
        }
    }

    if (!module.enums.empty()) {
        out += "## Enums\n\n";
        for (const auto& en : module.enums) {
            renderEnumMd(out, en);
        }
    }

    return out;
}

std::string DocGenerator::generateMarkdown(const DocProject& project) const {
    std::string out;
    out += "# " + project.title + " (v" + project.version + ")\n\n";
    if (!project.description.empty()) {
        out += project.description + "\n\n";
    }

    for (const auto& mod : project.modules) {
        out += generateMarkdown(mod);
        out += "---\n\n";
    }
    return out;
}

void DocGenerator::renderFunctionHtml(std::string& out, const DocFunction& fn, const std::string& parentScope) const {
    std::string id = parentScope.empty() ? fn.name : parentScope + "_" + fn.name;
    out += "<div class=\"doc-card\" id=\"" + escapeHtml(id) + "\">\n";
    out += "  <div class=\"doc-header\">\n";
    out += "    <span class=\"badge badge-fn\">fn</span>\n";
    out += "    <h3 class=\"doc-title\">" + escapeHtml(fn.name) + "</h3>\n";
    out += "  </div>\n";

    // Signature
    out += "  <pre class=\"signature\"><code>";
    if (fn.isAsync) out += "<span class=\"kw\">async</span> ";
    if (fn.isExtern) out += "<span class=\"kw\">extern</span> &quot;" + escapeHtml(fn.abi) + "&quot; ";
    out += "<span class=\"kw\">fn</span> <span class=\"fn-name\">" + escapeHtml(fn.name) + "</span>";
    if (!fn.typeParams.empty()) {
        out += "&lt;";
        for (size_t i = 0; i < fn.typeParams.size(); ++i) {
            if (i > 0) out += ", ";
            out += escapeHtml(fn.typeParams[i]);
        }
        out += "&gt;";
    }
    out += "(";
    for (size_t i = 0; i < fn.params.size(); ++i) {
        if (i > 0) out += ", ";
        if (fn.params[i].isByRef) out += "&amp;";
        out += escapeHtml(fn.params[i].name) + ": <span class=\"type\">" + escapeHtml(fn.params[i].type) + "</span>";
    }
    out += ")";
    if (!fn.returnType.empty() && fn.returnType != "void") {
        out += ": <span class=\"type\">" + escapeHtml(fn.returnType) + "</span>";
    }
    out += "</code></pre>\n";

    if (!fn.doc.brief.empty()) {
        out += "  <p class=\"doc-brief\">" + escapeHtml(fn.doc.brief) + "</p>\n";
    }
    if (!fn.doc.detailed.empty()) {
        out += "  <p class=\"doc-detailed\">" + escapeHtml(fn.doc.detailed) + "</p>\n";
    }

    if (!fn.params.empty()) {
        out += "  <h4>Parameters</h4>\n";
        out += "  <table class=\"table\">\n";
        out += "    <thead><tr><th>Name</th><th>Type</th><th>Description</th></tr></thead>\n";
        out += "    <tbody>\n";
        for (const auto& p : fn.params) {
            out += "      <tr><td><code>" + escapeHtml(p.name) + "</code></td>";
            out += "<td><code>" + escapeHtml(p.type) + "</code></td>";
            out += "<td>" + (p.description.empty() ? "-" : escapeHtml(p.description)) + "</td></tr>\n";
        }
        out += "    </tbody>\n  </table>\n";
    }

    const auto* retTag = fn.doc.findTag("return");
    if (retTag || (!fn.returnType.empty() && fn.returnType != "void")) {
        out += "  <p><strong>Returns:</strong> <code>" + escapeHtml(fn.returnType) + "</code>";
        if (retTag && !retTag->description.empty()) {
            out += " &mdash; " + escapeHtml(retTag->description);
        }
        out += "</p>\n";
    }

    auto throwsTags = fn.doc.getTags("throws");
    if (!throwsTags.empty()) {
        out += "  <h4>Throws</h4>\n  <ul>\n";
        for (const auto& t : throwsTags) {
            out += "    <li><code>" + escapeHtml(t.target) + "</code>: " + escapeHtml(t.description) + "</li>\n";
        }
        out += "  </ul>\n";
    }

    out += "</div>\n";
}

std::string DocGenerator::generateHtml(const DocProject& project) const {
    std::string out;
    out += "<!DOCTYPE html>\n<html lang=\"en\">\n<head>\n";
    out += "  <meta charset=\"UTF-8\">\n";
    out += "  <meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">\n";
    out += "  <title>" + escapeHtml(project.title) + "</title>\n";
    out += "  <style>\n";
    out += "    :root {\n";
    out += "      --bg-primary: #0f172a;\n";
    out += "      --bg-secondary: #1e293b;\n";
    out += "      --bg-card: #1e293b;\n";
    out += "      --border: #334155;\n";
    out += "      --text-main: #f8fafc;\n";
    out += "      --text-muted: #94a3b8;\n";
    out += "      --accent: #38bdf8;\n";
    out += "      --accent-hover: #0ea5e9;\n";
    out += "      --code-bg: #090d16;\n";
    out += "      --kw: #f43f5e;\n";
    out += "      --type: #38bdf8;\n";
    out += "      --fn: #a855f7;\n";
    out += "    }\n";
    out += "    * { box-sizing: border-box; margin: 0; padding: 0; }\n";
    out += "    body { font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, Helvetica, Arial, sans-serif; background: var(--bg-primary); color: var(--text-main); line-height: 1.6; display: flex; min-height: 100vh; }\n";
    out += "    #sidebar { width: 300px; background: var(--bg-secondary); border-right: 1px solid var(--border); padding: 1.5rem; position: sticky; top: 0; height: 100vh; overflow-y: auto; }\n";
    out += "    #main { flex: 1; padding: 2.5rem; max-width: 1000px; margin: 0 auto; }\n";
    out += "    h1, h2, h3, h4 { color: var(--text-main); font-weight: 600; margin-bottom: 0.75rem; }\n";
    out += "    h1 { font-size: 2.25rem; border-bottom: 1px solid var(--border); padding-bottom: 0.75rem; margin-bottom: 1.5rem; }\n";
    out += "    h2 { font-size: 1.5rem; margin-top: 2rem; margin-bottom: 1rem; color: var(--accent); border-bottom: 1px solid var(--border); padding-bottom: 0.5rem; }\n";
    out += "    #search { width: 100%; padding: 0.6rem 0.8rem; background: var(--bg-primary); border: 1px solid var(--border); border-radius: 6px; color: var(--text-main); margin-bottom: 1.25rem; font-size: 0.9rem; }\n";
    out += "    #search:focus { outline: none; border-color: var(--accent); }\n";
    out += "    .nav-group { margin-bottom: 1.25rem; }\n";
    out += "    .nav-title { font-size: 0.75rem; text-transform: uppercase; letter-spacing: 0.05em; color: var(--text-muted); margin-bottom: 0.5rem; font-weight: 700; }\n";
    out += "    .nav-list { list-style: none; }\n";
    out += "    .nav-item { margin-bottom: 0.35rem; }\n";
    out += "    .nav-link { color: var(--text-muted); text-decoration: none; font-size: 0.9rem; transition: color 0.15s; display: block; overflow: hidden; text-overflow: ellipsis; white-space: nowrap; }\n";
    out += "    .nav-link:hover { color: var(--accent); }\n";
    out += "    .doc-card { background: var(--bg-card); border: 1px solid var(--border); border-radius: 8px; padding: 1.5rem; margin-bottom: 1.75rem; }\n";
    out += "    .doc-header { display: flex; align-items: center; gap: 0.75rem; margin-bottom: 1rem; }\n";
    out += "    .badge { font-size: 0.75rem; text-transform: uppercase; font-weight: 700; padding: 0.2rem 0.5rem; border-radius: 4px; }\n";
    out += "    .badge-fn { background: #3b82f6; color: white; }\n";
    out += "    .badge-struct { background: #10b981; color: white; }\n";
    out += "    .badge-class { background: #8b5cf6; color: white; }\n";
    out += "    .badge-interface { background: #f59e0b; color: white; }\n";
    out += "    .badge-enum { background: #ec4899; color: white; }\n";
    out += "    pre.signature { background: var(--code-bg); border: 1px solid var(--border); border-radius: 6px; padding: 1rem; overflow-x: auto; margin-bottom: 1rem; font-family: ui-monospace, Menlo, Monaco, Consolas, monospace; font-size: 0.95rem; }\n";
    out += "    .kw { color: var(--kw); font-weight: 600; }\n";
    out += "    .type { color: var(--type); }\n";
    out += "    .fn-name { color: var(--fn); font-weight: 600; }\n";
    out += "    .doc-brief { font-size: 1.05rem; margin-bottom: 0.75rem; }\n";
    out += "    .doc-detailed { color: var(--text-muted); margin-bottom: 1rem; }\n";
    out += "    table.table { width: 100%; border-collapse: collapse; margin-top: 0.75rem; margin-bottom: 1rem; }\n";
    out += "    table.table th, table.table td { border: 1px solid var(--border); padding: 0.6rem 0.8rem; text-align: left; font-size: 0.9rem; }\n";
    out += "    table.table th { background: var(--bg-primary); color: var(--text-muted); font-weight: 600; }\n";
    out += "    footer { margin-top: 4rem; padding-top: 1.5rem; border-top: 1px solid var(--border); color: var(--text-muted); font-size: 0.85rem; text-align: center; }\n";
    out += "  </style>\n";
    out += "</head>\n<body>\n";

    // Sidebar
    out += "  <nav id=\"sidebar\">\n";
    out += "    <h2>" + escapeHtml(project.title) + "</h2>\n";
    out += "    <p style=\"color: var(--text-muted); font-size: 0.85rem; margin-bottom: 1rem;\">Version " + escapeHtml(project.version) + "</p>\n";
    out += "    <input type=\"text\" id=\"search\" placeholder=\"Search symbols...\" oninput=\"filterSymbols()\">\n";

    for (const auto& mod : project.modules) {
        if (!mod.functions.empty()) {
            out += "    <div class=\"nav-group\">\n      <div class=\"nav-title\">Functions (" + escapeHtml(mod.name) + ")</div>\n      <ul class=\"nav-list\">\n";
            for (const auto& fn : mod.functions) {
                out += "        <li class=\"nav-item\"><a class=\"nav-link\" href=\"#" + escapeHtml(fn.name) + "\">" + escapeHtml(fn.name) + "</a></li>\n";
            }
            out += "      </ul>\n    </div>\n";
        }
        if (!mod.structs.empty()) {
            out += "    <div class=\"nav-group\">\n      <div class=\"nav-title\">Structs</div>\n      <ul class=\"nav-list\">\n";
            for (const auto& st : mod.structs) {
                out += "        <li class=\"nav-item\"><a class=\"nav-link\" href=\"#struct_" + escapeHtml(st.name) + "\">" + escapeHtml(st.name) + "</a></li>\n";
            }
            out += "      </ul>\n    </div>\n";
        }
        if (!mod.classes.empty()) {
            out += "    <div class=\"nav-group\">\n      <div class=\"nav-title\">Classes</div>\n      <ul class=\"nav-list\">\n";
            for (const auto& cl : mod.classes) {
                out += "        <li class=\"nav-item\"><a class=\"nav-link\" href=\"#class_" + escapeHtml(cl.name) + "\">" + escapeHtml(cl.name) + "</a></li>\n";
            }
            out += "      </ul>\n    </div>\n";
        }
        if (!mod.interfaces.empty()) {
            out += "    <div class=\"nav-group\">\n      <div class=\"nav-title\">Interfaces</div>\n      <ul class=\"nav-list\">\n";
            for (const auto& in : mod.interfaces) {
                out += "        <li class=\"nav-item\"><a class=\"nav-link\" href=\"#interface_" + escapeHtml(in.name) + "\">" + escapeHtml(in.name) + "</a></li>\n";
            }
            out += "      </ul>\n    </div>\n";
        }
        if (!mod.enums.empty()) {
            out += "    <div class=\"nav-group\">\n      <div class=\"nav-title\">Enums</div>\n      <ul class=\"nav-list\">\n";
            for (const auto& en : mod.enums) {
                out += "        <li class=\"nav-item\"><a class=\"nav-link\" href=\"#enum_" + escapeHtml(en.name) + "\">" + escapeHtml(en.name) + "</a></li>\n";
            }
            out += "      </ul>\n    </div>\n";
        }
    }
    out += "  </nav>\n";

    // Main Content
    out += "  <main id=\"main\">\n";
    out += "    <h1>" + escapeHtml(project.title) + "</h1>\n";
    if (!project.description.empty()) {
        out += "    <p style=\"font-size: 1.1rem; color: var(--text-muted); margin-bottom: 2rem;\">" + escapeHtml(project.description) + "</p>\n";
    }

    for (const auto& mod : project.modules) {
        out += "    <section>\n";
        out += "      <h2>Module: " + escapeHtml(mod.name) + "</h2>\n";
        if (!mod.moduleDoc.brief.empty()) {
            out += "      <p class=\"doc-brief\">" + escapeHtml(mod.moduleDoc.brief) + "</p>\n";
        }

        // Functions
        for (const auto& fn : mod.functions) {
            renderFunctionHtml(out, fn);
        }

        // Structs
        for (const auto& st : mod.structs) {
            out += "<div class=\"doc-card\" id=\"struct_" + escapeHtml(st.name) + "\">\n";
            out += "  <div class=\"doc-header\">\n";
            out += "    <span class=\"badge badge-struct\">struct</span>\n";
            out += "    <h3 class=\"doc-title\">" + escapeHtml(st.name) + "</h3>\n";
            out += "  </div>\n";
            if (!st.doc.brief.empty()) out += "  <p class=\"doc-brief\">" + escapeHtml(st.doc.brief) + "</p>\n";
            if (!st.fields.empty()) {
                out += "  <h4>Fields</h4>\n  <table class=\"table\">\n";
                out += "    <thead><tr><th>Field</th><th>Type</th><th>Description</th></tr></thead>\n<tbody>\n";
                for (const auto& f : st.fields) {
                    out += "      <tr><td><code>" + escapeHtml(f.name) + "</code></td><td><code>" + escapeHtml(f.type) + "</code></td><td>" + (f.doc.brief.empty() ? "-" : escapeHtml(f.doc.brief)) + "</td></tr>\n";
                }
                out += "    </tbody>\n  </table>\n";
            }
            out += "</div>\n";
        }

        // Classes
        for (const auto& cl : mod.classes) {
            out += "<div class=\"doc-card\" id=\"class_" + escapeHtml(cl.name) + "\">\n";
            out += "  <div class=\"doc-header\">\n";
            out += "    <span class=\"badge badge-class\">class</span>\n";
            out += "    <h3 class=\"doc-title\">" + escapeHtml(cl.name) + "</h3>\n";
            out += "  </div>\n";
            if (!cl.doc.brief.empty()) out += "  <p class=\"doc-brief\">" + escapeHtml(cl.doc.brief) + "</p>\n";
            if (!cl.fields.empty()) {
                out += "  <h4>Properties</h4>\n  <table class=\"table\">\n";
                out += "    <thead><tr><th>Property</th><th>Type</th><th>Description</th></tr></thead>\n<tbody>\n";
                for (const auto& f : cl.fields) {
                    out += "      <tr><td><code>" + escapeHtml(f.name) + "</code></td><td><code>" + escapeHtml(f.type) + "</code></td><td>" + (f.doc.brief.empty() ? "-" : escapeHtml(f.doc.brief)) + "</td></tr>\n";
                }
                out += "    </tbody>\n  </table>\n";
            }
            for (const auto& m : cl.methods) {
                renderFunctionHtml(out, m, cl.name);
            }
            out += "</div>\n";
        }

        // Interfaces
        for (const auto& in : mod.interfaces) {
            out += "<div class=\"doc-card\" id=\"interface_" + escapeHtml(in.name) + "\">\n";
            out += "  <div class=\"doc-header\">\n";
            out += "    <span class=\"badge badge-interface\">interface</span>\n";
            out += "    <h3 class=\"doc-title\">" + escapeHtml(in.name) + "</h3>\n";
            out += "  </div>\n";
            if (!in.doc.brief.empty()) out += "  <p class=\"doc-brief\">" + escapeHtml(in.doc.brief) + "</p>\n";
            for (const auto& m : in.methods) {
                renderFunctionHtml(out, m, in.name);
            }
            out += "</div>\n";
        }

        // Enums
        for (const auto& en : mod.enums) {
            out += "<div class=\"doc-card\" id=\"enum_" + escapeHtml(en.name) + "\">\n";
            out += "  <div class=\"doc-header\">\n";
            out += "    <span class=\"badge badge-enum\">enum</span>\n";
            out += "    <h3 class=\"doc-title\">" + escapeHtml(en.name) + "</h3>\n";
            out += "  </div>\n";
            if (!en.doc.brief.empty()) out += "  <p class=\"doc-brief\">" + escapeHtml(en.doc.brief) + "</p>\n";
            if (!en.variants.empty()) {
                out += "  <h4>Variants</h4>\n  <table class=\"table\">\n";
                out += "    <thead><tr><th>Variant</th><th>Value</th><th>Description</th></tr></thead>\n<tbody>\n";
                for (const auto& v : en.variants) {
                    out += "      <tr><td><code>" + escapeHtml(v.name) + "</code></td><td>" + (v.explicitValue.empty() ? "-" : "<code>" + escapeHtml(v.explicitValue) + "</code>") + "</td><td>" + (v.doc.brief.empty() ? "-" : escapeHtml(v.doc.brief)) + "</td></tr>\n";
                }
                out += "    </tbody>\n  </table>\n";
            }
            out += "</div>\n";
        }

        out += "    </section>\n";
    }

    out += "    <footer>Generated by SantientPHP docgen.</footer>\n";
    out += "  </main>\n";

    // Search filter script
    out += "  <script>\n";
    out += "    function filterSymbols() {\n";
    out += "      const query = document.getElementById('search').value.toLowerCase();\n";
    out += "      const items = document.querySelectorAll('.nav-item');\n";
    out += "      items.forEach(item => {\n";
    out += "        const text = item.textContent.toLowerCase();\n";
    out += "        item.style.display = text.includes(query) ? '' : 'none';\n";
    out += "      });\n";
    out += "    }\n";
    out += "  </script>\n";
    out += "</body>\n</html>\n";

    return out;
}

std::string DocGenerator::generateHtml(const DocModule& module) const {
    DocProject project;
    project.title = module.name;
    project.modules.push_back(module);
    return generateHtml(project);
}

bool DocGenerator::generateToFile(const DocProject& project, const std::filesystem::path& outPath,
                                  std::string_view format) const {
    std::filesystem::path parent = outPath.parent_path();
    if (!parent.empty()) {
        std::error_code ec;
        std::filesystem::create_directories(parent, ec);
    }

    std::ofstream file(outPath);
    if (!file.is_open()) return false;

    if (format == "md" || format == "markdown") {
        file << generateMarkdown(project);
    } else {
        file << generateHtml(project);
    }
    return true;
}

} // namespace sph

