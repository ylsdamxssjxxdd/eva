#include "ui_widget.h"
#include "widget.h"
#include <QFontMetrics>

//-------------------------------------------------------------------------
//----------------------------------编码动画--------------------------------
//-------------------------------------------------------------------------

void Widget::decode_play()
{
    // 在状态区新增一行固定的“解码中”提示，并在该行播放动画（避免干扰其他日志）
    reflash_state(QString("ui:") + jtr("input decode"), USUAL_SIGNAL);
    decodeLineNumber_ = ui->state->document()->blockCount() - 1;
    decode_action = 0;
    decodeTimer_.restart();
    decode_pTimer->start(120); // 动画帧间隔
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
        QString::fromUtf8("⠇"), QString::fromUtf8("⠏")
    };
    static const QVector<QString> asciiFrames = { "|", "/", "-", "\\" };
    static bool decided = false;
    static bool useUnicode = true;
    if (!decided) {
        QFontMetrics fm(ui->state->font());
        // 检测首帧字符是否在当前字体中（不可用则回退 ASCII）
        useUnicode = fm.inFont(QChar(0x280B)); // '⠋'
        decided = true;
    }
    const QVector<QString> &frames = useUnicode ? uniFrames : asciiFrames;
    const QString &spin = frames.at(decode_action % frames.size());

    const QString base = QString("ui:") + jtr("input decode");
    // 仅显示转轮与用时（移除省略点）
    const QString line = QString("%1  %2  %3 s").arg(base).arg(spin).arg(QString::number(secs, 'f', 1));

    // 将该行整行替换为动画文本
    QTextBlock block = ui->state->document()->findBlockByLineNumber(decodeLineNumber_);
    if (!block.isValid()) return;
    QTextCursor cursor(block);
    cursor.select(QTextCursor::LineUnderCursor);
    cursor.removeSelectedText();
    cursor.insertText(line);

    decode_action++;
}

void Widget::decode_handleTimeout()
{
    if (decode_pTimer->isActive()) decode_pTimer->stop();
    decode_move();
    decode_pTimer->start(120);
}
