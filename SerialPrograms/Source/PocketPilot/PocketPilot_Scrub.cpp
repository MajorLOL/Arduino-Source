/*  PocketPilot Scrub
 *
 *  Part of PocketPilot, which is based on Pokemon Automation
 *  (https://github.com/PokemonAutomation/).
 *
 */

#include <vector>
#include <QRegularExpression>
#include "PocketPilot_Scrub.h"

namespace PokemonAutomation{
namespace PocketPilot{


namespace{

struct Rule{
    QRegularExpression pattern;
    QString replacement;
};

const std::vector<Rule>& rules(){
    static const std::vector<Rule> RULES = []{
        const auto I = QRegularExpression::CaseInsensitiveOption;
        std::vector<Rule> list;
        //  A Discord webhook: the address is the whole secret.
        list.push_back({QRegularExpression("https?://(?:[a-z]+\\.)?discord(?:app)?\\.com/api/(?:v\\d+/)?webhooks/\\d+/[A-Za-z0-9._-]+", I), "<webhook address>"});
        //  The key of the Claude API.
        list.push_back({QRegularExpression("sk-ant-[A-Za-z0-9_-]{8,}"), "<api key>"});
        //  GitHub tokens, the new and the old forms.
        list.push_back({QRegularExpression("github_pat_[A-Za-z0-9_]{20,}"), "<token>"});
        list.push_back({QRegularExpression("\\bgh[pousr]_[A-Za-z0-9]{20,}"), "<token>"});
        //  A Discord bot token: three parts with dots.
        list.push_back({QRegularExpression("\\b[MNO][A-Za-z0-9_-]{23,27}\\.[A-Za-z0-9_-]{6}\\.[A-Za-z0-9_-]{27,}"), "<token>"});
        //  "Authorization: Bearer abc", "Bearer abc".
        list.push_back({QRegularExpression("\\bBearer\\s+[A-Za-z0-9._~+/=-]{8,}", I), "Bearer <hidden>"});
        //  token = value, "password": "value", api key: value.
        list.push_back({
            QRegularExpression(
                "(\\b(?:[A-Za-z_]*token|[A-Za-z_]*secret|password|passwd|api[ _-]?key|authorization)\\b[\"']?\\s*[:=]\\s*)(?:\"[^\"]*\"|'[^']*'|[^\\s,;]+)",
                I
            ),
            "\\1<hidden>"
        });
        //  The name of the account in a path (it may have spaces: then it is
        //  all up to the next slash).
        list.push_back({
            QRegularExpression(
                "([A-Za-z]:[\\\\/]+Users[\\\\/]+)(?:[^\\\\/\"'<>|:*?\\r\\n]+(?=[\\\\/])|[^\\\\/\\s\"'<>|:*?]+)",
                I
            ),
            "\\1<user>"
        });
        list.push_back({QRegularExpression("(/(?:Users|home)/)[^/\\s\"'<>|:*?]+"), "\\1<user>"});
        return list;
    }();
    return RULES;
}

}


QString scrub_private_text(const QString& text){
    QString out = text;
    for (const Rule& rule : rules()){
        out.replace(rule.pattern, rule.replacement);
    }
    return out;
}



}
}
