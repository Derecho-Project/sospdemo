#include <derecho-component/categorizer_tier.hpp>
#include <derecho-component/function_tier.hpp>
#include <derecho-component/server_logic.hpp>
#include <derecho/core/derecho.hpp>
#include <fcntl.h>
#include <grpc-component/client_logic.hpp>
#include <iostream>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>

void print_help(const char* cmd) {
    std::cout << "Usage:" << cmd << " <mode> <mode specific args>\n"
              << "The mode could be one of the following:\n"
              << "    client - the web client.\n"
              << "    server - the server node. Configuration file determines if this "
                 "is a categorizer tier node or a function tier node. \n"
              << "1) to start a server node:\n"
              << "    " << cmd << " server \n"
              << "2) to perform inference: \n"
              << "    " << cmd
              << " client <function-tier-node> inference <tags> <photo>\n"
              << "    tags could be a single tag or multiple tags like 1,2,3,...\n"
              << "3) to install a model: \n"
              << "    " << cmd
              << " client <function-tier-node> installmodel <tag> <synset> <symbol> "
                 "<params>\n"
              << "4) to remove a model: \n"
              << "    " << cmd << " client <function-tier-node> removemodel <tag>"
              << std::endl;
}

/**
 * Application: this tester demonstrates how to build an IoT application with
 * Derecho. The IoT application consists of two kinds of nodes: Derecho servers
 * and web clients. The server nodes run either a computation node identifying
 * which object is in an uploaded image or a function tier node that receives
 * requests from clients and dispatches them to one of the computation nodes.
 * A web client sends GRPC-formatted requests to the function-tier server nodes.
 */
int main(int argc, char** argv) {
    if(argc < 2) {
        print_help(argv[0]);
        return -1;
    } else if(std::string(argv[1]) == "client") {
        do_client(argc, argv);
    } else if(std::string(argv[1]) == "server") {
        do_server(argc, argv);
    } else {
        std::cerr << "Unknown mode:" << argv[1] << std::endl;
        print_help(argv[0]);
    }
    return 0;
}
