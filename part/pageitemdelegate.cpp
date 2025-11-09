/*
    SPDX-FileCopyrightText: 2006 Pino Toscano <pino@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "pageitemdelegate.h"

// qt/kde includes
#include <QAbstractItemDelegate>
#include <QApplication>
#include <QDebug>
#include <QHeaderView>
#include <QModelIndex>
#include <QTextDocument>
#include <QTextLayout>
#include <QVariant>
#include <qdebug.h>

// local includes
#include "gui/tocmodel.h"
#include "settings.h"

#define PAGEITEMDELEGATE_INTERNALMARGIN 3

class PageItemDelegate::Private
{
public:
    Private()
    {
    }

    QModelIndex index;
    QTreeView *parent;
    QAbstractItemModel *model;
};

PageItemDelegate::PageItemDelegate(QTreeView *parent, QAbstractItemModel *model)
    : QItemDelegate(parent)
    , d(new Private)
{
    d->parent = parent;
    d->model = model;
}

PageItemDelegate::~PageItemDelegate()
{
    delete d;
}

void PageItemDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    d->index = index;
    QItemDelegate::paint(painter, option, index);
}

void PageItemDelegate::drawDisplay(QPainter *painter, const QStyleOptionViewItem &option, const QRect &rect, const QString &text) const
{
    QVariant pageVariant = d->index.data(TOCModel::PageRole);
    QVariant labelVariant = d->index.data(TOCModel::PageLabelRole);
    if ((labelVariant.metaType().id() != QMetaType::QString && !pageVariant.canConvert(QMetaType {QMetaType::QString})) || !Okular::Settings::tocPageColumn()) {
        QItemDelegate::drawDisplay(painter, option, rect, text);
        return;
    }
    QString label = labelVariant.toString();
    QString page = label.isEmpty() ? pageVariant.toString() : label;
    QTextDocument document;
    document.setPlainText(page);
    document.setDefaultFont(option.font);
    int margindelta = QApplication::style()->pixelMetric(QStyle::PM_FocusFrameHMargin) + 1;
    int pageRectWidth = (int)document.size().width();
    QRect newRect(rect);
    QRect pageRect(rect);
    pageRect.setWidth(pageRectWidth + 2 * margindelta);
    newRect.setWidth(newRect.width() - pageRectWidth - PAGEITEMDELEGATE_INTERNALMARGIN);
    if (option.direction == Qt::RightToLeft) {
        newRect.translate(pageRectWidth + PAGEITEMDELEGATE_INTERNALMARGIN, 0);
    } else {
        pageRect.translate(newRect.width() + PAGEITEMDELEGATE_INTERNALMARGIN - 2 * margindelta, 0);
    }
    QItemDelegate::drawDisplay(painter, option, newRect, text);
    QStyleOptionViewItem newoption(option);
    newoption.displayAlignment = (option.displayAlignment & ~Qt::AlignHorizontal_Mask) | Qt::AlignRight;
    QItemDelegate::drawDisplay(painter, newoption, pageRect, page);
}

QSize PageItemDelegate::sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    if (!Okular::Settings::self()->tOCWordWrap()) {
        return QItemDelegate::sizeHint(option, index);
    }

    // small hack to get the current index level
    int indexLvl = 0;
    for (QModelIndex p = index.parent(); p.isValid(); p = p.parent()) {
        indexLvl++;
    }
    if (d->parent->rootIsDecorated()) {
        indexLvl++;
    }

    QFontMetrics metrics(index.data(Qt::FontRole).value<QFont>());

    int sectionSize = d->parent->header()->sectionSize(option.viewItemPosition);
    int indentSize = d->parent->indentation();

    int margindelta = QApplication::style()->pixelMetric(QStyle::PM_FocusFrameHMargin) + 1;

    // a copy of what's inside `drawDisplay` to get the correct width
    QVariant pageVariant = d->index.data(TOCModel::PageRole);
    QVariant labelVariant = d->index.data(TOCModel::PageLabelRole);
    /**
     * while debugging, sometimes the page number label (both pageVariant and labelVariant) return incorrect page numbers in this function.
     * in the future this should be looked into to improve this function
     */
    QString label = labelVariant.toString();
    QString page = label.isEmpty() ? pageVariant.toString() : label;
    QTextDocument document;
    document.setPlainText(page);
    document.setDefaultFont(option.font);
    int pageRectWidth = (int)document.size().width();

    // due to the page number not beign correct as noted aboce, we also add one digit character to compensate
    QSize sz = QSize(sectionSize - (indentSize * indexLvl) - pageRectWidth - PAGEITEMDELEGATE_INTERNALMARGIN - (2 * margindelta) - metrics.horizontalAdvance(u'0'), 10000);

    QRect outRect = metrics.boundingRect(QRect(QPoint(0, 0), sz), Qt::AlignLeft | Qt::TextWordWrap, index.data().toString());
    sz.setHeight(outRect.height());
    sz.setWidth(-1);
    qDebug() << "calc H" << outRect.height();
    return sz;
}

#include "moc_pageitemdelegate.cpp"
