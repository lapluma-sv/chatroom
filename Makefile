# 编译器和选项
CC = gcc
CFLAGS = -Wall -g -O0

# 目录
SRC_DIR = src
BIN_DIR = bin
OBJ_DIR = obj

# 源文件和目标文件
PROTOCOL_SRC = $(SRC_DIR)/protocol.c
SERVER_SRC = $(SRC_DIR)/server.c
CLIENT_SRC = $(SRC_DIR)/client.c
SERVER_BIN = $(BIN_DIR)/server
CLIENT_BIN = $(BIN_DIR)/client
TEST_SRC = $(SRC_DIR)/test_protocol.c
TEST_BIN = $(BIN_DIR)/test_protocol

# 默认目标
.PHONY: all clean dirs test

all: dirs $(SERVER_BIN) $(CLIENT_BIN)

# 创建目录
dirs:
	@mkdir -p $(BIN_DIR) $(OBJ_DIR)

# 编译 server
$(SERVER_BIN): $(SERVER_SRC) $(PROTOCOL_SRC)
	$(CC) $(CFLAGS) -Iinclude -o $@ $(SERVER_SRC) $(PROTOCOL_SRC)

# 编译 client
$(CLIENT_BIN): $(CLIENT_SRC) $(PROTOCOL_SRC)
	$(CC) $(CFLAGS) -Iinclude -o $@ $(CLIENT_SRC) $(PROTOCOL_SRC)

# 编译 test
$(TEST_BIN): $(TEST_SRC) $(PROTOCOL_SRC)
	$(CC) $(CFLAGS) -Iinclude -o $@ $(TEST_SRC) $(PROTOCOL_SRC)

# 运行测试（需要先启动 server）
test: $(TEST_BIN)
	$(TEST_BIN)

# 清理
clean:
	rm -f $(BIN_DIR)/* $(OBJ_DIR)/*
