#include "mainwindow.h"
#include "chatclient.h"
#include "localdb.h"
#include "common/protocol.h"
#include "common/constants.h"
#include "ui/leftsidebar.h"
#include "ui/profiledialog.h"
#include "ui/addfrienddialog.h"
#include "ui/chatwidget.h"
#include "ui/friendrequestwidget.h"
#include "ui/friendrequestnotification.h"
#include "ui/nearbypeoplewidget.h"
#include "ui/changepassworddialog.h"
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
#include <QDir>
#include <QFile>
#include <QUuid>
#include <QDebug>

MainWindow::MainWindow(ChatClient *client, const QString &username, const QString &role,
                       const QString &nickname, const QByteArray &avatarData, QWidget *parent)
    : QMainWindow(parent), m_client(client), m_username(username), m_role(role),
      m_nickname(nickname), m_avatarData(avatarData)
{
    setWindowTitle(QString("远程问诊系统 - %1 (%2)").arg(m_nickname, role == "doctor" ? "医生" : "患者"));
    resize(960, 640);

    setupUI();
    applyStyles();

    // 初始化 LocalDB（SQLite 双库 + 目录结构）
    if (!LocalDB::instance().init(m_username)) {
        QMessageBox::critical(this, "初始化失败", "无法初始化本地数据库，应用可能无法正常工作。");
    }

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
    connect(m_client, &ChatClient::fileSizeExceeded, this, &MainWindow::onFileSizeExceeded);
    connect(m_client, &ChatClient::fileSendInitiated, this, &MainWindow::onFileSendInitiated);
    connect(m_client, &ChatClient::fileSendCompleted, this, &MainWindow::onFileSendCompleted);
    connect(m_client, &ChatClient::serverError, this, &MainWindow::onServerError);
    connect(m_client, &ChatClient::strangerError, this, &MainWindow::onStrangerError);
    connect(m_client, &ChatClient::offlineSyncDone, this, &MainWindow::onOfflineSyncDone);
    connect(m_client, &ChatClient::friendRequestReceived, this, &MainWindow::onFriendRequestReceived);
    connect(m_client, &ChatClient::friendRequestCountChanged, this, &MainWindow::onFriendRequestCountChanged);
    connect(m_client, &ChatClient::friendRequestConflict, this, &MainWindow::onFriendRequestConflict);
    connect(m_client, &ChatClient::onlineUsersUpdated, this, &MainWindow::onOnlineUsersUpdated);
    connect(m_client, &ChatClient::friendResponseReceived, this, [this](bool success, const QString &username, const QString &message) {
        if (success) {
            statusBar()->showMessage(message, 5000);
            // 刷新联系人列表和会话列表
            m_sidebar->setContacts(m_client->contacts());
            loadSessionsList();
        } else {
            statusBar()->showMessage("添加好友失败: " + message, 5000);
        }
    });
    connect(m_client, &ChatClient::disconnected, this, &MainWindow::onDisconnected);

    connect(m_client, &ChatClient::contactProfileChanged, this, &MainWindow::onContactProfileChanged);

    connect(m_sidebar, &LeftSidebar::contactSelected, this, &MainWindow::onContactSelected);
    connect(m_sidebar, &LeftSidebar::sessionSelected, this, &MainWindow::onSessionSelected);
    connect(m_sidebar, &LeftSidebar::avatarClicked, this, &MainWindow::onAvatarClicked);
    connect(m_sidebar, &LeftSidebar::addFriendRequested, this, &MainWindow::onAddFriendRequested);
    connect(m_sidebar, &LeftSidebar::friendRequestEntryClicked, this, &MainWindow::onFriendRequestEntryClicked);
    connect(m_sidebar, &LeftSidebar::nearbyPeopleEntryClicked, this, &MainWindow::onNearbyPeopleEntryClicked);

    connect(m_friendRequestWidget, &FriendRequestWidget::acceptRequested, this, &MainWindow::onAcceptFriendRequest);
    connect(m_friendRequestWidget, &FriendRequestWidget::rejectRequested, this, &MainWindow::onRejectFriendRequest);
    connect(m_nearbyPeopleWidget, &NearbyPeopleWidget::addFriendRequested, this, &MainWindow::onNearbyAddFriendRequested);

    // 初始化侧栏资料
    m_sidebar->setSelfProfile(m_nickname, m_avatarData);

    // 加载会话列表
    loadSessionsList();

    // 加载待处理好友请求
    loadFriendRequests();

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

    // 左侧：LeftSidebar（双Tab视图）
    m_sidebar = new LeftSidebar;
    m_sidebar->setFixedWidth(280);
    splitter->addWidget(m_sidebar);

    // 右侧：QStackedWidget 切换占位页 / 聊天页
    m_chatStack = new QStackedWidget;

    // Page 0: 占位页（未选择联系人时显示）
    QWidget *placeholder = new QWidget;
    placeholder->setObjectName("chatPlaceholder");
    m_chatStack->addWidget(placeholder);

    // Page 1: 聊天页
    m_chatWidget = new ChatWidget;
    m_chatStack->addWidget(m_chatWidget);

    // Page 2: 好友请求页
    m_friendRequestWidget = new FriendRequestWidget;
    m_chatStack->addWidget(m_friendRequestWidget);

    // Page 3: 附近的人页
    m_nearbyPeopleWidget = new NearbyPeopleWidget;
    m_chatStack->addWidget(m_nearbyPeopleWidget);

    m_chatStack->setCurrentIndex(0);  // 初始显示占位页

    splitter->addWidget(m_chatStack);

    splitter->setStretchFactor(0, 0);
    splitter->setStretchFactor(1, 1);

    mainLayout->addWidget(splitter);
}

void MainWindow::applyStyles()
{
    setStyleSheet(
        "QSplitter::handle { background: #ddd; }"
        "QStatusBar { background: #f5f5f5; font-size: 12px; color: #666; }"
        "#chatPlaceholder { background: #fafafa; }"
    );
}

QString MainWindow::displayName(const QString &username) const
{
    if (username == m_username)
        return m_nickname;
    const ContactInfo ci = m_client->contacts().value(username);
    return ci.nickname.isEmpty() ? username : ci.nickname;
}

// ============================================================
// 联系人切换 —— 从 SQLite 加载历史消息
// ============================================================

void MainWindow::onContactSelected(const QString &username)
{
    // 再次点击同一联系人 → 取消选中，回到占位页
    if (m_chatWidget->currentPartner() == username) {
        m_chatWidget->clearChat();
        m_chatStack->setCurrentIndex(0);
        m_sidebar->clearSelection();
        return;
    }

    // 清空聊天区域
    m_chatWidget->clearChat();

    // 切换到聊天页
    m_chatStack->setCurrentIndex(1);

    if (username == QString::fromUtf8(MsgType::FileHelper)) {
        m_chatWidget->setChatPartner(username, "helper", QString::fromUtf8(MsgType::FileHelper));
        loadMessagesFromDB(username, "me");  // 文件传输助手双侧都显示 "me"
        return;
    }

    QMap<QString, ContactInfo> contacts = m_client->contacts();
    QString role = contacts.value(username).role;
    QString nick = contacts.value(username).nickname;
    if (nick.isEmpty()) nick = username;
    m_chatWidget->setChatPartner(username, role, nick);

    // 从 SQLite 加载历史消息（对方显示 nickname）
    loadMessagesFromDB(username, nick);

    // 清除未读并刷新会话列表
    LocalDB::instance().clearUnread(username);
    loadSessionsList();
}

void MainWindow::loadMessagesFromDB(const QString &contactUid, const QString &partnerNick)
{
    QVector<StoredMessage> messages = LocalDB::instance().loadMessages(contactUid);
    if (!messages.isEmpty())
        m_chatWidget->loadHistoryMessages(messages, partnerNick,
            [this](const QString &fid) {
                return m_client->isTransferActive(fid)
                    || m_userAcceptedFiles.contains(fid);
            });
}

// ============================================================
// 会话列表
// ============================================================

void MainWindow::loadSessionsList()
{
    QVector<SessionInfo> sessions = LocalDB::instance().loadSessions();
    QMap<QString, ContactInfo> contacts = m_client->contacts();

    // 合并昵称和头像数据到每个会话
    for (int i = 0; i < sessions.size(); ++i) {
        const QString &uid = sessions[i].contactUid;
        if (uid == QString::fromUtf8(MsgType::FileHelper)) {
            sessions[i].nickname = QString::fromUtf8(MsgType::FileHelper);
            // 文件传输助手使用默认头像
        } else if (contacts.contains(uid)) {
            const ContactInfo &ci = contacts.value(uid);
            sessions[i].nickname = ci.nickname.isEmpty() ? uid : ci.nickname;
            sessions[i].avatarData = ci.avatarData;
        } else {
            sessions[i].nickname = uid;
        }
    }

    m_sidebar->setSessions(sessions);
}

// ============================================================
// 从聊天 Tab 会话列表点击联系人
// ============================================================

void MainWindow::onSessionSelected(const QString &username)
{
    // 复用 onContactSelected 的逻辑
    onContactSelected(username);
}

// ============================================================
// 头像点击 → 打开个人资料编辑
// ============================================================

void MainWindow::onAvatarClicked()
{
    ProfileDialog *dlg = new ProfileDialog(
        ProfileDialog::SelfProfile, m_username, m_nickname, m_role, m_avatarData, this);

    connect(dlg, &ProfileDialog::profileSaved,
        this, [this, dlg](const QString &nickname, const QByteArray &avatarData) {
            // 更新本地状态
            m_nickname = nickname;
            m_avatarData = avatarData;

            // 发送到服务器
            m_client->sendProfileUpdate(nickname, avatarData);

            // 更新侧栏显示
            m_sidebar->setSelfProfile(nickname, avatarData);

            // 更新窗口标题
            setWindowTitle(QString("远程问诊系统 - %1 (%2)")
                .arg(m_nickname, m_role == "doctor" ? "医生" : "患者"));

            dlg->accept();
        });

    connect(dlg, &ProfileDialog::changePasswordRequested,
        this, &MainWindow::onChangePasswordRequested);

    dlg->exec();
    dlg->deleteLater();
}

// ============================================================
// 修改密码
// ============================================================

void MainWindow::onChangePasswordRequested()
{
    ChangePasswordDialog *dlg = new ChangePasswordDialog(m_client, this);
    dlg->exec();
    dlg->deleteLater();
}

// ============================================================
// 联系人资料变更（服务端推送）
// ============================================================

void MainWindow::onContactProfileChanged(const QString &username, const QString &nickname,
                                          const QByteArray &avatarData)
{
    // 如果是自己的资料变更
    if (username == m_username) {
        m_nickname = nickname;
        m_avatarData = avatarData;
        m_sidebar->setSelfProfile(nickname, avatarData);
        setWindowTitle(QString("远程问诊系统 - %1 (%2)")
            .arg(m_nickname, m_role == "doctor" ? "医生" : "患者"));
    }

    // 刷新会话列表（昵称/头像可能已变）
    loadSessionsList();
}

// ============================================================
// 添加好友
// ============================================================

void MainWindow::onAddFriendRequested()
{
    AddFriendDialog *dlg = new AddFriendDialog(this);
    connect(dlg, &AddFriendDialog::friendRequestSent, this, [this](const QString &username) {
        if (username == m_username) {
            statusBar()->showMessage("不能添加自己为好友", 3000);
            return;
        }
        m_client->sendFriendRequest(username);
        statusBar()->showMessage("已向 " + username + " 发送好友请求", 3000);
    });
    dlg->exec();
    dlg->deleteLater();
}

// ============================================================
// 联系人列表更新
// ============================================================

void MainWindow::onContactListUpdated(const QMap<QString, ContactInfo> &contacts)
{
    m_sidebar->setContacts(contacts);
    loadSessionsList();  // 联系人列表到达后刷新会话昵称

    // 好友列表变化后，重算附近的人计数（附近的人 = 在线用户 - 好友 - 自己）
    // 因为 handleContactList 可能在 handleOnlineStatus 之后到达，
    // 此时 m_onlineUsers 不变但 m_contacts 变了，stranger 集合随之改变。
    QMap<QString, ContactInfo> onlineUsers = m_client->onlineUsers();
    QMap<QString, ContactInfo> strangers;
    for (auto it = onlineUsers.constBegin(); it != onlineUsers.constEnd(); ++it) {
        if (it.key() == m_username) continue;
        if (contacts.contains(it.key())) continue;
        strangers[it.key()] = it.value();
    }
    m_sidebar->setNearbyPeopleCount(strangers.size());
}

// ============================================================
// 文本消息收发
// ============================================================

void MainWindow::onTextMessageReceived(const QString &from, const QString &to,
                                        const QString &text, qint64 timestamp)
{
    QString partner = (to == m_username) ? from : to;

    // 持久化到 SQLite
    persistTextMessage(partner, text, timestamp, false);

    // 更新会话预览和时间（不改变未读数）
    SessionInfo si;
    si.contactUid = partner;
    si.username = partner;
    si.lastMsgPreview = text.left(50);
    si.lastTime = timestamp;
    si.unreadCount = 0;  // upsert 不覆盖未读
    LocalDB::instance().upsertSession(si);

    // 非当前查看的联系人 → 递增未读数
    if (m_chatWidget->currentPartner() != partner) {
        LocalDB::instance().incrementUnread(partner);
    }

    // 刷新会话列表
    loadSessionsList();

    // 仅当前对话联系人才实时显示
    if (m_chatWidget->currentPartner() == partner) {
        m_chatWidget->addTextMessage(displayName(from), text, timestamp, false);
    }
}

void MainWindow::onMessageAck(const QString &to, qint64 timestamp)
{
    Q_UNUSED(to)
    Q_UNUSED(timestamp)
}

void MainWindow::onSendTextMessage(const QString &to, const QString &text)
{
    qint64 now = QDateTime::currentMSecsSinceEpoch();

    // 文件传输助手：本地回环，不经过服务端
    if (to == QString::fromUtf8(MsgType::FileHelper)) {
        QString helperUid = QString::fromUtf8(MsgType::FileHelper);
        m_chatWidget->addTextMessage("me", text, now, true);
        m_chatWidget->addTextMessage("me", text, now, false);
        persistTextMessage(helperUid, text, now, true);
        persistTextMessage(helperUid, text, now, false);
        updateSession(helperUid, text.left(50), now);
        return;
    }

    m_client->sendTextMessage(to, text);

    // 持久化到 SQLite
    persistTextMessage(to, text, now, true);
    updateSession(to, text.left(50), now);

    // 立即在UI显示自己发送的消息（统一显示"me"）
    m_chatWidget->addTextMessage("me", text, now, true);
}

// ============================================================
// 文件发送
// ============================================================

void MainWindow::onSendFileRequest(const QString &to)
{
    QString filePath = QFileDialog::getOpenFileName(this, "选择要发送的文件",
        QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation));
    if (filePath.isEmpty()) return;

    QFileInfo fi(filePath);

    // 100MB 前置校验
    if (fi.size() > Constants::MAX_FILE_SIZE) {
        QMessageBox::warning(this, "文件过大",
            QString("文件 \"%1\" 超过100MB传输限制，无法发送。").arg(fi.fileName()));
        return;
    }

    // 文件传输助手：本地复制文件，不走网络，自动保存到 medchat_file 目录
    if (to == QString::fromUtf8(MsgType::FileHelper)) {
        qint64 now = QDateTime::currentMSecsSinceEpoch();
        QString helperUid = QString::fromUtf8(MsgType::FileHelper);

        // 与普通联系人一致：自动保存到 medchat_file/{uid}/file/{yyyy}/{MM}/
        QString ym = QDate::currentDate().toString("yyyy/MM");
        QString targetDir = LocalDB::instance().rootPath()
                            + "/" + Constants::FILE_SUBDIR + "/" + ym;
        QDir().mkpath(targetDir);
        QString savePath = LocalDB::instance().generateUniqueFilePath(targetDir, fi.fileName());

        QFile::copy(filePath, savePath);
        QFileInfo saveFi(savePath);

        // 生成唯一 fileId 并写入 file_index
        QString sendId = QUuid::createUuid().toString(QUuid::WithoutBraces);
        QString recvId = QUuid::createUuid().toString(QUuid::WithoutBraces);

        FileRecord sendRec;
        sendRec.fileId = sendId;
        sendRec.originalName = fi.fileName();
        sendRec.saveName = saveFi.fileName();
        sendRec.savePath = savePath;
        sendRec.size = fi.size();
        sendRec.status = 1;
        sendRec.yearMonth = ym;
        LocalDB::instance().insertFileRecord(sendRec);

        FileRecord recvRec = sendRec;
        recvRec.fileId = recvId;
        LocalDB::instance().insertFileRecord(recvRec);

        // 显示发送卡片 + 持久化
        m_chatWidget->addFileMessage("me", fi.fileName(), fi.size(), sendId, true);
        m_chatWidget->setFileCompleted(sendId);
        persistFileMessage(helperUid, fi.fileName(), sendId, now, true);

        // 显示接收卡片 + 持久化
        m_chatWidget->addFileMessage("me", fi.fileName(), fi.size(), recvId, false);
        m_chatWidget->setFileCompleted(recvId);
        persistFileMessage(helperUid, fi.fileName(), recvId, now, false);

        updateSession(helperUid, "[文件] " + fi.fileName(), now);
        statusBar()->showMessage("文件已保存到: " + savePath, 5000);
        return;
    }

    // 记录待发送目标和文件路径，等待 fileSendInitiated 信号获取真实 fileId
    m_pendingSendFileTo = to;
    m_pendingSendFilePath = filePath;
    m_client->sendFile(to, filePath);
}

void MainWindow::onFileSendInitiated(const QString &to, const QString &fileName,
                                      qint64 fileSize, const QString &fileId)
{
    Q_UNUSED(to)
    // 用真实 fileId 创建文件卡片（统一显示"me"）
    m_chatWidget->addFileMessage("me", fileName, fileSize, fileId, true);

    qint64 now = QDateTime::currentMSecsSinceEpoch();

    // 模仿微信：发送方也在本地保存一份文件副本
    QString savedPath;
    if (!m_pendingSendFilePath.isEmpty() && QFile::exists(m_pendingSendFilePath)) {
        QDate today = QDate::currentDate();
        QString ym = QString("%1/%2").arg(today.year()).arg(today.month(), 2, 10, QChar('0'));
        QString targetDir = LocalDB::instance().rootPath()
                            + "/" + Constants::FILE_SUBDIR + "/" + ym;
        QDir().mkpath(targetDir);

        QString destPath = LocalDB::instance().generateUniqueFilePath(targetDir, fileName);
        if (QFile::copy(m_pendingSendFilePath, destPath)) {
            savedPath = destPath;
        } else {
            qWarning() << "Failed to copy sent file to local archive:" << destPath;
        }
    }

    // 写入 file_index 记录（发送方，status=1 已完成）
    FileRecord rec;
    rec.fileId = fileId;
    rec.originalName = fileName;
    rec.size = fileSize;
    rec.status = savedPath.isEmpty() ? 2 : 1; // 复制失败则标记失败
    rec.savePath = savedPath;
    QFileInfo fi(savedPath);
    rec.saveName = fi.fileName();
    rec.yearMonth = QDate::currentDate().toString("yyyy/MM");
    LocalDB::instance().insertFileRecord(rec);

    // 持久化文件消息到 SQLite
    persistFileMessage(m_pendingSendFileTo, fileName, fileId, now, true);
    updateSession(m_pendingSendFileTo, "[文件] " + fileName, now);
    m_pendingSendFileTo.clear();
    m_pendingSendFilePath.clear();
}

void MainWindow::onFileSendCompleted(const QString &fileId)
{
    // 实时将发送方卡片更新为已完成状态
    m_chatWidget->setFileCompleted(fileId);

    // 确保 file_index 记录为已完成（status=1）
    LocalDB::instance().updateFileRecord(fileId, 1);
}

// ============================================================
// 文件接收
// ============================================================

void MainWindow::onFileOfferReceived(const QString &from, const QString &fileName,
                                      qint64 fileSize, const QString &fileId)
{
    m_pendingFileOffers[fileId] = from;

    // 预写 file_index 记录（status=0 待接收），避免切换对话时 loadHistory 找不到记录
    FileRecord rec;
    rec.fileId = fileId;
    rec.originalName = fileName;
    rec.size = fileSize;
    rec.status = 0; // pending — acceptFile() 后会更新
    LocalDB::instance().insertFileRecord(rec);

    // 持久化文件消息到 SQLite
    qint64 now = QDateTime::currentMSecsSinceEpoch();
    persistFileMessage(from, fileName, fileId, now, false);

    // 更新会话预览和时间
    SessionInfo si;
    si.contactUid = from;
    si.username = from;
    si.lastMsgPreview = "[文件] " + fileName;
    si.lastTime = now;
    si.unreadCount = 0;
    LocalDB::instance().upsertSession(si);

    // 非当前查看的联系人 → 递增未读数
    if (m_chatWidget->currentPartner() != from) {
        LocalDB::instance().incrementUnread(from);
    }

    // 刷新会话列表
    loadSessionsList();

    // 当前联系人 → 显示文件卡片；非当前 → 弹窗通知
    if (m_chatWidget->currentPartner() == from) {
        m_chatWidget->addFileMessage(displayName(from), fileName, fileSize, fileId, false);
    } else {
        showMessage("文件接收", displayName(from) + " 向您发送文件: " + fileName);
    }
}

void MainWindow::onFileAccepted(const QString &fileId)
{
    qDebug() << "File accepted, starting transfer:" << fileId;
}

void MainWindow::onFileRejected(const QString &fileId, const QString &reason)
{
    m_chatWidget->setFileRejected(fileId, reason);
    m_pendingFileOffers.remove(fileId);

    // 发送方被拒绝：更新 file_index 为已拒绝(status=3)并删除本地复制的文件
    FileRecord rec = LocalDB::instance().getFileRecord(fileId);
    if (!rec.savePath.isEmpty() && QFile::exists(rec.savePath)) {
        QFile::remove(rec.savePath);
    }
    LocalDB::instance().updateFileRecord(fileId, 3);
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

void MainWindow::onFileSizeExceeded(const QString &fileId, const QString &fileName, qint64 fileSize)
{
    Q_UNUSED(fileId)
    Q_UNUSED(fileSize)
    QMessageBox::warning(this, "文件过大",
        QString("文件 \"%1\" 超过100MB传输限制，已自动拒绝。").arg(fileName));
}

void MainWindow::onFileAcceptFromUI(const QString &fileId)
{
    // 不再弹出 QFileDialog，自动保存到 medchat_file/{uid}/file/ 目录
    m_userAcceptedFiles.insert(fileId);
    m_client->acceptFile(fileId);
}

void MainWindow::onFileRejectFromUI(const QString &fileId)
{
    m_client->rejectFile(fileId);
    m_pendingFileOffers.remove(fileId);
    LocalDB::instance().updateFileRecord(fileId, 3); // 标记为已拒绝

    // 实时更新接收方卡片为已拒绝状态（fileRejected 信号仅在发送方触发）
    m_chatWidget->setFileRejected(fileId, "用户拒绝");
}

// ============================================================
// 持久化辅助方法
// ============================================================

void MainWindow::persistTextMessage(const QString &contactUid, const QString &content,
                                     qint64 timestamp, bool isMine)
{
    StoredMessage msg;
    msg.contactUid = contactUid;
    msg.type = 0; // 文本
    msg.content = content;
    msg.timestamp = timestamp;
    msg.isMine = isMine;
    LocalDB::instance().insertMessage(msg);
}

void MainWindow::persistFileMessage(const QString &contactUid, const QString &fileName,
                                     const QString &fileId, qint64 timestamp, bool isMine)
{
    StoredMessage msg;
    msg.contactUid = contactUid;
    msg.type = 1; // 文件
    msg.content = fileName;
    msg.fileId = fileId;
    msg.timestamp = timestamp;
    msg.isMine = isMine;
    LocalDB::instance().insertMessage(msg);
}

void MainWindow::updateSession(const QString &contactUid, const QString &preview, qint64 timestamp)
{
    SessionInfo si;
    si.contactUid = contactUid;
    si.username = contactUid;
    si.lastMsgPreview = preview;
    si.lastTime = timestamp;
    si.unreadCount = 0;
    LocalDB::instance().upsertSession(si);

    // 刷新侧栏会话列表
    loadSessionsList();
}

// ============================================================
// 系统事件
// ============================================================

void MainWindow::onServerError(const QString &error)
{
    statusBar()->showMessage("服务器错误: " + error, 5000);
}

void MainWindow::onStrangerError(const QString &text)
{
    // 陌生人拦截提示：状态栏 + 弹窗
    statusBar()->showMessage(text, 5000);
    QMessageBox::warning(this, "发送失败", text);
}

void MainWindow::onOfflineSyncDone()
{
    statusBar()->showMessage("离线消息同步完成", 3000);
}

void MainWindow::onFriendRequestReceived(int requestId, const QString &from, const QString &nickname,
                                          const QString &text, const QByteArray &avatarData, bool isSynced)
{
    QString display = nickname.isEmpty() ? from : nickname;

    // 记录到待处理映射
    m_pendingFriendRequests[requestId] = from;

    // 非登录同步的实时请求 → 弹出通知
    if (!isSynced) {
        FriendRequestNotification *notif = new FriendRequestNotification(display, avatarData, this);
        connect(notif, &FriendRequestNotification::viewRequested, this, &MainWindow::onFriendRequestEntryClicked);
        notif->show();
    }

    // 刷新好友请求面板（如果当前正在查看）
    loadFriendRequests();
}

void MainWindow::onFriendRequestEntryClicked()
{
    // 再次点击 → 取消选中，回到占位页
    if (m_chatStack->currentIndex() == 2) {
        m_chatStack->setCurrentIndex(0);
        m_sidebar->clearSelection();
        return;
    }
    // 切换到好友请求页面
    m_chatStack->setCurrentIndex(2);
    loadFriendRequests();
}

void MainWindow::onAcceptFriendRequest(int requestId, const QString &fromUsername)
{
    m_client->sendFriendResponse(requestId, fromUsername, true);
    m_friendRequestWidget->removeRequest(requestId);
    m_pendingFriendRequests.remove(requestId);
    statusBar()->showMessage("已接受 " + displayName(fromUsername) + " 的好友请求", 3000);

    // 刷新联系人列表
    m_sidebar->setContacts(m_client->contacts());
    loadSessionsList();
}

void MainWindow::onRejectFriendRequest(int requestId, const QString &fromUsername)
{
    m_client->sendFriendResponse(requestId, fromUsername, false);
    m_friendRequestWidget->removeRequest(requestId);
    m_pendingFriendRequests.remove(requestId);
    statusBar()->showMessage("已拒绝好友请求", 3000);
}

void MainWindow::onFriendRequestCountChanged(int count)
{
    m_sidebar->setFriendRequestCount(count);
}

void MainWindow::onFriendRequestConflict(const QString &target, const QString &direction,
                                          const QString &message, int requestId)
{
    Q_UNUSED(requestId)
    QString display = displayName(target);

    if (direction == "incoming") {
        // 对方已向你发送好友请求 → 引导去好友请求面板处理
        statusBar()->showMessage(message, 5000);
        // 自动跳转到好友请求面板
        m_chatStack->setCurrentIndex(2);
        loadFriendRequests();
    } else {
        // 你已向对方发送过请求 → 提示等待
        statusBar()->showMessage(
            QString("已向 %1 发送过好友请求，请等待对方处理").arg(display), 5000);
    }
}

void MainWindow::loadFriendRequests()
{
    QVector<FriendRequestInfo> requests = LocalDB::instance().loadPendingFriendRequests();

    // 更新映射
    m_pendingFriendRequests.clear();
    for (const FriendRequestInfo &r : requests) {
        m_pendingFriendRequests[r.requestId] = r.fromUsername;
    }

    m_friendRequestWidget->setRequests(requests);
    m_sidebar->setFriendRequestCount(requests.size());
}

void MainWindow::onOnlineUsersUpdated(const QMap<QString, ContactInfo> &onlineUsers)
{
    // 计算差集：在线用户 - 好友 - 自己 = 附近的人（陌生人）
    QMap<QString, ContactInfo> friends = m_client->contacts();
    QMap<QString, ContactInfo> strangers;
    for (auto it = onlineUsers.constBegin(); it != onlineUsers.constEnd(); ++it) {
        if (it.key() == m_username) continue;       // 排除自己
        if (friends.contains(it.key())) continue;   // 排除好友
        strangers[it.key()] = it.value();
    }
    m_sidebar->setNearbyPeopleCount(strangers.size());

    // 如果附近的人面板当前正在显示，刷新内容
    if (m_chatStack->currentIndex() == 3) {
        m_nearbyPeopleWidget->setStrangers(strangers);
    }
}

void MainWindow::onNearbyPeopleEntryClicked()
{
    // toggle：已在附近的人页 → 回到占位页
    if (m_chatStack->currentIndex() == 3) {
        m_chatStack->setCurrentIndex(0);
        m_sidebar->clearSelection();
        return;
    }
    // 计算差集并显示
    QMap<QString, ContactInfo> onlineUsers = m_client->onlineUsers();
    QMap<QString, ContactInfo> friends = m_client->contacts();
    QMap<QString, ContactInfo> strangers;
    for (auto it = onlineUsers.constBegin(); it != onlineUsers.constEnd(); ++it) {
        if (it.key() == m_username) continue;
        if (friends.contains(it.key())) continue;
        strangers[it.key()] = it.value();
    }
    m_nearbyPeopleWidget->setStrangers(strangers);
    m_chatStack->setCurrentIndex(3);
}

void MainWindow::onNearbyAddFriendRequested(const QString &username)
{
    m_client->sendFriendRequest(username);
    statusBar()->showMessage("已向 " + displayName(username) + " 发送好友请求", 3000);
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

void MainWindow::closeEvent(QCloseEvent *event)
{
    // 消息已增量持久化到 SQLite，无需批量保存
    event->accept();
}
