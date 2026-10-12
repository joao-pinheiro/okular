/*
    SPDX-FileCopyrightText: 2026 JPinheiro <jpinheiro@blackshield.pt>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <QTest>

#include "../gui/tocmodel.h"
#include "../settings_core.h"
#include "core/document.h"

#include <QTextDocument>

class TocModelTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase();
    void testToolTipShowsTitle_data();
    void testToolTipShowsTitle();
};

void TocModelTest::initTestCase()
{
    Okular::SettingsCore::instance(QStringLiteral("tocmodeltest"));
}

void TocModelTest::testToolTipShowsTitle_data()
{
    QTest::addColumn<QString>("title");

    QTest::newRow("plain") << QStringLiteral("Plain title");
    QTest::newRow("ampersand") << QStringLiteral("Tom & Jerry");
    QTest::newRow("tag") << QStringLiteral("The <img src=x> tag");
    QTest::newRow("comparison") << QStringLiteral("a < b and c > d");
    QTest::newRow("entity") << QStringLiteral("5 &lt; 6");
}

void TocModelTest::testToolTipShowsTitle()
{
    QFETCH(QString, title);

    Okular::DocumentSynopsis synopsis;
    synopsis.addChild(Okular::DocumentSynopsis::Element(Okular::DocumentSynopsis::ElementBuilder(title)));

    Okular::Document document(nullptr);
    TOCModel model(&document);
    model.fill(&synopsis);
    QCOMPARE(model.rowCount(), 1);
    const QModelIndex index = model.index(0, 0);

    QCOMPARE(model.data(index, Qt::DisplayRole).toString(), title);

    // a tooltip that looks like rich text is shown as rich text, the title has to come out of that unchanged
    const QString toolTip = model.data(index, Qt::ToolTipRole).toString();
    QString shown = toolTip;
    if (Qt::mightBeRichText(toolTip)) {
        QTextDocument rendered;
        rendered.setHtml(toolTip);
        shown = rendered.toPlainText();
    }
    QCOMPARE(shown, title);
}

QTEST_MAIN(TocModelTest)
#include "tocmodeltest.moc"
