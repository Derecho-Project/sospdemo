#include <derecho-component/categorizer_tier.hpp>
#include <mxnet-component/inference_engine.hpp>
#include <mxnet-component/utils.hpp>
#include <mxnet-cpp/MxNetCpp.h>
#include <opencv2/opencv.hpp>
#include <vector>

#ifndef NDEBUG
#include <fstream>
#endif

namespace sospdemo
{
#ifndef NDEBUG
void Model::dump_to_file() const
{
  // synset file
  std::fstream synset_fs("synset.dump",
                         synset_fs.binary | synset_fs.trunc | synset_fs.out);
  synset_fs.write(reinterpret_cast<char *>(model_data.bytes), synset_size);
  synset_fs.close();

  // symbole file
  std::fstream symbol_fs("symbol.dump",
                         symbol_fs.binary | symbol_fs.trunc | symbol_fs.out);
  symbol_fs.write(reinterpret_cast<char *>(model_data.bytes) + synset_size,
                  symbol_size);
  symbol_fs.close();

  // params file
  std::fstream params_fs("params.dump",
                         params_fs.binary | params_fs.trunc | params_fs.out);
  params_fs.write(reinterpret_cast<char *>(model_data.bytes) + synset_size +
                      symbol_size,
                  params_size);
  params_fs.close();
}
#endif
int InferenceEngine::load_model(const Model &model)
{
  try
  {
    // 1 - load synset
    model.get_synset_vector(synset_vector);

    // 2 - load engine.
    {
      // load symbol
      this->net = mxnet::cpp::Symbol::LoadJSON(model.get_symbol_json());
      // load parameters
      std::map<std::string, mxnet::cpp::NDArray> parameters;
      model.get_parameters_map(&parameters);
      for (const auto &k : parameters)
      {
        if (k.first.substr(0, 4) == "aux:")
        {
          auto name = k.first.substr(4, k.first.size() - 4);
          this->aux_map[name] = k.second.Copy(global_ctx);
        }
        else if (k.first.substr(0, 4) == "arg:")
        {
          auto name = k.first.substr(4, k.first.size() - 4);
          this->args_map[name] = k.second.Copy(global_ctx);
        }
      }
      mxnet::cpp::NDArray::WaitAll();
      this->args_map["data"] =
          mxnet::cpp::NDArray(input_shape, global_ctx, false, kFloat32);
      mxnet::cpp::Shape label_shape(input_shape[0]);
      this->args_map["softmax_label"] =
          mxnet::cpp::NDArray(label_shape, global_ctx, false);

      this->client_data =
          mxnet::cpp::NDArray(input_shape, global_ctx, false, kFloat32);
    }
    this->net.InferExecutorArrays(
        global_ctx, &arg_arrays, &grad_arrays, &grad_reqs, &aux_arrays,
        args_map, std::map<std::string, mxnet::cpp::NDArray>(),
        std::map<std::string, mxnet::cpp::OpReqType>(), aux_map);
    for (auto &i : grad_reqs)
      i = mxnet::cpp::OpReqType::kNullOp;
    this->executor_pointer.reset(new mxnet::cpp::Executor(
        net, global_ctx, arg_arrays, grad_arrays, grad_reqs, aux_arrays));
  }
  catch (const std::exception &e)
  {
    std::cerr << "Load model failed with exception " << e.what() << std::endl;
    return -1;
  }
  catch (...)
  {
    std::cerr << "Load model failed with unknown exception." << std::endl;
    return -1;
  }

  return 0;
}

struct ModelLoadException
{
};

InferenceEngine::InferenceEngine(Model &model)
    : global_ctx(mxnet::cpp::Context::cpu()),
      input_shape(std::vector<mxnet::cpp::index_t>({1, 3, 224, 224}))
{
  if (load_model(model) != 0)
  {
    std::cerr << "Failed to load model." << std::endl;
    throw ModelLoadException{};
  }
}

InferenceEngine::~InferenceEngine()
{
  // clean up the mxnet engine.
}

Guess InferenceEngine::inference(const Photo &photo)
{
  Guess guess;
  std::vector<unsigned char> decode_buf(photo.photo_data.size);
  std::memcpy(static_cast<void *>(decode_buf.data()),
              static_cast<const void *>(photo.photo_data.bytes),
              photo.photo_data.size);
  cv::Mat mat = cv::imdecode(decode_buf, CV_LOAD_IMAGE_COLOR);
  std::vector<mx_float> array;
  // transform to fit 3x224x224 input layer
  cv::resize(mat, mat, cv::Size(256, 256));
  for (int c = 0; c < 3; c++)
  { // channels GBR->RGB
    for (int i = 0; i < 224; i++)
    { // height
      for (int j = 0; j < 224; j++)
      { // width
        int _i = i + 16;
        int _j = j + 16;
        array.push_back(
            static_cast<float>(mat.data[(_i * 256 + _j) * 3 + (2 - c)]) / 256);
      }
    }
  }
  // copy to input layer:
  args_map["data"].SyncCopyFromCPU(array.data(), input_shape.Size());

  this->executor_pointer->Forward(false);
  mxnet::cpp::NDArray::WaitAll();
  // extract the result
  auto output_shape = executor_pointer->outputs[0].GetShape();
  mx_float max = -1e10;
  int idx = -1;
  for (unsigned int jj = 0; jj < output_shape[1]; jj++)
  {
    if (max < executor_pointer->outputs[0].At(0, jj))
    {
      max = executor_pointer->outputs[0].At(0, jj);
      idx = static_cast<int>(jj);
    }
  }
  guess.guess = synset_vector[idx];
  guess.p = max;

  return std::move(guess);
}
} // namespace sospdemo