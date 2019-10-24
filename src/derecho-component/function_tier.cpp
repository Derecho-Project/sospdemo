#include <derecho-component/blob.hpp>
#include <derecho-component/categorizer_tier.hpp>
#include <derecho-component/function_tier.hpp>
#include <grpc-component/function_tier-grpc.hpp>
#include <mxnet-component/utils.hpp>

namespace sospdemo {

using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::Status;

Status FunctionTier::Whatsthis(ServerContext *context,
                               grpc::ServerReader<PhotoRequest> *reader,
                               PhotoReply *reply) {

  // 1 - read the inference request
  // 1.1 - read metadata
  PhotoRequest request;
  if (!reader->Read(&request)) {
    std::cerr << "What's this: failed to read photo metadata 1." << std::endl;
    return Status::CANCELLED;
  }
  if (!request.has_metadata()) {
    std::cerr << "What's this: failed to read photo metadata 2." << std::endl;
    return Status::CANCELLED;
  }
  if (request.metadata().tags_size() > 1) {
    /**
    // TODO: implement support for multiple tags.
    // hint: iterate through tags as follows
    for (auto& tag: request.metadata().tags()) {
         // send photo to a model corresponding to tag
    }
    **/
    reply->set_desc("Multiple tags support to be implmemented.");
    return Status::OK;
  }
  uint32_t tag = request.metadata().tags(0);
  uint32_t photo_size = request.metadata().photo_size();
  char *photo_data = (char *)malloc(photo_size);
  uint32_t offset = 0;
  // 1.2 - read the photo file.
  request.clear_metadata();
  try {
    // receive model data
    while (reader->Read(&request)) {
      if (request.photo_chunk_case() != PhotoRequest::kFileData) {
        std::cerr << "What's this: fail to read model data 1." << std::endl;
        throw - 1;
      }
      if (static_cast<ssize_t>(offset + request.file_data().size()) >
          photo_size) {
        std::cerr << "What's this: received more data than claimed "
                  << photo_size << "." << std::endl;
        throw - 2;
      }
      std::memcpy(photo_data + offset, request.file_data().c_str(),
                  request.file_data().size());
      offset += request.file_data().size();
    }
    if (offset != photo_size) {
      std::cerr << "What's this: the size of received data (" << offset
                << " bytes) "
                << "does not match claimed (" << photo_size << " bytes)."
                << std::endl;
      throw - 3;
    }
  } catch (...) {
    delete photo_data;
    return Status::CANCELLED;
  }
#ifndef NDEBUG
  std::cout << "What's this?" << std::endl;
  std::cout << "tag = " << tag << std::endl;
  std::cout << "photo size = " << photo_size << std::endl;
  std::cout.flush();
#endif // NDEBUG

  // 2 - pass it to the categorizer tier.
  derecho::ExternalCaller<CategorizerTier> &categorizer_tier_handler =
      group->get_nonmember_subgroup<CategorizerTier>();
  auto shards = group->get_subgroup_members<CategorizerTier>();
  node_id_t target = shards[tag % shards.size()][0];
  // TODO: add randomness for load-balancing.

#ifndef NDEBUG
  std::cout << "get the handle of categorizer tier subgroup. external caller "
               "is valid:"
            << categorizer_tier_handler.is_valid() << std::endl;
  std::cout << "p2p_send using target = " << target << std::endl;
  std::cout.flush();
#endif

  // 3 - post it to the categorizer tier
  BlobWrapper photo_data_wrapper(photo_data, photo_size);
  Photo photo(tag, photo_data_wrapper);
  derecho::rpc::QueryResults<Guess> result =
      categorizer_tier_handler.p2p_send<RPC_NAME(inference)>(target, photo);
#ifndef NDEBUG
  std::cout << "p2p_send for inference returns." << std::endl;
  std::cout.flush();
#endif
  Guess ret = result.get().get(target);

#ifndef NDEBUG
  std::cout << "response from the categorizer tier is received with ret = "
            << ret.guess << "." << std::endl;
  std::cout.flush();
#endif
  delete photo_data;

  // 4 - return Status::OK;
  reply->set_desc(ret.guess);

  return Status::OK;
}

Status
FunctionTier::InstallModel(grpc::ServerContext *context,
                           grpc::ServerReader<InstallModelRequest> *reader,
                           ModelReply *reply) {

  try {
    auto parsed_args = parse_grpc_install_args(context, reader, reply);
    const uint32_t tag = parsed_args.tag;
    const ssize_t synset_size = parsed_args.synset_size;
    const ssize_t symbol_size = parsed_args.symbol_size;
    const ssize_t params_size = parsed_args.params_size;
    const ssize_t data_size = parsed_args.data_size;
    char *model_data = parsed_args.model_data;

    // 2 - find the shard
    // Currently, we use the one-shard implementation.
    auto shards = group->get_subgroup_members<CategorizerTier>();
    node_id_t target = shards[tag % shards.size()][0];
    // TODO: add randomness for load-balancing.

    // 3 - post it to the categorizer tier
    derecho::ExternalCaller<CategorizerTier> &categorizer_tier_handler =
        group->get_nonmember_subgroup<CategorizerTier>();
    BlobWrapper model_data_wrapper(model_data, data_size);
    derecho::rpc::QueryResults<int> result =
        categorizer_tier_handler.p2p_send<RPC_NAME(install_model)>(
            target, tag, synset_size, symbol_size, params_size,
            model_data_wrapper);
#ifndef NDEBUG
    std::cout << "p2p_send for install_model returns." << std::endl;
    std::cout.flush();
#endif
    int ret = result.get().get(target);

#ifndef NDEBUG
    std::cout << "response from the categorizer tier is received with ret = "
              << ret << "." << std::endl;
    std::cout.flush();
#endif
    delete model_data;

    // 4 - return Status::OK;
    reply->set_error_code(ret);
    if (ret == 0)
      reply->set_error_desc("install model successfully.");
    else
      reply->set_error_desc("some error occurs.");

    return Status::OK;
  } catch (const RequestCancel &) {
    return Status::CANCELLED;
  }
}

Status FunctionTier::RemoveModel(grpc::ServerContext *context,
                                 const RemoveModelRequest *request,
                                 ModelReply *reply) {
  // 1 - get the model tag
  uint32_t tag = request->tag();

  // 2 - find the shard
  // currently, we use the one-shard implementation.
  derecho::ExternalCaller<CategorizerTier> &categorizer_tier_handler =
      group->get_nonmember_subgroup<CategorizerTier>();
  auto shards = group->get_subgroup_members<CategorizerTier>();
  node_id_t target = shards[tag % shards.size()]
                           [0]; // TODO: add randomness for load-balancing.
#ifndef NDEBUG
  std::cout << "get the handle of categorizer tier subgroup. external caller "
               "is valid:"
            << categorizer_tier_handler.is_valid() << std::endl;
  std::cout << "p2p_send using target = " << target << std::endl;
  std::cout.flush();
#endif

  // 3 - post it to the categorizer tier
  derecho::rpc::QueryResults<int> result =
      categorizer_tier_handler.p2p_send<RPC_NAME(remove_model)>(target, tag);
  int ret = result.get().get(target);

  reply->set_error_code(ret);
  if (ret == 0)
    reply->set_error_desc("remove model successfully.");
  else
    reply->set_error_desc("some error occurs.");

  return Status::OK;
}
} // namespace sospdemo
