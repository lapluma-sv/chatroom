# 编译器和选项
CC = gcc
CFLAGS = -Wall -g -O0

# 目录
SRC_DIR = src
BIN_DIR = bin
OBJ_DIR = obj

# 源文件和目标文件
SERVER_SRC = $(SRC_DIR)/server.c
CLIENT_SRC = $(SRC_DIR)/client.c
SERVER_BIN = $(BIN_DIR)/server
CLIENT_BIN = $(BIN_DIR)/client

# 默认目标
.PHONY: all clean dirs

all: dirs $(SERVER_BIN) $(CLIENT_BIN)

# 创建目录
dirs:
	@mkdir -p $(BIN_DIR) $(OBJ_DIR)

# 编译 server
$(SERVER_BIN): $(SERVER_SRC)
	$(CC) $(CFLAGS) -o $@ $<

# 编译 client
$(CLIENT_BIN): $(CLIENT_SRC)
	$(CC) $(CFLAGS) -o $@ $<

# 清理
clean:
	rm -f $(BIN_DIR)/* $(OBJ_DIR)/*
