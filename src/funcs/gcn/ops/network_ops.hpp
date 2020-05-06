#ifndef __NTWK_OPS_HPP__
#define __NTWK_OPS_HPP__

#include <unistd.h>

#include <zmq.hpp>

#include "../../../common/matrix.hpp"
#include "../../../common/utils.hpp"

#include "../utils.hpp"

#define TIMEOUT_PERIOD (2000) // ms

#define SND_MORE true
#define NO_MORE false

#define RESEND false

int recvTensor(zmq::socket_t& socket, Matrix &mat);

std::vector<Matrix> reqTensors(zmq::socket_t& socket, Chunk &chunk,
                            std::vector<std::string>& tensorRequests);

void sendTensors(zmq::socket_t& socket, Chunk &chunk,
    std::vector<Matrix>& matrices, bool ack = false);

void sendAccLoss(zmq::socket_t &socket, Matrix &predicts, Matrix &labels, Chunk &chunk);

void sendFinMsg(zmq::socket_t& socket, Chunk &chunk);

static inline void
populateHeader(void* header, unsigned op, Chunk &chunk) {
    char *ptr = (char *)header;
    memcpy(ptr, &op, sizeof(unsigned));
    memcpy(ptr + sizeof(unsigned), &chunk, sizeof(chunk));
}


#endif
