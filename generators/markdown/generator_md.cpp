/*
    SPDX-FileCopyrightText: 2017 Julian Wolff <wolff@julianwolff.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "generator_md.h"

#include <QCheckBox>

#include <KAboutData>
#include <KConfigDialog>
#include <KLocalizedString>

#include "converter.h"
#include "debug_md.h"

K_PLUGIN_CLASS_WITH_JSON(MarkdownGenerator, "libokularGenerator_md.json")

MarkdownGenerator::MarkdownGenerator(QObject *parent, const QVariantList &args)
    : Okular::TextDocumentGenerator(new Markdown::Converter, QStringLiteral("okular_markdown_generator_settings"), parent, args)
{
    Okular::TextDocumentSettings *mdSettings = generalSettings();

    mdSettings->addItemBool(QStringLiteral("SmartyPants"), m_isFancyPantsConfigEnabled, true);
    mdSettings->addItemBool(QStringLiteral("RenderLaTeX"), m_isRenderLaTeXConfigEnabled, false);
    mdSettings->load();
    m_wasFancyPantsConfigEnabled = m_isFancyPantsConfigEnabled;
    m_wasRenderLaTeXConfigEnabled = m_isRenderLaTeXConfigEnabled;
    Markdown::Converter *c = static_cast<Markdown::Converter *>(converter());
    c->setFancyPantsEnabled(m_isFancyPantsConfigEnabled);
    c->setLatexEnabled(m_isRenderLaTeXConfigEnabled);
}

bool MarkdownGenerator::reparseConfig()
{
    const bool textDocumentGeneratorChangedConfig = Okular::TextDocumentGenerator::reparseConfig();

    if (m_wasFancyPantsConfigEnabled != m_isFancyPantsConfigEnabled || m_wasRenderLaTeXConfigEnabled != m_isRenderLaTeXConfigEnabled) {
        m_wasFancyPantsConfigEnabled = m_isFancyPantsConfigEnabled;
        m_wasRenderLaTeXConfigEnabled = m_isRenderLaTeXConfigEnabled;

        Markdown::Converter *c = static_cast<Markdown::Converter *>(converter());
        c->setFancyPantsEnabled(m_isFancyPantsConfigEnabled);
        c->setLatexEnabled(m_isRenderLaTeXConfigEnabled);
        if (c->document()) {
            c->convertAgain();
            setTextDocument(c->document());
        }

        return true;
    }

    return textDocumentGeneratorChangedConfig;
}

void MarkdownGenerator::addPages(KConfigDialog *dlg)
{
    Okular::TextDocumentSettingsWidget *widget = new Okular::TextDocumentSettingsWidget();

    QCheckBox *enableSmartyPants = new QCheckBox(dlg);
    enableSmartyPants->setObjectName(QStringLiteral("kcfg_SmartyPants"));
    widget->addRow(i18n("Enable SmartyPants formatting"), enableSmartyPants);

    QCheckBox *enableRenderLaTeX = new QCheckBox(dlg);
    enableRenderLaTeX->setObjectName(QStringLiteral("kcfg_RenderLaTeX"));
    widget->addRow(i18n("Render LaTeX equations"), enableRenderLaTeX);

    dlg->addPage(widget, generalSettings(), i18n("Markdown"), QStringLiteral("text-markdown"), i18n("Markdown Backend Configuration"));
}

Q_LOGGING_CATEGORY(OkularMdDebug, "org.kde.okular.generators.md", QtWarningMsg)

#include "generator_md.moc"
