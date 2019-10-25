#pragma once
#include <derecho-component/function_tier.hpp>

namespace sospdemo {
class RequestCancel {
};
class StatusOK {};

struct ParsedInstallArguments {
    uint32_t tag;
    ssize_t synset_size;
    ssize_t symbol_size;
    ssize_t params_size;
    ssize_t data_size;
    char* model_data;
    ParsedInstallArguments(const uint32_t tag, const ssize_t synset_size,
                           const ssize_t symbol_size, const ssize_t params_size,
                           const ssize_t data_size, char* model_data)
            : tag(tag), synset_size(synset_size), symbol_size(symbol_size), params_size(params_size), data_size(data_size), model_data(model_data) {
    }
    ParsedInstallArguments();
    ParsedInstallArguments(const ParsedInstallArguments&) = delete;
    ParsedInstallArguments(ParsedInstallArguments&&);
    ParsedInstallArguments& operator=(ParsedInstallArguments&&);
    ~ParsedInstallArguments();
};
ParsedInstallArguments parse_grpc_install_args(grpc::ServerContext* context,
                                               grpc::ServerReader<InstallModelRequest>* reader,
                                               ModelReply* reply);

struct ParsedWhatsThisArguments {
    std::vector<uint32_t> tags;
    uint32_t photo_size;
    char* photo_data;
    ParsedWhatsThisArguments(std::vector<uint32_t> tags,
                             const uint32_t photo_size,
                             char* photo_data);
    ParsedWhatsThisArguments();
    ParsedWhatsThisArguments(const ParsedWhatsThisArguments&) = delete;
    ParsedWhatsThisArguments(ParsedWhatsThisArguments&&);
    ParsedWhatsThisArguments& operator=(ParsedWhatsThisArguments&&);
    ~ParsedWhatsThisArguments();
};

ParsedWhatsThisArguments parse_grpc_whatsthis_args(grpc::ServerContext* context,
                                                   grpc::ServerReader<PhotoRequest>* reader,
                                                   PhotoReply* reply);
}  // namespace sospdemo
