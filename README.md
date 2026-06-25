# MedChat v0.7.1

基于 Qt 5.12 的 C++ 远程问诊聊天系统，C/S 架构，同一可执行文件通过 `--server` 参数切换服务端模式。

## 功能特性

### 已实现
- 文字消息收发 + SQLite 增量持久化
- 文件传输全链路（offer → accept/reject → 分片 → MD5 校验 → 归档）
- 文件卡片 5 种状态 UI（Pending / Transferring / Completed / Rejected / Error）
- 文件传输助手（本地回环）
- 登录 / 注册分离页面，昵称支持
- 服务端 SQLite 用户数据库
- 联系人在线状态排序和搜索
- 好友关系基础设施（friends 表 + areFriends 查询 + 陌生人拦截）
- 离线消息队列（发 → 标记 → ACK → 删 + 客户端去重）
- 消息 sender 统一（自己 → "me"，对方 → nickname）
- 密保问题（注册时设置，忘记密码时通过 3 步向导找回）
- 密码重置 / 修改（修改后服务端强制踢出客户端）
- 个人资料 UI 重构（仿微信/QQ 风格，圆形头像、字符计数、分组布局）
- 头像上传 + EXIF Orientation 自动校正（手机竖拍照片方向正确）
- 聊天侧边栏图标初始高亮（绿色 #07C160）

### 待实现
- 好友请求 UI（加好友 / 接受 / 拒绝）
- 联系人服务端搜索
- 断点续传

## 技术栈

- Qt 5.12.12 / MinGW 7.3 64-bit / C++14
- qmake + mingw32-make
- JSON over TCP（4 字节大端长度前缀）
- SQLite 双库（meta.db + messages.db）

## 构建

```bash
export PATH="/c/Qt/Qt5.12.12/5.12.12/mingw73_64/bin:/c/Qt/Qt5.12.12/Tools/mingw730_64/bin:$PATH"
cd source/build
mingw32-make -j4
```

生成 `source/build/release/MedChat.exe`。

## 运行

**客户端：**
```bash
./MedChat.exe --server 127.0.0.1 --port 12345
```

**服务端：**
```bash
./MedChat.exe --server
```

## 项目结构

```
source/
├── main.cpp                  # 入口，双角色分发
├── protocol.h/.cpp           # 协议定义 + JSON 序列化
├── server/                   # 服务端：ChatServer / ClientHandler / UserManager
├── client/
│   ├── chatclient.h/.cpp     # 客户端网络层
│   └── localdb.h/.cpp        # 本地 SQLite
└── ui/
    ├── loginwindow.h/.cpp    # 登录 / 注册
    ├── mainwindow.h/.cpp     # 主窗口
    ├── leftsidebar.h/.cpp    # 左侧导航栏
    ├── sessionlistwidget.h/.cpp  # 会话列表
    ├── contactlistwidget.h/.cpp  # 联系人列表
    ├── chatwidget.h/.cpp     # 聊天窗口
    ├── messagebubble.h/.cpp  # 消息气泡
    ├── avatarcropper.h/.cpp  # 头像裁剪 + 圆形处理
    ├── profiledialog.h/.cpp  # 个人资料页
    ├── forgotpassworddialog.h/.cpp  # 忘记密码向导
    └── changepassworddialog.h/.cpp  # 密码修改
```

## 数据库

- `meta.db`：用户表（users）、好友表（friends）、好友请求表（friend_requests）
- `messages.db`：离线消息表（offline_messages）

users 表字段：`id, username, password_hash, sec_question, sec_answer_hash, nickname, signature, gender, birthday, region, avatar`

## GitHub

https://github.com/OneStepBeyond666/MedChat.git
