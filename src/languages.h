#pragma once

#include <QString>
#include <QVariantList>

// The languages Google Translate accepts, named the way their speakers write
// them, so the picker needs no translation of its own. "auto" is only valid
// as a source language.
QString languageName(const QString &code);
QString languageEnglishName(const QString &code);
bool isSupportedLanguage(const QString &code);
QVariantList languageOptions(bool includeAuto);

// The language to translate into when the person hasn't picked one: Omarchy's
// own language when Google has it, English otherwise. Omarchy codes that
// Google spells differently (pt-BR, zh-CN) are mapped over.
QString targetForOmarchyLanguage(const QString &code);
