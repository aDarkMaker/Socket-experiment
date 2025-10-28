# Web 服务器编译文件

CXX = g++
CXXFLAGS = -std=c++11 -Wall -Wextra -O2
TARGET = webserver
SOURCES = webserver.cpp
OBJECTS = $(SOURCES:.cpp=.o)

# 目标
$(TARGET): $(OBJECTS)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(OBJECTS)
	@echo "编译成功！"
	@echo "使用 ./$(TARGET) 运行服务器"

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

# 清理
clean:
	rm -f $(OBJECTS) $(TARGET)
	@echo "清理完成"

# 运行
run: $(TARGET)
	./$(TARGET)

.PHONY: clean run

