#include "ui/MessageFormatting.h"

#include <QRegularExpression>
#include <QString>

namespace devicehub::message_formatting {

namespace {
// Негативный lookbehind на \w/'.' перед '@' — не подхватывает "@" в
// email-подобных строках ("user@example.com"), где ему предшествует
// буква/цифра/точка, а не начало строки/пробел/знак пунктуации.
const QRegularExpression& mentionPattern() {
    static const QRegularExpression pattern(QStringLiteral(R"((?<![\w.])@([A-Za-z0-9_-]+))"));
    return pattern;
}
}  // namespace

QString highlightMentions(const QString& body) {
    QString result = body;
    result.replace(mentionPattern(), QStringLiteral("**@\\1**"));
    return result;
}

}  // namespace devicehub::message_formatting
