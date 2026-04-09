#include "MacroChartWindow.h"
#include <QPainterPath>
#include <algorithm>
#include <cmath>
#include <limits>

// ─── MacroChartWindow ───

MacroChartWindow::MacroChartWindow(QWidget *parent) : QMainWindow(parent) {
    setWindowTitle("Macro Indicators — CPI / WALCL / UMCSENT");
    resize(1400, 850);
    m_chart = new MacroChartWidget(this);
    setCentralWidget(m_chart);
}

void MacroChartWindow::onObservation(MacroObservation obs) {
    m_chart->addObservation(obs);
}

// ─── MacroChartWidget ───

MacroChartWidget::MacroChartWidget(QWidget *parent)
    : QWidget(parent),
      m_viewStartSec(0), m_viewEndSec(0),
      m_globalMinSec(0), m_globalMaxSec(0),
      m_dragging(false), m_crosshairActive(false), m_cacheDirty(true)
{
    for (int i = 0; i < kPanelCount; ++i) { m_min[i] = 0; m_max[i] = 0; }
    setMinimumSize(600, 400);
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
    setCursor(Qt::CrossCursor);
}

int MacroChartWidget::panelForIndicator(MacroIndicator ind) const {
    switch (ind) {
        case MacroIndicator::CPI:               return 0;
        case MacroIndicator::FederalFundsRate:   return 1;
        case MacroIndicator::ConsumerSentiment:  return 2;
    }
    return 0;
}

const char* MacroChartWidget::panelLabel(int panel) const {
    switch (panel) {
        case 0: return "CPI (CPIAUCSL)";
        case 1: return "Fed Balance Sheet (WALCL)";
        case 2: return "Consumer Sentiment (UMCSENT)";
    }
    return "";
}

QColor MacroChartWidget::lineColor(int panel) const {
    switch (panel) {
        case 0: return m_colorCpiLine;
        case 1: return m_colorWalclLine;
        case 2: return m_colorUmcsentLine;
    }
    return m_colorText;
}

QColor MacroChartWidget::fillColor(int panel) const {
    switch (panel) {
        case 0: return m_colorCpiFill;
        case 1: return m_colorWalclFill;
        case 2: return m_colorUmcsentFill;
    }
    return QColor(100, 100, 100, 30);
}

void MacroChartWidget::addObservation(const MacroObservation &obs) {
    int panel = panelForIndicator(obs.indicator);

    MacroPoint pt;
    pt.date = QDateTime::fromString(obs.date, "yyyy-MM-dd");
    if (!pt.date.isValid()) pt.date = obs.fetchedAt;
    pt.value = obs.value;

    // Insert sorted by date, skip duplicates
    auto &vec = m_data[panel];
    auto it = std::lower_bound(vec.begin(), vec.end(), pt,
        [](const MacroPoint &a, const MacroPoint &b) { return a.date < b.date; });

    if (it != vec.end() && it->date == pt.date) {
        it->value = pt.value;  // update existing
    } else {
        vec.insert(it, pt);
    }

    computeGlobalDateRange();
    recalcRange();
    m_cacheDirty = true;
    update();
}

void MacroChartWidget::computeGlobalDateRange() {
    qint64 gMin = std::numeric_limits<qint64>::max();
    qint64 gMax = std::numeric_limits<qint64>::min();

    for (int p = 0; p < kPanelCount; ++p) {
        const auto &vec = m_data[p];
        if (vec.isEmpty()) continue;
        qint64 s = vec.first().date.toSecsSinceEpoch();
        qint64 e = vec.last().date.toSecsSinceEpoch();
        if (s < gMin) gMin = s;
        if (e > gMax) gMax = e;
    }

    if (gMin >= gMax) { gMax = gMin + 86400; } // at least 1 day

    m_globalMinSec = gMin;
    m_globalMaxSec = gMax;

    // On first data arrival, set view to full range
    if (m_viewStartSec == 0 && m_viewEndSec == 0) {
        m_viewStartSec = gMin;
        m_viewEndSec   = gMax;
    }

    // Clamp view to global bounds
    if (m_viewStartSec < m_globalMinSec) m_viewStartSec = m_globalMinSec;
    if (m_viewEndSec   > m_globalMaxSec) m_viewEndSec   = m_globalMaxSec;
    if (m_viewEndSec  <= m_viewStartSec) m_viewEndSec = m_viewStartSec + 86400;
}

double MacroChartWidget::dateToX(const QDateTime &dt, const QRectF &area) const {
    qint64 sec = dt.toSecsSinceEpoch();
    qint64 range = m_viewEndSec - m_viewStartSec;
    if (range <= 0) return area.left();
    double frac = (double)(sec - m_viewStartSec) / (double)range;
    return area.left() + frac * area.width();
}

int MacroChartWidget::nearestIndex(int panel, double x, const QRectF &area) const {
    const auto &vec = m_data[panel];
    if (vec.isEmpty()) return -1;

    // Convert x back to seconds
    qint64 range = m_viewEndSec - m_viewStartSec;
    if (range <= 0) return 0;
    double frac = (x - area.left()) / area.width();
    qint64 sec = m_viewStartSec + (qint64)(frac * range);

    // Binary search for nearest
    QDateTime target = QDateTime::fromSecsSinceEpoch(sec);
    MacroPoint probe;
    probe.date = target;
    auto it = std::lower_bound(vec.begin(), vec.end(), probe,
        [](const MacroPoint &a, const MacroPoint &b) { return a.date < b.date; });

    int idx = (int)(it - vec.begin());
    if (idx >= vec.size()) idx = vec.size() - 1;
    if (idx > 0) {
        // Check which neighbor is closer
        qint64 d1 = qAbs(vec[idx].date.toSecsSinceEpoch() - sec);
        qint64 d0 = qAbs(vec[idx - 1].date.toSecsSinceEpoch() - sec);
        if (d0 < d1) idx--;
    }
    return idx;
}

void MacroChartWidget::recalcRange() {
    for (int p = 0; p < kPanelCount; ++p) {
        const auto &vec = m_data[p];
        if (vec.isEmpty()) { m_min[p] = 0; m_max[p] = 1; continue; }

        // Only consider points within the visible date window
        double lo = 1e18, hi = -1e18;
        for (int i = 0; i < vec.size(); ++i) {
            qint64 sec = vec[i].date.toSecsSinceEpoch();
            if (sec < m_viewStartSec || sec > m_viewEndSec) continue;
            lo = qMin(lo, vec[i].value);
            hi = qMax(hi, vec[i].value);
        }
        if (lo > hi) { lo = 0; hi = 1; } // no visible points
        double r = hi - lo;
        double margin = (r > 0) ? r * 0.08 : qAbs(hi) * 0.05;
        if (margin < 0.01) margin = 1;
        m_min[p] = lo - margin;
        m_max[p] = hi + margin;
    }
}

// ─── Panel geometry ───

QRectF MacroChartWidget::panelArea(int panel) const {
    double ch = height() - kTopMargin - kBottomMargin;
    double panelH = ch * kPanelPct;
    double gapH   = ch * kGapPct;
    double y = kTopMargin + panel * (panelH + gapH);
    return QRectF(kLeftMargin, y,
                  width() - kLeftMargin - kRightMargin, panelH);
}

double MacroChartWidget::valueToY(double val, double vmin, double vmax,
                                   const QRectF &a) const {
    if (vmax <= vmin) return a.center().y();
    return a.bottom() - (val - vmin) / (vmax - vmin) * a.height();
}

// ─── Paint ───

void MacroChartWidget::paintEvent(QPaintEvent *) {
    if (m_cacheDirty || m_cache.size() != size()) {
        rebuildCache();
        m_cacheDirty = false;
    }
    QPainter p(this);
    p.drawPixmap(0, 0, m_cache);

    if (m_crosshairActive)
        drawCrosshair(p);
}

void MacroChartWidget::rebuildCache() {
    m_cache = QPixmap(size());
    m_cache.fill(m_colorBg);
    QPainter p(&m_cache);

    QFont f = font();
    f.setPixelSize(m_fontSize);
    f.setFamily(m_fontFamily);
    p.setFont(f);

    bool empty = true;
    for (int i = 0; i < kPanelCount; ++i)
        if (!m_data[i].isEmpty()) { empty = false; break; }

    if (empty) {
        p.setPen(m_colorText);
        QFont ef = font(); ef.setPixelSize(14); ef.setFamily(m_fontFamily); p.setFont(ef);
        p.drawText(rect(), Qt::AlignCenter, "Waiting for FRED data...");
        return;
    }

    drawGrid(p);
    for (int i = 0; i < kPanelCount; ++i) {
        drawSeries(p, i);
        drawValueAxis(p, i);
        drawLastValueGuide(p, i);
    }
    drawTimeAxis(p);
}

// ─── Grid ───

void MacroChartWidget::drawGrid(QPainter &p) {
    p.setRenderHint(QPainter::Antialiasing, false);

    for (int panel = 0; panel < kPanelCount; ++panel) {
        QRectF a = panelArea(panel);
        p.fillRect(a, m_colorPanelBg);

        // Horizontal grid lines
        QPen gridPen(m_colorGrid, 1);
        p.setPen(gridPen);
        int nh = 4;
        for (int i = 1; i < nh; ++i) {
            double y = a.top() + a.height() * i / nh;
            p.drawLine(QPointF(a.left(), y), QPointF(a.right(), y));
        }

        // Sub-grid
        QPen subPen(m_colorGridSub, 1);
        p.setPen(subPen);
        for (int i = 0; i < nh; ++i) {
            double y = a.top() + a.height() * (i + 0.5) / nh;
            p.drawLine(QPointF(a.left(), y), QPointF(a.right(), y));
        }

        // Border
        p.setPen(QPen(m_colorAxisLine, 1));
        p.drawRect(a);

        // Panel label
        p.setPen(m_colorText);
        QFont lf = p.font(); lf.setPixelSize(10); p.setFont(lf);
        p.drawText(QRectF(a.left() + 6, a.top() + 4, 300, 14),
                   Qt::AlignLeft | Qt::AlignVCenter, panelLabel(panel));
        lf.setPixelSize(m_fontSize); p.setFont(lf);
    }

    // Vertical grid lines at regular time intervals
    qint64 rangeSec = m_viewEndSec - m_viewStartSec;
    if (rangeSec <= 0) { p.setRenderHint(QPainter::Antialiasing, true); return; }

    // Choose a nice time step (~8 grid lines)
    qint64 stepSec = qMax((qint64)1, rangeSec / 8);

    // Round to nearest month boundary for cleaner labels
    qint64 monthSec = 30LL * 86400;
    if (stepSec > monthSec / 2) {
        int months = qMax(1, (int)(stepSec / monthSec));
        stepSec = months * monthSec;
    }

    QPen gridPen(m_colorGrid, 1);
    p.setPen(gridPen);

    QRectF refArea = panelArea(0);
    // Start from a rounded time
    qint64 tStart = (m_viewStartSec / stepSec) * stepSec;
    for (qint64 t = tStart; t <= m_viewEndSec; t += stepSec) {
        if (t < m_viewStartSec) continue;
        QDateTime dt = QDateTime::fromSecsSinceEpoch(t);
        double x = dateToX(dt, refArea);
        for (int panel = 0; panel < kPanelCount; ++panel) {
            QRectF a = panelArea(panel);
            p.drawLine(QPointF(x, a.top()), QPointF(x, a.bottom()));
        }
    }

    p.setRenderHint(QPainter::Antialiasing, true);
}

// ─── Series (line + area fill + dots) ───

void MacroChartWidget::drawSeries(QPainter &p, int panel) {
    const auto &vec = m_data[panel];
    if (vec.isEmpty()) return;

    QRectF a = panelArea(panel);
    p.setRenderHint(QPainter::Antialiasing, true);

    // Collect visible points (within view window)
    struct VisPoint { double x, y; };
    QVector<VisPoint> pts;
    for (int i = 0; i < vec.size(); ++i) {
        qint64 sec = vec[i].date.toSecsSinceEpoch();
        if (sec < m_viewStartSec || sec > m_viewEndSec) continue;
        double x = dateToX(vec[i].date, a);
        double y = valueToY(vec[i].value, m_min[panel], m_max[panel], a);
        pts.append({x, y});
    }

    if (pts.isEmpty()) return;

    if (pts.size() == 1) {
        p.setPen(Qt::NoPen);
        p.setBrush(lineColor(panel));
        p.drawEllipse(QPointF(pts[0].x, pts[0].y), 4, 4);
        p.setBrush(Qt::NoBrush);
        return;
    }

    QPainterPath line, fill;
    line.moveTo(pts[0].x, pts[0].y);
    fill.moveTo(pts[0].x, pts[0].y);
    for (int i = 1; i < pts.size(); ++i) {
        line.lineTo(pts[i].x, pts[i].y);
        fill.lineTo(pts[i].x, pts[i].y);
    }

    // Area fill
    fill.lineTo(pts.last().x, a.bottom());
    fill.lineTo(pts.first().x, a.bottom());
    fill.closeSubpath();

    QLinearGradient grad(0, a.top(), 0, a.bottom());
    QColor fc = fillColor(panel);
    grad.setColorAt(0, fc);
    QColor fcBot = fc; fcBot.setAlpha(5);
    grad.setColorAt(1, fcBot);
    p.setPen(Qt::NoPen);
    p.setBrush(grad);
    p.drawPath(fill);

    // Line
    p.setPen(QPen(lineColor(panel), 2));
    p.setBrush(Qt::NoBrush);
    p.drawPath(line);

    // Data point dots — scale radius based on point density
    double avgSpacing = a.width() / qMax(1, pts.size() - 1);
    double dotR = qBound(2.0, avgSpacing * 0.08, 5.0);

    p.setPen(Qt::NoPen);
    p.setBrush(lineColor(panel));
    for (const auto &pt : pts) {
        p.drawEllipse(QPointF(pt.x, pt.y), dotR, dotR);
    }

    // Last visible dot highlighted
    p.setBrush(m_colorDot);
    p.drawEllipse(QPointF(pts.last().x, pts.last().y), dotR + 2, dotR + 2);
    p.setBrush(lineColor(panel));
    p.drawEllipse(QPointF(pts.last().x, pts.last().y), dotR, dotR);
    p.setBrush(Qt::NoBrush);
}

// ─── Last value guide (dashed line + pill label) ───

void MacroChartWidget::drawLastValueGuide(QPainter &p, int panel) {
    const auto &vec = m_data[panel];
    if (vec.isEmpty()) return;

    QRectF a = panelArea(panel);

    // Find the last visible point in this series
    int lastVis = -1;
    for (int i = vec.size() - 1; i >= 0; --i) {
        qint64 sec = vec[i].date.toSecsSinceEpoch();
        if (sec >= m_viewStartSec && sec <= m_viewEndSec) { lastVis = i; break; }
    }
    if (lastVis < 0) return;

    double val = vec[lastVis].value;
    double y = valueToY(val, m_min[panel], m_max[panel], a);
    QColor lc = lineColor(panel);

    QPen dp(lc, 1, Qt::CustomDashLine);
    dp.setDashPattern({6, 4});
    p.setPen(dp);
    p.drawLine(QPointF(a.left(), y), QPointF(a.right(), y));

    // Value label pill on the right
    QString txt;
    if (val >= 1e9) txt = QString::number(val / 1e9, 'f', 2) + " B";
    else if (val >= 1e6) txt = QString::number(val / 1e6, 'f', 1) + " M";
    else if (val >= 1e3) txt = QString::number(val / 1e3, 'f', 1) + " K";
    else txt = QString::number(val, 'f', 2);

    QFont f = p.font(); f.setPixelSize(11); f.setBold(true); p.setFont(f);
    QFontMetrics fm(f);
    int tw = fm.horizontalAdvance(txt) + 16, th = 20;
    double lx = a.right() + 8, ly = y - th / 2.0;

    QPainterPath pill;
    pill.addRoundedRect(QRectF(lx, ly, tw, th), 10, 10);
    p.setPen(Qt::NoPen);
    p.setBrush(lc);
    p.drawPath(pill);
    p.setPen(Qt::white);
    p.drawText(QRectF(lx, ly, tw, th), Qt::AlignCenter, txt);
    f.setBold(false); p.setFont(f);
    p.setBrush(Qt::NoBrush);
}

// ─── Value axis (left side) ───

void MacroChartWidget::drawValueAxis(QPainter &p, int panel) {
    QRectF a = panelArea(panel);
    p.setPen(m_colorText);

    auto fmt = [](double v) -> QString {
        if (qAbs(v) >= 1e9) return QString::number(v / 1e9, 'f', 1) + "B";
        if (qAbs(v) >= 1e6) return QString::number(v / 1e6, 'f', 0) + "M";
        if (qAbs(v) >= 1e3) return QString::number(v / 1e3, 'f', 0) + "K";
        return QString::number(v, 'f', 1);
    };

    int n = 4;
    double step = (m_max[panel] - m_min[panel]) / n;
    for (int i = 0; i <= n; ++i) {
        double val = m_min[panel] + step * i;
        double y = valueToY(val, m_min[panel], m_max[panel], a);
        p.drawText(QRectF(2, y - 8, kLeftMargin - 8, 16),
                   Qt::AlignRight | Qt::AlignVCenter, fmt(val));
    }
}

// ─── Time axis (below bottom panel) ───

void MacroChartWidget::drawTimeAxis(QPainter &p) {
    qint64 rangeSec = m_viewEndSec - m_viewStartSec;
    if (rangeSec <= 0) return;

    QRectF bottomPanel = panelArea(kPanelCount - 1);
    double ly = bottomPanel.bottom() + 4;

    p.setPen(m_colorText);

    // Choose time step (~8 labels)
    qint64 stepSec = qMax((qint64)1, rangeSec / 8);
    qint64 monthSec = 30LL * 86400;
    if (stepSec > monthSec / 2) {
        int months = qMax(1, (int)(stepSec / monthSec));
        stepSec = months * monthSec;
    }

    qint64 tStart = (m_viewStartSec / stepSec) * stepSec;
    for (qint64 t = tStart; t <= m_viewEndSec; t += stepSec) {
        if (t < m_viewStartSec) continue;
        QDateTime dt = QDateTime::fromSecsSinceEpoch(t);
        double x = dateToX(dt, bottomPanel);
        p.drawText(QRectF(x - 30, ly, 60, 16), Qt::AlignCenter,
                   dt.toString("yyyy-MM"));
    }
}

// ─── Crosshair ───

void MacroChartWidget::drawCrosshair(QPainter &p) {
    double mx = m_crosshairPos.x(), my = m_crosshairPos.y();

    // Which panel are we in?
    int activePanel = -1;
    for (int i = 0; i < kPanelCount; ++i) {
        if (panelArea(i).contains(m_crosshairPos)) { activePanel = i; break; }
    }
    if (activePanel < 0) return;

    QPen cp(m_colorCrosshair, 1, Qt::DashLine);
    cp.setDashPattern({4, 3});
    p.setPen(cp);

    // Horizontal line in active panel
    QRectF ap = panelArea(activePanel);
    p.drawLine(QPointF(ap.left(), my), QPointF(ap.right(), my));

    // Value label on left axis
    double val = m_min[activePanel] + (ap.bottom() - my) / ap.height()
                 * (m_max[activePanel] - m_min[activePanel]);
    QString valTxt;
    if (qAbs(val) >= 1e9) valTxt = QString::number(val / 1e9, 'f', 2) + "B";
    else if (qAbs(val) >= 1e6) valTxt = QString::number(val / 1e6, 'f', 0) + "M";
    else valTxt = QString::number(val, 'f', 2);

    QFont f = p.font(); f.setPixelSize(10); p.setFont(f);
    QFontMetrics fm(f);
    QRectF lb(2, my - 9, kLeftMargin - 8, 18);
    p.setPen(Qt::NoPen); p.setBrush(m_colorLabelBg);
    p.drawRoundedRect(lb, 3, 3);
    p.setPen(m_colorTextBright);
    p.drawText(lb, Qt::AlignCenter, valTxt);
    p.setBrush(Qt::NoBrush);
    p.setPen(cp);

    // Vertical line across all panels
    for (int i = 0; i < kPanelCount; ++i) {
        QRectF a = panelArea(i);
        p.drawLine(QPointF(mx, a.top()), QPointF(mx, a.bottom()));
    }

    // Convert mouse X to a date for the time label
    QRectF refArea = panelArea(0);
    qint64 rangeSec = m_viewEndSec - m_viewStartSec;
    if (rangeSec <= 0) return;
    double frac = (mx - refArea.left()) / refArea.width();
    qint64 cursorSec = m_viewStartSec + (qint64)(frac * rangeSec);
    QDateTime cursorDt = QDateTime::fromSecsSinceEpoch(cursorSec);

    // Time label below bottom panel
    QRectF bottomPanel = panelArea(kPanelCount - 1);
    QString tl = cursorDt.toString("yyyy-MM-dd");
    int tw = fm.horizontalAdvance(tl) + 12;
    QRectF tb(mx - tw / 2.0, bottomPanel.bottom() + 2, tw, 16);
    p.setPen(Qt::NoPen); p.setBrush(m_colorLabelBg);
    p.drawRoundedRect(tb, 3, 3);
    p.setPen(m_colorTextBright);
    p.drawText(tb, Qt::AlignCenter, tl);
    p.setBrush(Qt::NoBrush);

    // Tooltip: find nearest data point in each series
    QString info;
    for (int i = 0; i < kPanelCount; ++i) {
        int idx = nearestIndex(i, mx, panelArea(i));
        if (idx < 0) continue;
        // Only show if the nearest point is reasonably close (within ~15 days)
        qint64 ptSec = m_data[i][idx].date.toSecsSinceEpoch();
        if (qAbs(ptSec - cursorSec) > 15LL * 86400) continue;

        if (!info.isEmpty()) info += "  |  ";
        double v = m_data[i][idx].value;
        QString label;
        switch (i) {
            case 0: label = "CPI"; break;
            case 1: label = "WALCL"; break;
            case 2: label = "UMCSENT"; break;
        }
        if (qAbs(v) >= 1e9) info += label + " " + QString::number(v / 1e9, 'f', 2) + "B";
        else if (qAbs(v) >= 1e6) info += label + " " + QString::number(v / 1e6, 'f', 0) + "M";
        else info += label + " " + QString::number(v, 'f', 2);
    }

    if (!info.isEmpty()) {
        int iw = fm.horizontalAdvance(info) + 16;
        double ix = qMin(mx + 14, (double)(width() - iw - 4));
        double iy = qMax(my - 28, (double)panelArea(0).top());
        QRectF ib(ix, iy, iw, 18);
        p.setPen(Qt::NoPen); p.setBrush(m_colorTooltipBg);
        p.drawRoundedRect(ib, 4, 4);
        p.setPen(m_colorTextBright);
        p.drawText(ib, Qt::AlignCenter, info);
        p.setBrush(Qt::NoBrush);
    }
}

// ─── Interaction ───

void MacroChartWidget::wheelEvent(QWheelEvent *event) {
    QRectF a = panelArea(0);
    double mx = event->position().x();
    double frac = (mx - a.left()) / a.width();
    frac = qBound(0.0, frac, 1.0);

    qint64 rangeSec = m_viewEndSec - m_viewStartSec;
    qint64 pivotSec = m_viewStartSec + (qint64)(frac * rangeSec);

    double factor = (event->angleDelta().y() > 0) ? 0.85 : 1.18; // zoom in / out
    qint64 newRange = (qint64)(rangeSec * factor);

    // Clamp: at least 7 days, at most the full global range
    qint64 minRange = 7LL * 86400;
    qint64 maxRange = m_globalMaxSec - m_globalMinSec;
    newRange = qBound(minRange, newRange, maxRange);

    // Keep pivot point under cursor
    m_viewStartSec = pivotSec - (qint64)(frac * newRange);
    m_viewEndSec   = m_viewStartSec + newRange;

    // Clamp to global bounds
    if (m_viewStartSec < m_globalMinSec) {
        m_viewStartSec = m_globalMinSec;
        m_viewEndSec = m_viewStartSec + newRange;
    }
    if (m_viewEndSec > m_globalMaxSec) {
        m_viewEndSec = m_globalMaxSec;
        m_viewStartSec = m_viewEndSec - newRange;
        if (m_viewStartSec < m_globalMinSec) m_viewStartSec = m_globalMinSec;
    }

    recalcRange();
    m_cacheDirty = true;
    update();
}

void MacroChartWidget::mousePressEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton) {
        m_dragging = true;
        m_lastMousePos = event->pos();
    }
}

void MacroChartWidget::mouseMoveEvent(QMouseEvent *event) {
    m_crosshairPos = event->pos();
    m_crosshairActive = true;

    if (m_dragging) {
        int dx = event->pos().x() - m_lastMousePos.x();
        if (dx != 0) {
            QRectF a = panelArea(0);
            qint64 rangeSec = m_viewEndSec - m_viewStartSec;
            // Convert pixel shift to time shift
            qint64 dtSec = -(qint64)((double)dx / a.width() * rangeSec);

            qint64 newStart = m_viewStartSec + dtSec;
            qint64 newEnd   = m_viewEndSec   + dtSec;

            // Clamp to global bounds
            if (newStart < m_globalMinSec) {
                newStart = m_globalMinSec;
                newEnd = newStart + rangeSec;
            }
            if (newEnd > m_globalMaxSec) {
                newEnd = m_globalMaxSec;
                newStart = newEnd - rangeSec;
                if (newStart < m_globalMinSec) newStart = m_globalMinSec;
            }

            m_viewStartSec = newStart;
            m_viewEndSec   = newEnd;
            m_lastMousePos = event->pos();
            recalcRange();
            m_cacheDirty = true;
        }
    }
    update();
}

void MacroChartWidget::mouseReleaseEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton) m_dragging = false;
}

void MacroChartWidget::resizeEvent(QResizeEvent *event) {
    QWidget::resizeEvent(event);
    recalcRange();
    m_cacheDirty = true;
}
