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

static void debug_target_valid(
        derecho::ExternalCaller<CategorizerTier>& categorizer_tier_handler,
        node_id_t target) {
#ifndef NDEBUG
    std::cout << "Got the handle of categorizer tier subgroup. external caller is valid:"
              << categorizer_tier_handler.is_valid() << std::endl;
    std::cout << "p2p_send using target = " << target << std::endl;
    std::cout.flush();
#endif
}

Status FunctionTier::Whatsthis(ServerContext* context,
                               grpc::ServerReader<PhotoRequest>* reader,
                               PhotoReply* reply) {
    ParsedWhatsThisArguments parsed_args;
    try {
        parsed_args = parse_grpc_whatsthis_args(context, reader, reply);
    } catch(const RequestCancel&) {
        return Status::CANCELLED;
    } catch(const StatusOK&) {
        return Status::OK;
    }
    //retrieve subgroup handler for this call
    derecho::ExternalCaller<CategorizerTier>& categorizer_tier_handler
            = group->get_nonmember_subgroup<CategorizerTier>();

    //get list of shards for this subgroup
    auto shards = group->get_subgroup_members<CategorizerTier>();

    //We might want to ask more than one shard to identify our image.
    //We'll store all their responses in this vector
    using response_pair = std::pair<derecho::rpc::QueryResults<Guess>, node_id_t>;
    std::vector<response_pair> responses;
    if(parsed_args.tags.size() > 1) {
        /**
         * TODO: implement support for multiple tags.
         * hint: the else branch of this conditional looks like it'd make a good body for a for-loop.
         * Why not loop over the parsed_args.tags array?
         **/
        reply->set_desc("Multiple tags support to be implemented.");
        return Status::OK;
    } else {
        std::size_t tag_index = 0;
        // 2 - pass it to the categorizer tier.
        node_id_t target = shards[parsed_args.tags[tag_index] % shards.size()][0];
        // TODO: add randomness for load-balancing.                       ^^^^^^^
        debug_target_valid(categorizer_tier_handler, target);  //(just for debugging)
        // 3 - post it to the categorizer tier
        responses.emplace_back(
                categorizer_tier_handler.p2p_send<RPC_NAME(inference)>(
                        target, Photo{parsed_args.tags[tag_index],
                                      parsed_args.photo_data,
                                      parsed_args.photo_size}),
                target);
#ifndef NDEBUG
        std::cout << "p2p_send for inference returned." << std::endl;
        std::cout.flush();
#endif
    }

    //Time to wait for (and process) the responses
    std::vector<Guess> guesses;
    for(auto& result : responses) {
        guesses.emplace_back(result.first.get().get(result.second));
#ifndef NDEBUG
        std::cout << "Received response from the categorizer tier with ret = "
                  << guesses.back().guess << "." << std::endl;
        std::cout.flush();
#endif
    }

    // 4 - return Status::OK;
    std::string reply_string = guesses.at(0).guess;
    for(uint i = 1; i < guesses.size(); ++i) {
        reply_string += " or " + guesses.at(i).guess;
    }

    reply->set_desc(reply_string);

    return Status::OK;
}

Status FunctionTier::InstallModel(grpc::ServerContext* context,
                                  grpc::ServerReader<InstallModelRequest>* reader,
                                  ModelReply* reply) {
    ParsedInstallArguments parsed_args;
    try {
        parsed_args = parse_grpc_install_args(context, reader, reply);
    } catch(const RequestCancel&) {
        return Status::CANCELLED;
    }
    const uint32_t tag = parsed_args.tag;
    const ssize_t synset_size = parsed_args.synset_size;
    const ssize_t symbol_size = parsed_args.symbol_size;
    const ssize_t params_size = parsed_args.params_size;
    const ssize_t data_size = parsed_args.data_size;
    char* model_data = parsed_args.model_data;

    // 2 - find the shard
    // Currently, we use the one-shard implementation.
    auto shards = group->get_subgroup_members<CategorizerTier>();
    node_id_t target = shards[tag % shards.size()][0];
    // TODO: add randomness for load-balancing.

    // 3 - post it to the categorizer tier
    derecho::ExternalCaller<CategorizerTier>& categorizer_tier_handler
            = group->get_nonmember_subgroup<CategorizerTier>();
    BlobWrapper model_data_wrapper(model_data, data_size);
    derecho::rpc::QueryResults<int> result = categorizer_tier_handler.p2p_send<RPC_NAME(install_model)>(
            target, tag, synset_size, symbol_size, params_size, model_data_wrapper);
#ifndef NDEBUG
    std::cout << "p2p_send for install_model returned." << std::endl;
    std::cout.flush();
#endif
    int ret = result.get().get(target);

#ifndef NDEBUG
    std::cout << "Received response from the categorizer tier with ret = "
              << ret << "." << std::endl;
    std::cout.flush();
#endif

    // 4 - return Status::OK;
    reply->set_error_code(ret);
    if(ret == 0)
        reply->set_error_desc("Installed model successfully.");
    else
        reply->set_error_desc("Some error occurred!");

    return Status::OK;
}

Status FunctionTier::RemoveModel(grpc::ServerContext* context,
                                 const RemoveModelRequest* request,
                                 ModelReply* reply) {
    // 1 - get the model tag
    uint32_t tag = request->tag();

    // 2 - find the shard
    // currently, we use the one-shard implementation.
    derecho::ExternalCaller<CategorizerTier>& categorizer_tier_handler
            = group->get_nonmember_subgroup<CategorizerTier>();
    auto shards = group->get_subgroup_members<CategorizerTier>();
    node_id_t target = shards[tag % shards.size()][0];
    // TODO: add randomness for load-balancing.

#ifndef NDEBUG
    std::cout << "Got the handle of categorizer tier subgroup. external caller is valid:"
              << categorizer_tier_handler.is_valid() << std::endl;
    std::cout << "p2p_send using target = " << target << std::endl;
    std::cout.flush();
#endif

    // 3 - post it to the categorizer tier
    derecho::rpc::QueryResults<int> result = categorizer_tier_handler.p2p_send<RPC_NAME(remove_model)>(target, tag);
    int ret = result.get().get(target);

    reply->set_error_code(ret);
    if(ret == 0)
        reply->set_error_desc("Removed model successfully.");
    else
        reply->set_error_desc("Some error occurred!");

    return Status::OK;
}
}  // namespace sospdemo
