#include "paddle_inference_api.h"
#include <gflags/gflags.h>
#include <iostream>
#include <thread>
#include <fstream>
#include <iterator>
#include <algorithm>
#include <numeric>
#include <functional>

void produce_data(std::vector<std::vector<float>>* data);
void model_forward_test();

int main(int argc, char* argv[]) {
    gflags::ParseCommandLineFlags(&argc, &argv, true);
    model_forward_test();
    return 0;
}

void model_forward_test() {
    std::cout << "1. read the data" << std::endl;
    std::vector<std::vector<float>> feats;
    produce_data(&feats);
    
    std::cout << "2. load the model" << std::endl;;
    std::string model_graph = "../../../../model/paddle_online_deepspeech/model/avg_1.jit.pdmodel";
    std::string model_params = "../../../../model/paddle_online_deepspeech/model/avg_1.jit.pdiparams";
    paddle_infer::Config config;
    config.SetModel(model_graph, model_params);
    config.SwitchIrOptim(false);
    config.DisableFCPadding();
    auto predictor = paddle_infer::CreatePredictor(config);
    
    std::cout << "3. feat shape, row=" << feats.size() << ",col=" << feats[0].size() << std::endl;
    std::vector<float> paddle_input_feature_matrix;
    for(const auto& item : feats) {
        paddle_input_feature_matrix.insert(paddle_input_feature_matrix.end(), item.begin(), item.end());
    }

    std::cout << "4. fead the data to model" << std::endl;
    int row = feats.size();
    int col = feats[0].size();
    std::vector<std::string> input_names = predictor->GetInputNames();
    std::vector<std::string> output_names = predictor->GetOutputNames();

    std::unique_ptr<paddle_infer::Tensor> input_tensor = 
        predictor->GetInputHandle(input_names[0]);
    std::vector<int> INPUT_SHAPE = {1, row, col};
    input_tensor->Reshape(INPUT_SHAPE);
    input_tensor->CopyFromCpu(paddle_input_feature_matrix.data());
    
    std::unique_ptr<paddle_infer::Tensor> input_len = predictor->GetInputHandle(input_names[1]);
    std::vector<int> input_len_size = {1};
    input_len->Reshape(input_len_size);
    std::vector<int64_t> audio_len;
    audio_len.push_back(row);
    input_len->CopyFromCpu(audio_len.data());

    std::unique_ptr<paddle_infer::Tensor> chunk_state_h_box = predictor->GetInputHandle(input_names[2]);
    std::vector<int> chunk_state_h_box_shape = {3, 1, 1024};
    chunk_state_h_box->Reshape(chunk_state_h_box_shape);
    int chunk_state_h_box_size = std::accumulate(chunk_state_h_box_shape.begin(), chunk_state_h_box_shape.end(), 
                                    1, std::multiplies<int>());
    std::vector<float> chunk_state_h_box_data(chunk_state_h_box_size, 0.0f);
    chunk_state_h_box->CopyFromCpu(chunk_state_h_box_data.data());

    std::unique_ptr<paddle_infer::Tensor> chunk_state_c_box = predictor->GetInputHandle(input_names[3]);
    std::vector<int> chunk_state_c_box_shape = {3, 1, 1024};
    chunk_state_c_box->Reshape(chunk_state_c_box_shape);
    int chunk_state_c_box_size = std::accumulate(chunk_state_c_box_shape.begin(), chunk_state_c_box_shape.end(), 
                                    1, std::multiplies<int>());
    std::vector<float> chunk_state_c_box_data(chunk_state_c_box_size, 0.0f);
    chunk_state_c_box->CopyFromCpu(chunk_state_c_box_data.data());

    bool success = predictor->Run();

    std::unique_ptr<paddle_infer::Tensor> h_out = predictor->GetOutputHandle(output_names[2]);
    std::vector<int> h_out_shape = h_out->shape();
    int h_out_size = std::accumulate(h_out_shape.begin(), h_out_shape.end(),
                                      1, std::multiplies<int>());
    std::vector<float> h_out_data(h_out_size);
    h_out->CopyToCpu(h_out_data.data());

    std::unique_ptr<paddle_infer::Tensor> c_out = predictor->GetOutputHandle(output_names[3]);
    std::vector<int> c_out_shape = c_out->shape();
    int c_out_size = std::accumulate(c_out_shape.begin(), c_out_shape.end(),
                                      1, std::multiplies<int>());
    std::vector<float> c_out_data(c_out_size);
    c_out->CopyToCpu(c_out_data.data());

    std::unique_ptr<paddle_infer::Tensor> output_tensor =
        predictor->GetOutputHandle(output_names[0]);
    std::vector<int> output_shape = output_tensor->shape();
    std::vector<float> output_probs;
    int output_size = std::accumulate(output_shape.begin(), output_shape.end(),
                                      1, std::multiplies<int>());
    output_probs.resize(output_size);
    output_tensor->CopyToCpu(output_probs.data());
    row = output_shape[1];
    col = output_shape[2];

    std::vector<std::vector<float>> probs;
    probs.reserve(row);
    for (int i = 0; i < row; i++) {
        probs.push_back(std::vector<float>());
        probs.back().reserve(col);

        for (int j = 0; j < col; j++) {
            probs.back().push_back(output_probs[i * col + j]);
        }
    }

    std::vector<std::vector<float>> log_feat = probs;
    std::cout << "probs, row: " << log_feat.size() << " col: " << log_feat[0].size() << std::endl;
    for (size_t row_idx = 0; row_idx < log_feat.size(); ++row_idx) {
      for (size_t col_idx = 0; col_idx < log_feat[row_idx].size(); ++col_idx) {
        std::cout << log_feat[row_idx][col_idx] << " "; 
      }
      std::cout << std::endl;
    }
}

void produce_data(std::vector<std::vector<float>>* data) {
  int chunk_size = 35;
  int col_size = 161;
  data->reserve(chunk_size);
  data->back().reserve(col_size);
  for (int row = 0; row < chunk_size; ++row) {
    data->push_back(std::vector<float>());
    for (int col_idx = 0; col_idx < col_size; ++col_idx) {
      data->back().push_back(0.201);
    }
  }
}
