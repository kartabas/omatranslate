#include "languages.h"

#include <QCoreApplication>
#include <QHash>

namespace {

struct Language {
    const char *code;
    const char *name;
    const char *english;
};

// Google's own codes; keep in step with what the translate endpoint accepts.
const Language Table[] = {
    {"af", "Afrikaans", "Afrikaans"},
    {"sq", "Shqip", "Albanian"},
    {"am", "አማርኛ", "Amharic"},
    {"ar", "العربية", "Arabic"},
    {"hy", "Հայերեն", "Armenian"},
    {"az", "Azərbaycan", "Azerbaijani"},
    {"eu", "Euskara", "Basque"},
    {"be", "Беларуская", "Belarusian"},
    {"bn", "বাংলা", "Bengali"},
    {"bs", "Bosanski", "Bosnian"},
    {"bg", "Български", "Bulgarian"},
    {"ca", "Català", "Catalan"},
    {"ceb", "Cebuano", "Cebuano"},
    {"zh-CN", "简体中文", "Chinese (Simplified)"},
    {"zh-TW", "繁體中文", "Chinese (Traditional)"},
    {"hr", "Hrvatski", "Croatian"},
    {"cs", "Čeština", "Czech"},
    {"da", "Dansk", "Danish"},
    {"nl", "Nederlands", "Dutch"},
    {"en", "English", "English"},
    {"eo", "Esperanto", "Esperanto"},
    {"et", "Eesti", "Estonian"},
    {"tl", "Filipino", "Filipino"},
    {"fi", "Suomi", "Finnish"},
    {"fr", "Français", "French"},
    {"gl", "Galego", "Galician"},
    {"ka", "ქართული", "Georgian"},
    {"de", "Deutsch", "German"},
    {"el", "Ελληνικά", "Greek"},
    {"gu", "ગુજરાતી", "Gujarati"},
    {"ht", "Kreyòl ayisyen", "Haitian Creole"},
    {"he", "עברית", "Hebrew"},
    {"hi", "हिन्दी", "Hindi"},
    {"hmn", "Hmoob", "Hmong"},
    {"hu", "Magyar", "Hungarian"},
    {"is", "Íslenska", "Icelandic"},
    {"id", "Bahasa Indonesia", "Indonesian"},
    {"ga", "Gaeilge", "Irish"},
    {"it", "Italiano", "Italian"},
    {"ja", "日本語", "Japanese"},
    {"kn", "ಕನ್ನಡ", "Kannada"},
    {"kk", "Қазақ тілі", "Kazakh"},
    {"km", "ខ្មែរ", "Khmer"},
    {"ko", "한국어", "Korean"},
    {"lo", "ລາວ", "Lao"},
    {"la", "Latina", "Latin"},
    {"lv", "Latviešu", "Latvian"},
    {"lt", "Lietuvių", "Lithuanian"},
    {"mk", "Македонски", "Macedonian"},
    {"ms", "Bahasa Melayu", "Malay"},
    {"mt", "Malti", "Maltese"},
    {"mr", "मराठी", "Marathi"},
    {"mn", "Монгол", "Mongolian"},
    {"my", "မြန်မာ", "Myanmar (Burmese)"},
    {"ne", "नेपाली", "Nepali"},
    {"no", "Norsk", "Norwegian"},
    {"fa", "فارسی", "Persian"},
    {"pl", "Polski", "Polish"},
    {"pt", "Português", "Portuguese"},
    {"pa", "ਪੰਜਾਬੀ", "Punjabi"},
    {"ro", "Română", "Romanian"},
    {"ru", "Русский", "Russian"},
    {"sr", "Српски", "Serbian"},
    {"sk", "Slovenčina", "Slovak"},
    {"sl", "Slovenščina", "Slovenian"},
    {"es", "Español", "Spanish"},
    {"sw", "Kiswahili", "Swahili"},
    {"sv", "Svenska", "Swedish"},
    {"ta", "தமிழ்", "Tamil"},
    {"te", "తెలుగు", "Telugu"},
    {"th", "ไทย", "Thai"},
    {"tr", "Türkçe", "Turkish"},
    {"uk", "Українська", "Ukrainian"},
    {"ur", "اردو", "Urdu"},
    {"uz", "Oʻzbek", "Uzbek"},
    {"vi", "Tiếng Việt", "Vietnamese"},
    {"cy", "Cymraeg", "Welsh"},
    {"yi", "ייִדיש", "Yiddish"},
    {"zu", "isiZulu", "Zulu"},
};

const Language *find(const QString &code)
{
    for (const auto &language : Table)
        if (code == QLatin1String(language.code))
            return &language;
    return nullptr;
}

}

QString languageName(const QString &code)
{
    if (code == QLatin1String("auto"))
        return QCoreApplication::translate("Backend", "Auto-detect");
    if (const auto *language = find(code))
        return QString::fromUtf8(language->name);
    return code;
}

QString languageEnglishName(const QString &code)
{
    if (const auto *language = find(code))
        return QString::fromUtf8(language->english);
    return code;
}

bool isSupportedLanguage(const QString &code)
{
    return find(code) != nullptr;
}

QVariantList languageOptions(bool includeAuto)
{
    QVariantList options;
    if (includeAuto)
        options.append(QVariantMap{{"code", "auto"}, {"name", languageName("auto")}});
    for (const auto &language : Table)
        options.append(QVariantMap{{"code", QString::fromUtf8(language.code)},
                                   {"name", QString::fromUtf8(language.name)},
                                   {"english", QString::fromUtf8(language.english)}});
    return options;
}

QString targetForOmarchyLanguage(const QString &code)
{
    static const QHash<QString, QString> aliases{
        {"pt-BR", "pt"}, {"zh", "zh-CN"}, {"zh-Hans", "zh-CN"}, {"zh-Hant", "zh-TW"},
        {"zh-CN", "zh-CN"}, {"zh-TW", "zh-TW"}, {"fil", "tl"},
    };
    if (aliases.contains(code) && isSupportedLanguage(aliases.value(code)))
        return aliases.value(code);
    if (isSupportedLanguage(code))
        return code;
    const QString base = code.section('-', 0, 0);
    if (aliases.contains(base) && isSupportedLanguage(aliases.value(base)))
        return aliases.value(base);
    if (isSupportedLanguage(base))
        return base;
    return QStringLiteral("en");
}
