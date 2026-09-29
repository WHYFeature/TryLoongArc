#pragma once

#include <torch/torch.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <unordered_map>

class Dataset : public torch::data::Dataset<Dataset> {
public:
    // 枚举类型，用于表示数据集的阶段
    enum class Phase { Train, Test };

    // 构造函数
    Dataset(const std::string& csv_file, const std::string& data_folder, Phase phase);

    // 获取数据集大小
    c10::optional<size_t> size() const override;

    // 获取数据
    torch::data::Example<> get(size_t index) override;

private:
    // 加载数据
    void load_data(const std::string& csv_file);

    std::string data_folder; // 数据文件夹路径
    Phase phase; // 当前数据集阶段
    std::vector<std::pair<int, std::string>> data; // 存储所有数据
    std::vector<std::pair<int, std::string>> train_data; // 训练集
    std::vector<std::pair<int, std::string>> test_data; // 测试集
};