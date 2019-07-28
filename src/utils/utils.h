#ifndef UTILS_H
#define UTILS_H

typedef float DTYPE;
static const int32_t HEADER_SIZE = sizeof(int32_t) * 5;
enum OP { PUSH, PULL, REQ, RESP };

// Serialization functions
template<class T>
void serialize(char* buf, int32_t offset, T val) {
	std::memcpy(buf + (offset * sizeof(T)), &val, sizeof(T));
}


// ID represents either layer or data partition, depending on server responding.
void populateHeader(char* header, int32_t op, int32_t id, int32_t rows = 0,
	int32_t cols = 0) {
	serialize<int32_t>(header, 0, op);
	serialize<int32_t>(header, 1, id);
	serialize<int32_t>(header, 2, rows);
	serialize<int32_t>(header, 3, cols);
}
// End serialization functions


// Logging
void message(int id, std::string msg) {
	printf("[%d] %s\n", id, msg.c_str());
}

void message(std::string id, std::string msg) {
	printf("[%s] %s\n", id.c_str(), msg.c_str());
}

struct Matrix {
    int32_t rows;
    int32_t cols;

    std::unique_ptr<DTYPE[]> data;

    Matrix() { 
        rows = 0;
        cols = 0;
    }

    Matrix(int _rows, int _cols) {
        rows = _rows;
        cols = _cols;
    }

    Matrix(int _rows, int _cols, DTYPE* _data) {
        rows = _rows;
        cols = _cols;
        
        data = std::unique_ptr<DTYPE[]>(_data);
    }

    Matrix(int _rows, int _cols, char* _data) {
        rows = _rows;
        cols = _cols;
        
        data = std::unique_ptr<DTYPE[]>((DTYPE*)_data);
    }

    DTYPE* getData() const { return data.get(); }
	size_t getDataSize() const { return rows * cols * sizeof(DTYPE); }

    void setRows(int32_t _rows) { rows = _rows; }
    void setCols(int32_t _cols) { cols = _cols; }
    void setDims(int32_t _rows, int32_t _cols) { rows = _rows; cols = _cols; }
    void setData(std::unique_ptr<DTYPE[]> _data) { data = std::move(_data); }

    bool empty() { return rows == 0 || cols == 0; }

    std::string shape() {
        return "(" + std::to_string(rows) + "," + std::to_string(cols) + ")";
    }

    std::string str() {
        std::stringstream output;
        output << "Matrix Dims: " << shape() << "\n";
        for (int32_t i = 0; i < rows; ++i) {
            for (int32_t j = 0; j < cols; ++j) {
                output << data[i*cols + j] << " ";
            }
            output << "\n";
        }

        return output.str();
    }
};

struct Timer {
    std::chrono::high_resolution_clock::time_point begin;
    std::chrono::high_resolution_clock::time_point end;

    void start() {
        begin = std::chrono::high_resolution_clock::now();
    }

    void stop() {
        end = std::chrono::high_resolution_clock::now();
    }

    double getTime() {
        std::chrono::duration<double> time_span = std::chrono::duration_cast<std::chrono::duration<double>>(end - begin);

        return time_span.count();
    }
};


#endif
