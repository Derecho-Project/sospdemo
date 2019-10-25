#pragma once
#include <derecho/core/derecho.hpp>
#include <derecho/mutils-serialization/SerializationSupport.hpp>
#include <function_tier.grpc.pb.h>
#include <grpcpp/grpcpp.h>
#include <memory>
#include <mutex>

namespace sospdemo {

#define FUNCTION_TIER_GRPC_PORT_BASE (28000)

/**
 * The front end subgroup type.
 * It appears to do nothing (has no registered RPC methods) since it provides
 * no services to other Derecho nodes.
 */
class FunctionTier : public mutils::ByteRepresentable,
                     public derecho::GroupReference,
                     public FunctionTierService::Service {
protected:
    /**
     * photo tag -> shard number
     * all photos with an unknown tag will be directed to shard 0.
     */
    std::map<uint64_t, ssize_t> tag_to_shard;

    std::atomic<bool> started;
    std::mutex service_mutex;
    std::unique_ptr<grpc::Server> server;

    /**
     * the workhorses
     */
    virtual grpc::Status Whatsthis(grpc::ServerContext* context,
                                   grpc::ServerReader<PhotoRequest>* reader,
                                   PhotoReply* reply) override;
    virtual grpc::Status InstallModel(grpc::ServerContext* context,
                                      grpc::ServerReader<InstallModelRequest>* reader,
                                      ModelReply* reply) override;
    virtual grpc::Status RemoveModel(grpc::ServerContext* context,
                                     const RemoveModelRequest* request,
                                     ModelReply* reply) override;

    /**
     * Start the function tier web service
     */
    virtual void start();
    /**
     * Shutdown the function tier web service
     */
    virtual void shutdown();

public:
    /**
     * Default constructor
     */
    FunctionTier() {
        started = false;
        start();
    }
    /**
     * Constructor that supplies an initial photo tag-to-shard mapping
     * @param rhs The tag-to-shard map to use
     */
    FunctionTier(std::map<uint64_t, ssize_t>& rhs) {
        this->tag_to_shard = std::move(rhs);
        started = false;
        start();
    }
    /**
     * Destructor
     */
    virtual ~FunctionTier();
    /**
     * We don't need to register any Derecho RPC functions for the function tier
     */
    static auto register_functions() { return std::tuple<>(); };

    DEFAULT_SERIALIZATION_SUPPORT(FunctionTier, tag_to_shard);
};

}  // namespace sospdemo
