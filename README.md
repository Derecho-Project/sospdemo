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
$ [sudo] apt install -y git-lfs
$ git lfs install
$ git clone https://github.com/Derecho-Project/sospdemo
$ cd sospdemo
$ mkdir build;cd build
```
Please note that you need to use the same cmake build mode(`Debug` or `Release`) for this demo as well as Derecho. We will remove this limitation in the next updates of Derecho. Assume you install Derecho with `Debug` mode:
```
$ cmake -DCMAKE_BUILD_TYPE=Debug ..
$ make -j `nproc`
$ cd ..
```
Then, you should see the binary `build/src/sospdemo` is built. This binary includes both the client and server.

## Run the demo
We pre-deployed a demo setup with four Derecho nodes (two function tier nodes and two categorizer tier nodes) running on your local host. The folder `test/n0` to `test/n3` contains the configuration for node id 0~3 respectively. Let's start open four command line terminals and `cd` to each of those configuration folders (Tip: `tmux` or `screen` helps a lot). To start the service, run the following command in each of the terminals:
```
$ ../build/src/sospdemo server
```
This command will start a derecho node in either the function or the categorizer tier, depending on the configuration. Once all nodes started, the group is active and you should see the following outputs.

A function tier node:
```
$ ../../build/src/sospdemo server
[14:53:34.318270] [derecho_debug] [Thread 006855] [info] Derecho library running version 0.9.1 + 159 commits
function tier nodes = 0:127.0.0.1:28001,1:127.0.0.1:28002
server_address=127.0.0.1:28001
FunctionTier listening on 127.0.0.1:28001
Finished constructing derecho group.
Press ENTER to stop.
```
A categorizer tier node:
```
$ ../../build/src/sospdemo server
[14:53:38.882686] [derecho_debug] [Thread 006927] [info] Derecho library running version 0.9.1 + 159 commits
Finished constructing derecho group.
Press ENTER to stop.
```

Now, let's open another terminal and change directory to `test/client` folder. This folder contains a configuration file and a pre-trained flower identification model from MXNet's [example](https://mxnet.apache.org/api/python/docs/tutorials/getting-started/gluon_from_experiment_to_deployment.html). 

Unpack the pre-trained model:
```
$ tar -jxf flower-model.tar.bz2
$ ls flower-model
flower-recognition-0040.params  pic0.ndarray  pic101.ndarray  pic32.ndarray  pic86.ndarray  pic98.ndarray
flower-recognition-symbol.json  pic1.ndarray  pic23.ndarray   pic78.ndarray  pic91.ndarray  synset.txt
```
File `flower-recognition-symbol.json` contains the `ResNet50 V2` model. File `flower-recognition-0040.params` contains the parameters for the model to identify 102 types of flowers. File `synset.txt` contains the name of the 102 flowers, in the order corresponding to the output layer of the model. Files with name 'picx.ndarray' are flower pictures converted into ndarray format.

Since the service we just started does not contain any model at the beginning, let's install the flower model by issuing the following command in the client terminal:
```
$ ../../build/src/sospdemo
Usage:../../build/src/sospdemo <mode> <mode specific args>
The mode could be one of the following:
    client - the web client.
    server - the server node. Configuration file determines if this is a categorizer tier node or a function tier server. 
1) to start a server node:
    ../../build/src/sospdemo server 
2) to perform inference: 
    ../../build/src/sospdemo client inference <tags> <photo>
    tags could be a single tag or multiple tags like 1,2,3,...
3) to install a model: 
    ../../build/src/sospdemo client installmodel <tag> <synset> <symbol> <params>
4) to remove a model: 
    ../../build/src/sospdemo client removemodel <tag>
$ ../../build/src/sospdemo client installmodel 1 flower-model/synset.txt flower-model/flower-recognition-symbol.json flower-model/flower-recognition-0040.params 
Use function tier node: 127.0.0.1:28001
return code:0
description:install model successfully.
```
Please note that it's up to the user which tag to assign to a model.

Now, we can do the inference as following:
```
$ ../../build/src/sospdemo client inference 1 pic91.ndarray
Use function tier node: 127.0.0.1:28001
photo description:tiger lily

```

## Explanation on the code
TODO.
