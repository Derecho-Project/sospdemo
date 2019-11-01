# Derecho Demo @SOSP2019 
At SOSP 2019, we ran a tutorial that started with a 90 minute overview of Derecho (find the slides at http://www.cs.cornell.edu/ken/slides), and then concluded with having the attendees download the system, run a prebuilt demo that you can find in this folder, and then make a small change, rebuild the demo, and rerun it.  The intent was that any attendee with a laptop be able to learn to use Derecho on their own laptop, as if the laptop itself was a small cluster of compute nodes.  Obviously one would later move the application to a real cloud or a real cluster, but for teaching purposes, we didn't take that step.  The demo can be downloaded in source form if you already run Linux on your machine, or as a VM that you can run using a virtualization solution such as the Oracle VirtualBox.

This demo shows how to build a Machine Learning application using Derecho. In the toy application, we have a set of clients, which could be drones or robots taking photos in the wild and identifying the plants and animals in the photos. These clients don't have much local knowledge or computation power, so they submit each photo to a cloud service to do the job. We build the cloud service with two Derecho subgroups: the function tier and the categorizer tier. Each node in the function tier runs a [gRPC](https://grpc.io/) service to handle incoming inference requests (i.e. requests to identify what is contained in the photo using a CNN model) from the client. Upon receiving such a request, a function tier node forwards it to a categorizer node with knowledge relevant to that image, using Derecho's `p2p_query`. Image knowledge is represented by many CNN models, each of which handles a specific type of object - there is a flower model, a pets model, and so on. In the categorizer tier subgroup, each shard is responsible for a partition of the models; the nodes within a shard all serve the same set of models. Thus, when a function tier node receives an inference request, it must forward it to the shard (or shards) with the right model for identifying the image, but it can choose any node within that shard to contact.

In this document, we are going to guide you through building the demo and run it with examples. You should be able to build your own application with Derecho afterwards.

We assume your OS is Ubuntu 18.04.

## Install Prerequisites
This demo is built with [Derecho](https://github.com/Derecho-Project/derecho/tree/sospdemo) [gRPC](https://grpc.io/) (for client-server communication), and [MXNet](https://mxnet.apache.org/) (for inference using CNN models).

### 1. Derecho
Please follow the installation instructions on the [Derecho project page](https://github.com/Derecho-Project/derecho/tree/sospdemo), with one addendum: you'll need to be on the `sospdemo` branch.  

### 2. gRPC
We have only tested this demo with gRPC v1.20.0, but it should also work with other gRPC versions. You can install this version with the following commands:
```
$ [sudo] apt install -y libgflags-dev libgtest-dev clang-5.0 libc++-dev build-essential autoconf libtool pkg-config
$ git clone https://github.com/grpc/grpc.git
$ cd grpc
$ git checkout 9dfbd34
$ git submodule update --init
$ git clean -f -d -x && git submodule foreach --recursive git clean -f -d -x
$ make -j `$nproc`
$ [sudo] make install
$ cd third_party/protobuf/
$ [sudo] make install
$ cd ../../..
```
If the above make process crashes, the reason might be that gRPC's build system is a little too aggressive. You can try using fewer CPU cores by calling `make -j` with a smaller number than `nproc` reports.

### 3. MXNet
We use MXNet v1.5.0. Use these commands to install it:
```
$ [sudo] apt install -y libopenblas-dev libopencv-dev python
$ git clone https://github.com/apache/incubator-mxnet.git
$ cd incubator-mxnet
$ git checkout 75a9e18
$ git submodule update --init
$ cmake -DUSE_CUDA=0 -DENABLE_CUDA_RTC=0 -DUSE_JEMALLOC=0 -DUSE_CUDNN=0 -DUSE_MKLDNN=0 -DUSE_CPP_PACKAGE=1
$ make -j `nproc`
$ make install
$ cd cpp-package
$ make
$ make install
$ cd ../..
```

## Build
Once you have all the prerequisites built, building the demo is straight-forward.
```
$ git clone https://github.com/Derecho-Project/sospdemo
$ cd sospdemo
$ mkdir build;cd build
$ cmake -DCMAKE_BUILD_TYPE=Release ..
$ make -j `nproc`
$ cd ..
```
Then, you should see the binary `build/src/sospdemo` is built. This binary includes both the client and server.

## Run the demo
We pre-deployed a demo setup with N Derecho nodes (half function tier nodes and half categorizer tier nodes) running on your local host. The folders `test-N-nodes/n?` contain the configuration for node id 0 through N-1.  We've created this directory structure for 2, 4, and 6 nodes.  As Derecho can be somewhat resource-intensive, we recommend starting with the 2 node case.  Start by opening N terminals and `cd` to each of those configuration folders (tip: `tmux` or `screen` helps a lot). To start the service, run the following command in each of the terminals:
```
$ ../../build/src/sospdemo server
```
This command will start a Derecho node in either the function or the categorizer tier, depending on the configuration. Once all nodes have started, the group is active and you should see the following outputs.

A function tier node:
```
$ ../../build/src/sospdemo server
[22:37:35.915642] [derecho_debug] [Thread 057025] [info] Derecho library running version 0.9.1 + 167 commits
//////////////////////////////////////////
FunctionTier listening on 127.0.0.1:28000
//////////////////////////////////////////
Finished constructing derecho group.
Press ENTER to stop.
```
A categorizer tier node:
```
$ ../../build/src/sospdemo server
[22:37:41.378909] [derecho_debug] [Thread 057039] [info] Derecho library running version 0.9.1 + 167 commits
Finished constructing derecho group.
Press ENTER to stop.
```

Now, let's open another terminal and change directory to `test/client` folder. Let's download our pre-trained models. We created these identification models following MXNet's [example](https://mxnet.apache.org/api/python/docs/tutorials/getting-started/gluon_from_experiment_to_deployment.html).
```
$ wget -c https://derecho.cs.cornell.edu/files/flower-model.tar.bz2
$ wget -c https://derecho.cs.cornell.edu/files/pet-model.tar.bz2
```
Unpack a pre-trained model:
```
$ tar -jxf flower-model.tar.bz2
$ ls flower-model
flower-1.jpg                   flower-4.jpg                   flower-recognition-symbol.json
flower-2.jpg                   flower-5.jpg                   synset.txt
flower-3.jpg                   flower-recognition-0040.params
```
File `flower-recognition-symbol.json` contains the `ResNet50 V2` model. File `flower-recognition-0040.params` contains the parameters for the model to identify 102 types of flowers. File `synset.txt` contains the name of the 102 flowers, in the order corresponding to the output layer of the model. The jpg files are example flower photos we downloaded from google image search.

Since the service we just started does not contain any models at the beginning, let's install the flower model by issuing the following command in the client terminal:
```
$ ../../build/src/sospdemo
Usage:./sospdemo <mode> <mode specific args>
The mode could be one of the following:
    client - the web client.
    server - the server node. Configuration file determines if this is a categorizer tier node or a function tier server. 
1) to start a server node:
    ./sospdemo server 
2) to perform inference: 
    ./sospdemo client <function-tier-node> inference <tags> <photo>
    tags could be a single tag or multiple tags like 1,2,3,...
3) to install a model: 
    ./sospdemo client <function-tier-node> installmodel <tag> <synset> <symbol> <params>
4) to remove a model: 
    ./sospdemo client <function-tier-node> removemodel <tag>
$ ../../build/src/sospdemo client 127.0.0.1:28000 installmodel 1 flower-model/synset.txt flower-model/flower-recognition-symbol.json flower-model/flower-recognition-0040.params 
Use function tier node: 127.0.0.1:28000
return code:0
description:install model successfully.
```
Please note that it's up to the user which tag to assign to a model.

Now, we can do the inference as follows:
```
$ ../../build/src/sospdemo client 127.0.0.1:28000 inference 1 flower-model/flower-1.jpg
Use function tier node: 127.0.0.1:28000
photo description:rose
```
