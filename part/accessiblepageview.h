
#ifndef _OKULAR_ACCESSIBLEPAGEVIEW_H_
#define _OKULAR_ACCESSIBLEPAGEVIEW_H_
#include "pageview.h"
#include <QAccessible>
#include <QAccessibleTextInterface>
#include <QAccessibleWidget>

class AccessiblePageView : public QAccessibleWidget, public QAccessibleTextInterface
{
public:
    explicit AccessiblePageView(QWidget *w, const QString &name = QString());

    QString text(QAccessible::Text t) const override;
    void setText(QAccessible::Text t, const QString &text) override;
    QAccessible::State state() const override;
    void *interface_cast(QAccessible::InterfaceType t) override;

    // QAccessibleTextInterface
    void addSelection(int startOffset, int endOffset) override;
    QString attributes(int offset, int *startOffset, int *endOffset) const override;
    int cursorPosition() const override;
    QRect characterRect(int offset) const override;
    int selectionCount() const override;
    int offsetAtPoint(const QPoint &point) const override;
    void selection(int selectionIndex, int *startOffset, int *endOffset) const override;
    QString text(int startOffset, int endOffset) const override;
    QString textBeforeOffset(int offset, QAccessible::TextBoundaryType boundaryType, int *startOffset, int *endOffset) const override;
    QString textAfterOffset(int offset, QAccessible::TextBoundaryType boundaryType, int *startOffset, int *endOffset) const override;
    QString textAtOffset(int offset, QAccessible::TextBoundaryType boundaryType, int *startOffset, int *endOffset) const override;
    void removeSelection(int selectionIndex) override;
    void setCursorPosition(int position) override;
    void setSelection(int selectionIndex, int startOffset, int endOffset) override;
    int characterCount() const override;
    void scrollToSubstring(int startIndex, int endIndex) override;

protected:
    PageView *pageView() const;
};

#endif // AccessiblePageView
