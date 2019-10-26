#ifndef __LAMBDA_COMM_HPP__
#define __LAMBDA_COMM_HPP__


#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <cmath>
#include <fstream>
#include <functional>
#include <iostream>
#include <mutex>
#include <random>
#include <sstream>
#include <climits>
#include <string>
#include <thread>
#include <vector>
#include <zmq.hpp>

#include "resource_comm.hpp"
#include "lambdaworker.hpp"
#include "../utils/utils.hpp"
#include "../../common/matrix.hpp"
#include "../../common/utils.hpp"


#define SLEEP_PERIOD 5000 // sleep 5000us and then check the condition.
#define TIMEOUT_PERIOD 5000.0 // lambda is considered timed out after 5000ms.


class LambdaWorker;

/**
 *
 * Class of a lambda threads communication handler.
 *
 */
class LambdaComm : public ResourceComm {

public:

    LambdaComm(std::string nodeIp_, unsigned dataserverPort_, std::string coordserverIp_, unsigned coordserverPort_, unsigned nodeId_,
               unsigned numLambdasForward_, unsigned numLambdasBackward_);
    ~LambdaComm();

    void setTrainValidationSplit(float trainPortion, unsigned numLocalVertices);

    // For forward-prop.
    void newContextForward(FeatType *dataBuf, FeatType *zData,
        FeatType *actData, unsigned numLocalVertices, unsigned numFeats,
        unsigned numFeatsNext, bool eval);
    void requestForward(unsigned layer, bool lastLayer);
    void invokeLambdaForward(unsigned layer, unsigned lambdaId, bool lastLayer);
    void waitLambdaForward(unsigned layer, bool lastLayer);

    // For backward-prop.
    void newContextBackward(FeatType **zBufs, FeatType **actBufs, FeatType *targetBuf,
                            unsigned numLocalVertices, std::vector<unsigned> layerConfig);
    void newContextBackward(FeatType *oldGradBuf, FeatType *newGradBuf, std::vector<Matrix> *savedTensors, FeatType *targetBuf,
                            unsigned numLocalVertices, unsigned inFeatDim, unsigned outFeatDim, unsigned targetDim);
    void requestBackward(unsigned layer, bool lastLayer);
    void invokeLambdaBackward(unsigned layer, unsigned lambdaId, bool lastLayer);
    void waitLambdaBackward(unsigned layer, bool lastLayer);

    // Send a message to the coordination server to shutdown.
    void sendShutdownMessage();

    // simple LambdaWorker initialization
    friend LambdaWorker::LambdaWorker(LambdaComm *manager);

// private:
    unsigned numLambdasForward;
    unsigned numLambdasBackward;

    bool evaluate;
    std::vector<bool> trainPartitions;

    unsigned numListeners;

    unsigned countForward;
    bool *forwardLambdaTable;
    double forwardTimer;
    unsigned countBackward;
    bool *backwardLambdaTable;
    double backwardTimer;

    unsigned numCorrectPredictions;
    float totalLoss;
    unsigned numValidationVertices;
    unsigned evalPartitions;

    zmq::context_t ctx;
    zmq::socket_t frontend;
    zmq::socket_t backend;
    zmq::socket_t coordsocket;

    unsigned nodeId;
    std::string nodeIp;
    unsigned dataserverPort;

    std::string coordserverIp;
    unsigned coordserverPort;
};


#endif // LAMBDA_COMM_HPP
