#include "model.h"

Model::Model() {
    // 初始化卷积层
    conv1 = register_module("conv1", torch::nn::Sequential(
        torch::nn::Conv1d(torch::nn::Conv1dOptions(1, 3, 6).stride(2).padding(2)),
        torch::nn::ReLU(true),
        torch::nn::BatchNorm1d(3)
    ));

    conv2 = register_module("conv2", torch::nn::Sequential(
        torch::nn::Conv1d(torch::nn::Conv1dOptions(3, 5, 5).stride(2).padding(2)),
        torch::nn::ReLU(true),
        torch::nn::BatchNorm1d(5)
    ));

    conv3 = register_module("conv3", torch::nn::Sequential(
        torch::nn::Conv1d(torch::nn::Conv1dOptions(5, 10, 4).stride(2).padding(2)),
        torch::nn::ReLU(true),
        torch::nn::BatchNorm1d(10)
    ));

    conv4 = register_module("conv4", torch::nn::Sequential(
        torch::nn::Conv1d(torch::nn::Conv1dOptions(10, 20, 4).stride(2).padding(2)),
        torch::nn::ReLU(true),
        torch::nn::BatchNorm1d(20)
    ));

    conv5 = register_module("conv5", torch::nn::Sequential(
        torch::nn::Conv1d(torch::nn::Conv1dOptions(20, 20, 4).stride(2).padding(2)),
        torch::nn::ReLU(true),
        torch::nn::BatchNorm1d(20)
    ));

    // 初始化自适应池化层
    adaptive_pool = register_module("adaptive_pool", torch::nn::AdaptiveAvgPool1d(1));

    // 初始化全连接层
    fc1 = register_module("fc1", torch::nn::Sequential(
        torch::nn::Dropout(0.5),
        torch::nn::Linear(20, 10) // 20 是通道数
    ));

    fc2 = register_module("fc2", torch::nn::Linear(10, 2)); // 10 是 fc1 的输出
}

torch::Tensor Model::forward(torch::Tensor input) {
    auto conv1_output = conv1->forward(input);
    auto conv2_output = conv2->forward(conv1_output);
    auto conv3_output = conv3->forward(conv2_output);
    auto conv4_output = conv4->forward(conv3_output);
    auto conv5_output = conv5->forward(conv4_output);

    auto pooled_output = adaptive_pool->forward(conv5_output);
    auto flattened_output = pooled_output.view({-1, 20}); // 20 是通道数

    auto fc1_output = torch::relu(fc1->forward(flattened_output));
    auto fc2_output = fc2->forward(fc1_output);
    return fc2_output;
}



/*
torch::Tensor Model::forward(torch::Tensor input)
{
  auto conv1_output = conv1->forward(input);
  std::cout << "conv1 output shape: " << conv1_output.sizes() << std::endl;

  auto conv2_output = conv2->forward(conv1_output);
  std::cout << "conv2 output shape: " << conv2_output.sizes() << std::endl;

  auto conv3_output = conv3->forward(conv2_output);
  std::cout << "conv3 output shape: " << conv3_output.sizes() << std::endl;

  auto conv4_output = conv4->forward(conv3_output);
  std::cout << "conv4 output shape: " << conv4_output.sizes() << std::endl;

  auto conv5_output = conv5->forward(conv4_output);
  std::cout << "conv5 output shape: " << conv5_output.sizes() << std::endl;

  // 使用自适应池化层
  auto pooled_output = adaptive_pool->forward(conv5_output);

  // 自适应池化后的输出形状应为 [batch_size, channels, 1]
  // 因此展平后的形状为 [batch_size, channels]
  auto flattened_output = pooled_output.view({-1, 20}); // 注意这里的20是通道数

  auto fc1_output = torch::relu(fc1->forward(flattened_output));
  auto fc2_output = fc2->forward(fc1_output);
  std::cout << "fc2 output shape: " << fc2_output.sizes() << std::endl;
  return fc2_output;
}*/