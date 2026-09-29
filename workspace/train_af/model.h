#ifndef MODEL_H  // 检查宏 MODEL_H 是否未定义
#define MODEL_H  // 定义宏 MODEL_H
#include <torch/torch.h>
#include <torch/serialize/archive.h>
#include <iostream>

class Model : public torch::nn::Module {
public:
    // 构造函数
    Model();

    // 前向传播
    torch::Tensor forward(torch::Tensor input);

private:
    // 卷积层
    torch::nn::Sequential conv1, conv2, conv3, conv4, conv5;
    // 全连接层
    torch::nn::Sequential fc1;
    torch::nn::Linear fc2 = nullptr; 
    torch::nn::AdaptiveAvgPool1d adaptive_pool = nullptr; // 自适应池化层
};

#endif