#include "chatwidget.h"
#include "messagebubble.h"
#include "client/localdb.h"
#include "client/chatclient.h"
#include <QHBoxLayout>
#include <QScrollBar>
#include <QDateTime>
#include <QTimer>
#include <QVector>
#include <QMessageBox>

ChatWidget::ChatWidget(QWidget *parent)
    : QWidget(parent)
{
    setupUI();
    applyStyles();
}

void ChatWidget::setupUI()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // 顶部标题栏
    QWidget *header = new QWidget;
    header->setFixedHeight(55);
    header->setObjectName("chatHeader");
    QHBoxLayout *headerLayout = new QHBoxLayout(header);
    headerLayout->setContentsMargins(16, 0, 16, 0);

    m_partnerLabel = new QLabel("请选择联系人开始对话");
    m_partnerLabel->setObjectName("partnerName");
    headerLayout->addWidget(m_partnerLabel);

    m_partnerRoleLabel = new QLabel("");
    m_partnerRoleLabel->setObjectName("partnerRole");
    headerLayout->addWidget(m_partnerRoleLabel);
    headerLayout->addStretch();

    mainLayout->addWidget(header);

    // 分隔线
    QWidget *separator = new QWidget;
    separator->setFixedHeight(1);
    separator->setStyleSheet("background-color: #e0e0e0;");
    mainLayout->addWidget(separator);

    // 消息滚动区域
    m_scrollArea = new QScrollArea;
    m_scrollArea->setWidgetResizable(true);
    m_scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_scrollArea->setObjectName("chatScrollArea");

    m_messageContainer = new QWidget;
    m_messageContainer->setObjectName("messageContainer");
    m_messageLayout = new QVBoxLayout(m_messageContainer);
    m_messageLayout->setContentsMargins(0, 8, 0, 8);
    m_messageLayout->setSpacing(4);
    m_messageLayout->addStretch();

    m_scrollArea->setWidget(m_messageContainer);
    mainLayout->addWidget(m_scrollArea, 1);

    // 底部分隔
    QWidget *sep2 = new QWidget;
    sep2->setFixedHeight(1);
    sep2->setStyleSheet("background-color: #e0e0e0;");
    mainLayout->addWidget(sep2);

    // 输入区域
    QWidget *inputArea = new QWidget;
    inputArea->setObjectName("inputArea");
    inputArea->setFixedHeight(120);
    QVBoxLayout *inputLayout = new QVBoxLayout(inputArea);
    inputLayout->setContentsMargins(10, 6, 10, 6);
    inputLayout->setSpacing(6);

    m_inputEdit = new QTextEdit;
    m_inputEdit->setPlaceholderText("输入消息...");
    m_inputEdit->setObjectName("inputEdit");
    m_inputEdit->setAcceptRichText(false);
    inputLayout->addWidget(m_inputEdit);

    QHBoxLayout *btnRow = new QHBoxLayout;
    btnRow->addStretch();

    m_fileBtn = new QPushButton("发送文件");
    m_fileBtn->setObjectName("fileBtn");
    m_fileBtn->setFixedSize(80, 32);
    connect(m_fileBtn, &QPushButton::clicked, this, &ChatWidget::onFileBtnClicked);
    btnRow->addWidget(m_fileBtn);

    m_sendBtn = new QPushButton("发送");
    m_sendBtn->setObjectName("sendBtn");
    m_sendBtn->setFixedSize(70, 32);
    connect(m_sendBtn, &QPushButton::clicked, this, &ChatWidget::onSendClicked);
    btnRow->addWidget(m_sendBtn);

    inputLayout->addLayout(btnRow);
    mainLayout->addWidget(inputArea);
}

void ChatWidget::applyStyles()
{
    setStyleSheet(
        "#chatHeader { background-color: #f5f5f5; }"
        "#partnerName { font-size: 16px; font-weight: bold; color: #333; }"
        "#partnerRole { font-size: 12px; color: #999; padding-left: 6px; }"
        "#chatScrollArea { border: none; background: #ebebeb; }"
        "#messageContainer { background: #ebebeb; }"
        "#inputArea { background: #f5f5f5; }"
        "#inputEdit { border: 1px solid #ddd; border-radius: 4px; font-size: 14px; "
        "  background: white; padding: 4px; }"
        "#sendBtn { background: #07c160; color: white; border: none; border-radius: 4px; "
        "  font-size: 13px; font-weight: bold; }"
        "#sendBtn:hover { background: #06ad56; }"
        "#sendBtn:pressed { background: #059a4c; }"
        "#fileBtn { background: white; color: #07c160; border: 1px solid #07c160; "
        "  border-radius: 4px; font-size: 13px; }"
        "#fileBtn:hover { background: #e8f8ef; }"
    );
}

void ChatWidget::setChatPartner(const QString &username, const QString &role, const QString &displayName)
{
    m_partner = username;
    m_partnerRole = role;
    m_partnerLabel->setText(displayName);
    if (role == "helper")
        m_partnerRoleLabel->setText("(\xe6\x9c\xac\xe5\x9c\xb0)");
    else
        m_partnerRoleLabel->setText(role == "doctor" ? "(医生)" : "(患者)");
}

void ChatWidget::clearChat()
{
    // 清除所有消息气泡
    while (m_messageLayout->count() > 1) { // 保留最后的 stretch
        QLayoutItem *item = m_messageLayout->takeAt(0);
        if (item->widget())
            item->widget()->deleteLater();
        delete item;
    }
    m_messages.clear();
    m_fileCards.clear();
    m_partner.clear();
    m_partnerRole.clear();
    m_partnerLabel->setText("请选择联系人开始对话");
    m_partnerRoleLabel->setText("");
}

void ChatWidget::addTextMessage(const QString &sender, const QString &text,
                                 qint64 timestamp, bool isMine)
{
    if (m_partner.isEmpty()) return;

    ChatMessage msg;
    msg.type = ChatMessage::Text;
    msg.sender = sender;
    msg.text = text;
    msg.timestamp = timestamp;
    msg.isMine = isMine;
    m_messages.append(msg);

    // 在 stretch 之前插入
    int insertIndex = m_messageLayout->count() - 1;
    MessageBubble *bubble = new MessageBubble(text, sender, formatTime(timestamp), isMine);
    m_messageLayout->insertWidget(insertIndex, bubble);

    scrollToBottom();
}

void ChatWidget::addFileMessage(const QString &sender, const QString &fileName,
                                 qint64 fileSize, const QString &fileId, bool isMine)
{
    if (m_partner.isEmpty()) return;

    ChatMessage msg;
    msg.type = ChatMessage::File;
    msg.sender = sender;
    msg.fileName = fileName;
    msg.fileSize = fileSize;
    msg.fileId = fileId;
    msg.timestamp = QDateTime::currentMSecsSinceEpoch();
    msg.isMine = isMine;
    m_messages.append(msg);

    int insertIndex = m_messageLayout->count() - 1;
    FileMessageCard *card = new FileMessageCard(fileName, fileSize, isMine,
                                                 sender, formatTime(msg.timestamp));
    card->fileId = fileId;
    connect(card, &FileMessageCard::acceptClicked, this, &ChatWidget::onFileAcceptClicked);
    connect(card, &FileMessageCard::rejectClicked, this, &ChatWidget::onFileRejectClicked);
    connect(card, &FileMessageCard::openClicked, this, &ChatWidget::onFileOpenClicked);

    m_fileCards[fileId] = card;
    m_messageLayout->insertWidget(insertIndex, card);

    scrollToBottom();
}

void ChatWidget::updateFileProgress(const QString &fileId, qint64 received, qint64 total)
{
    if (m_fileCards.contains(fileId)) {
        m_fileCards[fileId]->setProgress(received, total);
        m_fileCards[fileId]->setState(FileMessageCard::Transferring);
    }
}

void ChatWidget::setFileCompleted(const QString &fileId)
{
    if (m_fileCards.contains(fileId))
        m_fileCards[fileId]->setState(FileMessageCard::Completed);
}

void ChatWidget::setFileRejected(const QString &fileId, const QString &reason)
{
    if (m_fileCards.contains(fileId))
        m_fileCards[fileId]->setState(FileMessageCard::Rejected, reason);
}

void ChatWidget::setFileError(const QString &fileId, const QString &error)
{
    if (m_fileCards.contains(fileId))
        m_fileCards[fileId]->setState(FileMessageCard::Error, error);
}

void ChatWidget::onSendClicked()
{
    if (m_partner.isEmpty()) return;
    QString text = m_inputEdit->toPlainText().trimmed();
    if (text.isEmpty()) return;

    emit sendMessage(m_partner, text);
    m_inputEdit->clear();
}

void ChatWidget::onFileBtnClicked()
{
    if (m_partner.isEmpty()) return;
    emit sendFileRequest(m_partner);
}

void ChatWidget::onFileAcceptClicked()
{
    FileMessageCard *card = qobject_cast<FileMessageCard*>(sender());
    if (card) {
        emit fileAccepted(card->fileId);
    }
}

void ChatWidget::onFileRejectClicked()
{
    FileMessageCard *card = qobject_cast<FileMessageCard*>(sender());
    if (card) {
        emit fileRejected(card->fileId);
    }
}

void ChatWidget::scrollToBottom()
{
    QTimer::singleShot(50, this, [this]() {
        QScrollBar *bar = m_scrollArea->verticalScrollBar();
        bar->setValue(bar->maximum());
    });
}

QString ChatWidget::formatTime(qint64 msecsSinceEpoch)
{
    QDateTime dt = QDateTime::fromMSecsSinceEpoch(msecsSinceEpoch);
    return dt.toString("HH:mm:ss");
}

void ChatWidget::onFileOpenClicked()
{
    FileMessageCard *card = qobject_cast<FileMessageCard*>(sender());
    if (!card) return;

    QString fileId = card->fileId;
    FileRecord rec = LocalDB::instance().getFileRecord(fileId);
    if (rec.fileId.isEmpty() || rec.status != 1) {
        QMessageBox::information(this, "提示", "文件已过期或被清理");
        return;
    }
    if (!LocalDB::instance().openFile(fileId)) {
        QMessageBox::information(this, "提示", "文件已过期或被清理");
    }
}

void ChatWidget::loadHistoryMessages(const QVector<StoredMessage> &messages,
                                     const QString &partnerNick,
                                     std::function<bool(const QString&)> transferActiveCheck)
{
    for (const StoredMessage &sm : messages) {
        if (sm.type == 0) {
            // 文本消息 — 直接添加（不重复入库）
            ChatMessage msg;
            msg.type = ChatMessage::Text;
            msg.sender = sm.isMine ? "me" : partnerNick;
            msg.text = sm.content;
            msg.timestamp = sm.timestamp;
            msg.isMine = sm.isMine;
            m_messages.append(msg);

            int insertIndex = m_messageLayout->count() - 1;
            MessageBubble *bubble = new MessageBubble(sm.content, msg.sender,
                                                       formatTime(sm.timestamp), sm.isMine);
            m_messageLayout->insertWidget(insertIndex, bubble);
        } else {
            // 文件消息 — 查询 file_index 获取实际状态
            ChatMessage msg;
            msg.type = ChatMessage::File;
            msg.sender = sm.isMine ? "me" : partnerNick;
            msg.fileName = sm.content;
            msg.fileId = sm.fileId;
            msg.timestamp = sm.timestamp;
            msg.isMine = sm.isMine;
            m_messages.append(msg);

            // 查询文件大小
            FileRecord rec = LocalDB::instance().getFileRecord(sm.fileId);
            qint64 fsize = rec.size;

            int insertIndex = m_messageLayout->count() - 1;
            FileMessageCard *fcard = new FileMessageCard(
                sm.content, fsize, sm.isMine,
                msg.sender, formatTime(sm.timestamp));
            fcard->fileId = sm.fileId;

            // 连接全部信号（包括 accept/reject，之前漏掉了）
            connect(fcard, &FileMessageCard::acceptClicked, this, &ChatWidget::onFileAcceptClicked);
            connect(fcard, &FileMessageCard::rejectClicked, this, &ChatWidget::onFileRejectClicked);
            connect(fcard, &FileMessageCard::openClicked, this, &ChatWidget::onFileOpenClicked);

            if (rec.status == 1) {
                fcard->setState(FileMessageCard::Completed);
            } else if (rec.status == 3) {
                // status=3: 用户拒绝
                fcard->setState(FileMessageCard::Rejected, "用户拒绝");
            } else if (rec.status == 2) {
                fcard->setState(FileMessageCard::Error, "传输失败");
            } else {
                // status=0: 需要区分「尚未接收」「正在传输」「stale 中断」
                bool active = transferActiveCheck && transferActiveCheck(sm.fileId);
                if (active) {
                    // 传输正在进行中 → 显示传输中（无按钮，等待 progress 信号更新）
                    fcard->setState(FileMessageCard::Transferring);
                } else if (!sm.isMine && rec.savePath.isEmpty()) {
                    // 接收方，尚未 accept（无 savePath）→ 显示等待接收（带按钮）
                    fcard->setState(FileMessageCard::Pending);
                } else {
                    // stale: 已 accept 但传输中断，或发送方异常 → 错误
                    fcard->setState(FileMessageCard::Error, "传输中断");
                }
            }

            m_fileCards[sm.fileId] = fcard;
            m_messageLayout->insertWidget(insertIndex, fcard);
        }
    }
    scrollToBottom();
}
