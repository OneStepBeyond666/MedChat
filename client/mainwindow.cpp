#include "mainwindow.h"
#include "chatclient.h"
#include "chathistory.h"
#include "common/protocol.h"
#include "ui/contactlistwidget.h"
#include "ui/chatwidget.h"
#include <QSplitter>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QStatusBar>
#include <QMessageBox>
#include <QFileDialog>
#include <QStandardPaths>
#include <QApplication>
#include <QDateTime>
#include <QFileInfo>
#include <QLabel>
#include <QDebug>

MainWindow::MainWindow(ChatClient *client, const QString &username, const QString &role,
                       const QString &nickname, QWidget *parent)
    : QMainWindow(parent), m_client(client), m_username(username), m_role(role), m_nickname(nickname)
{
    setWindowTitle(QString("远程问诊系统 - %1 (%2)").arg(m_nickname, role == "doctor" ? "医生" : "患者"));
    resize(960, 640);

    setupUI();
    applyStyles();

    // 初始化聊天历史管理器（exe同目录下的 history 文件夹）
    m_history = new ChatHistory(QApplication::applicationDirPath() + "/history", this);

    // 连接信号
    connect(m_client, &ChatClient::contactListUpdated, this, &MainWindow::onContactListUpdated);
    connect(m_client, &ChatClient::textMessageReceived, this, &MainWindow::onTextMessageReceived);
    connect(m_client, &ChatClient::messageAck, this, &MainWindow::onMessageAck);
    connect(m_client, &ChatClient::fileOfferReceived, this, &MainWindow::onFileOfferReceived);
    connect(m_client, &ChatClient::fileAccepted, this, &MainWindow::onFileAccepted);
    connect(m_client, &ChatClient::fileRejected, this, &MainWindow::onFileRejected);
    connect(m_client, &ChatClient::fileProgress, this, &MainWindow::onFileProgress);
    connect(m_client, &ChatClient::fileCompleted, this, &MainWindow::onFileCompleted);
    connect(m_client, &ChatClient::fileError, this, &MainWindow::onFileError);
    connect(m_client, &ChatClient::serverError, this, &MainWindow::onServerError);
    connect(m_client, &ChatClient::disconnected, this, &MainWindow::onDisconnected);

    connect(m_contactList, &ContactListWidget::contactSelected, this, &MainWindow::onContactSelected);
    connect(m_chatWidget, &ChatWidget::sendMessage, this, &MainWindow::onSendTextMessage);
    connect(m_chatWidget, &ChatWidget::sendFileRequest, this, &MainWindow::onSendFileRequest);
    connect(m_chatWidget, &ChatWidget::fileAccepted, this, &MainWindow::onFileAcceptFromUI);
    connect(m_chatWidget, &ChatWidget::fileRejected, this, &MainWindow::onFileRejectFromUI);

    statusBar()->showMessage("已连接 - " + m_nickname + " (" + (role == "doctor" ? "医生" : "患者") + ")");
}

void MainWindow::setupUI()
{
    QWidget *central = new QWidget;
    setCentralWidget(central);

    QHBoxLayout *mainLayout = new QHBoxLayout(central);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    QSplitter *splitter = new QSplitter(Qt::Horizontal);
    splitter->setHandleWidth(1);

    // 左侧联系人列表
    QWidget *leftPanel = new QWidget;
    leftPanel->setObjectName("leftPanel");
    leftPanel->setFixedWidth(260);
    QVBoxLayout *leftLayout = new QVBoxLayout(leftPanel);
    leftLayout->setContentsMargins(0, 0, 0, 0);

    m_contactList = new ContactListWidget;
    leftLayout->addWidget(m_contactList);

    splitter->addWidget(leftPanel);

    // 右侧：QStackedWidget 切换占位页 / 聊天页
    m_chatStack = new QStackedWidget;

    // Page 0: 占位页（未选择联系人时显示）
    QWidget *placeholder = new QWidget;
    placeholder->setObjectName("chatPlaceholder");
    QVBoxLayout *phLayout = new QVBoxLayout(placeholder);
    phLayout->setAlignment(Qt::AlignCenter);
    QLabel *phLabel = new QLabel("请选择联系人开始对话");
    phLabel->setObjectName("placeholderLabel");
    phLabel->setAlignment(Qt::AlignCenter);
    phLayout->addWidget(phLabel);
    m_chatStack->addWidget(placeholder);

    // Page 1: 聊天页
    m_chatWidget = new ChatWidget;
    m_chatStack->addWidget(m_chatWidget);

    m_chatStack->setCurrentIndex(0);  // 初始显示占位页

    splitter->addWidget(m_chatStack);

    splitter->setStretchFactor(0, 0);
    splitter->setStretchFactor(1, 1);

    mainLayout->addWidget(splitter);
}

void MainWindow::applyStyles()
{
    setStyleSheet(
        "#leftPanel { background: #f0f0f0; border-right: 1px solid #ddd; }"
        "QSplitter::handle { background: #ddd; }"
        "QStatusBar { background: #f5f5f5; font-size: 12px; color: #666; }"
        "#chatPlaceholder { background: #fafafa; }"
        "#placeholderLabel { font-size: 18px; color: #aaa; }"
    );
}

QString MainWindow::displayName(const QString &username) const
{
    if (username == m_username)
        return m_nickname;
    const ContactInfo ci = m_client->contacts().value(username);
    return ci.nickname.isEmpty() ? username : ci.nickname;
}

void MainWindow::onContactSelected(const QString &username)
{
    // 1. 保存当前对话历史到磁盘
    saveCurrentHistory();

    // 再次点击同一联系人 → 取消选中，回到占位页
    if (m_chatWidget->currentPartner() == username) {
        m_chatWidget->clearChat();
        m_chatStack->setCurrentIndex(0);
        m_contactList->clearSelection();
        return;
    }

    // 2. 清空聊天区域
    m_chatWidget->clearChat();

    // 3. 切换到聊天页
    m_chatStack->setCurrentIndex(1);

    if (username == QString::fromUtf8(MsgType::FileHelper)) {
        m_chatWidget->setChatPartner(username, "helper", QString::fromUtf8(MsgType::FileHelper));
        // 加载文件传输助手历史
        QList<HistoryMessage> history = m_history->loadHistory(m_username, username);
        if (!history.isEmpty())
            m_chatWidget->loadHistoryMessages(history);
        return;
    }

    QMap<QString, ContactInfo> contacts = m_client->contacts();
    QString role = contacts.value(username).role;
    QString nick = contacts.value(username).nickname;
    if (nick.isEmpty()) nick = username;
    m_chatWidget->setChatPartner(username, role, nick);

    // 4. 从磁盘加载该联系人的历史消息
    QList<HistoryMessage> history = m_history->loadHistory(m_username, username);
    if (!history.isEmpty())
        m_chatWidget->loadHistoryMessages(history);

    // 5. 追加缓冲区中的新消息（尚未落盘的部分）
    flushBufferToUI(username);
}

void MainWindow::onContactListUpdated(const QMap<QString, ContactInfo> &contacts)
{
    m_contactList->updateContacts(contacts);
}

void MainWindow::onTextMessageReceived(const QString &from, const QString &to,
                                        const QString &text, qint64 timestamp)
{
    QString partner = (to == m_username) ? from : to;

    // 始终写入缓冲区
    MsgBufEntry entry;
    entry.type = 0;
    entry.sender = from;
    entry.text = text;
    entry.timestamp = timestamp;
    entry.isMine = (from == m_username);
    entry.fileSize = 0;
    entry.fileId = "";
    m_msgBuffers[partner].append(entry);

    // 仅当前对话联系人才实时显示
    if (m_chatWidget->currentPartner() == partner) {
        m_chatWidget->addTextMessage(displayName(from), text, timestamp, false);
    }
}

void MainWindow::onMessageAck(const QString &to, qint64 timestamp)
{
    // 发送成功确认，在当前对话窗口添加自己的消息
    if (m_chatWidget->currentPartner() == to) {
        // 我们需要获取之前输入的文本... 这里简化处理
        // 实际上消息已经在发送时添加到UI了（见 onSendTextMessage）
    }
    Q_UNUSED(timestamp)
}

void MainWindow::onSendTextMessage(const QString &to, const QString &text)
{
    qint64 now = QDateTime::currentMSecsSinceEpoch();

    // 文件传输助手：本地回环，不经过服务端
    if (to == QString::fromUtf8(MsgType::FileHelper)) {
        m_chatWidget->addTextMessage(m_nickname, text, now, true);
        m_chatWidget->addTextMessage(m_nickname, text, now, false);
        return;
    }

    m_client->sendTextMessage(to, text);

    // 写入缓冲区（保证持久化一致性）
    MsgBufEntry entry;
    entry.type = 0;
    entry.sender = m_username;
    entry.text = text;
    entry.timestamp = now;
    entry.isMine = true;
    entry.fileSize = 0;
    entry.fileId = "";
    m_msgBuffers[to].append(entry);

    // 立即在UI显示自己发送的消息（发送时一定在当前对话）
    m_chatWidget->addTextMessage(m_nickname, text, now, true);
}

void MainWindow::onSendFileRequest(const QString &to)
{
    QString filePath = QFileDialog::getOpenFileName(this, "选择要发送的文件",
        QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation));
    if (filePath.isEmpty()) return;

    QFileInfo fi(filePath);

    // 文件传输助手：本地复制文件，不走网络
    if (to == QString::fromUtf8(MsgType::FileHelper)) {
        QString savePath = QFileDialog::getSaveFileName(this, "保存文件到",
            QStandardPaths::writableLocation(QStandardPaths::DownloadLocation) + "/" + fi.fileName());
        if (savePath.isEmpty()) return;

        // 显示发送卡片
        m_chatWidget->addFileMessage(m_nickname, fi.fileName(), fi.size(), "helper_send", true);
        m_chatWidget->setFileCompleted("helper_send");

        // 复制文件
        QFile::copy(filePath, savePath);

        // 显示接收卡片
        m_chatWidget->addFileMessage(m_nickname, fi.fileName(), fi.size(), "helper_recv", false);
        m_chatWidget->setFileCompleted("helper_recv");

        statusBar()->showMessage("文件已保存到: " + savePath, 5000);
        return;
    }

    m_client->sendFile(to, filePath);

    // 在UI显示文件发送卡片
    m_chatWidget->addFileMessage(m_nickname, fi.fileName(), fi.size(), "pending_send", true);
}

void MainWindow::onFileOfferReceived(const QString &from, const QString &fileName,
                                      qint64 fileSize, const QString &fileId)
{
    m_pendingFileOffers[fileId] = from;

    // 始终写入缓冲区
    MsgBufEntry entry;
    entry.type = 1;
    entry.sender = from;
    entry.text = "";
    entry.timestamp = QDateTime::currentMSecsSinceEpoch();
    entry.isMine = false;
    entry.fileName = fileName;
    entry.fileSize = fileSize;
    entry.fileId = fileId;
    m_msgBuffers[from].append(entry);

    // 当前联系人 → 显示文件卡片；非当前 → 弹窗通知
    if (m_chatWidget->currentPartner() == from) {
        m_chatWidget->addFileMessage(displayName(from), fileName, fileSize, fileId, false);
    } else {
        showMessage("文件接收", displayName(from) + " 向您发送文件: " + fileName);
    }
}

void MainWindow::onFileAccepted(const QString &fileId)
{
    // 发送方收到接收确认，开始传输 — UI已在发送时创建
    qDebug() << "File accepted, starting transfer:" << fileId;
}

void MainWindow::onFileRejected(const QString &fileId, const QString &reason)
{
    m_chatWidget->setFileRejected(fileId, reason);
    m_pendingFileOffers.remove(fileId);
}

void MainWindow::onFileProgress(const QString &fileId, qint64 received, qint64 total)
{
    m_chatWidget->updateFileProgress(fileId, received, total);
}

void MainWindow::onFileCompleted(const QString &fileId, const QString &savePath)
{
    m_chatWidget->setFileCompleted(fileId);
    if (!savePath.isEmpty()) {
        statusBar()->showMessage("文件已保存到: " + savePath, 5000);
    }
}

void MainWindow::onFileError(const QString &fileId, const QString &error)
{
    m_chatWidget->setFileError(fileId, error);
}

void MainWindow::onFileAcceptFromUI(const QString &fileId)
{
    QString savePath = QFileDialog::getSaveFileName(this, "保存文件到",
        QStandardPaths::writableLocation(QStandardPaths::DownloadLocation));
    if (savePath.isEmpty()) return;

    m_client->acceptFile(fileId, savePath);
}

void MainWindow::onFileRejectFromUI(const QString &fileId)
{
    m_client->rejectFile(fileId);
    m_pendingFileOffers.remove(fileId);
}

void MainWindow::onServerError(const QString &error)
{
    statusBar()->showMessage("服务器错误: " + error, 5000);
}

void MainWindow::onDisconnected()
{
    statusBar()->showMessage("已断开连接");
    QMessageBox::warning(this, "连接断开", "与服务器的连接已断开。");
}

void MainWindow::showMessage(const QString &title, const QString &text)
{
    QMessageBox::information(this, title, text);
}

void MainWindow::saveCurrentHistory()
{
    QString partner = m_chatWidget->currentPartner();
    if (partner.isEmpty()) return;
    QList<HistoryMessage> messages = m_chatWidget->getMessages();
    if (!messages.isEmpty())
        m_history->saveHistory(m_username, partner, messages);
    // 当前联系人的缓冲区消息已全部显示在视图中并保存到磁盘，清空缓冲区
    m_msgBuffers.remove(partner);
}

void MainWindow::flushBufferToUI(const QString &partner)
{
    QList<MsgBufEntry> &buffer = m_msgBuffers[partner];
    if (buffer.isEmpty()) return;

    for (const MsgBufEntry &e : buffer) {
        QString sender = displayName(e.sender);
        if (e.type == 0) {
            m_chatWidget->addTextMessage(sender, e.text, e.timestamp, e.isMine);
        } else {
            m_chatWidget->addFileMessage(sender, e.fileName, e.fileSize, e.fileId, e.isMine);
        }
    }
    buffer.clear();
}

void MainWindow::saveAllBuffers()
{
    for (auto it = m_msgBuffers.begin(); it != m_msgBuffers.end(); ++it) {
        if (it.value().isEmpty()) continue;
        QList<HistoryMessage> messages;
        for (const MsgBufEntry &e : it.value()) {
            HistoryMessage hm;
            hm.type = e.type;
            hm.sender = e.sender;
            hm.text = e.text;
            hm.timestamp = e.timestamp;
            hm.isMine = e.isMine;
            hm.fileName = e.fileName;
            hm.fileSize = e.fileSize;
            messages.append(hm);
        }
        m_history->saveHistory(m_username, it.key(), messages);
    }
    m_msgBuffers.clear();
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    saveCurrentHistory();
    saveAllBuffers();
    event->accept();
}
