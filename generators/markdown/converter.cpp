/*
    SPDX-FileCopyrightText: 2017 Julian Wolff <wolff@julianwolff.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "converter.h"

#include <QGuiApplication>
#include <QPalette>
#include <QProcess>
#include <QRegularExpression>
#include <QScreen>
#include <QTemporaryDir>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextFrame>
#include <QTextStream>

#include <KLocalizedString>

#include <core/action.h>

#include "debug_md.h"

extern "C" {
#include <mkdio.h>
}

#define PAGE_WIDTH 980
#define PAGE_HEIGHT 1307
#define PAGE_MARGIN 45
#define CONTENT_WIDTH (PAGE_WIDTH - 2 * PAGE_MARGIN)

using namespace Markdown;

QString detail::fixupHtmlTags(QString &&html)
{
    // Discount emits <del> tags for ~~ but Qt doesn't understand them and
    // removes them. Instead replace them with <s> tags which Qt does
    // understand.
    return html.replace(QStringLiteral("<del>"), QStringLiteral("<s>")).replace(QStringLiteral("</del>"), QStringLiteral("</s>"));
}

Converter::Converter()
    : m_markdownFile(nullptr)
    , m_isFancyPantsEnabled(true)
    , m_latexEnabled(false)
{
    m_latexCache.setMaxCost(100);
}

Converter::~Converter()
{
    if (m_markdownFile) {
        fclose(m_markdownFile);
    }
}

QTextDocument *Converter::convert(const QString &fileName)
{
    if (m_markdownFile) {
        fclose(m_markdownFile);
    }
    m_markdownFile = fopen(fileName.toLocal8Bit().constData(), "rb");
    if (!m_markdownFile) {
        Q_EMIT error(i18n("Failed to open the document"), -1);
        return nullptr;
    }

    m_fileDir = QDir(fileName.left(fileName.lastIndexOf(QLatin1Char('/'))));

    QTextDocument *doc = convertOpenFile();
    QHash<QString, QTextFragment> internalLinks;
    QHash<QString, QTextBlock> documentAnchors;
    extractLinks(doc->rootFrame(), internalLinks, documentAnchors);

    for (auto linkIt = internalLinks.constBegin(); linkIt != internalLinks.constEnd(); ++linkIt) {
        auto anchorIt = documentAnchors.constFind(linkIt.key());
        if (anchorIt != documentAnchors.constEnd()) {
            const Okular::DocumentViewport viewport = calculateViewport(doc, anchorIt.value());
            Okular::GotoAction *action = new Okular::GotoAction(QString(), viewport);
            Q_EMIT addAction(action, linkIt.value().position(), linkIt.value().position() + linkIt.value().length());
        } else {
            qDebug() << "Could not find destination for" << linkIt.key();
        }
    }

    return doc;
}

void Converter::convertAgain()
{
    setDocument(convertOpenFile());
}

QTextDocument *Converter::convertOpenFile()
{
    int result = fseek(m_markdownFile, 0, SEEK_SET);
    if (result != 0) {
        Q_EMIT error(i18n("Failed to open the document"), -1);
        return nullptr;
    }

#if defined(MKD_NOLINKS)
    // on discount 2 MKD_NOLINKS is a define
    MMIOT *markdownHandle = mkd_in(m_markdownFile, 0);

    int flags = MKD_FENCEDCODE | MKD_GITHUBTAGS | MKD_AUTOLINK | MKD_TOC | MKD_IDANCHOR;
    if (!m_isFancyPantsEnabled) {
        flags |= MKD_NOPANTS;
    }
    if (!mkd_compile(markdownHandle, flags)) {
        Q_EMIT error(i18n("Failed to compile the Markdown document."), -1);
        return nullptr;
    }
#else
    // on discount 3 MKD_NOLINKS is an enum value
    MMIOT *markdownHandle = mkd_in(m_markdownFile, nullptr);

    mkd_flag_t *flags = mkd_flags();
    // These flags aren't bitflags, so they can't be | together
    mkd_set_flag_num(flags, MKD_FENCEDCODE);
    mkd_set_flag_num(flags, MKD_GITHUBTAGS);
    mkd_set_flag_num(flags, MKD_AUTOLINK);
    mkd_set_flag_num(flags, MKD_TOC);
    mkd_set_flag_num(flags, MKD_IDANCHOR);
    if (!m_isFancyPantsEnabled) {
        mkd_set_flag_num(flags, MKD_NOPANTS);
    }
    if (!mkd_compile(markdownHandle, flags)) {
        Q_EMIT error(i18n("Failed to compile the Markdown document."), -1);
        mkd_free_flags(flags);
        return nullptr;
    }
    mkd_free_flags(flags);
#endif

    char *htmlDocument;
    const int size = mkd_document(markdownHandle, &htmlDocument);

    const QString html = detail::fixupHtmlTags(QString::fromUtf8(htmlDocument, size));

    QTextDocument *textDocument = new QTextDocument;
    textDocument->setPageSize(QSizeF(PAGE_WIDTH, PAGE_HEIGHT));
    textDocument->setHtml(html);
    if (generator()) {
        textDocument->setDefaultFont(generator()->generalSettings()->font());
    }

    processMath(textDocument);

    mkd_cleanup(markdownHandle);

    QTextFrameFormat frameFormat;
    frameFormat.setMargin(PAGE_MARGIN);

    QTextFrame *rootFrame = textDocument->rootFrame();
    rootFrame->setFrameFormat(frameFormat);

    convertImages(rootFrame, m_fileDir, textDocument);

    return textDocument;
}

void Converter::processMath(QTextDocument *textDocument)
{
    if (!m_latexEnabled) {
        return;
    }

    int imageId = 0;
    QRegularExpression displayMath(QStringLiteral("(?<!(?<!\\\\)\\\\)\\$\\$(.*?)(?<!(?<!\\\\)\\\\)\\$\\$"), QRegularExpression::DotMatchesEverythingOption);
    QRegularExpression inlineMath(QStringLiteral("(?<!(?<!\\\\)\\\\)\\$(?!\\s)(.*?)(?<!\\s)(?<!(?<!\\\\)\\\\)\\$"), QRegularExpression::DotMatchesEverythingOption);

    QTextCursor cursor(textDocument);
    cursor.beginEditBlock();

    // Collect display math matches
    QList<QTextCursor> displayMatches;
    QTextCursor matchCursor(textDocument);
    while (!(matchCursor = textDocument->find(displayMath, matchCursor)).isNull()) {
        displayMatches.prepend(matchCursor);
    }

    for (auto &match : displayMatches) {
        if (match.charFormat().fontFixedPitch()) {
            continue;
        }

        QString equation = match.selectedText();
        equation = equation.mid(2, equation.length() - 4); // Strip $$
        equation.replace(QChar::ParagraphSeparator, QLatin1Char('\n'));

        const QImage img = renderLatexToImage(equation, true);
        if (img.isNull() || img.width() == 0 || img.height() == 0) {
            continue;
        }

        const QString resourceName = QStringLiteral("latex_%1").arg(imageId++);
        textDocument->addResource(QTextDocument::ImageResource, QUrl(resourceName), img);
        QTextImageFormat format;
        format.setName(resourceName);
        const qreal dpr = QGuiApplication::primaryScreen() ? QGuiApplication::primaryScreen()->devicePixelRatio() : 1.0;
        format.setWidth(img.width() / dpr);
        format.setHeight(img.height() / dpr);
        match.insertImage(format);
    }

    // Collect inline math matches
    QList<QTextCursor> inlineMatches;
    matchCursor = QTextCursor(textDocument);
    while (!(matchCursor = textDocument->find(inlineMath, matchCursor)).isNull()) {
        inlineMatches.prepend(matchCursor);
    }

    for (auto &match : inlineMatches) {
        if (match.charFormat().fontFixedPitch()) {
            continue;
        }

        QString equation = match.selectedText();
        equation = equation.mid(1, equation.length() - 2); // Strip $
        equation.replace(QChar::ParagraphSeparator, QLatin1Char('\n'));

        const QImage img = renderLatexToImage(equation, false);
        if (img.isNull() || img.width() == 0 || img.height() == 0) {
            continue;
        }

        const QString resourceName = QStringLiteral("latex_%1").arg(imageId++);
        textDocument->addResource(QTextDocument::ImageResource, QUrl(resourceName), img);
        QTextImageFormat format;
        format.setName(resourceName);
        const qreal dpr = QGuiApplication::primaryScreen() ? QGuiApplication::primaryScreen()->devicePixelRatio() : 1.0;
        format.setWidth(img.width() / dpr);
        format.setHeight(img.height() / dpr);
        match.insertImage(format);
    }

    cursor.endEditBlock();
}

QImage Converter::renderLatexToImage(const QString &equation, bool display)
{
    const int fontSize = generator() ? generator()->generalSettings()->font().pointSize() : 12;
    const QColor textColor = QGuiApplication::palette().color(QPalette::WindowText);
    const QString cacheKey = equation + QLatin1Char('_') + (display ? QLatin1Char('1') : QLatin1Char('0')) + QLatin1Char('_') + QString::number(fontSize) + QLatin1Char('_') + textColor.name();

    QImage *cachedImg = m_latexCache.object(cacheKey);
    if (cachedImg) {
        return *cachedImg;
    }

    QTemporaryDir tempDir;
    if (!tempDir.isValid()) {
        return QImage();
    }

    const QString texFilePath = tempDir.path() + QLatin1String("/math.tex");
    QFile texFile(texFilePath);
    if (!texFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return QImage();
    }

    QTextStream out(&texFile);
    out << "\\documentclass[" << fontSize << "pt]{article}\n"
        << "\\usepackage{amsmath}\n"
        << "\\usepackage{amsfonts}\n"
        << "\\usepackage{amssymb}\n"
        << "\\usepackage{xcolor}\n"
        << "\\pagestyle{empty}\n"
        << "\\begin{document}\n"
        << "\\color[rgb]{" << textColor.redF() << "," << textColor.greenF() << "," << textColor.blueF() << "}\n"
        << (display ? "\\[\n" : "$") << equation << (display ? "\n\\]\n" : "$\n") << "\\end{document}\n";
    texFile.close();

    QProcess latexProcess;
    latexProcess.setWorkingDirectory(tempDir.path());
    latexProcess.start(QStringLiteral("latex"), {QStringLiteral("-interaction=nonstopmode"), QStringLiteral("-halt-on-error"), QStringLiteral("-no-shell-escape"), QStringLiteral("-output-directory=") + tempDir.path(), texFilePath});
    if (!latexProcess.waitForFinished(5000)) {
        latexProcess.kill();
        return QImage();
    }
    if (latexProcess.exitCode() != 0) {
        return QImage();
    }

    const qreal dpr = QGuiApplication::primaryScreen() ? QGuiApplication::primaryScreen()->devicePixelRatio() : 1.0;
    QProcess dvisvgmProcess;
    dvisvgmProcess.setWorkingDirectory(tempDir.path());
    dvisvgmProcess.start(QStringLiteral("dvisvgm"), {QStringLiteral("--no-fonts"), QStringLiteral("--exact"), QStringLiteral("--scale=") + QString::number(dpr), QStringLiteral("math.dvi")});
    if (!dvisvgmProcess.waitForFinished(5000)) {
        dvisvgmProcess.kill();
        return QImage();
    }
    if (dvisvgmProcess.exitCode() != 0) {
        return QImage();
    }

    QImage img;
    if (img.load(tempDir.path() + QLatin1String("/math.svg"))) {
        m_latexCache.insert(cacheKey, new QImage(img));
        return img;
    }
    return QImage();
}

void Converter::extractLinks(QTextFrame *parent, QHash<QString, QTextFragment> &internalLinks, QHash<QString, QTextBlock> &documentAnchors)
{
    for (QTextFrame::iterator it = parent->begin(); !it.atEnd(); ++it) {
        QTextFrame *textFrame = it.currentFrame();
        const QTextBlock textBlock = it.currentBlock();

        if (textFrame) {
            extractLinks(textFrame, internalLinks, documentAnchors);
        } else if (textBlock.isValid()) {
            extractLinks(textBlock, internalLinks, documentAnchors);
        }
    }
}

void Converter::extractLinks(const QTextBlock &parent, QHash<QString, QTextFragment> &internalLinks, QHash<QString, QTextBlock> &documentAnchors)
{
    for (QTextBlock::iterator it = parent.begin(); !it.atEnd(); ++it) {
        const QTextFragment textFragment = it.fragment();
        if (textFragment.isValid()) {
            const QTextCharFormat textCharFormat = textFragment.charFormat();
            if (textCharFormat.isAnchor()) {
                const QString href = textCharFormat.anchorHref();
                if (href.startsWith(QLatin1Char('#'))) { // It's an internal link, store it and we'll resolve it at the end
                    internalLinks.insert(href.mid(1), textFragment);
                } else {
                    Okular::BrowseAction *action = new Okular::BrowseAction(QUrl(textCharFormat.anchorHref()));
                    Q_EMIT addAction(action, textFragment.position(), textFragment.position() + textFragment.length());
                }

                const QStringList anchorNames = textCharFormat.anchorNames();
                for (const QString &anchorName : anchorNames) {
                    documentAnchors.insert(anchorName, parent);
                }
            }
        }
    }
}

void Converter::convertImages(QTextFrame *parent, const QDir &dir, QTextDocument *textDocument)
{
    for (QTextFrame::iterator it = parent->begin(); !it.atEnd(); ++it) {
        QTextFrame *textFrame = it.currentFrame();
        const QTextBlock textBlock = it.currentBlock();

        if (textFrame) {
            convertImages(textFrame, dir, textDocument);
        } else if (textBlock.isValid()) {
            convertImages(textBlock, dir, textDocument);
        }
    }
}

void Converter::convertImages(const QTextBlock &parent, const QDir &dir, QTextDocument *textDocument)
{
    for (QTextBlock::iterator it = parent.begin(); !it.atEnd(); ++it) {
        const QTextFragment textFragment = it.fragment();
        if (textFragment.isValid()) {
            const QTextCharFormat textCharFormat = textFragment.charFormat();
            if (textCharFormat.isImageFormat()) {
                QTextImageFormat format;

                const qreal specifiedHeight = textCharFormat.toImageFormat().height();
                const qreal specifiedWidth = textCharFormat.toImageFormat().width();

                QTextCursor cursor(textDocument);
                cursor.setPosition(textFragment.position(), QTextCursor::MoveAnchor);
                cursor.setPosition(textFragment.position() + textFragment.length(), QTextCursor::KeepAnchor);

                const QString imageFilePath = QDir::cleanPath(dir.absoluteFilePath(QUrl::fromPercentEncoding(textCharFormat.toImageFormat().name().toUtf8())));

                if (QFile::exists(imageFilePath)) {
                    cursor.removeSelectedText();
                    format.setName(imageFilePath);
                    const QImage img = QImage(format.name());

                    setImageSize(format, specifiedWidth, specifiedHeight, img.width(), img.height());

                    cursor.insertImage(format);
                } else if ((!textCharFormat.toImageFormat().property(QTextFormat::ImageAltText).toString().isEmpty())) {
                    cursor.insertText(textCharFormat.toImageFormat().property(QTextFormat::ImageAltText).toString());
                }
            }
        }
    }
}

void Converter::setImageSize(QTextImageFormat &format, const qreal specifiedWidth, const qreal specifiedHeight, const qreal originalWidth, const qreal originalHeight)
{
    qreal width = 0;
    qreal height = 0;

    const bool hasSpecifiedSize = specifiedHeight > 0 || specifiedWidth > 0;
    if (hasSpecifiedSize) {
        width = specifiedWidth;
        height = specifiedHeight;
        if (width == 0 && originalHeight > 0) {
            width = originalWidth * height / originalHeight;
        } else if (height == 0 && originalWidth > 0) {
            height = originalHeight * width / originalWidth;
        }
    } else {
        width = originalWidth;
        height = originalHeight;
    }

    if (width > CONTENT_WIDTH) {
        height = height * CONTENT_WIDTH / width;
        width = CONTENT_WIDTH;
    }
    format.setWidth(width);
    format.setHeight(height);
}
