#pragma once

#include <blob.hpp>
#include <derecho/core/derecho.hpp>
#include <derecho/mutils-serialization/SerializationSupport.hpp>
#include <mxnet-cpp/MxNetCpp.h>
#include <mxnet-cpp/initializer.h>
#include <mxnet/c_api.h>
#include <mxnet/tuple.h>
#include <shared_mutex>
#include <string>

namespace sospdemo {
/**
 * The back end subgroup type
 */
class Photo : public mutils::ByteRepresentable {
public:
  uint32_t tag;
  BlobWrapper photo_data;

  Photo() {}
  Photo(uint32_t &_tag, BlobWrapper &_photo_data)
      : tag(_tag), photo_data(_photo_data) {}

  DEFAULT_SERIALIZATION_SUPPORT(Photo, tag, photo_data);
};

class Guess : public mutils::ByteRepresentable {
public:
  std::string guess;
  float p;

  Guess() {}
  Guess(std::string &_guess, float &_p) : guess(_guess), p(_p) {}

  DEFAULT_SERIALIZATION_SUPPORT(Guess, guess, p);
};

/**
 * The raw model data
 */
struct Model : public mutils::ByteRepresentable {
  ssize_t synset_size;
  ssize_t symbol_size;
  ssize_t params_size;
  Blob model_data;

  Model() : synset_size(0), symbol_size(0), params_size(0) {}

  std::vector<std::string> &
  get_synset_vector(std::vector<std::string> &synset_vector) const {
    std::string synset_string(model_data.bytes, synset_size);
    std::istringstream iss(synset_string);
    char buf[256];
    synset_vector.clear();
    while (iss.getline(buf, 256)) {
      synset_vector.emplace_back(buf);
    }
    return synset_vector;
  }

  std::string get_symbol_json() const {
    return std::string(model_data.bytes + synset_size, symbol_size);
  }

  void get_parameters_map(
      std::map<std::string, mxnet::cpp::NDArray> *parameters_map) const {
    mxnet::cpp::NDArray::LoadFromBuffer(
        static_cast<const void *>(model_data.bytes + synset_size + symbol_size),
        params_size, 0, parameters_map);
  }

  Model(ssize_t &_synset_size, ssize_t &_symbol_size, ssize_t &_params_size,
        Blob &_model_data)
      : synset_size(_synset_size), symbol_size(_symbol_size),
        params_size(_params_size), model_data(_model_data) {}

#ifndef NDEBUG
  void dump_to_file() const;
#endif

  DEFAULT_SERIALIZATION_SUPPORT(Model, synset_size, symbol_size, params_size,
                                model_data);
};

/**
 * The MXNet inference Engine
 */
class InferenceEngine {
  /**
   * the synset explains inference result.
   */
  std::vector<std::string> synset_vector;
  /**
   * symbol
   */
  mxnet::cpp::Symbol net;
  /**
   * argument parameters
   */
  std::map<std::string, mxnet::cpp::NDArray> args_map;
  /**
   * auxliary parameters
   */
  std::map<std::string, mxnet::cpp::NDArray> aux_map;
  /**
   * global ctx
   */
  mxnet::cpp::Context global_ctx;
  /**
   * the input shape
   */
  mxnet::cpp::Shape input_shape;
  /**
   * argument arrays
   */
  std::vector<mxnet::cpp::NDArray> arg_arrays;
  /**
   * gradient arrays
   */
  std::vector<mxnet::cpp::NDArray> grad_arrays;
  /**
   * ??
   */
  std::vector<mxnet::cpp::OpReqType> grad_reqs;
  /**
   * auxliary array
   */
  std::vector<mxnet::cpp::NDArray> aux_arrays;
  /**
   * client data
   */
  mxnet::cpp::NDArray client_data;
  /**
   * the work horse: mxnet executor
   */
  std::unique_ptr<mxnet::cpp::Executor> executor_pointer;

private:
  /**
   * load the model into mxnet executor.
   * @param model - the raw model data
   * @return - 0 for success, other values for failure.
   */
  int load_model(const Model &model);

public:
  /**
   * constructor
   */
  InferenceEngine(Model &model);

  /**
   * destructor
   */
  virtual ~InferenceEngine();

  /**
   * inference
   */
  Guess inference(const Photo &photo);
};

} // namespace sospdemo
