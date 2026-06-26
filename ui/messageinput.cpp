#include "messageinput.h"
#include <QKeyEvent>
#include <QContextMenuEvent>

MessageInput::MessageInput(QWidget *parent)
    : QTextEdit(parent)
{
}

void MessageInput::keyPressEvent(QKeyEvent *event)
{
    // 处理 Enter/Return 发送
    if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
        // 如果按住了 Shift，插入换行（标准行为）
        if (event->modifiers() & Qt::ShiftModifier) {
            QTextEdit::keyPressEvent(event);
            return;
        }
        // 否则发射 sendTriggered()，并拦截默认行为
        emit sendTriggered();
        event->accept();
        return;
    }

    // 处理 Tab 插入换行，并阻止焦点转移
    if (event->key() == Qt::Key_Tab) {
        this->insertPlainText("\n");
        event->accept();
        return;
    }

    // 其他按键保持默认行为
    QTextEdit::keyPressEvent(event);
}

void MessageInput::contextMenuEvent(QContextMenuEvent *event)
{
    // 暂不自定义，使用 QTextEdit 默认右键菜单
    QTextEdit::contextMenuEvent(event);
}
