#pragma once
#include <torch/torch.h>

#include "dataset.h"
#include "model.h"

class Trainer {
 public:
  Trainer(int log_interval) : log_interval_(log_interval){};

  void train(
      size_t epoch,
      Model& model,
      torch::optim::Optimizer& optimizer,
      torch::Device device,
      Dataset& train_dataset,
      int batch_size,
      int num_workers);

  void test(
      Model& model,
      torch::Device device,
      Dataset& test_dataset,
      int batch_size,
      int num_workers);

 private:
  int log_interval_;
};
