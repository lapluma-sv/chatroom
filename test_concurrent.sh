#!/bin/bash
# 100 个客户端并发测试 - 清晰版
# 阶段1: 100个客户端同时连接，等待
# 阶段2: 同时发送消息
# 阶段3: 同时断开

SERVER="127.0.0.1"
PORT=8888
CLIENT="./bin/client"
COUNT=100
MSG_COUNT=10
MSG_INTERVAL=1
LOG_FILE="server.log"
SYNC_FILE="/tmp/test_sync_$$"

echo "=== 100 个客户端并发测试 ==="
echo ""

# 清理旧进程和文件
pkill -f "./bin/server" 2>/dev/null
rm -f "$SYNC_FILE"
sleep 1

# 启动服务器
> "$LOG_FILE"
./bin/server > "$LOG_FILE" 2>&1 &
SERVER_PID=$!
sleep 1

if ! kill -0 $SERVER_PID 2>/dev/null; then
    echo "服务器启动失败！"
    cat "$LOG_FILE"
    exit 1
fi

echo "[阶段1] 启动 $COUNT 个客户端并连接..."
START=$(date +%s)

# 启动客户端，但先等待同步信号
for i in $(seq 1 $COUNT); do
    (
        # 等待同步信号（检测文件创建）
        while [ ! -f "$SYNC_FILE" ]; do
            sleep 0.01
        done
        
        # 收到信号，开始发送消息
        for j in $(seq 1 $MSG_COUNT); do
            echo "msg $j from client $i"
            sleep $MSG_INTERVAL
        done
    ) | $CLIENT $SERVER $PORT &
done

# 等待所有客户端连接上
sleep 3

# 检查当前在线人数
CONNECTED=$(grep '当前客户端数' "$LOG_FILE" | grep -oP '当前客户端数 = \K\d+' | tail -1)
echo "[阶段1] 完成，当前在线: ${CONNECTED:-0} 人"
echo ""

echo "[阶段2] 发送同步信号，所有客户端同时开始发送消息..."
SYNC_TIME=$(date +%s)
touch "$SYNC_FILE"

# 等待所有客户端完成
wait

END=$(date +%s)
ELAPSED=$((END - START))

# 等待服务器处理完最后的断开
sleep 2

# 停止服务器（发送 SIGINT，让服务器优雅关闭）
kill -INT $SERVER_PID 2>/dev/null
wait $SERVER_PID 2>/dev/null

echo "[阶段2] 完成，耗时: ${ELAPSED}秒"
echo ""

# 清理
rm -f "$SYNC_FILE"

# 统计
echo "=== 统计信息 ==="
TOTAL_MSG=$((COUNT * MSG_COUNT))
RECV_MSG=$(grep -c 'recv \[' "$LOG_FILE" 2>/dev/null || echo 0)
MAX_ONLINE=$(grep '当前客户端数' "$LOG_FILE" | grep -oP '当前客户端数 = \K\d+' | sort -n | tail -1)

echo "总连接数: $COUNT"
echo "总发送消息数: $TOTAL_MSG"
echo "服务器接收消息数: $RECV_MSG"
echo "服务器最大在线人数: ${MAX_ONLINE:-0}"
echo ""

# 显示服务器日志关键部分
echo "=== 服务器日志 - 连接阶段 ==="
grep '新连接加入' "$LOG_FILE" | head -10
echo "... (共 $(grep -c '新连接加入' "$LOG_FILE") 个连接)"
echo ""

echo "=== 服务器日志 - 消息接收（前20条）==="
grep 'recv \[' "$LOG_FILE" | head -20
echo ""

echo "=== 服务器日志 - 消息接收（最后20条）==="
grep 'recv \[' "$LOG_FILE" | tail -20
echo ""

echo "=== 服务器日志 - 断开阶段 ==="
grep '客户端退出\|客户端断开' "$LOG_FILE" | head -10
echo "... (共 $(grep -c '客户端退出\|客户端断开' "$LOG_FILE") 个断开)"
