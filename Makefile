# Makefile cho dự án text_analyst

# Compiler và flags
CXX = g++
CC = gcc
CXXFLAGS = -Wall -Wextra -std=c++11 -g
CFLAGS = -Wall -Wextra -g

# Tên file thực thi
TARGET = text_analyst.exe

# Các file nguồn
CXX_SOURCES = text_analyst.cpp
C_SOURCES = compress.c hashtable.c

# Các file object
CXX_OBJECTS = $(CXX_SOURCES:.cpp=.o)
C_OBJECTS = $(C_SOURCES:.c=.o)
OBJECTS = $(CXX_OBJECTS) $(C_OBJECTS)

# Các file header
HEADERS = compress.h hashtable.h

# Rule mặc định
all: $(TARGET)

# Rule để tạo file thực thi
$(TARGET): $(OBJECTS)
	$(CXX) $(OBJECTS) -o $(TARGET)
	@echo "success: $(TARGET)"

# Rule để biên dịch file C++ thành object
%.o: %.cpp $(HEADERS)
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Rule để biên dịch file C thành object
%.o: %.c $(HEADERS)
	$(CC) $(CFLAGS) -c $< -o $@

# Rule để dọn dẹp
clean:
	del /Q $(OBJECTS) $(TARGET) 2>nul || echo "Cleaning completed"
	@echo "success: Cleaned object files and executable"

# Rule để rebuild hoàn toàn
rebuild: clean all

# Rule để chạy chương trình với ví dụ
test: $(TARGET)
	@echo "run test file text.txt:"
	$(TARGET) read text.txt

# Rule để hiển thị trợ giúp
help:
	@echo "Available Make commands:"
	@echo "  make         - Compile the project"
	@echo "  make all     - Compile the project"
	@echo "  make clean   - Remove object files and executable"
	@echo "  make rebuild - Clean and compile again"
	@echo "  make test    - Compile and run test"
	@echo "  make help    - Show this help message"

# Đánh dấu các rule không phải là file
.PHONY: all clean rebuild test help
