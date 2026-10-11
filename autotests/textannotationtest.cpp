/*
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <QTest>

#include "../core/annotations_p.h"

class TextAnnotationTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void testRotatedQuad_data();
    void testRotatedQuad();
};

void TextAnnotationTest::testRotatedQuad_data()
{
    QTest::addColumn<int>("rotation");
    for (int r = 0; r < 4; ++r) {
        QTest::newRow(qPrintable(QString::number(r * 90))) << r;
    }
}

void TextAnnotationTest::testRotatedQuad()
{
    QFETCH(int, rotation);

    const QList<Okular::NormalizedPoint> points = {{0.125, 0.3125}, {0.5, 0.3125}, {0.5, 0.25}, {0.125, 0.25}};
    Okular::HighlightAnnotation::Quad quad;
    for (int i = 0; i < 4; ++i) {
        quad.setPoint(points[i], i);
    }
    Okular::HighlightAnnotation annotation;
    annotation.highlightQuads().append(quad);

    QTransform matrix;
    matrix.translate(0.5, 0.5);
    matrix.rotate(-rotation * 90);
    matrix.translate(-0.5, -0.5);
    Okular::AnnotationPrivate::get(&annotation)->baseTransform(matrix);

    // Insertion must update the stored points, not just the display cache.
    const auto &storedQuad = annotation.highlightQuads().constFirst();
    for (int i = 0; i < 4; ++i) {
        const QPointF expected = matrix.map(QPointF(points[i].x, points[i].y));
        QCOMPARE(storedQuad.point(i).x, expected.x());
        QCOMPARE(storedQuad.point(i).y, expected.y());
    }
}

QTEST_GUILESS_MAIN(TextAnnotationTest)
#include "textannotationtest.moc"
