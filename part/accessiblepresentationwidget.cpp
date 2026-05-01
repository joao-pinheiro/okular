#ifndef ACCESSIBLRESENTATION_WIDGET
#define ACCESSIBLE_PRESENTATION_WIDGET

#include "accessiblepresentationwidget.h"
#include "core/document.h"
#include "core/page.h"
#include "presentationwidget.h"
#include <QDebug>

/*!
  Constructs an AccessiblePresentationWidget object for \a w.
  \a name is propagated to the QAccessibleWidget constructor.
*/
AccessiblePresentationWidget::AccessiblePresentationWidget(QWidget *w, const QString &name)
    : QAccessibleWidget(w, QAccessible::Document, name)
{
    // addControllingSignal( QLatin1String("textChanged(const QString&)"));
    // addControllingSignal("returnPressed()"_L1);
}

/*! Returns the presentation widget. */
PresentationWidget *AccessiblePresentationWidget::presentationWidget() const
{
    return qobject_cast<PresentationWidget *>(object());
}

QString AccessiblePresentationWidget::text(QAccessible::Text t) const
{
    QString str;
    if (t == QAccessible::Value) {
        if (presentationWidget()->m_document->isOpened()) {
            str = presentationWidget()->m_document->text();
        }
    }
    if (str.isEmpty())
        str = QAccessibleWidget::text(t);
    // not sure if we need to do specific things for Name and Description
    // or leave the accessibleName placeholder defined in the PageView class
    return str;
}

void AccessiblePresentationWidget::setText(QAccessible::Text t, const QString &text)
{
    if (t != QAccessible::Value) {
        QAccessibleWidget::setText(t, text);
        return;
    }
}

QAccessible::State AccessiblePresentationWidget::state() const
{
    QAccessible::State state = QAccessibleWidget::state();
    // we are setting it read-only for now
    state.editable = false;
    state.readOnly = true;
    state.selectableText = true;
    return state;
}

void *AccessiblePresentationWidget::interface_cast(QAccessible::InterfaceType t)
{
    if (t == QAccessible::TextInterface)
        return static_cast<QAccessibleTextInterface *>(this);
    return QAccessibleWidget::interface_cast(t);
}

QString AccessiblePresentationWidget::attributes(int offset, int *startOffset, int *endOffset) const
{
    // PresentationWidget doesn't have text attributes
    Q_UNUSED(offset);
    *startOffset = -1;
    *endOffset = -1;
    return QString();
}

int AccessiblePresentationWidget::cursorPosition() const
{
    if (!presentationWidget()->m_document->isOpened()) {
        return -1;
    }
    return presentationWidget()->m_document->offset();
}

QRect AccessiblePresentationWidget::characterRect(int offset) const
{
    if (!presentationWidget()->m_document->isOpened() || (presentationWidget()->m_frameIndex == -1)) {
        return QRect();
    }
    // scale normalized co-ordinates for the charBox and  adjust for any frame offsets.
    const QRect fg = presentationWidget()->fGeometry();
    double leftOffset = (presentationWidget()->m_width - fg.width()) / 2;
    double topOffset = (presentationWidget()->m_height - fg.height()) / 2;
    QRect r;
    std::unique_ptr<Okular::NormalizedRect> nRect = presentationWidget()->m_document->cPage()->charBoxAtOffset(offset);
    // This version doesn't check pageAtOffset, but assumes current page is in sync.
    // if the offset is off the page, nothing happens
    if (nRect) {
        // scale
        r = nRect->geometry(fg.width(), fg.height());
        // adjust offsets
        r.translate(leftOffset, topOffset);
    }
    return r;
}

int AccessiblePresentationWidget::offsetAtPoint(const QPoint &point) const
{
    if (!presentationWidget()->m_document->isOpened() || (presentationWidget()->m_frameIndex < 0)) {
        return -1;
    }
    // we don't need to worry about crop, as this is full screen
    // but there may well be an  or y x offset
    const QRect fg = presentationWidget()->fGeometry();
    double leftOffset = (presentationWidget()->m_width - fg.width()) / 2;
    double topOffset = (presentationWidget()->m_height - fg.height()) / 2;
    QPoint contentPos = presentationWidget()->mapFromGlobal(point) - QPoint(leftOffset, topOffset);
    return presentationWidget()->m_document->cPage()->offsetAtPoint(Okular::NormalizedPoint(contentPos.x() / fg.width(), contentPos.y() / fg.height()));
}

QString AccessiblePresentationWidget::text(int startOffset, int endOffset) const
{
    // sanity check - queries may happen with an ex-document
    if (!presentationWidget()->m_document->isOpened()) {
        return QString();
    }

    if (startOffset >= endOffset) {
        return QString();
    } else {
        // Note: endOffset is the first position *after* the text to be returned
        QString s = presentationWidget()->m_document->textBetween(startOffset, endOffset - 1);
        return s;
    }
}

QString AccessiblePresentationWidget::textBeforeOffset(int offset, QAccessible::TextBoundaryType boundaryType, int *startOffset, int *endOffset) const
{
    if (!presentationWidget()->m_document->isOpened()) {
        return QString();
    }

    if (presentationWidget()->m_document->pageAtOffset(offset) != presentationWidget()->m_document->cPageNum()) {
        offset = presentationWidget()->m_document->offset();
    }

    if (offset == -2) {
        offset = cursorPosition();
        return QAccessibleTextInterface::textBeforeOffset(offset, boundaryType, startOffset, endOffset);
    }

    QString s;
    int textStart = -1;
    switch (boundaryType) {
    case QAccessible::CharBoundary:
        textStart = offset - 1;
        s = presentationWidget()->m_document->charAtOffset(offset);
        break;
    case QAccessible::WordBoundary:
        textStart = presentationWidget()->m_document->prevWordStart(offset);
        s = presentationWidget()->m_document->wordAtOffset(textStart);
        break;
        //    case QAccessible::sentenceBoundary:
        // TODO
        //        break;
    case QAccessible::LineBoundary:
    case QAccessible::ParagraphBoundary:
        textStart = presentationWidget()->m_document->prevLineStart(offset);
        s = presentationWidget()->m_document->lineAtOffset(textStart);
        break;
    default:
        break;
    }

    // more bounds checking
    if (presentationWidget()->m_document->pageAtOffset(textStart) != presentationWidget()->m_document->cPageNum()) {
        textStart = -1;
    }

    if (textStart > -1) {
        *startOffset = textStart;
        *endOffset = textStart + s.size();
        return s;
    } else {
        *startOffset = -1;
        *endOffset = -1;
        return QString();
    }
}

QString AccessiblePresentationWidget::textAfterOffset(int offset, QAccessible::TextBoundaryType boundaryType, int *startOffset, int *endOffset) const
{
    if (!presentationWidget()->m_document->isOpened()) {
        return QString();
    }
    if (presentationWidget()->m_document->pageAtOffset(offset) != presentationWidget()->m_document->cPageNum()) {
        offset = presentationWidget()->m_document->offset();
    }

    if (offset == -2) {
        offset = cursorPosition();
        return QAccessibleTextInterface::textAfterOffset(offset, boundaryType, startOffset, endOffset);
    }

    QString s;
    int textStart = -1;
    switch (boundaryType) {
    case QAccessible::CharBoundary:
        textStart = offset + 1;
        s = presentationWidget()->m_document->charAtOffset(offset);
        break;
    case QAccessible::WordBoundary:
        textStart = presentationWidget()->m_document->nextWordStart(offset);
        s = presentationWidget()->m_document->wordAtOffset(textStart);
        break;
        //    case QAccessible::sentenceBoundary:
        // TODO
        //        break;
    case QAccessible::LineBoundary:
    case QAccessible::ParagraphBoundary:
        textStart = presentationWidget()->m_document->nextLineStart(offset);
        s = presentationWidget()->m_document->lineAtOffset(textStart);
        break;
    default:
        break;
    }

    // more bounds checking
    if (presentationWidget()->m_document->pageAtOffset(textStart) != presentationWidget()->m_document->cPageNum()) {
        textStart = -1;
    }

    if (textStart > -1) {
        *startOffset = textStart;
        *endOffset = textStart + s.size();
        return s;
    } else {
        *startOffset = -1;
        *endOffset = -1;
        return QString();
    }
}

QString AccessiblePresentationWidget::textAtOffset(int offset, QAccessible::TextBoundaryType boundaryType, int *startOffset, int *endOffset) const
{
    // sanity check - queries may happen with an ex-document
    if (!presentationWidget()->m_document->isOpened()) {
        return QString();
    }

    // sanity check: are we on the same page?
    // as these are lookaround functions, they are page-specific
    // if continuous mode  works, this may change
    // but as things stand, the offset  seems unpredictable
    if (presentationWidget()->m_document->pageAtOffset(offset) != presentationWidget()->m_document->cPageNum()) {
        offset = presentationWidget()->m_document->offset();
    }

    if (offset == -2) {
        offset = cursorPosition();
        return QAccessibleTextInterface::textAtOffset(offset, boundaryType, startOffset, endOffset);
    }

    QString s;
    int textStart = -1;
    switch (boundaryType) {
    case QAccessible::CharBoundary:
        textStart = offset;
        s = presentationWidget()->m_document->charAtOffset(offset);
        break;
    case QAccessible::WordBoundary:
        textStart = presentationWidget()->m_document->wordStart(offset);
        s = presentationWidget()->m_document->wordAtOffset(textStart);
        break;
        //    case QAccessible::sentenceBoundary:
        // TODO
        //        break;
    case QAccessible::LineBoundary:
    case QAccessible::ParagraphBoundary:
        textStart = presentationWidget()->m_document->lineStart(offset);
        s = presentationWidget()->m_document->lineAtOffset(textStart);
        break;
    default:
        break;
    }
    if (textStart > -1) {
        *startOffset = textStart;
        *endOffset = textStart + s.size();
        return s;
    } else {
        *startOffset = -1;
        *endOffset = -1;
        return QString();
    }
}

void AccessiblePresentationWidget::setCursorPosition(int position)
{
    if (!presentationWidget()->m_document->isOpened()) {
        return;
    }
    presentationWidget()->updateTextCursor(presentationWidget()->m_document->setOffset(position));
}

int AccessiblePresentationWidget::characterCount() const
{
    // sanity check - queries may happen with an ex-document
    if (!presentationWidget()->m_document->isOpened()) {
        return 0;
    }

    return presentationWidget()->m_document->end() + 1;
}

void AccessiblePresentationWidget::scrollToSubstring(int startIndex, int endIndex)
{
    /// moving the text cursor also scrolls the display
    // scrolling to the last position first is a bit of a hack
    // implementing a RegularAreaRect might be a better way of doing it,
    // without feeling seasick!
    presentationWidget()->updateTextCursor(presentationWidget()->m_document->setOffset(endIndex));
    presentationWidget()->updateTextCursor(presentationWidget()->m_document->setOffset(startIndex));
}

// Selecting text
void AccessiblePresentationWidget::addSelection(int startOffset, int endOffset)
{
    setSelection(0, startOffset, endOffset);
}

int AccessiblePresentationWidget::selectionCount() const
{
    if (!presentationWidget()->m_document->isOpened()) {
        return 0;
    }
    return (presentationWidget()->m_document->anchorTextCursor() > -1) ? 1 : 0;
}

void AccessiblePresentationWidget::selection(int selectionIndex, int *startOffset, int *endOffset) const
{
    if ((selectionIndex != 0) || !presentationWidget()->m_document->isOpened() || (presentationWidget()->m_document->anchorTextCursor() < 0)) {
        *startOffset = -1;
        *endOffset = -1;
    } else {
        if (presentationWidget()->m_document->anchorTextCursor() > presentationWidget()->m_document->offset()) {
            *startOffset = presentationWidget()->m_document->offset();
            *endOffset = presentationWidget()->m_document->anchorTextCursor();
        } else {
            *startOffset = presentationWidget()->m_document->anchorTextCursor();
            *endOffset = presentationWidget()->m_document->offset();
        }
    }
}

void AccessiblePresentationWidget::removeSelection(int selectionIndex)
{
    if ((selectionIndex != 0) || !presentationWidget()->m_document->isOpened() || (presentationWidget()->m_document->anchorTextCursor() < 0)) {
        return;
    } else {
        presentationWidget()->unsetTextSelection();
    }
}

void AccessiblePresentationWidget::setSelection(int selectionIndex, int startOffset, int endOffset)
{
    if ((selectionIndex != 0) || !presentationWidget()->m_document->isOpened()) {
        return;
    } else {
        presentationWidget()->updateTextCursor(presentationWidget()->m_document->setTextSelection(startOffset, endOffset), true);
    }
}

#endif
