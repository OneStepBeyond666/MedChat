#include "chatwidget.h"
#include "messagebubble.h"
#include "client/localdb.h"
#include "client/chatclient.h"
#include "common/constants.h"
#include <QHBoxLayout>
#include <QScrollBar>
#include <QDateTime>
#include <QTimer>
#include <QVector>
#include <QMessageBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QUrl>

ChatWidget::ChatWidget(QWidget *parent)
    : QWidget(parent)
{
    setupUI();
    applyStyles();
    setAcceptDrops(true);
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

    // ---- 文件预览区（默认隐藏）----
    m_previewArea = new QWidget;
    m_previewArea->setObjectName("previewArea");
    m_previewArea->setFixedHeight(50);
    m_previewArea->hide();

    QHBoxLayout *previewLayout = new QHBoxLayout(m_previewArea);
    previewLayout->setContentsMargins(10, 6, 10, 6);
    previewLayout->setSpacing(8);

    QLabel *fileIcon = new QLabel("\xF0\x9F\x93\x84");  // 📄 emoji
    fileIcon->setStyleSheet("font-size: 20px; background: transparent;");
    previewLayout->addWidget(fileIcon);

    m_previewNameLabel = new QLabel;
    m_previewNameLabel->setObjectName("previewName");
    m_previewNameLabel->setStyleSheet("font-size: 13px; color: #333; background: transparent;");
    previewLayout->addWidget(m_previewNameLabel);

    m_previewSizeLabel = new QLabel;
    m_previewSizeLabel->setObjectName("previewSize");
    m_previewSizeLabel->setStyleSheet("font-size: 12px; color: #999; background: transparent;");
    previewLayout->addWidget(m_previewSizeLabel);

    previewLayout->addStretch();

    QPushButton *closeBtn = new QPushButton("\xC3\x97");  // × symbol
    closeBtn->setObjectName("previewCloseBtn");
    closeBtn->setFixedSize(22, 22);
    connect(closeBtn, &QPushButton::clicked, this, &ChatWidget::onPreviewClose);
    previewLayout->addWidget(closeBtn);

    mainLayout->addWidget(m_previewArea);

    // 底部分隔
    QWidget *sep2 = new QWidget;
    sep2->setFixedHeight(1);
    sep2->setStyleSheet("background-color: #e0e0e0;");
    mainLayout->addWidget(sep2);

    // ---- 输入区域 ----
    QWidget *inputArea = new QWidget;
    inputArea->setObjectName("inputArea");
    QVBoxLayout *inputLayout = new QVBoxLayout(inputArea);
    inputLayout->setContentsMargins(10, 6, 10, 6);
    inputLayout->setSpacing(4);

    // 输入框
    m_inputEdit = new QTextEdit;
    m_inputEdit->setPlaceholderText("输入消息...");
    m_inputEdit->setObjectName("inputEdit");
    m_inputEdit->setAcceptRichText(false);
    m_inputEdit->setFixedHeight(60);
    inputLayout->addWidget(m_inputEdit);

    // 工具栏：+ 按钮 (左) + 弹性空间 + 发送按钮 (右)
    QHBoxLayout *toolBar = new QHBoxLayout;
    toolBar->setSpacing(0);

    m_plusBtn = new QToolButton;
    m_plusBtn->setObjectName("plusBtn");
    m_plusBtn->setText("+");
    m_plusBtn->setFixedSize(28, 28);
    m_plusBtn->setToolButtonStyle(Qt::ToolButtonTextOnly);
    connect(m_plusBtn, &QToolButton::clicked, this, &ChatWidget::onPlusClicked);
    toolBar->addWidget(m_plusBtn);

    toolBar->addStretch();

    m_sendBtn = new QPushButton("发送");
    m_sendBtn->setObjectName("sendBtn");
    m_sendBtn->setMinimumWidth(60);
    m_sendBtn->setFixedHeight(30);
    connect(m_sendBtn, &QPushButton::clicked, this, &ChatWidget::onSendClicked);
    toolBar->addWidget(m_sendBtn);

    inputLayout->addLayout(toolBar);
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

        // 文件预览区
        "#previewArea { background-color: #F5F5F5; border: 1px dashed #CCCCCC; border-radius: 4px; margin: 4px 10px 0 10px; }"
        "#previewCloseBtn { background: transparent; border: none; color: #999; font-size: 16px; font-weight: bold; border-radius: 11px; }"
        "#previewCloseBtn:hover { background: #E0E0E0; color: #666; }"

        // 输入区
        "#inputArea { background: #f5f5f5; }"
        "#inputEdit { border: 1px solid #E0E0E0; border-radius: 4px; font-size: 14px; "
        "  background: white; padding: 4px; }"
        "#inputEdit:focus { border: 1px solid #07C160; }"

        // + 按钮
        "#plusBtn { background: transparent; border: none; color: #555; font-size: 20px; font-weight: bold; border-radius: 4px; }"
        "#plusBtn:hover { background: #E5E5E5; color: #07C160; }"

        // 发送按钮
        "#sendBtn { background: #07C160; color: white; border: none; border-radius: 4px; "
        "  font-size: 13px; font-weight: bold; }"
        "#sendBtn:hover { background: #06AD56; }"
        "#sendBtn:pressed { background: #059A4C; }"
    );
}

// ============================================================
// + 按钮：直接打开文件选择对话框
// ============================================================

void ChatWidget::onPlusClicked()
{
    if (m_partner.isEmpty()) return;

    QString filePath = QFileDialog::getOpenFileName(this, "选择要发送的文件");
    if (!filePath.isEmpty())
        showFilePreview(filePath);
}

// ============================================================
// 文件预览
// ============================================================

void ChatWidget::showFilePreview(const QString &filePath)
{
    QFileInfo fi(filePath);
    if (!fi.exists()) return;

    if (fi.size() > Constants::MAX_FILE_SIZE) {
        QMessageBox::warning(this, "文件过大",
            QString("文件 \"%1\" 超过100MB传输限制，无法发送。").arg(fi.fileName()));
        return;
    }

    m_pendingFilePath = filePath;

    // 文件名：超过20字符截断
    QString name = fi.fileName();
    if (name.length() > 20)
        name = name.left(17) + "...";
    m_previewNameLabel->setText(name);
    m_previewSizeLabel->setText(humanFileSize(fi.size()));

    m_previewArea->show();
}

void ChatWidget::clearFilePreview()
{
    m_pendingFilePath.clear();
    m_previewNameLabel->clear();
    m_previewSizeLabel->clear();
    m_previewArea->hide();
}

void ChatWidget::onPreviewClose()
{
    clearFilePreview();
}

QString ChatWidget::humanFileSize(qint64 bytes) const
{
    if (bytes < 1024)
        return QString("%1 B").arg(bytes);
    if (bytes < 1024 * 1024)
        return QString("%1 KB").arg(bytes / 1024.0, 0, 'f', 1);
    return QString("%1 MB").arg(bytes / (1024.0 * 1024.0), 0, 'f', 1);
}

// ============================================================
// 发送逻辑
// ============================================================

void ChatWidget::onSendClicked()
{
    if (m_partner.isEmpty()) return;

    // 如果有待发送文件
    if (m_previewArea->isVisible() && !m_pendingFilePath.isEmpty()) {
        emit sendFileWithPath(m_partner, m_pendingFilePath);
        clearFilePreview();
        return;
    }

    // 否则发送文本
    QString text = m_inputEdit->toPlainText().trimmed();
    if (text.isEmpty()) return;

    emit sendMessage(m_partner, text);
    m_inputEdit->clear();
}

// ============================================================
// 拖拽
// ============================================================

void ChatWidget::dragEnterEvent(QDragEnterEvent *event)
{
    if (event->mimeData()->hasUrls()) {
        event->acceptProposedAction();
        // 视觉反馈：显示预览区虚线框
        if (m_previewArea->isHidden()) {
            m_previewArea->setStyleSheet(
                "#previewArea { background-color: #E3F2FD; border: 1px dashed #90CAF9; border-radius: 4px; margin: 4px 10px 0 10px; }");
            m_previewArea->show();
            m_previewNameLabel->setText("释放文件以预览...");
            m_previewSizeLabel->clear();
        }
    }
}

void ChatWidget::dragLeaveEvent(QDragLeaveEvent *event)
{
    Q_UNUSED(event);
    // 恢复预览区样式
    if (m_pendingFilePath.isEmpty()) {
        clearFilePreview();
    }
    // 恢复原始样式
    m_previewArea->setStyleSheet("");  // 清除临时样式，回到 applyStyles 中的定义
    applyStyles();
}

void ChatWidget::dropEvent(QDropEvent *event)
{
    if (event->mimeData()->hasUrls()) {
        QList<QUrl> urls = event->mimeData()->urls();
        if (!urls.isEmpty()) {
            QString filePath = urls.first().toLocalFile();
            if (!filePath.isEmpty()) {
                showFilePreview(filePath);
            }
        }
        event->acceptProposedAction();
    }
}

// ============================================================
// 以下方法保持不变
// ============================================================

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
    while (m_messageLayout->count() > 1) {
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
    clearFilePreview();
}

void ChatWidget::addTextMessage(const QString &sender, const QString &text,
                                 qint64 timestamp, bool isMine)
{
    addTextMessageWithId(sender, text, timestamp, isMine);
}

MessageBubble *ChatWidget::addTextMessageWithId(const QString &sender, const QString &text,
                                 qint64 timestamp, bool isMine)
{
    if (m_partner.isEmpty()) return nullptr;

    ChatMessage msg;
    msg.type = ChatMessage::Text;
    msg.sender = sender;
    msg.text = text;
    msg.timestamp = timestamp;
    msg.isMine = isMine;
    // msgId will be set later by MainWindow after persisting to DB
    m_messages.append(msg);

    int insertIndex = m_messageLayout->count() - 1;
    MessageBubble *bubble = new MessageBubble(text, sender, formatTime(timestamp), isMine);
    bubble->setTimestamp(timestamp);
    connect(bubble, &MessageBubble::deleteRequested, this, &ChatWidget::deleteRequested);
    connect(bubble, &MessageBubble::recallRequested, this, &ChatWidget::recallRequested);
    connect(bubble, &MessageBubble::forwardRequested, this, &ChatWidget::forwardRequested);
    m_messageLayout->insertWidget(insertIndex, bubble);

    scrollToBottom();
    return bubble;
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
    connect(card, &FileMessageCard::forwardRequested, this, &ChatWidget::forwardRequested);
    connect(card, &FileMessageCard::deleteRequested, this, &ChatWidget::deleteRequested);

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

void ChatWidget::setFileTransferring(const QString &fileId)
{
    if (m_fileCards.contains(fileId))
        m_fileCards[fileId]->setState(FileMessageCard::Transferring);
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
            ChatMessage msg;
            msg.type = ChatMessage::Text;
            msg.sender = sm.isMine ? "me" : partnerNick;
            msg.text = sm.content;
            msg.timestamp = sm.timestamp;
            msg.isMine = sm.isMine;
            msg.msgId = sm.msgId;
            msg.isRecalled = sm.isRecalled;
            m_messages.append(msg);

            int insertIndex = m_messageLayout->count() - 1;
            MessageBubble *bubble = new MessageBubble(sm.content, msg.sender,
                                                       formatTime(sm.timestamp), sm.isMine);
            bubble->setMsgId(sm.msgId);
            bubble->setTimestamp(sm.timestamp);
            connect(bubble, &MessageBubble::deleteRequested, this, &ChatWidget::deleteRequested);
            connect(bubble, &MessageBubble::recallRequested, this, &ChatWidget::recallRequested);
            connect(bubble, &MessageBubble::forwardRequested, this, &ChatWidget::forwardRequested);
            if (sm.isRecalled) {
                bubble->setRecalled(true, sm.isMine);
            }
            m_messageLayout->insertWidget(insertIndex, bubble);
        } else {
            ChatMessage msg;
            msg.type = ChatMessage::File;
            msg.sender = sm.isMine ? "me" : partnerNick;
            msg.fileName = sm.content;
            msg.fileId = sm.fileId;
            msg.timestamp = sm.timestamp;
            msg.isMine = sm.isMine;
            m_messages.append(msg);

            FileRecord rec = LocalDB::instance().getFileRecord(sm.fileId);
            qint64 fsize = rec.size;

            int insertIndex = m_messageLayout->count() - 1;
            FileMessageCard *fcard = new FileMessageCard(
                sm.content, fsize, sm.isMine,
                msg.sender, formatTime(sm.timestamp));
            fcard->fileId = sm.fileId;
            fcard->setMsgId(sm.msgId);

            connect(fcard, &FileMessageCard::acceptClicked, this, &ChatWidget::onFileAcceptClicked);
            connect(fcard, &FileMessageCard::rejectClicked, this, &ChatWidget::onFileRejectClicked);
            connect(fcard, &FileMessageCard::openClicked, this, &ChatWidget::onFileOpenClicked);
            connect(fcard, &FileMessageCard::forwardRequested, this, &ChatWidget::forwardRequested);
            connect(fcard, &FileMessageCard::deleteRequested, this, &ChatWidget::deleteRequested);

            if (rec.status == 1) {
                fcard->setState(FileMessageCard::Completed);
            } else if (rec.status == 3) {
                fcard->setState(FileMessageCard::Rejected, "用户拒绝");
            } else if (rec.status == 2) {
                fcard->setState(FileMessageCard::Error, "传输失败");
            } else {
                bool active = transferActiveCheck && transferActiveCheck(sm.fileId);
                if (active) {
                    fcard->setState(FileMessageCard::Transferring);
                } else if (!sm.isMine && rec.savePath.isEmpty()) {
                    fcard->setState(FileMessageCard::Pending);
                } else {
                    fcard->setState(FileMessageCard::Error, "传输中断");
                }
            }

            m_fileCards[sm.fileId] = fcard;
            m_messageLayout->insertWidget(insertIndex, fcard);
        }
    }
    scrollToBottom();
}

void ChatWidget::removeMessageByMsgId(qint64 msgId)
{
    // 遍历 layout 找到对应的 MessageBubble
    for (int i = 0; i < m_messageLayout->count(); ++i) {
        QLayoutItem *item = m_messageLayout->itemAt(i);
        if (!item || !item->widget()) continue;

        MessageBubble *bubble = qobject_cast<MessageBubble*>(item->widget());
        if (bubble && bubble->msgId() == msgId) {
            // 从 layout 中移除
            m_messageLayout->removeWidget(bubble);
            bubble->deleteLater();

            // 从 m_messages 列表中移除对应条目
            for (int j = 0; j < m_messages.size(); ++j) {
                if (m_messages[j].msgId == msgId) {
                    m_messages.removeAt(j);
                    break;
                }
            }
            break;
        }
    }
}

void ChatWidget::setMessageRecalled(qint64 msgId, bool isMine)
{
    for (int i = 0; i < m_messageLayout->count(); ++i) {
        QLayoutItem *item = m_messageLayout->itemAt(i);
        if (!item || !item->widget()) continue;

        MessageBubble *bubble = qobject_cast<MessageBubble*>(item->widget());
        if (bubble && bubble->msgId() == msgId && !bubble->isRecalled()) {
            bubble->setRecalled(true, isMine);
            // 更新 m_messages 中的状态
            for (int j = 0; j < m_messages.size(); ++j) {
                if (m_messages[j].msgId == msgId) {
                    m_messages[j].isRecalled = true;
                    m_messages[j].text = isMine
                        ? QStringLiteral("[你撤回了一条消息]")
                        : QStringLiteral("[对方撤回了一条消息]");
                    break;
                }
            }
            break;
        }
    }
}
