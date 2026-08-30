/*
    SPDX-FileCopyrightText: 2026 Pierre Bourré <bourre.pi@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef _OKULAR_TEXTPAGEJOB_P_H_
#define _OKULAR_TEXTPAGEJOB_P_H_

#include <QList>
#include <QRunnable>
#include <QString>

namespace Okular
{
class Generator;
class Page;
class TextPage;
struct TextMatch;

/**
 * Extracts and searches one page's text on a worker thread. It never reads the
 * Page's geometry, which PagePrivate::rotateAt() changes with no locking: the
 * caller passes a snapshot. searchID and generation identify the scan it belongs to.
 */
class TextPageJob : public QRunnable
{
public:
    TextPageJob(Generator *generator, const Page *page, int pageNumber, int searchID, int generation, double pageWidth, double pageHeight, const QString &searchText, Qt::CaseSensitivity caseSensitivity);
    ~TextPageJob() override;

    void run() override;

    int pageNumber() const;
    int searchID() const;
    int generation() const;
    /** Ownership passes to the caller. Null when extraction was aborted. */
    TextPage *takeResult();
    /** In unrotated page coordinates; the areas pass to the caller. */
    QList<TextMatch> takeMatches();

private:
    Generator *m_generator;
    const Page *m_page;
    int m_pageNumber;
    int m_searchID;
    int m_generation;
    double m_pageWidth;
    double m_pageHeight;
    QString m_searchText;
    Qt::CaseSensitivity m_caseSensitivity;
    TextPage *m_result = nullptr;
    QList<TextMatch> m_matches;
};

}

#endif
