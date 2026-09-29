cmake_minimum_required(VERSION 3.0 FATAL_ERROR)
project(LibTorchDemo)

# 设置 LibTorch 的安装路径
set(TORCH_INSTALL_PREFIX "/tmp/loongTensor")
#set(TORCH_INSTALL_PREFIX "/home/emd/lib/libtorchx86")

# 设置 LibTorch 的配置文件路径
set(Torch_DIR "${TORCH_INSTALL_PREFIX}/share/cmake/Torch")

# 设置 Caffe2 的配置文件路径（如果需要）
set(Caffe2_DIR "${TORCH_INSTALL_PREFIX}/share/cmake/Caffe2")

# 编译选项
set(CMAKE_CXX_FLAGS_RELEASE "-O3")
set(CMAKE_CXX_STANDARD 14)

# 查找 LibTorch 包
find_package(Torch REQUIRED)

# 添加可执行文件
add_executable(train main.cpp model.cpp dataset.cpp trainer.cpp)

# 链接 LibTorch 库
target_link_libraries(train PRIVATE ${TORCH_LIBRARIES} pthread)

# 使用 find_package 设置的头文件路径
target_include_directories(train PRIVATE ${TORCH_INCLUDE_DIRS})