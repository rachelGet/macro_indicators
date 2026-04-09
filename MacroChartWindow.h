#ifndef MACRO_CHART_WINDOW_H
#define MACRO_CHART_WINDOW_H

#include "MacroTypes.h"

#include <QMainWindow>
#include <QWidget>
#include <QPainter>
#include <QPixmap>
#include <QVector>
#include <QDateTime>
#include <QMouseEvent>
#include <QWheelEvent>

struct MacroPoint {
    QDateTime date;
    double value;
};

class MacroChartWidget : public QWidget {
    Q_OBJECT

    Q_PROPERTY(QColor colorBg           READ colorBg           WRITE setColorBg)
    Q_PROPERTY(QColor colorPanelBg      READ colorPanelBg      WRITE setColorPanelBg)
    Q_PROPERTY(QColor colorGrid         READ colorGrid         WRITE setColorGrid)
    Q_PROPERTY(QColor colorGridSub      READ colorGridSub      WRITE setColorGridSub)
    Q_PROPERTY(QColor colorAxisLine     READ colorAxisLine     WRITE setColorAxisLine)
    Q_PROPERTY(QColor colorText         READ colorText         WRITE setColorText)
    Q_PROPERTY(QColor colorTextBright   READ colorTextBright   WRITE setColorTextBright)
    Q_PROPERTY(QColor colorCrosshair    READ colorCrosshair    WRITE setColorCrosshair)
    Q_PROPERTY(QColor colorTooltipBg    READ colorTooltipBg    WRITE setColorTooltipBg)
    Q_PROPERTY(QColor colorLabelBg      READ colorLabelBg      WRITE setColorLabelBg)
    Q_PROPERTY(QColor colorCpiLine      READ colorCpiLine      WRITE setColorCpiLine)
    Q_PROPERTY(QColor colorCpiFill      READ colorCpiFill      WRITE setColorCpiFill)
    Q_PROPERTY(QColor colorWalclLine    READ colorWalclLine    WRITE setColorWalclLine)
    Q_PROPERTY(QColor colorWalclFill    READ colorWalclFill    WRITE setColorWalclFill)
    Q_PROPERTY(QColor colorUmcsentLine  READ colorUmcsentLine  WRITE setColorUmcsentLine)
    Q_PROPERTY(QColor colorUmcsentFill  READ colorUmcsentFill  WRITE setColorUmcsentFill)
    Q_PROPERTY(QColor colorDot          READ colorDot          WRITE setColorDot)
    Q_PROPERTY(QString fontFamily       READ fontFamily        WRITE setFontFamily)
    Q_PROPERTY(int fontSize             READ fontSize          WRITE setFontSize)

public:
    explicit MacroChartWidget(QWidget *parent = nullptr);
    void addObservation(const MacroObservation &obs);

    // QSS property accessors
    QColor colorBg()          const { return m_colorBg; }
    QColor colorPanelBg()     const { return m_colorPanelBg; }
    QColor colorGrid()        const { return m_colorGrid; }
    QColor colorGridSub()     const { return m_colorGridSub; }
    QColor colorAxisLine()    const { return m_colorAxisLine; }
    QColor colorText()        const { return m_colorText; }
    QColor colorTextBright()  const { return m_colorTextBright; }
    QColor colorCrosshair()   const { return m_colorCrosshair; }
    QColor colorTooltipBg()   const { return m_colorTooltipBg; }
    QColor colorLabelBg()     const { return m_colorLabelBg; }
    QColor colorCpiLine()     const { return m_colorCpiLine; }
    QColor colorCpiFill()     const { return m_colorCpiFill; }
    QColor colorWalclLine()   const { return m_colorWalclLine; }
    QColor colorWalclFill()   const { return m_colorWalclFill; }
    QColor colorUmcsentLine() const { return m_colorUmcsentLine; }
    QColor colorUmcsentFill() const { return m_colorUmcsentFill; }
    QColor colorDot()         const { return m_colorDot; }
    QString fontFamily()      const { return m_fontFamily; }
    int fontSize()            const { return m_fontSize; }

    void setColorBg(const QColor &c)          { m_colorBg = c; m_cacheDirty = true; }
    void setColorPanelBg(const QColor &c)     { m_colorPanelBg = c; m_cacheDirty = true; }
    void setColorGrid(const QColor &c)        { m_colorGrid = c; m_cacheDirty = true; }
    void setColorGridSub(const QColor &c)     { m_colorGridSub = c; m_cacheDirty = true; }
    void setColorAxisLine(const QColor &c)    { m_colorAxisLine = c; m_cacheDirty = true; }
    void setColorText(const QColor &c)        { m_colorText = c; m_cacheDirty = true; }
    void setColorTextBright(const QColor &c)  { m_colorTextBright = c; m_cacheDirty = true; }
    void setColorCrosshair(const QColor &c)   { m_colorCrosshair = c; m_cacheDirty = true; }
    void setColorTooltipBg(const QColor &c)   { m_colorTooltipBg = c; m_cacheDirty = true; }
    void setColorLabelBg(const QColor &c)     { m_colorLabelBg = c; m_cacheDirty = true; }
    void setColorCpiLine(const QColor &c)     { m_colorCpiLine = c; m_cacheDirty = true; }
    void setColorCpiFill(const QColor &c)     { m_colorCpiFill = c; m_cacheDirty = true; }
    void setColorWalclLine(const QColor &c)   { m_colorWalclLine = c; m_cacheDirty = true; }
    void setColorWalclFill(const QColor &c)   { m_colorWalclFill = c; m_cacheDirty = true; }
    void setColorUmcsentLine(const QColor &c) { m_colorUmcsentLine = c; m_cacheDirty = true; }
    void setColorUmcsentFill(const QColor &c) { m_colorUmcsentFill = c; m_cacheDirty = true; }
    void setColorDot(const QColor &c)         { m_colorDot = c; m_cacheDirty = true; }
    void setFontFamily(const QString &f)      { m_fontFamily = f; m_cacheDirty = true; }
    void setFontSize(int s)                   { m_fontSize = s; m_cacheDirty = true; }

protected:
    void paintEvent(QPaintEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    // Panel index: 0=CPI, 1=WALCL, 2=UMCSENT
    static constexpr int kPanelCount = 3;

    void rebuildCache();
    void drawGrid(QPainter &p);
    void drawSeries(QPainter &p, int panel);
    void drawValueAxis(QPainter &p, int panel);
    void drawTimeAxis(QPainter &p);
    void drawCrosshair(QPainter &p);
    void drawLastValueGuide(QPainter &p, int panel);

    void recalcRange();
    QRectF panelArea(int panel) const;
    double valueToY(double val, double vmin, double vmax, const QRectF &a) const;

    // Date-to-X mapping helpers
    double dateToX(const QDateTime &dt, const QRectF &area) const;
    int nearestIndex(int panel, double x, const QRectF &area) const;
    void computeGlobalDateRange();

    // Data per series
    QVector<MacroPoint> m_data[kPanelCount];
    double m_min[kPanelCount], m_max[kPanelCount];

    int panelForIndicator(MacroIndicator ind) const;
    const char* panelLabel(int panel) const;
    QColor lineColor(int panel) const;
    QColor fillColor(int panel) const;

    // Time-based view window (in seconds since epoch)
    qint64 m_viewStartSec;   // left edge of visible window
    qint64 m_viewEndSec;     // right edge of visible window
    qint64 m_globalMinSec;   // earliest date across all series
    qint64 m_globalMaxSec;   // latest date across all series

    bool m_dragging;
    QPoint m_lastMousePos;
    QPoint m_crosshairPos;
    bool m_crosshairActive;

    QPixmap m_cache;
    bool m_cacheDirty;

    // Layout
    static constexpr int kLeftMargin = 80;
    static constexpr int kRightMargin = 100;
    static constexpr int kTopMargin = 12;
    static constexpr int kBottomMargin = 32;
    static constexpr double kPanelPct = 0.30;
    static constexpr double kGapPct   = 0.05;

    // Theme defaults (TradingView dark)
    QColor m_colorBg          = QColor("#131722");
    QColor m_colorPanelBg     = QColor("#1e222d");
    QColor m_colorGrid        = QColor("#2a2e39");
    QColor m_colorGridSub     = QColor("#222630");
    QColor m_colorAxisLine    = QColor("#363a45");
    QColor m_colorText        = QColor("#787b86");
    QColor m_colorTextBright  = QColor("#d1d4dc");
    QColor m_colorCrosshair   = QColor("#9598a1");
    QColor m_colorTooltipBg   = QColor(30, 34, 45, 220);
    QColor m_colorLabelBg     = QColor("#363a45");
    QColor m_colorCpiLine     = QColor("#f5c518");
    QColor m_colorCpiFill     = QColor(245, 197, 24, 30);
    QColor m_colorWalclLine   = QColor("#42a5f5");
    QColor m_colorWalclFill   = QColor(66, 165, 245, 30);
    QColor m_colorUmcsentLine = QColor("#26a69a");
    QColor m_colorUmcsentFill = QColor(38, 166, 154, 30);
    QColor m_colorDot         = QColor("#ffffff");
    QString m_fontFamily      = "sans-serif";
    int m_fontSize            = 11;
};

class MacroChartWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MacroChartWindow(QWidget *parent = nullptr);

public slots:
    void onObservation(MacroObservation obs);

private:
    MacroChartWidget *m_chart;
};

#endif // MACRO_CHART_WINDOW_H
