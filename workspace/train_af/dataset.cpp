#include "dataset.h"

// 构造函数
Dataset::Dataset(const std::string& csv_file, const std::string& data_folder, Phase phase) 
    : data_folder(data_folder), phase(phase) {
    load_data(csv_file);
}

// 加载数据
void Dataset::load_data(const std::string& csv_file) {
    std::ifstream file(csv_file);
    std::string line;

    // 跳过表头
    std::getline(file, line);

    // 读取每一行
    while (std::getline(file, line)) {
        std::istringstream ss(line);
        std::string label_str, filename;
        std::getline(ss, label_str, ',');
        std::getline(ss, filename, ',');

        int label = std::stoi(label_str);
        data.push_back({label, filename});
    }

    // 根据 Phase 划分数据集
    size_t split_index = static_cast<size_t>(data.size() * 0.8); // 80% 训练集，20% 测试集
    if (phase == Phase::Train) {
        for (size_t i = 0; i < split_index; ++i) {
            train_data.push_back(data[i]);
        }
    } else {
        for (size_t i = split_index; i < data.size(); ++i) {
            test_data.push_back(data[i]);
        }
    }
}

// 获取数据集大小
c10::optional<size_t> Dataset::size() const {
    return (phase == Phase::Train) ? train_data.size() : test_data.size();
}

// 获取数据
torch::data::Example<torch::Tensor, torch::Tensor> Dataset::get(size_t index) {
    const auto& item = (phase == Phase::Train) ? train_data[index] : test_data[index];
    const auto& label = item.first;
    const auto& filename = item.second;

    // 读取 TXT 文件内容
    std::string file_path = data_folder + "/" + filename;
    std::ifstream file(file_path);
    std::string line;
    std::vector<float> sequence;

    while (std::getline(file, line)) {
        std::istringstream iss(line);
        float value;
        while (iss >> value) {
            sequence.push_back(value);
        }
    }

    // 将序列数据转换为张量
    torch::Tensor data_tensor = torch::tensor(sequence).view({1, static_cast<long>(sequence.size())}); // 1 x N 的张量
    
    return {data_tensor, torch::tensor(label)};
}