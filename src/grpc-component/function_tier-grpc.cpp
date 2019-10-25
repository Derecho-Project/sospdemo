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
    if(started) {
        return;
    }
    // lock it
    std::lock_guard<std::mutex> lck(service_mutex);
    // we need to check it again.
    if(started) {
        return;
    }
    // get the function tier server addresses
    node_id_t my_id = derecho::getConfUInt32(CONF_DERECHO_LOCAL_ID);
    const uint32_t port = FUNCTION_TIER_GRPC_PORT_BASE + my_id;
    std::string grpc_service_address = derecho::getConfString(CONF_DERECHO_LOCAL_IP) + ":" + std::to_string(port);

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

template <typename RequestType>
void check_request(RequestType& request,
                   grpc::ServerReader<RequestType>* reader) {
    if(!reader->Read(&request)) {
        std::cerr << "Failed to read metadata 1." << std::endl;
        throw RequestCancel{};
    }
    if(!request.has_metadata()) {
        std::cerr << "Failed to read metadata 2." << std::endl;
        throw RequestCancel{};
    }
}

template <typename Request_Type, typename RequestChunkCase>
void read_data_arg(Request_Type& request,
                   RequestChunkCase (*chunk_case)(Request_Type&),
                   grpc::ServerReader<Request_Type>* reader,
                   char* data_buf, ssize_t data_size) {
    try {
        ssize_t offset = 0;
        // receive model data
        while(reader->Read(&request)) {
            if(chunk_case(request) != Request_Type::kFileData) {
                std::cerr << "Failed to read data 1." << std::endl;
                throw - 1;
            }
            if(static_cast<ssize_t>(offset + request.file_data().size()) > data_size) {
                std::cerr << "Received more data than claimed "
                          << data_size << "." << std::endl;
                throw - 2;
            }
            std::memcpy(data_buf + offset, request.file_data().c_str(),
                        request.file_data().size());
            offset += request.file_data().size();
        }
        if(offset != data_size) {
            std::cerr << "The size of received data (" << offset
                      << " bytes) "
                      << "does not match claimed (" << data_size << " bytes)."
                      << std::endl;
            throw - 3;
        }
    } catch(...) {
        throw RequestCancel{};
    }
}

ParsedInstallArguments parse_grpc_install_args(grpc::ServerContext* context,
                                               grpc::ServerReader<InstallModelRequest>* reader,
                                               ModelReply* reply) {
    InstallModelRequest request;
    // 1 - read the model
    // 1.1 - read the header
    check_request(request, reader);
    uint32_t tag = request.metadata().tag();
    uint32_t synset_size = request.metadata().synset_size();
    uint32_t symbol_size = request.metadata().symbol_size();
    uint32_t params_size = request.metadata().params_size();
    // 1.2 - read the model files.
    ssize_t data_size = synset_size + symbol_size + params_size;
    char* model_data = new char[data_size];
    request.clear_metadata();
    InstallModelRequest::ModelChunkCase (*chunk_case)(InstallModelRequest&) =
            [](InstallModelRequest& r) { return r.model_chunk_case(); };

    read_data_arg(request, chunk_case, reader, model_data, data_size);
    return ParsedInstallArguments{tag, synset_size, symbol_size, params_size,
                                  data_size, model_data};
}

ParsedInstallArguments::ParsedInstallArguments(ParsedInstallArguments&& o)
        : tag(o.tag),
          synset_size(o.synset_size),
          symbol_size(o.symbol_size),
          params_size(o.params_size),
          data_size(o.data_size),
          model_data(o.model_data) {
    o.model_data = nullptr;
}

ParsedInstallArguments::~ParsedInstallArguments() {
#ifndef NDEBUG
    std::cout << "Install Model:" << std::endl;
    std::cout << "synset_size = " << synset_size << std::endl;
    std::cout << "symbol_size = " << symbol_size << std::endl;
    std::cout << "params_size = " << params_size << std::endl;
    std::cout << "total = " << data_size << " bytes" << std::endl;
    std::cout.flush();
#endif
    if(model_data) delete model_data;
}

ParsedWhatsThisArguments parse_grpc_whatsthis_args(grpc::ServerContext* context,
                                                   grpc::ServerReader<PhotoRequest>* reader,
                                                   PhotoReply* reply) {
    // 1 - read the inference request
    // 1.1 - read metadata
    PhotoRequest request;
    check_request(request, reader);
    if(request.metadata().tags_size() > 1) {
        /**
    // TODO: implement support for multiple tags.
    // hint: iterate through tags as follows
    for (auto& tag: request.metadata().tags()) {
         // send photo to a model corresponding to tag
    }
    **/
        reply->set_desc("Multiple tags support to be implmemented.");
        throw StatusOK{};
    }
    uint32_t tag = request.metadata().tags(0);
    uint32_t photo_size = request.metadata().photo_size();
    char* photo_data = (char*)malloc(photo_size);
    // 1.2 - read the photo file.
    request.clear_metadata();
    PhotoRequest::PhotoChunkCase (*chunk_case)(PhotoRequest&) =
            [](PhotoRequest& r) { return r.photo_chunk_case(); };
    read_data_arg(request, chunk_case, reader, photo_data, photo_size);
    return ParsedWhatsThisArguments{tag, photo_size, photo_data};
}

ParsedWhatsThisArguments::ParsedWhatsThisArguments(
        const uint32_t tag,
        const uint32_t photo_size,
        char* photo_data) : tag(tag), photo_size(photo_size), photo_data(photo_data) {}

ParsedWhatsThisArguments::ParsedWhatsThisArguments(ParsedWhatsThisArguments&& o)
        : tag(o.tag), photo_size(o.photo_size), photo_data(o.photo_data) {
    o.photo_data = nullptr;
}

ParsedWhatsThisArguments::~ParsedWhatsThisArguments() {
#ifndef NDEBUG
    std::cout << "What's this?" << std::endl;
    std::cout << "tag = " << tag << std::endl;
    std::cout << "photo size = " << photo_size << std::endl;
    std::cout.flush();
#endif  // NDEBUG
    if(photo_data) delete photo_data;
}

void FunctionTier::shutdown() {
    // test if grpc server is started or not
    if(!started) {
        return;
    }
    // lock it
    std::lock_guard<std::mutex> lck(service_mutex);
    if(!started) {
        return;
    }
    // now shutdown the server.
    server->Shutdown();
    server->Wait();
}

FunctionTier::~FunctionTier() { shutdown(); }
};  // namespace sospdemo
