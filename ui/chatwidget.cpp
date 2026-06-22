#include "chatwidget.h"
#include "messagebubble.h"
#include "client/chathistory.h"
#include <QHBoxLayout>
#include <QFileDialog>
#include <QScrollBar>
#include <QDateTime>
#include <QTimer>

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

QList<HistoryMessage> ChatWidget::getMessages() const
{
    QList<HistoryMessage> result;
    for (const ChatMessage &cm : m_messages) {
        HistoryMessage hm;
        hm.type = (cm.type == ChatMessage::Text) ? 0 : 1;
        hm.sender = cm.sender;
        hm.text = cm.text;
        hm.timestamp = cm.timestamp;
        hm.isMine = cm.isMine;
        hm.fileName = cm.fileName;
        hm.fileSize = cm.fileSize;
        result.append(hm);
    }
    return result;
}

void ChatWidget::loadHistoryMessages(const QList<HistoryMessage> &messages)
{
    for (const HistoryMessage &hm : messages) {
        if (hm.type == 0) {
            addTextMessage(hm.sender, hm.text, hm.timestamp, hm.isMine);
        } else {
            // 文件消息 — 历史记录中以已完成状态显示
            QString fakeId = QString("history_%1_%2")
                .arg(hm.timestamp).arg(hm.fileName);
            addFileMessage(hm.sender, hm.fileName, hm.fileSize, fakeId, hm.isMine);
            setFileCompleted(fakeId);
        }
    }
}
