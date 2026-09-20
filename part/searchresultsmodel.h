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
#include "okularpart_export.h"

/** One match of a document-wide search, with the line of text it sits on. */
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

    /** Shortens @p result's snippet to @p maxLength characters around its match, moving the match offset accordingly. */
    static void elideSnippetAroundMatch(SearchResult &result, int maxLength);

    /** The viewport centred on @p result's match. */
    static Okular::DocumentViewport viewportForResult(const SearchResult &result);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;

    /** Inserts one page's results in page order; returns the row they landed at, or -1 if empty. */
    int insertResults(const QList<SearchResult> &results);

    void clear();

    const SearchResult &resultAt(int row) const;

private:
    QList<SearchResult> m_results;
};

#endif
