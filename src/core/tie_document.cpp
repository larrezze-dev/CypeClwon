#include "core/tie_document.h"

#include <map>

namespace cc::core {

namespace {

const std::map<int, std::string>& texts() {
    static const std::map<int, std::string> all = {
#include "core/tie_document_data.inc"
    };
    return all;
}

/// Replaces the %s of the original's format strings, in order.
std::string fill(std::string text, const std::vector<std::string>& args) {
    for (const std::string& arg : args) {
        const std::size_t at = text.find("%s");
        if (at == std::string::npos) break;
        text.replace(at, 2, arg);
    }
    return text;
}

}  // namespace

const std::vector<TieBlock>& tieDocument() {
    static const std::vector<TieBlock> blocks = [] {
        const auto& t = texts();
        std::vector<TieBlock> out;
        const auto add = [&](TieBlockKind kind, int id, std::vector<std::string> args = {}, int number = 0) {
            out.push_back({kind, id, fill(t.at(id), args), number});
        };
        out.push_back({TieBlockKind::Title, 64121, "CÁLCULO DE TIRANTES EN CYPE 3D", 0});
        add(TieBlockKind::Paragraph, 64092);
        add(TieBlockKind::Paragraph, 64093);
        add(TieBlockKind::Paragraph, 64094);
        add(TieBlockKind::Paragraph, 64095);
        out.push_back({TieBlockKind::FigurePlaceholder, 0, {}, 0});
        add(TieBlockKind::ListItem, 64096, {"20 %"}, 1);
        add(TieBlockKind::ListItem, 64097, {}, 2);
        add(TieBlockKind::ListItem, 64098, {}, 3);
        add(TieBlockKind::Heading, 64099);
        add(TieBlockKind::Paragraph, 64100);
        add(TieBlockKind::Paragraph, 64101);
        add(TieBlockKind::ListItem, 64102, {}, 1);
        add(TieBlockKind::ListItem, 64103, {}, 2);
        add(TieBlockKind::ListItem, 64104, {}, 3);
        add(TieBlockKind::ListItem, 64105, {}, 4);
        add(TieBlockKind::Paragraph, 64106);
        out.push_back({TieBlockKind::FigurePlaceholder, 0, {}, 0});
        add(TieBlockKind::Caption, 64107);
        add(TieBlockKind::Caption, 64108);
        add(TieBlockKind::Caption, 64109);
        add(TieBlockKind::Caption, 64110, {"C=0", "T*=T+|C|"});  // ed3ayuda FUN_180077490 (REA)
        add(TieBlockKind::Caption, 64111);
        add(TieBlockKind::Caption, 64112);
        add(TieBlockKind::Caption, 64113, {"C*"});
        add(TieBlockKind::Caption, 64114, {"C*"});
        add(TieBlockKind::Caption, 64115);
        add(TieBlockKind::Caption, 64116);
        add(TieBlockKind::Caption, 64117);
        add(TieBlockKind::Caption, 64118);
        add(TieBlockKind::Paragraph, 64119);
        add(TieBlockKind::Paragraph, 64120);
        out.push_back({TieBlockKind::FigurePlaceholder, 0, {}, 0});
        return out;
    }();
    return blocks;
}

}  // namespace cc::core
