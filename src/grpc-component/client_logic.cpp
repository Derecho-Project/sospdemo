#include <derecho-component/categorizer_tier.hpp>
#include <derecho-component/function_tier.hpp>
#include <derecho/core/derecho.hpp>
#include <fcntl.h>
#include <grpc-component/client_logic.hpp>
#include <iostream>
#include <mxnet-component/utils.hpp>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>

/**
 * validate a file for read.
 * @param filename
 * @return file size, negative number for invalid file
 */
inline ssize_t validate_readable_file(const char* filename) {
    struct stat st;

    if(stat(filename, &st) || access(filename, R_OK)) {
        return -1;
    }

    if((S_IFMT & st.st_mode) != S_IFREG) {
        return -2;
    }

    return static_cast<ssize_t>(st.st_size);
}

/**
 * A helper function uploading a file.
 * @param file filename
 * @param length length of the file
 * @param writer the client writer
 * @return number of bytes. Negative number for failure
 */
template <typename RequestType>
ssize_t file_uploader(const std::string& file, ssize_t length,
                      std::unique_ptr<grpc::ClientWriter<RequestType>>& writer) {
    int fd;
    void* file_data;
    const ssize_t chunk_size = (1ll << 15);  // 32K chunking size

    // open and map file
    if((fd = open(file.c_str(), O_RDONLY)) < 0) {
        std::cerr << "Failed to open file(" << file << ") in readonly mode with "
                  << "error:" << strerror(errno) << "." << std::endl;
        return -1;
    }

    if((file_data = mmap(nullptr, length, PROT_READ, MAP_PRIVATE | MAP_POPULATE,
                         fd, 0))
       == MAP_FAILED) {
        std::cerr << "Failed to map file(" << file << ") with "
                  << "error:" << strerror(errno) << "." << std::endl;
        return -2;
    }

    // upload file
    ssize_t offset = 0;
    RequestType request;

    while((length - offset) > 0) {
        ssize_t size = chunk_size;
        if((length - offset) < size) {
            size = length - offset;
        }
        request.set_file_data(
                static_cast<void*>(static_cast<char*>(file_data) + offset), size);
        if(!writer->Write(request)) {
            std::cerr << "failed to upload file(" << file << ") at offset " << offset
                      << "." << std::endl;
            break;
        }
        offset += size;
    }

    // unmap and close file
    if(munmap(file_data, length)) {
        std::cerr << "failed to unmap file(" << file << ") with "
                  << "error:" << strerror(errno) << "." << std::endl;
        return -3;
    }
    if(close(fd)) {
        std::cerr << "failed to close file(" << file << ") with "
                  << "error:" << strerror(errno) << "." << std::endl;
        return -4;
    }

    return offset;
}

/**
 * Send an install model request to a function tier node with gRPC.
 * @param stub_ - gRPC session
 * @param tag - model tag
 * @param synset_file - synset text file name
 * @param symbol_file - symbol json file name
 * @param params_file - parameter file name
 */
void client_install_model(
        std::unique_ptr<sospdemo::FunctionTierService::Stub>& stub_, uint32_t tag,
        const std::string& synset_file, const std::string& symbol_file,
        const std::string& params_file) {
    grpc::ClientContext context;
    sospdemo::ModelReply reply;

    // send metadata
    sospdemo::InstallModelRequest request;
    sospdemo::InstallModelRequest::ModelMetadata metadata;
    metadata.set_tag(tag);
    ssize_t synset_file_size = validate_readable_file(synset_file.c_str());
    if(synset_file_size < 0) {
        std::cerr << "Invalid model synset file: " << synset_file << std::endl;
        return;
    }
    metadata.set_synset_size(static_cast<uint32_t>(synset_file_size));
    ssize_t symbol_file_size = validate_readable_file(symbol_file.c_str());
    if(symbol_file_size < 0) {
        std::cerr << "Invalid model symbol file: " << symbol_file << std::endl;
        return;
    }
    metadata.set_symbol_size(static_cast<uint32_t>(symbol_file_size));
    ssize_t params_file_size = validate_readable_file(params_file.c_str());
    if(params_file_size < 0) {
        std::cerr << "Invalid model params file: " << params_file << std::endl;
        return;
    }
    metadata.set_params_size(static_cast<uint32_t>(params_file_size));
    request.set_allocated_metadata(&metadata);

    std::unique_ptr<grpc::ClientWriter<sospdemo::InstallModelRequest>> writer = stub_->InstallModel(&context, &reply);
    if(!writer->Write(request)) {
        request.release_metadata();
        std::cerr << "Failed to send install model metadata. The stream has been closed." << std::endl;
        return;
    }
    request.release_metadata();

    // send model files
    if(file_uploader(synset_file, synset_file_size, writer) != synset_file_size)
        return;
    if(file_uploader(symbol_file, symbol_file_size, writer) != symbol_file_size)
        return;
    if(file_uploader(params_file, params_file_size, writer) != params_file_size)
        return;

    // Finish up.
    writer->WritesDone();
    grpc::Status status = writer->Finish();

    if(status.ok()) {
        std::cerr << "Return code: " << reply.error_code() << std::endl;
        std::cerr << "Description: " << reply.error_desc() << std::endl;
    } else {
        std::cerr << "grpc::Status::error_code: " << status.error_code()
                  << std::endl;
        std::cerr << "grpc::Status::error_details: " << status.error_details()
                  << std::endl;
        std::cerr << "grpc::Status::error_message: " << status.error_message()
                  << std::endl;
    }

    return;
}

/**
 * Send an inference request to a function tier node with gRPC.
 * @param stub_ - gRPC session
 * @param tag - model tag
 * @param photo_file - photo file name
 */
void client_inference(std::unique_ptr<sospdemo::FunctionTierService::Stub>& stub_,
                      const std::string& tags, const std::string& photo_file) {
    sospdemo::PhotoRequest request;
    sospdemo::PhotoRequest::PhotoMetadata metadata;
    sospdemo::PhotoReply reply;
    grpc::ClientContext context;

    ssize_t photo_file_size = validate_readable_file(photo_file.c_str());
    if(photo_file_size < 0) {
        std::cerr << "Invalid photo file: " << photo_file << std::endl;
        return;
    }

    // 1 - send metadata
    std::string tags_string(tags);
    do {
        metadata.add_tags(static_cast<uint32_t>(std::atoi(tags_string.c_str())));
        size_t pos = tags_string.find(',');
        if(pos == std::string::npos)
            break;
        tags_string.erase(0, pos + 1);
    } while(!tags_string.empty());

    metadata.set_photo_size(photo_file_size);
    request.set_allocated_metadata(&metadata);
    std::unique_ptr<grpc::ClientWriter<sospdemo::PhotoRequest>> writer = stub_->Whatsthis(&context, &reply);
    if(!writer->Write(request)) {
        request.release_metadata();
        std::cerr << "Failed to send inference metadata. The stream has been closed." << std::endl;
        return;
    }
    request.release_metadata();

    // 2 - send photo
    if(file_uploader(photo_file, photo_file_size, writer) != photo_file_size)
        return;

    // 3 - Finish up.
    writer->WritesDone();
    grpc::Status status = writer->Finish();

    if(status.ok()) {
        std::cerr << "Photo description: " << reply.desc() << std::endl;
    } else {
        std::cerr << "grpc::Status::error_code: " << status.error_code()
                  << std::endl;
        std::cerr << "grpc::Status::error_details: " << status.error_details()
                  << std::endl;
        std::cerr << "grpc::Status::error_message: " << status.error_message()
                  << std::endl;
    }

    return;
}

/**
 * Send an remove model request to a function tier node with gRPC.
 * @param stub_ - gRPC session
 * @param tag - model tag
 */
void client_remove_model(
        std::unique_ptr<sospdemo::FunctionTierService::Stub>& stub_, uint32_t tag) {
    grpc::ClientContext context;
    sospdemo::ModelReply reply;

    // send metadata
    sospdemo::RemoveModelRequest request;
    request.set_tag(tag);

    grpc::Status status = stub_->RemoveModel(&context, request, &reply);

    if(status.ok()) {
        std::cerr << "Return code: " << reply.error_code() << std::endl;
        std::cerr << "Description: " << reply.error_desc() << std::endl;
    } else {
        std::cerr << "grpc::Status::error_code: " << status.error_code()
                  << std::endl;
        std::cerr << "grpc::Status::error_details: " << status.error_details()
                  << std::endl;
        std::cerr << "grpc::Status::error_message: " << status.error_message()
                  << std::endl;
    }

    return;
}

/**
 * The "main" method for client programs
 */
void do_client(int argc, char** argv) {
    // briefly check the arguments
    if(argc < 4 || std::string("client").compare(argv[1])) {
        std::cerr << "Invalid command." << std::endl;
        print_help(argv[0]);
        return;
    }

    // load sospdemo configuration
    derecho::Conf::initialize(argc, argv);
    std::string function_tier_node(argv[2]);  // pick this from registry
    std::cout << "Use function tier node: " << function_tier_node << std::endl;

    // prepare gRPC client
    std::unique_ptr<sospdemo::FunctionTierService::Stub> stub_(
            sospdemo::FunctionTierService::NewStub(grpc::CreateChannel(
                    function_tier_node, grpc::InsecureChannelCredentials())));

    // parse the command
    if(std::string("inference").compare(argv[3]) == 0) {
        if(argc < 6) {
            std::cerr << "Invalid inference command." << std::endl;
            print_help(argv[0]);
        } else {
            std::string photo_file(argv[5]);
            client_inference(stub_, std::string(argv[4]), photo_file);
        }
    } else if(std::string("installmodel").compare(argv[3]) == 0) {
        if(argc < 8) {
            std::cerr << "Invalid install model command." << std::endl;
            print_help(argv[0]);
        } else {
            uint32_t tag = static_cast<uint32_t>(std::atoi(argv[4]));
            std::string synset_file(argv[5]);
            std::string symbol_file(argv[6]);
            std::string params_file(argv[7]);
            client_install_model(stub_, tag, synset_file, symbol_file, params_file);
        }
    } else if(std::string("removemodel").compare(argv[3]) == 0) {
        if(argc < 5) {
            std::cerr << "Invalid remove model command." << std::endl;
            print_help(argv[0]);
        } else {
            uint32_t tag = static_cast<uint32_t>(std::atoi(argv[4]));
            client_remove_model(stub_, tag);
        }
    } else {
        std::cerr << "Invalid client command:" << argv[3] << std::endl;
        print_help(argv[0]);
    }
}
