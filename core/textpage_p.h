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

class TextPagePrivate
{
public:
    TextPagePrivate();
    ~TextPagePrivate();

    RegularAreaRect *
    findTextInternalForward(int searchID, const QString &query, TextComparisonFunction comparer, bool matchDiacritics, const TextEntity::List::ConstIterator start, int start_offset, const TextEntity::List::ConstIterator end);
    RegularAreaRect *
    findTextInternalBackward(int searchID, const QString &query, TextComparisonFunction comparer, bool matchDiacritics, const TextEntity::List::ConstIterator start, int start_offset, const TextEntity::List::ConstIterator end);

    /**
     * Copy a TextList to m_words, the pointers of list are adopted
     */
    void setWordList(const TextEntity::List &list);

    /**
     * Make necessary modifications in the TextList to make the text order correct, so
     * that textselection works fine
     */
    void correctTextOrder();

    /**
     * The text entities in logical reading order, which is what string
     * extraction and searching operate on. Falls back to the visual order when
     * no reading order was computed.
     */
    const TextEntity::List &wordsInReadingOrder() const
    {
        return m_wordsReading.isEmpty() ? m_words : m_wordsReading;
    }

    // variables those can be accessed directly from TextPage
    TextEntity::List m_words;
    // The same entities as m_words but in logical reading order (for RTL
    // scripts the words of each line are reversed). Only text extraction uses
    // this; geometry/selection code keeps working on the visual m_words order.
    TextEntity::List m_wordsReading;
    QMap<int, SearchPoint *> m_searchPoints;
    Page *m_page;

private:
    RegularAreaRect *searchPointToArea(const SearchPoint *sp);
};

}

#endif
