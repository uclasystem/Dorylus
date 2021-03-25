#ifndef __RESOURCE_COMM_HPP__
#define __RESOURCE_COMM_HPP__
#include <string>
#include "../utils/utils.hpp"
#include "../../common/matrix.hpp"
#include "../parallel/lock.hpp"

enum { LAMBDA, GPU, CPU };

class Engine;

//abstract interface for communicator
class ResourceComm {
public:
    virtual ~ResourceComm() {};

    virtual void setAsync(bool _async, unsigned currEpoch) = 0;

    virtual void NNCompute(Chunk &chunk) = 0;
    virtual void NNSync() = 0;
    // Push result chunks back to queues
    void NNRecvCallback(Engine *engine, bool async, Chunk &chunk);

    virtual void prefetchWeights() {};

    virtual unsigned getRelaunchCnt() { return 0u; };

private:
    void NNRecvCallbackGCN(Engine *engine, bool async, Chunk &chunk);
    void NNRecvCallbackGAT(Engine *engine, bool async, Chunk &chunk);
};

#endif // __RESOURCE_COMM_HPP__
