#include <string>
#include <torch/torch.h>
#include "dataset.h"
#include "model.h"
#include "trainer.h"

int main() {
  // 超参数设置
  std::string data_root = "./data";
  int train_batch_size = 32;
  int test_batch_size = 32;
  int total_epoch_num = 30;
  int log_interval = 10;
  int num_workers = 4;

  // 设置随机数种子
  torch::manual_seed(1);

  // 获取设备类型
  torch::DeviceType device_type = torch::kCPU;
  if (torch::cuda::is_available()) {
    device_type = torch::kCUDA;
  }
  torch::Device device(device_type);

  // 构造网络
  Model model;
  model.to(device);

  // 设置优化器
  torch::optim::SGD optimizer(
      model.parameters(), torch::optim::SGDOptions(0.01).momentum(0.5));

  // 构造训练和测试数据集
  Dataset train_dataset("./data/indice/test_indice.csv", "./data/training_dataset", Dataset::Phase::Train);
  Dataset test_dataset("./data/indice/test_indice.csv", "./data/training_dataset", Dataset::Phase::Test);
  
  // Trainer初始化
  Trainer trainer(log_interval);
  for (size_t epoch = 1; epoch <= total_epoch_num; ++epoch) {
    // 运行训练
    trainer.train(
        epoch,
        model,
        optimizer,
        device,
        train_dataset,
        train_batch_size,
        num_workers);
        
    // 运行测试
    trainer.test(model, device, test_dataset, test_batch_size, num_workers);
  }

  torch::serialize::OutputArchive archive;
	model.save(archive);
	archive.save_to("model.pt");
	printf("Save the training result to model.pt.\n");

  return 0;
}