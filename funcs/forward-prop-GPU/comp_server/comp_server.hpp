#ifndef __COMP_SERVER_HPP__
#define __COMP_SERVER_HPP__

#include <zmq.hpp>
#include <chrono>
#include <iostream>
#include <fstream>
#include "../comp_unit/comp_unit.hpp"
#include <string>
#include <cstring>
#include <sstream>
#include "../../../src/utils/utils.hpp"
#include <boost/algorithm/string/trim.hpp>
#include <vector>
#include <thread>

void doNotFreeBuffer(void *data, void *hint){
    printf("Buffer is not freed :)\n");
}

class ComputingServer {
public:
    ComputingServer(unsigned dPort_,const std::string& wServersFile,unsigned wPort_):
        dPort(dPort_),
        wPort(wPort_),
        dataSocket(dctx, ZMQ_REP),
        weightSocket(wctx, ZMQ_DEALER){
            loadWeightServers(weightServerAddrs,wServersFile);
            // printf("%zu\n",weightServerAddrs.size() );
            // printf("LOADING WSERVER FILE\n");
            // for(int i=0;i<weightServerAddrs.size();++i){
            //     printf("%s\n",weightServerAddrs[i] );
            // }

    }

    //read weight file 
    void loadWeightServers(std::vector<char *>& addresses, const std::string& wServersFile);

    // Keep listening to computing requests
    void run();

    // Sending and Requesting functions
    Matrix requestWeightsMatrix(zmq::socket_t& socket, unsigned layer);
    Matrix requestFeatsMatrix(unsigned rows,unsigned cols);
    void sendMatrices(Matrix& zResult, Matrix& actResult);


private:
    //ntw related objs
    zmq::context_t dctx;
    zmq::context_t wctx;
    zmq::socket_t dataSocket;
    zmq::socket_t weightSocket;

    std::vector<char*> weightServerAddrs;
    unsigned dPort;
    unsigned wPort;
    unsigned nodeId;

    //GPU part
    ComputingUnit cu;
};


void ComputingServer::run(){

    //use port as ipc addresss
    char ipc_addr[50];
    sprintf(ipc_addr, "ipc:///tmp/GPU_COMM:%u", dPort); 

    std::cout << "Binding computing server to " << ipc_addr << "..." << std::endl;
    dataSocket.bind(ipc_addr);

    // Keeps listening on coord's requests.
    std::cout << "[GPU] Starts listening for GPU requests from DATASERVER..." << std::endl;

    zmq::message_t confirm(5);
    zmq::message_t init_header(HEADER_SIZE);
    dataSocket.recv(&init_header);
    nodeId = parse<unsigned>((char *) init_header.data(), 0);
    dataSocket.send(confirm);
    printf("My nodeId = %u \n", nodeId);

    try {
        while (true) {
            zmq::message_t header(HEADER_SIZE);
            dataSocket.recv(&header);
            unsigned op = parse<unsigned>((char *) header.data(), 0);
            unsigned layer = parse<unsigned>((char *) header.data(), 1);
            unsigned rows = parse<unsigned>((char *) header.data(), 2);
            unsigned cols = parse<unsigned>((char *) header.data(), 3);

            printf("op %u\n", op);
            printf("Layer %u\n", layer);
            printf("Rows %u\n", rows);
            printf("Cols %u\n", cols);
            dataSocket.send(confirm);

            if(op==OP::TERM)
                break;

            Matrix weights;
            std::thread wThread=std::thread([&]{
                unsigned ipc_addr_len=strlen(ipc_addr);
                size_t identity_len = sizeof(unsigned) + ipc_addr_len;
                char identity[identity_len];
                memcpy(identity, (char *) &nodeId, sizeof(unsigned));
                memcpy(identity + sizeof(unsigned), ipc_addr, ipc_addr_len);

                std::cout << "< GPU SERVER FORWARD > Asking weightserver..." << std::endl;
                weightSocket.setsockopt(ZMQ_IDENTITY, identity, identity_len);
                char whost_port[50];
                sprintf(whost_port, "tcp://%s:%u", weightServerAddrs.at(nodeId%weightServerAddrs.size()), wPort);
                printf("connect to %s\n", whost_port);
                weightSocket.connect(whost_port);
                weights=requestWeightsMatrix(weightSocket, layer);
                std::cout << "<  GPU SERVER FORWARD > Got data from weightserver." << std::endl;
            });
            
            Matrix feats=requestFeatsMatrix(rows,cols);
            wThread.join();
            printf("joined\n");
            
            printf("frow: %u, fcol: %u\n", feats.getRows(),feats.getCols());
            printf("feats got %s\n", feats.str().c_str());
            Matrix z = cu.dot(feats, weights);
            printf("Z calculated %s\n", z.str().c_str());

            FeatType act_buffer[z.getRows()*z.getCols()];
            memcpy(act_buffer,z.getData(),z.getDataSize());
            Matrix act_z(z.getRows(),z.getCols(),act_buffer);
            cu.activate(act_z);
            sendMatrices(z,act_z);
        }
    } catch (std::exception& ex) {
        std::cerr << "[ERROR] " << ex.what() << std::endl;
    }
}


//Send multiplied matrix result back to dataserver.
void ComputingServer::sendMatrices(Matrix& zResult, Matrix& actResult) {
        // // Send push header.
        zmq::message_t header(HEADER_SIZE);
        populateHeader((char *) header.data(),zResult.getRows(), zResult.getCols());
        dataSocket.send(header, ZMQ_SNDMORE);

        // // Send zData and actData.
        zmq::message_t zData(zResult.getData(),zResult.getDataSize(),doNotFreeBuffer, NULL);
        dataSocket.send(zData, ZMQ_SNDMORE);
        zmq::message_t actData(actResult.getData(),actResult.getDataSize(),doNotFreeBuffer, NULL);
        dataSocket.send(actData);
        // // Wait for data settled reply.
        // zmq::message_t confirm;
        // socket.recv(&confirm);
}



void ComputingServer::loadWeightServers(std::vector<char *>& addresses, const std::string& wServersFile){
    std::ifstream infile(wServersFile);
    if (!infile.good())
        printf("Cannot open weight server file: %s [Reason: %s]\n", wServersFile.c_str(), std::strerror(errno));

    assert(infile.good());

    std::string line;
    while (!infile.eof()) {
        std::getline(infile, line);
        boost::algorithm::trim(line);

        if (line.length() == 0)
            continue;   
        
        char *addr = strdup(line.c_str());
        addresses.push_back(addr);
    }
}


Matrix
ComputingServer::requestFeatsMatrix(unsigned rows,unsigned cols) {
    zmq::message_t confirm(5);
    printf("rfRow: %u rfCol: %u\n", rows,cols);
    zmq::message_t aggreChunk;
    std::cout << "< GPU SERVER FORWARD  > Getting data from dataserver..." << std::endl;
    dataSocket.recv(&aggreChunk);
    printf("Feats size recved %u\n", aggreChunk.size());
    std::cout << "< GPU SERVER FORWARD > Got data from dataserver." << std::endl;
    FeatType * feats_buffer=new FeatType[rows * cols];
    printf("rf memcpy start\n");
    for (unsigned i=0;i<rows * cols;++i){ 
        ((FeatType*)(aggreChunk.data()))[i]+=1;
        // printf("%u\n",i );
        // printf("%f\n",((FeatType*)(aggreChunk.data()))[i] );
    }
    memcpy((char*)feats_buffer,(char*)aggreChunk.data(),rows * cols * sizeof(FeatType));
    printf("rf memcpy done\n");
    Matrix m(rows, cols,feats_buffer);
    return m;
}

/**
 *
 * Request the input matrix data from weightserver.
 * 
 */
//TODO: Can be modify to zerocopy
Matrix
ComputingServer::requestWeightsMatrix(zmq::socket_t& socket, unsigned layer) {
    
    // Send pull request.
    zmq::message_t header(HEADER_SIZE);
    populateHeader((char *) header.data(), OP::PULL_FORWARD, layer);
    socket.send(header);

    // Listen on respond.
    zmq::message_t respHeader(HEADER_SIZE);
    socket.recv(&respHeader);

    // Parse the respond.
    unsigned layerResp = parse<unsigned>((char *) respHeader.data(), 1);
    if ((int)layerResp == -1) {      // Failed.
        std::cerr << "[ ERROR ] No corresponding matrix chunk!" << std::endl;
        return Matrix();
    } else {                    // Get matrices data.
        unsigned rows = parse<unsigned>((char *) respHeader.data(), 2);
        unsigned cols = parse<unsigned>((char *) respHeader.data(), 3);
        zmq::message_t wData(rows * cols * sizeof(FeatType));
        socket.recv(&wData);
        FeatType *wBuffer = new FeatType[rows*cols];
        printf("rw memcpy start\n");
        memcpy((char*)wBuffer,(char*)wData.data(),rows * cols * sizeof(FeatType));
        printf("rw memcpy done\n");
        Matrix m(rows, cols, wBuffer);
        return m;
    }
}

#endif 
