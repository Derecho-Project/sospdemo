#include <derecho-component/categorizer_tier.hpp>
#include <derecho-component/function_tier.hpp>
#include <derecho-component/server_logic.hpp>
#include <derecho/core/derecho.hpp>
#include <iostream>

/**
 * Start a server node
 */
void do_server(int argc, char** argv) {
    // load configuration
    derecho::Conf::initialize(argc, argv);

    // 1 - create subgroup info using the default subgroup allocator function
    // Both the function tier and the categorizer tier subgroups have configuration
    // profiles that define how many shards they should have and the number of nodes
    // per shard, so we can use the ShardAllocationPolicy constructors that take only
    // a profile name.
    derecho::SubgroupInfo si{derecho::DefaultSubgroupAllocator(
            {{std::type_index(typeid(sospdemo::FunctionTier)),
              derecho::one_subgroup_policy(derecho::flexible_even_shards("FUNCTION_TIER"))},
             {std::type_index(typeid(sospdemo::CategorizerTier)),
              derecho::one_subgroup_policy(derecho::flexible_even_shards("CATEGORIZER_TIER"))}})};

    // 2 - prepare factories for the subgroup objects
    auto function_tier_factory = [](persistent::PersistentRegistry*) {
        return std::make_unique<sospdemo::FunctionTier>();
    };
    auto categorizer_tier_factory = [](persistent::PersistentRegistry*) {
        return std::make_unique<sospdemo::CategorizerTier>();
    };

    // 3 - create the group
    derecho::Group<sospdemo::FunctionTier, sospdemo::CategorizerTier> group(
            si, function_tier_factory, categorizer_tier_factory);
    std::cout << "Finished constructing Derecho group." << std::endl;

    // 4 - block the main thread and wait for keyboard input to shut down
    std::cout << "Press ENTER to stop." << std::endl;
    std::cin.get();
    group.barrier_sync();
    group.leave(true);
}
