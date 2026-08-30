/*
    SPDX-FileCopyrightText: 2006 Tobias Koenig <tokoe@kde.org>
    SPDX-FileCopyrightText: 2007 Pino Toscano <pino@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef _OKULAR_TEXTPAGE_P_H_
#define _OKULAR_TEXTPAGE_P_H_

#include "textpage.h"
#include <QList>
#include <QMap>
#include <QPair>
#include <QTransform>

class SearchPoint;

class RegionText;

namespace Okular
{
class PagePrivate;
class RegularAreaRect;
class Page;

/**
 * Returns whether the two strings match.
 * Satisfies the condition that if two strings match then their lengths are equal.
 */
typedef bool (*TextComparisonFunction)(QStringView from, const QStringView to);

/**
 * A list of RegionText. It keeps a bunch of TextList with their bounding rectangles
 */
typedef QList<RegionText> RegionTextList;

// A match with the area findText() returned; the area is owned by whoever holds the TextMatch.
struct TextMatch {
    RegularAreaRect *area = nullptr;
    SearchMatch match;
};

class TextPagePrivate
{
public:
    TextPagePrivate();
    ~TextPagePrivate();

    RegularAreaRect *findTextInternalForward(int searchID, const QString &query, TextComparisonFunction comparer, const TextEntity::List::ConstIterator start, int start_offset, const TextEntity::List::ConstIterator end);
    RegularAreaRect *findTextInternalBackward(int searchID, const QString &query, TextComparisonFunction comparer, const TextEntity::List::ConstIterator start, int start_offset, const TextEntity::List::ConstIterator end);

    /**
     * Copy a TextList to m_words, the pointers of list are adopted
     */
    void setWordList(const TextEntity::List &list);

    /**
     * Make necessary modifications in the TextList to make the text order correct, so
     * that textselection works fine. Width and height are passed in: this can run
     * off the main thread, where m_page's geometry may change concurrently.
     */
    void correctTextOrder(double pageWidth, double pageHeight);

    /** The matrix search results are transformed with: the page's rotation, or the identity when no page is attached. */
    QTransform searchMatrix() const;

    // variables those can be accessed directly from TextPage
    TextEntity::List m_words;
    QMap<int, SearchPoint *> m_searchPoints;
    /** Set by TextPage::prepareForPage() and nowhere else, so a non-null value
     *  also means the preparation ran: TextPage::isPreparedFor() is just a
     *  comparison against it. */
    const Page *m_page = nullptr;

private:
    RegularAreaRect *searchPointToArea(const SearchPoint *sp);
};

}

#endif
