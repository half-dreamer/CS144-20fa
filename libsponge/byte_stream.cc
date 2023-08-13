#include "byte_stream.hh"

// Dummy implementation of a flow-controlled in-memory byte stream.

// For Lab 0, please replace with a real implementation that passes the
// automated checks run by `make check_lab0`.

// You will need to add private members to the class declaration in `byte_stream.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

ByteStream::ByteStream(const size_t cap) : capacity(cap) { ; }

size_t ByteStream::write(const string &data) {
    if (bufferSize < capacity) {
        int bytesToWrite = static_cast<int>(data.size()) > capacity - bufferSize ? capacity - bufferSize : data.size();
        bytesWrittenNum += bytesToWrite;
        bufferSize += bytesToWrite;
        isReachAtEOF = false;
        for (int i = 0; i < bytesToWrite; ++i) {
            byteStream.push_back(data[i]);
        }
        return bytesToWrite;
    }
    return 0;
}

//! \param[in] len bytes will be copied from the output side of the buffer
// under the circumstance that we won't peek out of `bufferSize` bytes.
string ByteStream::peek_output(const size_t len) const {
    string peekOutput{};
    for (int i = 0; i < static_cast<int>(len); ++i) {
        peekOutput.push_back(byteStream.at(i));
    }
    return peekOutput;
}

//! \param[in] len bytes will be removed from the output side of the buffer
void ByteStream::pop_output(const size_t len) { 
    for (int i = 0; i < static_cast<int>(len); ++i) {
        byteStream.pop_front();
    }
    bytesReadNum += len;
    bufferSize -= len;
}

//! Read (i.e., copy and then pop) the next "len" bytes of the stream
//! \param[in] len bytes will be popped and returned
//! \returns a string
std::string ByteStream::read(const size_t len) { 
    string bytesRead{};
    int toReadNum = len;
    if (bufferSize <= static_cast<int>(len)) {
        isReachAtEOF = true;
        toReadNum = bufferSize;
    }
    bytesReadNum += toReadNum;
    bufferSize -= toReadNum;
    for (int i = 0; i < toReadNum; ++i) {
        bytesRead.push_back(byteStream.front());
        byteStream.pop_front();
    }
    return bytesRead;
}

void ByteStream::end_input() { 
    isInputEnded = true; 
}

bool ByteStream::input_ended() const { return isInputEnded; }

size_t ByteStream::buffer_size() const { return bufferSize;}

bool ByteStream::buffer_empty() const { return bufferSize == 0; }

bool ByteStream::eof() const { return bufferSize == 0 && isInputEnded; }

size_t ByteStream::bytes_written() const { return bytesWrittenNum; }

size_t ByteStream::bytes_read() const { return bytesReadNum; }

size_t ByteStream::remaining_capacity() const { return capacity - bufferSize; }
