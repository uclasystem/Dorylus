#include <iostream>
#include <fstream>
#include <vector>
#include <cassert>
#include <unistd.h>
#include "engine/engine.hpp"
#include "utils/utils.hpp"


/**
 *
 * Main entrance of the graph server logic.
 *
 */
int
main(int argc, char *argv[]) {

    // Initialize the engine.
    Engine engine;
    engine.init(argc, argv);

    engine.runForward(true);
    engine.destroy();
/*
    float splitPortion = 1.0 / 3.0;
    unsigned numEpochs = 2;
    unsigned valFreq = 10;

    if (engine.master())
        printLog(engine.getNodeId(), "% Train Data: %f, \
                    number of epochs: %u, validation frequency: %u",
                    splitPortion, numEpochs, valFreq);

    // Use one third of partitions as training and 2/3 as validation
    engine.setTrainValidationSplit(1.0 / 3.0);

    // Do specified number of epochs.
    for (unsigned epoch = 0; epoch < numEpochs; ++epoch) {
        printLog(engine.getNodeId(), "Starting Epoch %u", epoch+1);
        if (epoch != 0 && (epoch % valFreq == 0 || epoch == 1)) {
            if (engine.master())
                printLog(engine.getNodeId(), "Time for some validation");

            // Boolean of whether or not to run evaluation
            engine.runForward(true);

            engine.makeBarrier();

            engine.runBackward();
        } else {
            engine.runForward();

            // Do a backward-prop phase.
            if (engine.isGPUEnabled() == 0)
                engine.runBackward();
        }
    }

    // Procude the output files.
    engine.output();

    // Destroy the engine.
    engine.destroy();
*/
    return 0;
}
