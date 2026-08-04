/*
    SPDX-FileCopyrightText: 2017 Julian Wolff <wolff@julianwolff.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef MARKDOWN_CONVERTER_H
#define MARKDOWN_CONVERTER_H

#include <QCache>
#include <QDir>
#include <QImage>
#include <QTextFragment>

#include <core/textdocumentgenerator.h>

class QTextBlock;
class QTextFrame;

namespace Markdown
{
class Converter : public Okular::TextDocumentConverter
{
    Q_OBJECT

public:
    Converter();
    ~Converter() override;

    QTextDocument *convert(const QString &fileName) override;

    void convertAgain();

    void setFancyPantsEnabled(bool b)
    {
        m_isFancyPantsEnabled = b;
    }
    bool isFancyPantsEnabled() const
    {
        return m_isFancyPantsEnabled;
    }

    void setLatexEnabled(bool b)
    {
        m_latexEnabled = b;
    }
    bool isLatexEnabled() const
    {
        return m_latexEnabled;
    }

    QTextDocument *convertOpenFile();

private:
    void processMath(QTextDocument *textDocument);
    QImage renderLatexToImage(const QString &equation, bool display);

    void extractLinks(QTextFrame *parent, QHash<QString, QTextFragment> &internalLinks, QHash<QString, QTextBlock> &documentAnchors);
    void extractLinks(const QTextBlock &parent, QHash<QString, QTextFragment> &internalLinks, QHash<QString, QTextBlock> &documentAnchors);
    void convertImages(QTextFrame *parent, const QDir &dir, QTextDocument *textDocument);
    void convertImages(const QTextBlock &parent, const QDir &dir, QTextDocument *textDocument);
    void setImageSize(QTextImageFormat &format, const qreal specifiedWidth, const qreal specifiedHeight, const qreal originalWidth, const qreal originalHeight);

    FILE *m_markdownFile;
    QDir m_fileDir;
    bool m_isFancyPantsEnabled;
    bool m_latexEnabled;
    QCache<QString, QImage> m_latexCache;
};

namespace detail
{
QString fixupHtmlTags(QString &&html);
}

}

#endif
