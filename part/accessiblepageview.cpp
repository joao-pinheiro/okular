#ifndef ACCESSIBLE_PAGE_VIEW
# define ACCESSIBLE_PAGE_VIEW

#include "core/page.h"
#include "accessiblepageview.h"
#include "pageview.h"
#include <QDebug>


/*!
  Constructs an AccessiblePageView object for \a w.
  \a name is propagated to the QAccessibleWidget constructor.
*/
AccessiblePageView::AccessiblePageView(QWidget *w, const QString &name)
: QAccessibleWidget(w, QAccessible::Document,  name)
{
    //addControllingSignal( QLatin1String("textChanged(const QString&)"));
    //addControllingSignal("returnPressed()"_L1);
}

/*! Returns the page view. */
PageView *AccessiblePageView::pageView() const
{
    return qobject_cast<PageView*>(object());
}

QString AccessiblePageView::text(QAccessible::Text t) const
{
    QString str;
    if ( t == QAccessible::Value ){
        if (pageView()->document()->isOpened()){
            str = pageView()->document()->text();
        } 
    }
    if (str.isEmpty())
        str = QAccessibleWidget::text(t);
    // not sure if we need to do specific things for Name and Description
    // or leave the accessibleName placeholder defined in the PageView class
    return str;
}

void AccessiblePageView::setText(QAccessible::Text t, const QString &text)
{
    if (t != QAccessible::Value) {
        QAccessibleWidget::setText(t, text);
        return;
    }
}

QAccessible::State AccessiblePageView::state() const
{
    QAccessible::State state = QAccessibleWidget::state();
    // we are setting it read-only for now
    state.editable = false;
    state.readOnly = true;
    state.selectableText = true;
    return state;
}

void *AccessiblePageView::interface_cast(QAccessible::InterfaceType t)
{
    if (t == QAccessible::TextInterface)
        return static_cast<QAccessibleTextInterface*>(this);
    return QAccessibleWidget::interface_cast(t);
}


QString AccessiblePageView::attributes(int offset, int *startOffset, int *endOffset) const
{
    // PageView doesn't have text attributes
    Q_UNUSED(offset);
    *startOffset = -1;
    *endOffset = -1;
    return QString();
}

int AccessiblePageView::cursorPosition() const
{
    if (!pageView()->document()->isOpened()) {
        return -1;
    }
    return pageView()->document()->offset();
}

QRect AccessiblePageView::characterRect(int offset) const
{
    if (!pageView()->document()->isOpened()){
        return QRect();
    }

    int scaleX = pageView()->contentAreaWidth();
    int scaleY = pageView()->contentAreaHeight();
    QRect r;
    std::unique_ptr<Okular::NormalizedRect> nRect = pageView()->document()->page(pageView()->document()->pageAtOffset(offset))->charBoxAtOffset(offset);
    if (nRect) {
        r = nRect->geometry( scaleX, scaleY );
    }
    return r;
}

int AccessiblePageView::offsetAtPoint(const QPoint &point) const
{
    if (!pageView()->document()->isOpened()){
        return -1;
    }

    // this ignores crop (trim margins), which needs fixing
    double scaleX = pageView()->contentAreaWidth();
    double scaleY = pageView()->contentAreaHeight();
    QPoint contentPos = pageView()->contentAreaPoint( pageView()->mapFromGlobal( point ));
    return pageView()->document()->cPage()->offsetAtPoint(Okular::NormalizedPoint( contentPos.x() / scaleX, contentPos.y() / scaleY));
}


QString AccessiblePageView::text(int startOffset, int endOffset) const
{
    // sanity check - queries may happen with an ex-document
    if (!pageView()->document()->isOpened()){
        return QString();
    }

    if (startOffset >= endOffset) {
        return QString();
    } else {
    // Note: endOffset is the first position *after* the text to be returned 
    QString s = pageView()->document()->textBetween( startOffset, endOffset-1 );
    return s;
    }
}


QString AccessiblePageView::textBeforeOffset(int offset, QAccessible::TextBoundaryType boundaryType, int *startOffset, int *endOffset) const
{
    if (!pageView()->document()->isOpened()){
        return QString();
    }

    if (pageView()->document()->pageAtOffset(offset) != pageView()->document()->cPageNum()) {
        offset = pageView()->document()->offset();
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
        s = pageView()->document()->charAtOffset(offset);
        break;
    case QAccessible::WordBoundary:
        textStart = pageView()->document()->prevWordStart(offset);
        s = pageView()->document()->wordAtOffset(textStart);
        break;
//    case QAccessible::sentenceBoundary:
        //TODO
//        break;
    case QAccessible::LineBoundary:
    case QAccessible::ParagraphBoundary:
        textStart= pageView()->document()->prevLineStart(offset);
            s = pageView()->document()->lineAtOffset(textStart);
        break;
    default:
        break;
    }

    //more bounds checking
    if ( pageView()->document()->pageAtOffset(textStart) != pageView()->document()->cPageNum()){
        textStart = -1;
    }

    if (textStart > -1){
        *startOffset = textStart;
        *endOffset = textStart + s.size();
        return s;
    } else {
    *startOffset = -1;
    *endOffset = -1;
        return QString();
    }
}

QString AccessiblePageView::textAfterOffset(int offset, QAccessible::TextBoundaryType boundaryType,
        int *startOffset, int *endOffset) const
{
    if (!pageView()->document()->isOpened()){
        return QString();
    }
    if (pageView()->document()->pageAtOffset(offset) != pageView()->document()->cPageNum()) {
        offset = pageView()->document()->offset();
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
        s = pageView()->document()->charAtOffset(offset);
        break;
    case QAccessible::WordBoundary:
        textStart = pageView()->document()->nextWordStart(offset);
        s = pageView()->document()->wordAtOffset(textStart);
        break;
//    case QAccessible::sentenceBoundary:
        //TODO
//        break;
    case QAccessible::LineBoundary:
    case QAccessible::ParagraphBoundary:
        textStart= pageView()->document()->nextLineStart(offset);
            s = pageView()->document()->lineAtOffset(textStart);
        break;
    default:
        break;
    }

    //more bounds checking
    if ( pageView()->document()->pageAtOffset(textStart) != pageView()->document()->cPageNum()){
        textStart = -1;
    }

    if (textStart > -1){
        *startOffset = textStart;
        *endOffset = textStart + s.size();
        return s;
    } else {
        *startOffset = -1;
        *endOffset = -1;
        return QString();
    }
}


QString AccessiblePageView::textAtOffset(int offset, QAccessible::TextBoundaryType boundaryType, int *startOffset, int *endOffset) const
{
    // sanity check - queries may happen with an ex-document
    if (!pageView()->document()->isOpened()){
        return QString();
    }


    // sanity check: are we on the same page?
    // as these are lookaround functions, they are page-specific
    // if continuous mode  works, this may change
    // but as things stand, the offset  seems unpredictable 
    if (pageView()->document()->pageAtOffset(offset) != pageView()->document()->cPageNum()) {
        offset = pageView()->document()->offset();
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
        s = pageView()->document()->charAtOffset(offset);
        break;
    case QAccessible::WordBoundary:
        textStart = pageView()->document()->wordStart(offset);
        s = pageView()->document()->wordAtOffset(textStart);
        break;
//    case QAccessible::sentenceBoundary:
        //TODO
//        break;
    case QAccessible::LineBoundary:
    case QAccessible::ParagraphBoundary:
        textStart= pageView()->document()->lineStart(offset);
            s = pageView()->document()->lineAtOffset(textStart);
        break;
    default:
        break;
    }
    if (textStart > -1){
        *startOffset = textStart;
        *endOffset = textStart + s.size();
        return s;
    } else {
    *startOffset = -1;
    *endOffset = -1;
        return QString();
    }
}

void AccessiblePageView::setCursorPosition(int position)
{
    if (!pageView()->document()->isOpened()){
        return;
    }
    pageView()->updateTextCursor( pageView()->document()->setOffset( position ));
}

int AccessiblePageView::characterCount() const
{
    // sanity check - queries may happen with an ex-document
    if (!pageView()->document()->isOpened()){
        return 0;
    }

    return pageView()->document()->end()+1;
}

void AccessiblePageView::scrollToSubstring(int startIndex, int endIndex)
{
    ///moving the text cursor also scrolls the display
    // scrolling to the last position first is a bit of a hack
    // implementing a RegularAreaRect might be a better way of doing it, 
    // without feeling seasick!
    pageView()->updateTextCursor(pageView()->document()->setOffset(endIndex));
    pageView()->updateTextCursor( pageView()->document()->setOffset( startIndex ));
}



// Selecting text
void AccessiblePageView::addSelection(int startOffset, int endOffset)
{
    setSelection(0,startOffset,endOffset);
}

int AccessiblePageView::selectionCount() const
{
    if (!pageView()->document()->isOpened()){
        return 0;
    }
    return (pageView()->document()->anchorTextCursor() > -1) ? 1 : 0;
}


void AccessiblePageView::selection(int selectionIndex, int *startOffset, int *endOffset) const
{
    if ((selectionIndex != 0) || !pageView()->document()->isOpened() || (pageView()->document()->anchorTextCursor() <0)){
        *startOffset = -1;
    *endOffset = -1;
    } else {
        if ( pageView()->document()->anchorTextCursor() > pageView()->document()->offset()){
            *startOffset = pageView()->document()->offset();
            *endOffset = pageView()->document()->anchorTextCursor();
        } else {
            *startOffset = pageView()->document()->anchorTextCursor();
            *endOffset = pageView()->document()->offset();
        }
    }
}

void AccessiblePageView::removeSelection(int selectionIndex)
{
    if ((selectionIndex != 0) || !pageView()->document()->isOpened() || (pageView()->document()->anchorTextCursor() < 0 )) { 
    return;
    } else { 
        pageView()->unsetTextSelection();
    }
}

void AccessiblePageView::setSelection(int selectionIndex, int startOffset, int endOffset)
{
    if ((selectionIndex !=0 ) || !pageView()->document()->isOpened()) {
        return;
    } else {
        pageView()->updateTextCursor(pageView()->document()->setTextSelection(startOffset, endOffset), true);
    }
}

#endif
