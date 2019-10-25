#pragma once
#include <derecho-component/blob.hpp>
#include <derecho-component/categorizer_tier.hpp>
#include <derecho-component/function_tier.hpp>
#include <mxnet-component/utils.hpp>

namespace sospdemo {
class RequestCancel {
};

struct ParsedInstallArguments {
    const uint32_t tag;
    const ssize_t synset_size;
    const ssize_t symbol_size;
    const ssize_t params_size;
    const ssize_t data_size;
    char* model_data;
    ParsedInstallArguments(const uint32_t tag, const ssize_t synset_size,
                           const ssize_t symbol_size, const ssize_t params_size,
                           const ssize_t data_size, char* model_data)
            : tag(tag), synset_size(synset_size), symbol_size(symbol_size), params_size(params_size), data_size(data_size), model_data(model_data) {
    }
    ParsedInstallArguments(const ParsedInstallArguments&) = delete;
    ParsedInstallArguments(ParsedInstallArguments&&) = default;
    ~ParsedInstallArguments();
};
ParsedInstallArguments
parse_grpc_install_args(grpc::ServerContext* context,
                        grpc::ServerReader<InstallModelRequest>* reader,
                        ModelReply* reply);
}  // namespace sospdemo