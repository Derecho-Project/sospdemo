#pragma once
#include <derecho-component/blob.hpp>
#include <derecho/core/derecho.hpp>
#include <derecho/mutils-serialization/SerializationSupport.hpp>
#include <mxnet-component/inference_engine.hpp>
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
class CategorizerTier : public mutils::ByteRepresentable,
                        public derecho::GroupReference {
protected:
    // raw model data
    std::map<uint32_t, Model> raw_models;
    // inference engines
    std::map<uint32_t, std::unique_ptr<InferenceEngine>> inference_engines;
    std::shared_mutex inference_engines_mutex;

public:
    /**
     * Constructors
     */
    CategorizerTier() {}
    CategorizerTier(std::map<uint32_t, Model>& _raw_models)
            : raw_models(_raw_models) {}

    /**
     * Destructor
     */
    ~CategorizerTier();

    /**
     * Identify an object.
     */
    Guess inference(const Photo& photo);

    /**
     * Install Model
     * @param tag - model tag
     * @param synset_size - size of the synset data
     * @param symbol_size - size of the symbol data
     * @param params_size - size of the parameters
     * @param model_data - model data (synset_size + symbol_size + params_size)
     * @return 0 for success, a nonzero value for failure.
     */
    int install_model(const uint32_t& tag, const ssize_t& synset_size,
                      const ssize_t& symbol_size, const ssize_t& params_size,
                      const BlobWrapper& model_data);

    /**
     * Remove Model
     * @param tag - model tag
     * @return 0 for success, a nonzero value for failure.
     */
    int remove_model(const uint32_t& tag);

    /**
     * Install Model in all replicas
     * @param tag - model tag
     * @param synset_size - size of the synset data
     * @param symbol_size - size of the symbol data
     * @param params_size - size of the parameters
     * @param model_data - model data (synset_size + symbol_size + params_size)
     * @return 0 for success, a nonzero value for failure.
     */
    int ordered_install_model(const uint32_t& tag, const ssize_t& synset_size,
                              const ssize_t& symbol_size,
                              const ssize_t& params_size,
                              const BlobWrapper& model_data);

    /**
     * Remove Model in all replicas
     * @param tag - model tag
     * @return 0 for success, a nonzero value for failure.
     */
    int ordered_remove_model(const uint32_t& tag);

    REGISTER_RPC_FUNCTIONS(CategorizerTier, inference, install_model,
                           remove_model, ordered_install_model,
                           ordered_remove_model);

    DEFAULT_SERIALIZATION_SUPPORT(CategorizerTier, raw_models);
};

}  // namespace sospdemo
