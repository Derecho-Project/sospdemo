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