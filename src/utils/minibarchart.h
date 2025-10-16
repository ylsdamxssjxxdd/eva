// MiniBarChart - compact bar chart for 0..100 scores
#pragma once

#include <QStringList>
#include <QVector>
#include <QWidget>

class MiniBarChart : public QWidget
{
    Q_OBJECT
  public:
    explicit MiniBarChart(QWidget *parent = nullptr);
    QSize sizeHint() const override { return QSize(400, 160); }

    // Set six scores; pass < 0 for unavailable.
    Q_INVOKABLE void setScores(double s1, double s2, double s3, double s4, double s5, double s6);

    // Optional: override x-axis labels (length must be 6).
    // Order: TTFB / Generation / Common QA / Logic / Tools / Overall
    Q_INVOKABLE void setLabels(const QStringList &labels);

  protected:
    void paintEvent(QPaintEvent *event) override;

  private:
    QVector<double> m_scores; // six values in [0,100] or <0 as N/A
    QStringList m_labels;     // x-axis labels (6 entries)
};
