#include <derecho-component/categorizer_tier.hpp>
#include <mxnet-component/utils.hpp>
#include <mxnet-cpp/MxNetCpp.h>
#include <opencv2/opencv.hpp>
#include <vector>

#ifndef NDEBUG
#include <fstream>
#endif

namespace sospdemo
{

CategorizerTier::~CategorizerTier()
{
    // clean up.
}

Guess CategorizerTier::inference(const Photo &photo)
{
#ifndef NDEBUG
    std::cout << "CategorizerTier::inference() called with photo tag = "
              << photo.tag << std::endl;
    std::cout.flush();
#endif // NDEBUG
    // 1 - load model if required
    std::shared_lock read_lock(inference_engines_mutex);
    if (inference_engines.find(photo.tag) == inference_engines.end())
    {
        if (raw_models.find(photo.tag) == raw_models.end())
        {
            Guess guess;
            std::cerr << "Cannot find model for photo tag:" << photo.tag << "."
                      << std::endl;
            guess.guess = "Cannot find model for photo tag.";
            return guess;
        }
        read_lock.unlock();
        try
        {
            std::unique_ptr<InferenceEngine> engine =
                std::make_unique<InferenceEngine>(raw_models[photo.tag]);
            std::unique_lock write_lock(inference_engines_mutex);
            inference_engines[photo.tag] = std::move(engine);
            return inference_engines[photo.tag]->inference(photo);
        }
        catch (...)
        {
            std::cerr << "Fatal error loading model" << std::endl;
            Guess guess;
            guess.guess = "Cannot load model for photo tag.  Something is wrong.";
            return guess;
        }
    }

    // 2 - inference
    return inference_engines[photo.tag]->inference(photo);
}

int CategorizerTier::install_model(const uint32_t &tag,
                                   const ssize_t &synset_size,
                                   const ssize_t &symbol_size,
                                   const ssize_t &params_size,
                                   const BlobWrapper &model_data)
{
#ifndef DEBUG
    std::cout << "CategorizerTier::install_model() is called with tag=" << tag
              << std::endl;
    std::cout.flush();
#endif
    int ret = 0;
    auto &subgroup_handler = group->template get_subgroup<CategorizerTier>();
    // pass it to all replicas
    derecho::rpc::QueryResults<int> results =
        subgroup_handler.ordered_send<RPC_NAME(ordered_install_model)>(
            tag, synset_size, symbol_size, params_size, model_data);
    // check results
    decltype(results)::ReplyMap &replies = results.get();
    for (auto &reply_pair : replies)
    {
        int one_ret = reply_pair.second.get();
        std::cout << "Reply from node " << reply_pair.first << " is " << one_ret
                  << std::endl;
        if (one_ret != 0)
        {
            ret = one_ret;
        }
    }
#ifndef NDEBUG
    std::cout << "Returning from CategorizerTier::install_model() with ret="
              << ret << "." << std::endl;
    std::cout.flush();
#endif
    return ret;
}

int CategorizerTier::ordered_install_model(const uint32_t &tag,
                                           const ssize_t &synset_size,
                                           const ssize_t &symbol_size,
                                           const ssize_t &params_size,
                                           const BlobWrapper &model_data)
{
    // validation
    if (raw_models.find(tag) != raw_models.end())
    {
        std::cerr << "install_model failed because tag (" << tag
                  << ") has been taken." << std::endl;
        return -1;
    }

    Model model;
    model.synset_size = synset_size;
    model.symbol_size = symbol_size;
    model.params_size = params_size;
    model.model_data = Blob(model_data.bytes, model_data.size);
    raw_models.emplace(tag,
                       model); // TODO: recheck - unecessary copied is avoided?
#ifndef NDEBUG
    std::cout
        << "Returning from CategorizerTier::ordered_install_model() successfully."
        << std::endl;
    std::cout.flush();
#endif
    return 0;
}

int CategorizerTier::remove_model(const uint32_t &tag)
{
    int ret = 0;
    auto &subgroup_handler = group->template get_subgroup<CategorizerTier>();
    // pass it to all replicas
    derecho::rpc::QueryResults<int> results =
        subgroup_handler.ordered_send<RPC_NAME(ordered_remove_model)>(tag);
    // check results;
    decltype(results)::ReplyMap &replies = results.get();
    for (auto &reply_pair : replies)
    {
        int one_ret = reply_pair.second.get();
        std::cout << "Reply from node " << reply_pair.first << " is " << one_ret
                  << std::endl;
        if (one_ret != 0)
        {
            ret = one_ret;
        }
    }
    return ret;
}

int CategorizerTier::ordered_remove_model(const uint32_t &tag)
{
    // remove from raw_model.
    auto model_search = raw_models.find(tag);
    if (model_search == raw_models.end())
    {
        std::cerr << "remove_model failed because tag (" << tag << ") is not taken."
                  << std::endl;
        return -1;
    }

    raw_models.erase(model_search);

    // remove from inference_engines
    std::unique_lock write_lock(inference_engines_mutex);
    auto engine_search = inference_engines.find(tag);
    if (engine_search != inference_engines.end())
    {
        inference_engines.erase(engine_search);
    }

    return 0;
}
} // namespace sospdemo
