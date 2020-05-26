#include <algorithm>
#include <cassert>
#include <chrono>
#include <ratio>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <thread>
#include <cmath>

#include <cblas.h>
#include <zmq.hpp>

#include <aws/lambda-runtime/runtime.h>
#include <aws/core/Aws.h>
#include <aws/core/utils/json/JsonSerializer.h>

#include "utils.hpp"
#include "../../common/matrix.hpp"
#include "../../common/utils.hpp"
#include "ops/forward_ops.hpp"
#include "ops/backward_ops.hpp"
#include "ops/network_ops.hpp"


using namespace Aws::Utils::Json;
using namespace aws::lambda_runtime;
using namespace std::chrono;


invocation_response
apply_edge(zmq::socket_t& data_socket, zmq::socket_t& weights_socket, Chunk &chunk) {
    std::cout << "FORWARD APPLY EDGE LAYER " << chunk.layer << std::endl;

    unsigned startRequest = timestamp_ms();
    EdgeInfo eInfo = reqEdgeInfo(data_socket, chunk);
    unsigned endRequest = timestamp_ms();
    if (eInfo.numLvids == NOT_FOUND_ERR_FIELD) {
        std::cerr << "Tensor 'fedge' was not found on graph server" << std::endl;
        return constructResp(false, chunk.localId, "Tensor 'fedge' not found");
    } else if (eInfo.numLvids == DUPLICATE_REQ_ERR_FIELD) {
        std::cerr << "Chunk already running. Request rejected" << std::endl;
        return constructResp(false, chunk.localId, "Duplicate chunk request");
    } else if (eInfo.numLvids == CHUNK_DNE_ERR) {
        std::cerr << "Chunk not found on graph server" << std::endl;
        return constructResp(false, chunk.localId, "Chunk not found");
    } else if (eInfo.numLvids == ERR_HEADER_FIELD) {
        std::cerr << "Prorably a null pointer? I dunno" << std::endl;
        return constructResp(false, chunk.localId, "Got an error");
    }
    std::cout << "GOT E TENSOR" << std::endl;
    std::cout << "EDGE INFO: " << eInfo.numLvids << ", " << eInfo.nChunkEdges << std::endl;

    std::vector<std::string> dataRequests{"z"};
    std::vector<Matrix> matrices = reqTensors(data_socket, chunk, dataRequests);
    for (auto& M : matrices) {
        if (M.empty()){
            std::cout << M.name() << " is empty" << std::endl;
            return constructResp(false, chunk.localId, M.name() + " is empty");
        }
    }
    std::cout << "GOT DATA" << std::endl;

    std::vector<std::string> weightRequests{"a_i"};
    std::vector<Matrix> weights = reqTensors(weights_socket, chunk, weightRequests);
    for (auto& W : weights) {
        if (W.empty()){
            std::cout << W.name() << " is empty" << std::endl;
            return constructResp(false, chunk.localId, W.name() + " is empty");
        }
    }
    std::cout << "GOT WEIGHTS" << std::endl;

    if (matrices.empty() || weights.empty()) {
        return constructResp(false, chunk.localId, "Got error message from server");
    }

    Matrix& Z = matrices[0];
    Matrix& a = weights[0];

    std::cout << Z.shape() << std::endl;
    std::cout << a.shape() << std::endl;

    Matrix edgeValInputs = edgeMatMul(eInfo, Z, a);
    deleteMatrix(Z);
    deleteMatrix(a);
    deleteEdgeInfo(eInfo);
    edgeValInputs.setName("az");

    Matrix edgeVals = leakyReLU(edgeValInputs);
    edgeVals.setName("A");

    std::vector<Matrix> toSend;
    toSend.push_back(edgeVals);

    int ret = sendTensors(data_socket, chunk, toSend, true);

    for (auto& M : toSend)
        deleteMatrix(M);

    return constructResp(true, chunk.localId, "Finished apply edge");
}

invocation_response
apply_edge_backward(zmq::socket_t& data_socket, zmq::socket_t& weights_socket, Chunk &chunk) {
    std::cout << "BACKWARD APPLY EDGE LAYER " << chunk.layer << std::endl;

    unsigned startRequest = timestamp_ms();
    EdgeInfo eInfo = reqEdgeInfo(data_socket, chunk);
    unsigned endRequest = timestamp_ms();
    if (eInfo.numLvids == NOT_FOUND_ERR_FIELD) {
        std::cerr << "Tensor 'fedge' was not found on graph server" << std::endl;
        return constructResp(false, chunk.localId, "Tensor 'fedge' not found");
    } else if (eInfo.numLvids == DUPLICATE_REQ_ERR_FIELD) {
        std::cerr << "Chunk already running. Request rejected" << std::endl;
        return constructResp(false, chunk.localId, "Duplicate chunk request");
    } else if (eInfo.numLvids == CHUNK_DNE_ERR) {
        std::cerr << "Chunk not found on graph server" << std::endl;
        return constructResp(false, chunk.localId, "Chunk not found");
    } else if (eInfo.numLvids == ERR_HEADER_FIELD) {
        std::cerr << "Prorably a null pointer? I dunno" << std::endl;
        return constructResp(false, chunk.localId, "Got an error");
    }

    std::cout << "EDGE INFO: " << eInfo.numLvids << ", " << eInfo.nChunkEdges << std::endl;

    Matrix aZ = reqEdgeTensor(data_socket, chunk, "az");

    std::vector<std::string> dataRequests{"z", "grad"};
    std::vector<Matrix> matrices = reqTensors(data_socket, chunk, dataRequests);
    Matrix& Z = matrices[0];
    Matrix& dP = matrices[1];

    std::cout << "Z: " << Z.shape() << std::endl;
    std::cout << "dP: " << dP.shape() << std::endl;
    std::cout << "aZ: " << aZ.shape() << std::endl;

    std::vector<std::string> weightRequests{"a_i"};
    std::vector<Matrix> weights = reqTensors(weights_socket, chunk, weightRequests);
    Matrix& a = weights[0];
    std::cout << "a: " << a.shape() << std::endl;

    return constructResp(true, chunk.localId, "Finished apply edge backward");
}

invocation_response
apply_vertex(zmq::socket_t& data_socket, zmq::socket_t& weights_socket, Chunk &chunk) {
    std::cout << "FORWARD APPLY VERTEX LAYER " << chunk.layer << std::endl;

    std::vector<Matrix> matrices;
    // Request H directly
    std::vector<std::string> dataRequests{"h"};
    matrices = reqTensors(data_socket, chunk, dataRequests);
    for (auto& M : matrices) {
        if (M.empty()){
            std::cout << M.name() << " is empty" << std::endl;
            return constructResp(false, chunk.localId, M.name() + " is empty");
        }
    }

    Matrix& H = matrices[0];
    std::vector<std::string> weightRequests{"w"};
    std::vector<Matrix> weights = reqTensors(weights_socket, chunk, weightRequests);
    for (auto& W : weights) {
        if (W.empty()){
            std::cout << W.name() << " is empty" << std::endl;
            return constructResp(false, chunk.localId, W.name() + " is empty");
        }
    }

    if (matrices.empty() || weights.empty()) {
        return constructResp(false, chunk.localId, "Got error message from server");
    }

    Matrix& W = weights[0];
    std::cout << "SUM H: " << H.sum() << std::endl;
    std::cout << "SUM W: " << W.sum() << std::endl;

    Matrix Z = H.dot(W);
    Z.setName("z");
    deleteMatrix(H);
    deleteMatrix(W);

    std::cout << "SUM Z: " << Z.sum() << std::endl;

    std::vector<Matrix> toSend;
    toSend.push_back(Z);

    std::cout << "Sending Z tensor" << std::endl;
    int ret = sendTensors(data_socket, chunk, toSend, true);
    std::cout << "Fin send" << std::endl;

    for (auto& M : toSend)
        deleteMatrix(M);

    std::cout << "Data cleaned up" << std::endl;
    if (ret == -1) {
        return constructResp(false, chunk.localId, "This chunk is already done.");
    } else {
        return constructResp(true, chunk.localId, "Finished forward layer");
    }
    return constructResp(false, chunk.localId, "This chunk is already done.");
}

invocation_response
apply_vertex_backward(zmq::socket_t& data_socket, zmq::socket_t& weights_socket, Chunk &chunk) {
    std::cout << "BACKWARD APPLY VERTEX LAYER " << chunk.layer << std::endl;

    Matrix H;
    std::vector<Matrix> matrices;
    if (chunk.isFirstLayer()) {
        // Request H directly
        std::vector<std::string> dataRequests{"h"};
        matrices = reqTensors(data_socket, chunk, dataRequests);
        for (auto& M : matrices) {
            if (M.empty()){
                std::cout << M.name() << " is empty" << std::endl;
                return constructResp(false, chunk.localId, M.name() + " is empty");
            }
        }

        H = matrices[0];
    } else {
        // Request AH and compute H as tanh(AH)
        std::vector<std::string> dataRequests{"ah"};
        matrices = reqTensors(data_socket, chunk, dataRequests);
        for (auto& M : matrices) {
            if (M.empty()){
                std::cout << M.name() << " is empty" << std::endl;
                return constructResp(false, chunk.localId, M.name() + " is empty");
            }
        }

        Matrix& AH = matrices[0];
        H = tanh(AH);
        deleteMatrix(AH);
    }

    std::vector<std::string> weightRequests{"w", "a_i", "a_j"};
    std::cerr << "Request w, a_i, a_j" << std::endl;
    std::vector<Matrix> weights = reqTensors(weights_socket, chunk, weightRequests);
    for (auto& W : weights) {
        if (W.empty()){
            std::cout << W.name() << " is empty" << std::endl;
            return constructResp(false, chunk.localId, W.name() + " is empty");
        }
    }

    if (matrices.empty() || weights.empty()) {
        return constructResp(false, chunk.localId, "Got error message from server");
    }

    Matrix& W = weights[0];
    std::cout << "SUM H: " << H.sum() << std::endl;
    std::cout << "SUM W: " << W.sum() << std::endl;

    Matrix Z = H.dot(W);
    Z.setName("z");
    deleteMatrix(H);
    deleteMatrix(W);

    Matrix& a_i = weights[1];
    Matrix& a_j = weights[2];

    std::cout << "SUM a_i: " << a_i.sum() << std::endl;
    std::cout << "SUM a_j: " << a_j.sum() << std::endl;

    Matrix az_i = Z.dot(a_i);
    az_i.setName("az_i");
    deleteMatrix(a_i);

    Matrix az_j = Z.dot(a_j);
    az_j.setName("az_j");
    deleteMatrix(a_j);

    std::cout << "SUM Z: " << Z.sum() << std::endl;
    std::cout << "SUM az_i: " << az_i.sum() << std::endl;
    std::cout << "SUM az_j: " << az_j.sum() << std::endl;

    std::vector<Matrix> toSend;
    toSend.push_back(Z);
    toSend.push_back(az_i);
    toSend.push_back(az_j);

    std::cout << "Sending Z tensor" << std::endl;
    int ret = sendTensors(data_socket, chunk, toSend, true);
    std::cout << "Fin send" << std::endl;

    for (auto& M : toSend)
        deleteMatrix(M);

    std::cout << "Data cleaned up" << std::endl;
    if (ret == -1) {
        return constructResp(false, chunk.localId, "This chunk is already done.");
    } else {
        return constructResp(true, chunk.localId, "Finished forward layer");
    }
    return constructResp(false, chunk.localId, "This chunk is already done.");
}
/**
 *
 * Main logic:
 *
 *      1. Querying matrix data from dataserver;
 *      2. Querying weight matrix from weightserver;
 *      3. Conduct the matrix multiplication to get Z matrix;
 *      4. Perform activation on Z to get Activated matrix;
 *      5. Send both matrices back to data server.
 *      6. If evaluate is true, check the model precision
 *
 */
invocation_response
apply_phase(std::string dataserver, std::string weightserver, unsigned dport, unsigned wport, Chunk &chunk, bool eval) {
    zmq::context_t ctx(2);

    // Creating identity
    size_t identity_len = sizeof(unsigned) * 3 + dataserver.length();
    char identity[identity_len];
    memcpy(identity, (char *) &chunk.localId, sizeof(unsigned));
    std::srand(time(NULL));
    *(unsigned *)(identity + sizeof(unsigned)) = chunk.layer;
    *(unsigned *)(identity + sizeof(unsigned) * 2) = rand();
    memcpy(identity + sizeof(unsigned) * 3, (char *) dataserver.c_str(), dataserver.length());

    zmq::socket_t weights_socket(ctx, ZMQ_DEALER);
    zmq::socket_t data_socket(ctx, ZMQ_DEALER);
    std::cout << "Setting up comms" << std::endl;
    try {
        weights_socket.setsockopt(ZMQ_IDENTITY, identity, identity_len);
        if (RESEND) {
            weights_socket.setsockopt(ZMQ_RCVTIMEO, TIMEOUT_PERIOD);
        }
        char whost_port[50];
        sprintf(whost_port, "tcp://%s:%u", weightserver.c_str(), wport);
        weights_socket.connect(whost_port);

        data_socket.setsockopt(ZMQ_IDENTITY, identity, identity_len);
        if (RESEND) {
            data_socket.setsockopt(ZMQ_RCVTIMEO, TIMEOUT_PERIOD);
        }
        char dhost_port[50];
        sprintf(dhost_port, "tcp://%s:%u", dataserver.c_str(), dport);
        data_socket.connect(dhost_port);
    } catch(std::exception& ex) {
        return constructResp(false, chunk.localId, ex.what());
    }
    std::cout << "Finished comm setup" << std::endl;

    std::cout << chunk.str() << std::endl;
    if (chunk.vertex != 0 && chunk.dir == PROP_TYPE::FORWARD) {
        return apply_vertex(data_socket, weights_socket, chunk);
    } else if (chunk.vertex != 0 && chunk.dir == PROP_TYPE::BACKWARD) {
        return apply_vertex_backward(data_socket, weights_socket, chunk);
    } else if (chunk.vertex == 0 && chunk.dir == PROP_TYPE::FORWARD) {
        return apply_edge(data_socket, weights_socket, chunk);
    } else {
        return apply_edge_backward(data_socket, weights_socket, chunk);
    }

    std::cout << "Returning from function" << std::endl;

    weights_socket.setsockopt(ZMQ_LINGER, 0);
    weights_socket.close();
    data_socket.setsockopt(ZMQ_LINGER, 0);
    data_socket.close();
    ctx.close();

    return constructResp(false, chunk.localId, "Didn't run any config");
}


/** Handler that hooks with lambda API. */
invocation_response
my_handler(invocation_request const& request) {
    JsonValue json(request.payload);
    auto v = json.View();

    std::string dataserver = v.GetString("dserver");
    std::string weightserver = v.GetString("wserver");
    unsigned dport = v.GetInteger("dport");
    unsigned wport = v.GetInteger("wport");
    bool eval = v.GetBool("eval");

    Chunk chunk;
    chunk.localId = v.GetInteger("id");
    chunk.globalId = v.GetInteger("gid");
    chunk.lowBound = v.GetInteger("lb");
    chunk.upBound = v.GetInteger("ub");
    chunk.layer = v.GetInteger("layer");
    chunk.dir = static_cast<PROP_TYPE>(v.GetInteger("dir"));
    chunk.epoch = v.GetInteger("epoch");
    chunk.vertex = v.GetInteger("vtx");

    std::cout << "[ACCEPTED] Thread " << chunk.str() << " is requested from "
              << dataserver << ":" << dport << ", FORWARD layer " << chunk.layer
              << " " << chunk.vertex << std::endl;

    return apply_phase(dataserver, weightserver, dport, wport, chunk, eval);
}

int
main(int argc, char *argv[]) {
    run_handler(my_handler);

    return 0;
}
