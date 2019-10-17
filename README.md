# Derecho Demo @SOSP2019 
This demo shows how to build an Machine Learning application using Derecho. In the toy application, we have a set of clients, which could be drones or robots taking photos in the wild and identifying the plants and animals in the photos. But the drones and robots does not have much knowledge as well as the computation power. Therefore, they submit the photo to a cloud service to do the job. We build the cloud service with two derecho subgroups: the function tier and the categorize tier. Each node in the function tier runs a [gRPC](https://grpc.io/) service to handle inference request (identifying what is contained in the photo using a CNN model) from the client. On receiving such a request, the function tier node forwards it to a corresponding node with the specific knowledge through Derecho's `p2p_query`. The knowledge is represented by many CNN models, each of which handle a specific type of objects like a flower model, pets model, and so on. In the categorizer tier subgroup, a shard is responsible for a partition of the models for scalability. The nodes in side a shard serve a same set of models for performance and fault-tolerance.

In this document, we are going to guide you through building the demo and run it with examples. You should be able to build your own application with Derecho afterwards.

We assume your OS is Ubuntu 18.04.

## Install Prerequisites
This demo is built with [Derecho](https://github.com/Derecho-Project/derecho/tree/sospdemo)(of course), [gRPC](https://grpc.io/) (for client-server communication), and [MXNet](https://mxnet.apache.org/) (for inference using CNN models).

### 1. Derecho
Please follow the installation instruction on [Derecho project page](https://github.com/Derecho-Project/derecho/tree/sospdemo).

### 2. gRPC (TODO: please verify, may have some issues.)
We only test it with gRPC v1.20.0, but it should also work with other gRPC versions.
```
$ [sudo] apt install -y libgflags-dev libgtest-dev clang-5.0 libc++-dev build-essential autoconf libtool pkg-config
$ git clone https://github.com/grpc/grpc.git
$ cd grpc
$ git checkout 9dfbd34
$ git submodule update --init
$ git clean -f -d -x && git submodule foreach --recursive git clean -f -d -x
$ make -j `$nproc`
$ [sudo] make install
$ cd ..
```

### 3. MXNet (TODO: please verify, may have some issues)
We use MXNet v1.5.0
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
$ cd ..
```

## Build
Once you have all the prerequisite built, building the demo is straight-forward.
```
$ git clone https://github.com/Derecho-Project/sospdemo
$ cd sospdemo
$ mkdir build;cd build
```
Please note that you need to use the same cmake build mode(`Debug` or `Release`) for this demo as well as Derecho. We will remove this limitation in the next updates of Derecho. Assume you install Derecho with `Debug` mode:
```
$ cmake -DCMAKE_BUILD_TYPE=Debug ..
$ make -j `nproc`
```
Then, you should see the binary `build/src/sospdemo` is built. This binary includes both the client and server.

## Run the demo
We pre-deployed a demo setup with four Derecho nodes (two function tier nodes and two categorizer tier nodes). The folder `test/n0` to `test/n3` contains the configuration for node 0-3. 
