/*
    SPDX-FileCopyrightText: 2026 Pierre Bourré <bourre.pi@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef _SEARCHRESULTSMODEL_H_
#define _SEARCHRESULTSMODEL_H_

#include <QAbstractListModel>
#include <QString>

#include "core/area.h"
#include "core/document.h"
#include "core/textpage.h"
#include "okularpart_export.h"

namespace Okular
{
class Page;
}

/**
 * One match of a document-wide search: where it is, and the line of text it sits on.
 */
struct SearchResult {
    int pageNumber = -1;
    Okular::NormalizedRect boundingRect;
    QString snippet;
    /** Index of the match inside @c snippet, or -1 when it could not be located. */
    int matchOffset = -1;
    int matchLength = 0;
};

/** Flat list of the matches of a document-wide search, filled page by page while the scan runs. */
class OKULARPART_EXPORT SearchResultsModel : public QAbstractListModel
{
    Q_OBJECT

public:
    enum Roles {
        PageNumberRole = Qt::UserRole + 1, ///< int, zero based
        SnippetRole,                       ///< QString
        MatchOffsetRole,                   ///< int, index of the match in the snippet, -1 if unknown
        MatchLengthRole                    ///< int
    };

    explicit SearchResultsModel(QObject *parent = nullptr);
    ~SearchResultsModel() override;

    /** Default number of characters of context kept around a match. */
    static constexpr int DefaultSnippetLength = 120;

    /**
     * Builds the result for a match found at @p matchBoundingRect on @p page. The snippet
     * is the line the match sits on, shortened to @p maxSnippetLength characters around it.
     */
    static SearchResult searchResultForMatch(const Okular::Page *page, int pageNumber, const Okular::NormalizedRect &matchBoundingRect, int maxSnippetLength = DefaultSnippetLength);

    /**
     * Same, from a word list the caller already fetched with
     * Okular::Page::words(nullptr, Okular::TextPage::CentralPixelTextAreaInclusionBehaviour):
     * a page with many matches then walks its words once, not once per match.
     */
    static SearchResult searchResultForMatch(const Okular::TextEntity::List &pageWords, int pageNumber, const Okular::NormalizedRect &matchBoundingRect, int maxSnippetLength = DefaultSnippetLength);

    /**
     * Shortens @p result's snippet to at most @p maxLength characters taken around its
     * match, adding an ellipsis on each side that was cut, and moves its match offset
     * accordingly. A snippet already short enough is left untouched.
     */
    static void elideSnippetAroundMatch(SearchResult &result, int maxLength);

    /**
     * Returns the viewport that brings @p result's match into view, centred on it.
     */
    static Okular::DocumentViewport viewportForResult(const SearchResult &result);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;

    /**
     * Inserts a batch of results for one page, keeping the list in document page
     * order. Returns the row the batch landed at, or -1 when @p results is empty.
     */
    int insertResults(const QList<SearchResult> &results);

    /** Drops every result. */
    void clear();

    const SearchResult &resultAt(int row) const;

private:
    QList<SearchResult> m_results;
};

#endif
