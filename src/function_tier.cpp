#include <blob.hpp>
#include <categorizer_tier.hpp>
#include <function_tier.hpp>
#include <utils.hpp>

namespace sospdemo {

using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::Status;

void FunctionTier::start() {
  // test if grpc server is started or not.
  if (started) {
    return;
  }
  // lock it
  std::lock_guard<std::mutex> lck(service_mutex);
  // we need to check it again.
  if (started) {
    return;
  }
  // get the function tier server addresses
  node_id_t my_id = derecho::getConfUInt32(CONF_DERECHO_LOCAL_ID);
  const uint32_t port = FUNCTION_TIER_GRPC_PORT_BASE + my_id;
  std::string grpc_service_address =
      derecho::getConfString(CONF_DERECHO_LOCAL_IP) + ":" +
      std::to_string(port);

  // now, start the server
  ServerBuilder builder;
  builder.AddListeningPort(grpc_service_address,
                           grpc::InsecureServerCredentials());
  builder.RegisterService(this);
  this->server = std::unique_ptr(builder.BuildAndStart());
  // print messages
  std::cout << "//////////////////////////////////////////" << std::endl;
  std::cout << "FunctionTier listening on " << grpc_service_address
            << std::endl;
  std::cout << "//////////////////////////////////////////" << std::endl;
}

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

struct ParsedInstallArguments {
  const uint32_t tag;
  const ssize_t synset_size;
  const ssize_t symbol_size;
  const ssize_t params_size;
  const ssize_t data_size;
  char *model_data;
  ParsedInstallArguments(const uint32_t tag, const ssize_t synset_size,
                         const ssize_t symbol_size, const ssize_t params_size,
                         const ssize_t data_size, char *model_data)
      : tag(tag), synset_size(synset_size), symbol_size(symbol_size),
        params_size(params_size), data_size(data_size), model_data(model_data) {
  }
};

class RequestCancel {};

ParsedInstallArguments
parse_grpc_install_args(grpc::ServerContext *context,
                        grpc::ServerReader<InstallModelRequest> *reader,
                        ModelReply *reply) {

  // 1 - read the model
  // 1.1 - read the header
  InstallModelRequest request;
  if (!reader->Read(&request)) {
    std::cerr << "Install Model: fail to read model metadata 1." << std::endl;
    throw RequestCancel{};
  }
  if (!request.has_metadata()) {
    std::cerr << "Install Model: fail to read model metadata 2." << std::endl;
    throw RequestCancel{};
  }
  uint32_t tag = request.metadata().tag();
  uint32_t synset_size = request.metadata().synset_size();
  uint32_t symbol_size = request.metadata().symbol_size();
  uint32_t params_size = request.metadata().params_size();
  // 1.2 - read the model files.
  ssize_t data_size = synset_size + symbol_size + params_size;
  ssize_t offset = 0;
  char *model_data = new char[data_size];
  request.clear_metadata();
  try {
    // receive model data
    while (reader->Read(&request)) {
      if (request.model_chunk_case() != InstallModelRequest::kFileData) {
        std::cerr << "Install Model: fail to read model data 1." << std::endl;
        throw - 1;
      }
      if (static_cast<ssize_t>(offset + request.file_data().size()) >
          data_size) {
        std::cerr << "Install Model: received more data than claimed "
                  << data_size << "." << std::endl;
        throw - 2;
      }
      std::memcpy(model_data + offset, request.file_data().c_str(),
                  request.file_data().size());
      offset += request.file_data().size();
    }
    if (offset != data_size) {
      std::cerr << "Install Model: the size of received data (" << offset
                << " bytes) "
                << "does not match claimed (" << data_size << " bytes)."
                << std::endl;
      throw - 3;
    }
  } catch (...) {
    delete model_data;
    throw RequestCancel{};
  }

#ifndef NDEBUG
  std::cout << "Install Model:" << std::endl;
  std::cout << "synset_size = " << synset_size << std::endl;
  std::cout << "symbol_size = " << symbol_size << std::endl;
  std::cout << "params_size = " << params_size << std::endl;
  std::cout << "total = " << data_size << " bytes, received = " << offset
            << "bytes" << std::endl;
  std::cout.flush();
#endif
  return ParsedInstallArguments(tag, synset_size, symbol_size, params_size,
                                data_size, model_data);
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

void FunctionTier::shutdown() {
  // test if grpc server is started or not
  if (!started) {
    return;
  }
  // lock it
  std::lock_guard<std::mutex> lck(service_mutex);
  if (!started) {
    return;
  }
  // now shutdown the server.
  server->Shutdown();
  server->Wait();
}

FunctionTier::~FunctionTier() { shutdown(); }
}; // namespace sospdemo
