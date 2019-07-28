#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <cassert>
#include <cstdio>
#include <cstring>
#include <vector>
#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string/trim.hpp>

using namespace std;
struct FeaturesHeader{
    unsigned int numFeautures;
};
static FeaturesHeader head;

//todo: verify written file
//todo: add header

void readWriteFile(std::string featuresFileName) {
    std::ifstream infile(featuresFileName.c_str());
    if (!infile.good())
        printf("Cannot open feature file: %s [Reason: %s]\n", featuresFileName.c_str(), std::strerror(errno));
    assert(infile.good());

    std::ofstream bSStream;
    bSStream.open(featuresFileName+".bsnap", std::ios::binary);
    bSStream.write(reinterpret_cast<char*> (&head), sizeof (FeaturesHeader));

    std::string line;

    while (!infile.eof()) {
        std::getline(infile, line);
//        boost::algorithm::trim(line);
        std::vector<std::string> splited_strings;
        std::vector<float> feature_vec;

        // Split each line into numbers.
        boost::split(splited_strings, line, boost::is_any_of(", "), boost::token_compress_on);
        assert(size_t(head.numFeautures)==splited_strings.size());

         for (std::string& substr : splited_strings) {
             float f=std::stof(substr);
//             std::cout<<f<<std::endl;
             bSStream.write(reinterpret_cast<char*> (&f), sizeof (float));

         }
    }
}
void test(std::string featuresFileName){
    cout<<"test\n";

    std::ifstream infile((featuresFileName+".bsnap").c_str());
    if (!infile.good())
        printf("Cannot open feature file: %s [Reason: %s]\n", featuresFileName.c_str(), std::strerror(errno));
    assert(infile.good());
    FeaturesHeader h;
    infile.read(reinterpret_cast<char*> (&h) , sizeof(FeaturesHeader));
    cout<<h.numFeautures<<endl;

    int count=0;
    float curr;
    while (infile.read(reinterpret_cast<char*> (&curr) , sizeof(float))){
        std::cout<<curr<<std::endl;
        count++;
        cout<<"count "<<count<<endl;
    }
}

int main(int argc, char* argv[]) {
    if(argc < 4) {
        std::cout << "Usage: " << argv[0] << " --featurefile=<featurefile> --featurenumber=<number of features> --header=<0|1>" << std::endl;
        return -1;
    }

    std::string featureFile;
    bool withheader=false;
    for(int i=0; i<argc; ++i) {
        if(strncmp("--featurefile=", argv[i], 14) == 0)
            featureFile = argv[i] + 14;
        if(strncmp("--featuresize=", argv[i], 14) == 0) {
            sscanf(argv[i] + 14, "%u", & head.numFeautures);
        }
        if(strncmp("--header=", argv[i], 9) == 0) {
            int hdr = 0;
            sscanf(argv[i] + 9, "%d", &hdr);
            withheader = (hdr == 0 ? false : true);
        }
    }
    assert(head.numFeautures>0);

    if(featureFile.size() == 0) {
        std::cout << "Empty feature file (--featurefile)." << std::endl;
        return -1;
    }
    readWriteFile(featureFile);
    test(featureFile);
    std::cout << "Feature file: " << featureFile << std::endl;
    std::cout << "Feature size: " << head.numFeautures << std::endl;
    std::cout << "Feature header: " << withheader << std::endl;

    return 0;
}
