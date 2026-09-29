#include "trainer.h"
#include <torch/torch.h>
#include <cstdio>
#include <chrono>

void Trainer::train(
    size_t epoch,
    Model& model,
    torch::optim::Optimizer& optimizer,
    torch::Device device,
    Dataset& train_dataset,
    int batch_size,
    int num_workers) {
  model.train();
  auto start_time1 = std::chrono::high_resolution_clock::now();
  auto data_loader = torch::data::make_data_loader<torch::data::samplers::RandomSampler>(
      train_dataset,
      torch::data::DataLoaderOptions().batch_size(batch_size).workers(num_workers));
  // 记录结束时间并计算耗时
  auto end_time1 = std::chrono::high_resolution_clock::now();
  std::chrono::duration<double> epoch_duration1 = end_time1 - start_time1;
  std::cout << std::endl;
  std::cout << "Dataloader time : "<<epoch_duration1.count() <<"s"<< std::endl;

  size_t batch_idx = 0;
  auto start_time = std::chrono::high_resolution_clock::now();
  for (const auto& batch : *data_loader) {
    std::vector<torch::Tensor> data_vec, target_vec;
    for (const auto& example : batch) {
      data_vec.push_back(example.data.to(device));
      target_vec.push_back(example.target.unsqueeze(0).to(device));
    }
    
    torch::Tensor data = torch::stack(data_vec);
    torch::Tensor targets = torch::stack(target_vec).squeeze(1);

    // 调整张量形状以适应 Conv1d 层的输入要求
    int actual_batch_size = data.size(0); 
    data = data.view({actual_batch_size, 1, 1250}); // 确保形状为 [batch_size, channels, length]

    optimizer.zero_grad();
    auto output = model.forward(data);

    // 应用 log_softmax 到模型输出
    auto log_probs = torch::log_softmax(output, 1);

    // 使用 nll_loss 计算损失
    auto loss = torch::nn::functional::nll_loss(log_probs, targets);

    loss.backward();
    optimizer.step();
    
    // if (batch_idx == 0) { // 只打印第一个批次
    //     std::cout << "Sample outputs: " << output.sizes() << std::endl;
    //     std::cout << "Sample targets: " << targets.sizes() << std::endl;
    //     // 打印一些样本的输出和标签以确认匹配
    //     std::cout << "Sample outputs: " << output.slice(/*dim=*/0, /*start=*/0, /*end=*/5) << std::endl;
    //     std::cout << "Sample targets: " << targets.slice(/*dim=*/0, /*start=*/0, /*end=*/5) << std::endl;
    // }

    if (batch_idx++ % log_interval_ == 0) {
      std::printf(
          "\rTrain Epoch: %ld [%5lu/%5lu] Loss: %.4f",
          epoch,
          batch_idx * actual_batch_size,
          train_dataset.size().value(),
          loss.template item<float>());
      fflush(stdout);
    }
  }
  // 记录结束时间并计算耗时
  auto end_time = std::chrono::high_resolution_clock::now();
  std::chrono::duration<double> epoch_duration = end_time - start_time;
  std::cout << std::endl;
  std::cout << "Epoch batch_sum " <<batch_idx <<" time : "<<epoch_duration.count() <<"s"<< std::endl;
}

void Trainer::test(
    Model& model,
    torch::Device device,
    Dataset& test_dataset,
    int batch_size,
    int num_workers) {
  // 测试时要将模型置为eval模式
  model.eval();
  double test_loss = 0;
  int64_t correct = 0;
  auto start_time = std::chrono::high_resolution_clock::now();

  // 构造 DataLoader, 设置 batch size 和 worker 数目
  auto data_loader = torch::data::make_data_loader<torch::data::samplers::SequentialSampler>(
      test_dataset, // 注意这里不使用指针
      torch::data::DataLoaderOptions().batch_size(batch_size).workers(num_workers));

  // 禁用梯度计算以节省内存和加速推理
  torch::NoGradGuard no_grad;

  size_t batch_idx = 0;
  for (const auto& batch : *data_loader) {
    // 合并 batch 中的数据和标签
    std::vector<torch::Tensor> data_vec, target_vec;
    for (const auto& example : batch) {
      data_vec.push_back(example.data.to(device));
      target_vec.push_back(example.target.unsqueeze(0).to(device));
    }
    
    torch::Tensor data = torch::stack(data_vec);
    torch::Tensor targets = torch::stack(target_vec).squeeze(1);

    // 模型前向操作，得到预测输出
    auto output = model.forward(data);

    // 应用 log_softmax 到模型输出
    auto log_probs = torch::log_softmax(output, 1);

    // 使用 nll_loss 计算测试时的 loss
    test_loss += torch::nn::functional::nll_loss(
                     log_probs,
                     targets)
                     .item<float>();
    auto pred = output.argmax(1);
    correct += pred.eq(targets).sum().template item<int64_t>();
    batch_idx++;
  }

  test_loss /= test_dataset.size().value();
  std::printf(
      "\nInference : Average loss: %.4f | Accuracy: %.3f\n",
      test_loss,
      static_cast<double>(correct) / test_dataset.size().value());
  // 记录结束时间并计算耗时
  auto end_time = std::chrono::high_resolution_clock::now();
  std::chrono::duration<double> epoch_duration = end_time - start_time;
  std::cout << "Inference batch_sum: " <<batch_idx <<" time : "<<epoch_duration.count() <<"s"<< std::endl;
}