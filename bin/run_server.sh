#!/bin/bash

# 1. 解除文件描述符限制 (允許 10 萬連接)
ulimit -n 102400

# 2. 解除進程/線程數限制 (允許 10 萬線程)
ulimit -u 102400

# 3. 【關鍵】將線程棧縮小為 1MB (防止內存撐爆)
# 如果不加這一行，4GB 內存只能開 500 個線程！

echo "當前限制配置："
ulimit -a | grep -E "open files|max user processes|stack size"

echo "---------------------------------"
echo "正在啟動服务器."
echo "---------------------------------"
sysctl -w net.ipv4.ip_local_port_range="1024 65535"
sysctl -w net.ipv4.tcp_tw_reuse=1
# 啟動你的客戶端 (請確保路徑正確)



