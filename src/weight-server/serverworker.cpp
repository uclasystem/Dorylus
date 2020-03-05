#include "serverworker.hpp"


static void nofree(void* data, void* hint) {}

extern std::mutex term_mutex, update_mutex;
extern std::condition_variable cv;
extern bool finished;


/**
 *
 * ServerWorker constructor & destructor.
 *
 */
ServerWorker::ServerWorker(zmq::context_t& ctx_, WeightServer& _ws,
                           std::vector<Matrix>& weights_, std::vector<Matrix>& updates_,
                           std::map<std::string, Matrix>& _weightsStore,
                           unsigned& numLambdas_,unsigned& lambdaRecved_)
    : ctx(ctx_), workersocket(ctx, ZMQ_DEALER), ws(_ws),
      weightMats(weights_), updateMats(updates_),
      weightsStore(_weightsStore),
      numLambdas(numLambdas_),lambdaRecved(lambdaRecved_) {
    workersocket.setsockopt(ZMQ_LINGER, 0);
    workersocket.connect("inproc://backend");
}


ServerWorker::~ServerWorker() {
    std::cout << "Closing sockets" << std::endl;
    workersocket.close();

    std::cout << "Closing context" << std::endl;
    ctx.close();
}


/**
 *
 * Listen on lambda threads' requests.
 *
 */
void
ServerWorker::work() {
    std::cout << "[Weight] Starts listening for lambdas' requests..." << std::endl;
    try {
        while (true) {
            zmq::message_t identity;
            zmq::message_t header;
            workersocket.recv(&identity);
            workersocket.recv(&header);

            OP op = parse<OP>((char *) header.data(), 0);
            unsigned arg = parse<unsigned>((char *) header.data(), 1);

            // std::string accMsg;
            // if (op == OP::PULL_FORWARD)
            //     accMsg = "[ACCEPTED] Pull FORWARD for layer " + std::to_string(arg) + ".";
            // else if (op == OP::PULL_BACKWARD)
            //     accMsg = "[ACCEPTED] Pull BACKWARD from layer " + std::to_string(arg) + ".";
            // else if (op == OP::PUSH_BACKWARD)
            //     accMsg = "[ UPDATE ] Push BACKWARD from layer " + std::to_string(arg) + ".";
            // if (!accMsg.empty())
            //     std::cout << accMsg << std::endl;

            switch (op) {
                case (OP::PULL_FORWARD):
                    sendWeights(identity, arg, true);
                    break;
                case (OP::PULL_BACKWARD):
                    sendWeights(identity, arg, false);
                    break;
                case (OP::PUSH_BACKWARD):
                    recvUpdate(identity, arg);
                    break;
                case (OP::PUSH):
                    recvTensors(identity, arg);
                    break;
                case (OP::PULL):
                    sendTensors(identity);
                    break;
                case (OP::INFO):    // Used to tell how many lambda threads it should expect for this round.
                    setBackpropNumLambdas(identity, arg);
                    break;
                case (OP::TERM):
                    terminateServer(identity);
                    break;
                default:
                    std::cout << "Unknown op, message size: " << identity.size() << " " <<
                    header.size() << std::endl;
                    break;  /** Not an op that I care about. */
            }
        }
    } catch (std::exception& ex) { /** Context Termintated. */ }
}


/**
 *
 * Send weight matrix to lambdas.
 *
 */
void
ServerWorker::sendWeights(zmq::message_t& client_id, unsigned layer, bool forward) {
    if (layer >= weightMats.size()) {
        std::cerr << "[ERROR] No such weights corresponding to layer " << layer << std::endl;
    }

    workersocket.send(client_id, ZMQ_SNDMORE);    // The identity message will be implicitly consumed to route to the correct client.
    if (forward && layer == 0 && ws.servers_updates_done == false) {
        zmq::message_t header(HEADER_SIZE);
        populateHeader((char *) header.data(), OP::RESP, -1);
        workersocket.send(header);
    } else {
        zmq::message_t header(HEADER_SIZE);
        populateHeader((char *) header.data(), OP::RESP, 0, weightMats[layer].getRows(), weightMats[layer].getCols());
        workersocket.send(header, ZMQ_SNDMORE);

        zmq::message_t weightData(weightMats[layer].getDataSize());
        std::memcpy((char *) weightData.data(), weightMats[layer].getData(), weightMats[layer].getDataSize());
        workersocket.send(weightData);
    }
}



/**
 *
 * Receive a given update from a worker. If all udpates have been received, alert the weight server that it is
 * time to average and apply them.
 *
 */
void
ServerWorker::recvUpdate(zmq::message_t& client_id, unsigned layer) {
    zmq::message_t updateMsg;
    workersocket.recv(&updateMsg);

    // Send confirm ACK message.
    zmq::message_t confirm;
    workersocket.send(client_id, ZMQ_SNDMORE);
    workersocket.send(confirm);
    {
        // Grab lock then sum the data received in this update matrix.
        std::lock_guard<std::mutex> update_lock(update_mutex);
        lambdaRecved++;
        // if (numLambdas == lambdaRecved) { ws.servers_updates_done = false; }
        if (ws.servers_updates_done) {
            ws.servers_updates_done = false;
        }

        float *updateSum = updateMats[layer].getData();
        float *updateNew = (float *) updateMsg.data();

        for (unsigned u = 0; u < updateMats[layer].getNumElemts(); ++u)
            updateSum[u] +=  updateNew[u];
    }

    // If this is the final update, begin global aggregation.
    if (numLambdas == lambdaRecved) {
        ws.applyUpdate(layer);
    }
}

/**
 *
 * Update the weightserver with number of lambdas being called for this iteration.
 * Therefore it knows when to average.
 *
 */
void
ServerWorker::setBackpropNumLambdas(zmq::message_t& client_id, unsigned numLambdas_) {
    std::lock_guard<std::mutex> update_lock(update_mutex);
    numLambdas = numLambdas_;
    std::cout << "[  INFO  ] Number of lambdas set to " << numLambdas << "." << std::endl;

    // Send confirm ACK message.
    zmq::message_t confirm;
    workersocket.send(client_id, ZMQ_SNDMORE);
    workersocket.send(confirm);
}


/**
 *
 * After receiving the termination message from the graph server alert
 * the main thread that it can shutdown.
 *
 */
void
ServerWorker::terminateServer(zmq::message_t& client_id) {
    // Send confirm ACK message.
    zmq::message_t confirm;
    workersocket.send(client_id, ZMQ_SNDMORE);
    workersocket.send(confirm);

    std::cerr << "[SHUTDOWN] Server shutting down..." << std::endl;

    std::lock_guard<std::mutex> lk(term_mutex);
    finished = true;
    cv.notify_one();
}


// named-tensors
void ServerWorker::sendTensor(FeatType* dptr, std::string tensorName, unsigned rows,
  unsigned cols, unsigned& more) {
    zmq::message_t responseHeader(TENSOR_HDR_SIZE);
    populateHeader(responseHeader.data(), OP::PULL, tensorName.c_str(),
      rows, cols);
    unsigned bufSize = rows * cols * sizeof(FeatType);
    zmq::message_t tensorData(dptr, bufSize, nofree, NULL);

    workersocket.send(responseHeader, ZMQ_SNDMORE);

    size_t usize = sizeof(unsigned);
    workersocket.getsockopt(ZMQ_RCVMORE, &more, &usize);
    if (!more) {
        workersocket.send(tensorData);
    } else {
        workersocket.send(tensorData, ZMQ_SNDMORE);
    }
}

void ServerWorker::sendTensor(Matrix& tensor, unsigned& more) {
    zmq::message_t responseHeader(TENSOR_HDR_SIZE);
    populateHeader(responseHeader.data(), OP::PULL, tensor.name().c_str(),
      tensor.getRows(), tensor.getCols());
    unsigned bufSize = tensor.getRows() * tensor.getCols() * sizeof(FeatType);
    zmq::message_t tensorData(tensor.getData(), bufSize, nofree, NULL);

    workersocket.send(responseHeader, ZMQ_SNDMORE);

    size_t usize = sizeof(unsigned);
    workersocket.getsockopt(ZMQ_RCVMORE, &more, &usize);
    if (!more) {
        workersocket.send(tensorData);
    } else {
        workersocket.send(tensorData, ZMQ_SNDMORE);
    }
}

//void ServerWorker::getPartitionInfo(Matrix& tensor, unsigned partId, unsigned& more) {
//    unsigned partRows = std::ceil((float) tensor.getRows() / (float) manager->numLambdasForward);
//    unsigned thisPartRows = partRows;
//    if (((partId + 1) * partRows) > tensor.getRows())
//        thisPartRows = partRows - ((partId + 1) * partRows) + tensor.getRows();
//    FeatType* partStart = tensor.getData() + (partId * partRows * tensor.getCols());
//
//    sendTensor(partStart, tensor.name(), thisPartRows, tensor.getCols(), more);
//}

void ServerWorker::sendTensors(zmq::message_t& client_id) {
    unsigned more = 1;
    workersocket.send(client_id, ZMQ_SNDMORE);
    while (more) {
        zmq::message_t tensorHeader(TENSOR_HDR_SIZE);
        workersocket.recv(&tensorHeader);

        std::string name = parseName((char*)tensorHeader.data());
        auto found = weightsStore.find(name);
        if (found == weightsStore.end()) {
            std::cerr << "Requested tensor '" << name << "' not found" << std::endl;
            zmq::message_t errorHeader(TENSOR_HDR_SIZE);
            populateHeader(errorHeader.data(), ERR_HEADER_FIELD, name.c_str());
            workersocket.send(errorHeader);
            return;
        } else {
            std::cout << "Received request for '" << name << "'" << std::endl;
            Matrix& reqMatrix = found->second;
            sendTensor(reqMatrix, more);
        }
    }
}

void ServerWorker::recvUpdateTensor(unsigned layer) {
    zmq::message_t tensorHeader(TENSOR_HDR_SIZE);
    zmq::message_t tensorData;

    workersocket.recv(&tensorHeader);
    workersocket.recv(&tensorData);

    std::string name = parseName((char*)tensorHeader.data());
    if (weightsStore.find(name) == weightsStore.end()) {
        std::cerr << "Pushed tensor '" << name
          << "' not found. Make sure to allocate it before starting workers!" << std::endl;
    } else {
        std::cout << "Received push for '" << name << "'" << std::endl;
    }

    std::lock_guard<std::mutex> update_lock(update_mutex);
    lambdaRecved++;
    FeatType* updateSum = updateMats[layer].getData();
    FeatType* newUpdate = (FeatType*) tensorData.data();
    for (unsigned u = 0; u < updateMats[layer].getNumElemts(); ++u)
        updateSum[u] += newUpdate[u];

    if (numLambdas == lambdaRecved) {
        ws.applyUpdate(layer);
    }
}

void ServerWorker::recvTensors(zmq::message_t& client_id, unsigned layer) {
    unsigned more = 1;
    while (more) {
        recvUpdateTensor(layer);

        size_t usize = sizeof(more);
        workersocket.getsockopt(ZMQ_RCVMORE, &more, &usize);
    }
}
// end named-tensors
