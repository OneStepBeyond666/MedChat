#!/bin/bash
# MedChat Server 一键部署脚本 (Ubuntu/Debian)
# 用法: sudo bash deploy.sh

set -e

echo "========================================"
echo "  MedChat Server 部署脚本"
echo "========================================"

# 1. 安装依赖
echo ""
echo "[1/3] 安装 Qt5 开发环境..."
apt-get update -qq
apt-get install -y -qq qt5-default qtbase5-dev g++ make

# 2. 编译
echo ""
echo "[2/3] 编译服务器..."
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$SCRIPT_DIR"
qmake MedChatServer.pro
make -j$(nproc)

# 3. 完成
echo ""
echo "[3/3] 部署完成!"
echo ""
echo "启动服务器:"
echo "  ./medchat-server"
echo ""
echo "指定端口:"
echo "  ./medchat-server -p 端口号"
echo ""
echo "后台运行 (推荐):"
echo "  nohup ./medchat-server > server.log 2>&1 &"
echo ""
echo "查看日志:"
echo "  tail -f server.log"
echo ""
echo "========================================"
