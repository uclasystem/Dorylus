#include <iostream>
#include <fstream>
#include <vector>
#include <cassert>
#include "../engine/engine.cpp"


using namespace std;


char tmpDir[256];


/** Define the edge & vertex type. */
typedef float EType;
typedef vector<FeatType> VType;


/** Define the aggregate vertex program. */
template<typename VertexType, typename EdgeType>
class AggregateProgram : public VertexProgram<VertexType, EdgeType> {

public:

    // Define my own update function that to be called in each iteration.
    void update(Vertex<VertexType, EdgeType>& vertex, unsigned layer) {
        VType curr = vertex.data();

        for (unsigned i = 0; i < vertex.getNumInEdges(); ++i) {
            vector<FeatType> other = vertex.getSourceVertexDataAt(i, layer);
            sumVectors(curr, other);
        }

        vertex.addData(curr);   // Push to the back instead of modify the value.
    }

private:

    // Sum up with a neighbors feature value vector.
    void sumVectors(vector<FeatType>& curr, vector<FeatType>& other) {
        assert(curr.size() <= other.size());
        for (int i = 0; i < curr.size(); ++i) {
            curr[i] += other[i];
        }
    }
};


/** Define the writer vertex program for output. */
template<typename VertexType, typename EdgeType>
class WriterProgram : public VertexProgram<VertexType, EdgeType> {

private:

    std::ofstream outFile;

public:

    WriterProgram() {
        char filename[200];
        sprintf(filename, "%s/output_%u", tmpDir, NodeManager::getNodeId()); 
        outFile.open(filename);
    }

    ~WriterProgram() {
        outFile.close();
    }

    // Define the output generated for each vertex.
    void processVertex(Vertex<VertexType, EdgeType>& vertex) {
        std::vector<VType>& data_all = vertex.dataAll();
        outFile << vertex.getGlobalId() << ": ";
        for (int i = 0; i < data_all.size(); ++i) {
            VType curr = data_all[i];
            for (int j = 0; j < curr.size(); ++j)
                outFile << curr[j] << " ";
            outFile << "| ";
        }
        outFile << std::endl;
    }
};


/**
 *
 * Main entrance of the aggregate benchmark.
 * 
 */
int
main(int argc, char *argv[]) {
    init();

    parse(&argc, argv, "--bm-tmpdir=", tmpDir);

    // Initialize the engine.
    VType defaultVertex = vector<FeatType>(2, 1);
    Engine<VType, EType>::init(argc, argv, defaultVertex);

    // Start one run of the engine, on the aggregate program.
    AggregateProgram<VType, EType> aggregateProgram;
    Engine<VType, EType>::run(&aggregateProgram, true);

    // Procude the output files using the writer program.
    WriterProgram<VType, EType> writerProgram;
    Engine<VType, EType>::processAll(&writerProgram);

    // Destroy the engine.
    Engine<VType, EType>::destroy();

    return 0;
}
