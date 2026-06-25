# MedChat v0.8.6

基于 Qt 5.12 的 C++ 远程问诊聊天系统，C/S 架构，同一可执行文件通过 `--server` 参数切换服务端模式。

## 功能特性

### 基础功能
- 用户注册/登录，支持医生与患者角色，密码加盐 SHA-256 存储
- 实时文字消息收发，自定义二进制协议（4字节大端长度前缀 + JSON Body）
- 文件传输全链路：offer → accept/reject → 64KB分片 → MD5校验 → 本地归档（100MB上限）
- 文件卡片 5 种状态 UI（Pending / Transferring / Completed / Rejected / Error）
- 文件传输助手（本地回环，文本和文件均走 SQLite 存储）
- 好友关系系统：好友请求发送/接受/拒绝，陌生人消息拦截，好友申请冲突检测
- 附近的人：基于在线用户数据源隔离
- 个人资料管理：昵称/签名/性别/生日/地区，头像上传（圆形裁剪 + EXIF方向自动校正）
- 离线消息队列（发送 → 标记 → ACK → 删除 + 客户端去重）
- 历史消息持久化：客户端双库架构（meta.db + messages.db）
- 拼音索引通讯录：基于 GBK 编码二分查找的中文拼音首字母分组
- 会话列表：最近会话排序、未读计数、消息预览
- 密码管理：修改密码 + 忘记密码（密保问题找回）
- 自定义应用图标：EXE 嵌入多尺寸 ICO 图标

### v0.8.6 新增
- **消息右键菜单**：文本消息支持复制、放大阅读、转发、撤回（2分钟内）、删除
- **文件卡片右键菜单**：打开、转发、复制文件到剪贴板、复制文件名、删除
- **消息转发**：纯客户端逻辑，ForwardDialog 支持搜索过滤 + 多选联系人，文本和文件均可转发
- **消息撤回**：2分钟时限，双方实时同步显示「消息已撤回」
- **文件传输状态修复**：发送方在对方接受前正确显示「传输中」，重载不再误显示「传输中断」

### 待实现
- 断点续传
- 群聊
- 联系人服务端搜索

## 技术栈

- Qt 5.12.12 / MinGW 7.3 64-bit / C++14
- qmake + mingw32-make
- JSON over TCP（4字节大端长度前缀）
- SQLite 双库（客户端 meta.db + messages.db，服务端 server.db）

## 构建

```bash
# Git Bash 环境
export PATH="/c/Qt/Qt5.12.12/5.12.12/mingw73_64/bin:/c/Qt/Qt5.12.12/Tools/mingw730_64/bin:$PATH"
cd source/build
qmake ../MedChat.pro -spec win32-g++
mingw32-make -j4
```

生成 `source/build/release/MedChat.exe`。

发行版打包：`windeployqt MedChat.exe --no-translations --no-opengl-sw`

## 运行

**服务端：**
```bash
MedChat.exe --server
```
可选参数 `-p 端口号`，默认 9527。

**客户端：**
```bash
MedChat.exe
```
默认连接 `127.0.0.1:9527`，可在登录界面切换服务器地址。

## 项目结构

```
source/
├── main.cpp                     # 入口，双角色分发
├── MedChat.pro                 # Qt 工程文件
├── common/
│   ├── protocol.h/.cpp          # 协议编解码 + 消息类型定义
│   └── constants.h              # 全局常量（端口、文件大小限制等）
├── server/
│   ├── chatserver.h/.cpp        # TCP 服务端主逻辑、消息路由
│   ├── clienthandler.h/.cpp     # 单客户端连接管理、帧解码
│   ├── serverdb.h/.cpp          # 服务端 SQLite
│   └── usermanager.h/.cpp       # 用户注册/认证/资料管理
├── client/
│   ├── chatclient.h/.cpp        # 客户端网络层、文件传输引擎
│   ├── mainwindow.h/.cpp        # 主窗口、信号槽总线
│   ├── loginwindow.h/.cpp       # 登录/注册窗口
│   └── localdb.h/.cpp           # 客户端本地 SQLite 双库
├── ui/
│   ├── chatwidget.h/.cpp        # 聊天窗口（含文件预览+拖拽）
│   ├── messagebubble.h/.cpp     # 消息气泡 + 文件卡片 + 右键菜单
│   ├── forwarddialog.h/.cpp     # 转发联系人选择对话框
│   ├── leftsidebar.h/.cpp       # 左侧栏（IconBar + ContentPanel）
│   ├── sessionlistwidget.h/.cpp # 会话列表
│   ├── contactlistwidget.h/.cpp # 通讯录（拼音分组）
│   ├── addfrienddialog.h/.cpp   # 添加好友
│   ├── avatarcropper.h/.cpp     # 头像裁剪 + EXIF 处理
│   ├── profiledialog.h/.cpp     # 个人资料
│   ├── friendrequestwidget.h/.cpp      # 好友请求面板
│   ├── friendrequestnotification.h/.cpp # 好友请求通知弹窗
│   ├── nearbypeoplewidget.h/.cpp # 附近的人
│   ├── changepassworddialog.h/.cpp # 修改密码
│   └── forgotpassworddialog.h/.cpp # 忘记密码
└── resources/
    ├── app_icon.png             # 应用图标源文件
    ├── app_icon.ico             # 多尺寸 ICO
    ├── appicon.rc               # Windows 资源文件
    └── style.qrc                # Qt 资源文件
```

## 数据库

**客户端双库：**

`meta.db`：sessions 表（会话列表）、file_index 表（文件索引）、friend_requests 表（好友请求缓存）

`messages.db`：messages 表（历史消息，含 recalled 字段标记撤回状态）

**服务端：**

`server.db`：users 表、friends 表（双向记录）、offline_messages 表、friend_requests 表。WAL 模式。

## GitHub

https://github.com/OneStepBeyond666/MedChat.git
