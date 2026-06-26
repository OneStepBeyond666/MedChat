#ifndef MESSAGEINPUT_H
#define MESSAGEINPUT_H

#include <QTextEdit>
#include <QKeyEvent>

class MessageInput : public QTextEdit
{
    Q_OBJECT
public:
    explicit MessageInput(QWidget *parent = nullptr);

signals:
    void sendTriggered();  // 按下 Enter/Return 时发射（无修饰键）

protected:
    void keyPressEvent(QKeyEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;
};

#endif // MESSAGEINPUT_H
