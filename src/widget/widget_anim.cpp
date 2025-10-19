#include "ui_widget.h"
#include "widget.h"
#include <QFontMetrics>

//-------------------------------------------------------------------------
//----------------------------------等待动画--------------------------------
//-------------------------------------------------------------------------

void Widget::wait_play(const QString &labelKey)
{
    // 在状态区新增一行固定提示，并在该行播放动画（避免干扰其他日志）
    if (!ui || !ui->state) return;

    const QString key = labelKey.isEmpty() ? QStringLiteral("input decode") : labelKey;
    const QString baseText = QStringLiteral("ui:") + jtr(key);

    decodeLabelKey_ = key;
    decodeBaseText_ = baseText;
    reflash_state(baseText, USUAL_SIGNAL);
    decodeLineNumber_ = ui->state->document()->blockCount() - 1;
    decode_action = 0;
    decodeTimer_.restart();
    if (decode_pTimer) decode_pTimer->start(120); // 动画帧间隔
}

void Widget::startWaitOnStateLine(const QString &labelKey, const QString &lineText)
{
    if (!ui || !ui->state || lineText.isEmpty()) return;

    const QString key = labelKey.isEmpty() ? QStringLiteral("input decode") : labelKey;
    decodeLabelKey_ = key;
    decodeBaseText_ = lineText;
    decodeLineNumber_ = ui->state->document()->blockCount() - 1;
    decode_action = 0;
    decodeTimer_.restart();

    if (!decode_pTimer) return;
    if (decode_pTimer->isActive()) decode_pTimer->stop();
    decode_move();
    decode_pTimer->start(120);
}

void Widget::decode_move()
{
    if (!ui->state || decodeLineNumber_ < 0) return;

    // 计算经过时间（秒，1位小数）
    double secs = 0.0;
    if (decodeTimer_.isValid()) secs = decodeTimer_.nsecsElapsed() / 1e9;

    // Unicode 精细转轮（Braille Dots）优先，字体不支持时回退 ASCII
    static const QVector<QString> uniFrames = {
        QString::fromUtf8("⠋"), QString::fromUtf8("⠙"), QString::fromUtf8("⠹"), QString::fromUtf8("⠸"),
        QString::fromUtf8("⠼"), QString::fromUtf8("⠴"), QString::fromUtf8("⠦"), QString::fromUtf8("⠧"),
        QString::fromUtf8("⠇"), QString::fromUtf8("⠏")};
    static const QVector<QString> asciiFrames = {"|", "/", "-", "\\"};
    static bool decided = false;
    static bool useUnicode = true;
    if (!decided)
    {
        QFontMetrics fm(ui->state->font());
        // 检测首帧字符是否在当前字体中（不可用则回退 ASCII）
        useUnicode = fm.inFont(QChar(0x280B)); // '⠋'
        decided = true;
    }
    const QVector<QString> &frames = useUnicode ? uniFrames : asciiFrames;
    const QString &spin = frames.at(decode_action % frames.size());

    const QString base = decodeBaseText_.isEmpty() ? QStringLiteral("ui:") + jtr(decodeLabelKey_) : decodeBaseText_;
    // 仅显示转轮与用时（移除省略点）
    const QString line = QString("%1 %2 s %3").arg(base).arg(QString::number(secs, 'f', 1)).arg(spin);

    // 将该行整行替换为动画文本
    QTextBlock block = ui->state->document()->findBlockByLineNumber(decodeLineNumber_);
    if (!block.isValid()) return;
    QTextCursor cursor(block);
    cursor.select(QTextCursor::LineUnderCursor);
    const QTextCharFormat format = cursor.charFormat();
    cursor.removeSelectedText();
    cursor.insertText(line, format);

    decode_action++;
}

void Widget::decode_handleTimeout()
{
    if (decode_pTimer->isActive()) decode_pTimer->stop();
    decode_move();
    decode_pTimer->start(120);
}

// 在解码动画结束时，将动画行替换为完整的完成标志
void Widget::decode_finish()
{
    // 停止动画计时器
    if (decode_pTimer && decode_pTimer->isActive()) decode_pTimer->stop();

    if (!ui->state || decodeLineNumber_ < 0) return;

    // 计算总用时（秒，1位小数）
    double secs = 0.0;
    if (decodeTimer_.isValid()) secs = decodeTimer_.nsecsElapsed() / 1e9;

    // 选择完成标志：优先使用 Unicode ✓；字体不支持时回退 ASCII [OK]
    QString doneMark = QString::fromUtf8("✓");
    {
        QFontMetrics fm(ui->state->font());
        if (!fm.inFont(QChar(0x2713)))
        {
            doneMark = "[OK]";
        }
    }

    const QString base = decodeBaseText_.isEmpty() ? QStringLiteral("ui:") + jtr(decodeLabelKey_) : decodeBaseText_;
    const QString line = QString("%1 %2 s %3").arg(base).arg(QString::number(secs, 'f', 1)).arg(doneMark);

    // 将对应行整行替换为完成文本
    QTextBlock block = ui->state->document()->findBlockByLineNumber(decodeLineNumber_);
    if (!block.isValid()) return;
    QTextCursor cursor(block);
    cursor.select(QTextCursor::LineUnderCursor);
    const QTextCharFormat format = cursor.charFormat();
    cursor.removeSelectedText();
    cursor.insertText(line, format);

    // 重置内部计数，避免下次复用旧行号
    decode_action = 0;
    decodeLineNumber_ = -1;
    decodeBaseText_.clear();
}

// 在解码动画失败（装载失败）时，将动画行替换为失败标志
void Widget::decode_fail()
{
    if (decode_pTimer && decode_pTimer->isActive()) decode_pTimer->stop();

    if (!ui->state || decodeLineNumber_ < 0) return;

    double secs = 0.0;
    if (decodeTimer_.isValid()) secs = decodeTimer_.nsecsElapsed() / 1e9;

    // 优先使用 Unicode ×；字体不支持时回退 ASCII [FAIL]
    QString failMark = QString::fromUtf8("×");
    {
        QFontMetrics fm(ui->state->font());
        if (!fm.inFont(QChar(0x00D7))) // '×'
        {
            failMark = "[FAIL]";
        }
    }

    const QString base = decodeBaseText_.isEmpty() ? QStringLiteral("ui:") + jtr(decodeLabelKey_) : decodeBaseText_;
    const QString line = QString("%1 %2 s %3").arg(base).arg(QString::number(secs, 'f', 1)).arg(failMark);

    QTextBlock block = ui->state->document()->findBlockByLineNumber(decodeLineNumber_);
    if (!block.isValid()) return;
    QTextCursor cursor(block);
    cursor.select(QTextCursor::LineUnderCursor);
    const QTextCharFormat format = cursor.charFormat();
    cursor.removeSelectedText();
    cursor.insertText(line, format);

    decode_action = 0;
    decodeLineNumber_ = -1;
    decodeBaseText_.clear();
}
