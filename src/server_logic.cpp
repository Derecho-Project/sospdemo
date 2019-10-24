#include <categorizer_tier.hpp>
#include <client_logic.hpp>
#include <derecho/core/derecho.hpp>
#include <fcntl.h>
#include <function_tier.hpp>
#include <iostream>
#include <server_logic.hpp>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <utils.hpp>
#include <vector>

#define CONF_SOSPDEMO_FUNCTION_TIER_MIN_NODES "SOSPDEMO/function_tier_min_nodes"
#define CONF_SOSPDEMO_FUNCTION_TIER_MAX_NODES "SOSPDEMO/function_tier_max_nodes"
#define CONF_SOSPDEMO_CATEGORIZER_TIER_NUM_SHARD                               \
  "SOSPDEMO/categorizer_tier_num_shard"
#define CONF_SOSPDEMO_CATEGORIZER_TIER_MIN_NODES                               \
  "SOSPDEMO/categorizer_tier_min_nodes"
#define CONF_SOSPDEMO_CATEGORIZER_TIER_MAX_NODES                               \
  "SOSPDEMO/categorizer_tier_max_nodes"

/**
 * Start a server node
 */
void do_server(int argc, char **argv) {
  // load configuration
  derecho::Conf::initialize(argc, argv);

  // 1 - create subgroup info using the default subgroup allocator function
  // Both the function tier and the categorizer tier subgroups have one shard,
  // with two members in each shard,respectively.
  uint32_t function_tier_min_nodes =
      derecho::getConfUInt32(CONF_SOSPDEMO_FUNCTION_TIER_MIN_NODES);
  uint32_t function_tier_max_nodes =
      derecho::getConfUInt32(CONF_SOSPDEMO_FUNCTION_TIER_MAX_NODES);
  uint32_t categorizer_tier_num_shard =
      derecho::getConfUInt32(CONF_SOSPDEMO_CATEGORIZER_TIER_NUM_SHARD);
  uint32_t categorizer_tier_min_nodes =
      derecho::getConfUInt32(CONF_SOSPDEMO_CATEGORIZER_TIER_MIN_NODES);
  uint32_t categorizer_tier_max_nodes =
      derecho::getConfUInt32(CONF_SOSPDEMO_CATEGORIZER_TIER_MAX_NODES);

  derecho::SubgroupInfo si{derecho::DefaultSubgroupAllocator(
      {{std::type_index(typeid(sospdemo::FunctionTier)),
        derecho::one_subgroup_policy(derecho::flexible_even_shards(
            1, // number of shards in function tier subgroup
            static_cast<int>(
                function_tier_min_nodes), // minimum number of nodes in the
                                          // single shard of function tier
                                          // subgroup
            static_cast<int>(
                function_tier_max_nodes), // maximum number of nodes in the
                                          // single shard of function tier
                                          // subgroup
            "FUNCTION_TIER"))},
       {std::type_index(typeid(sospdemo::CategorizerTier)),
        derecho::one_subgroup_policy(derecho::flexible_even_shards(
            static_cast<int>(
                categorizer_tier_num_shard), // number of shards in categorizer
                                             // tier subgroup
            static_cast<int>(
                categorizer_tier_min_nodes), // minimum number of nodes in the
                                             // single shard of categorizer tier
                                             // subgroup
            static_cast<int>(
                categorizer_tier_max_nodes), // minimum number of nodes in the
                                             // single shard of categorizer tier
                                             // subgroup
            "CATEGORIZER_TIER"))}})};

  // 2 - prepare factories
  auto function_tier_factory = [](persistent::PersistentRegistry *) {
    return std::make_unique<sospdemo::FunctionTier>();
  };
  auto categorizer_tier_factory = [](persistent::PersistentRegistry *) {
    return std::make_unique<sospdemo::CategorizerTier>();
  };

  // 3 - create the group
  derecho::Group<sospdemo::FunctionTier, sospdemo::CategorizerTier> group(
      {}, si, nullptr, std::vector<derecho::view_upcall_t>{},
      function_tier_factory, categorizer_tier_factory);
  std::cout << "Finished constructing derecho group." << std::endl;

  // 4 - waiting for an input to leave
  std::cout << "Press ENTER to stop." << std::endl;
  std::cin.get();
  group.barrier_sync();
  group.leave(true);
}
