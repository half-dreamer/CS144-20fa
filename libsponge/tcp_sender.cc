#include "tcp_sender.hh"

#include "tcp_config.hh"

#include <random>

// Dummy implementation of a TCP sender

// For Lab 3, please replace with a real implementation that passes the
// automated checks run by `make check_lab3`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

//! \param[in] capacity the capacity of the outgoing byte stream
//! \param[in] retx_timeout the initial amount of time to wait before retransmitting the oldest outstanding segment
//! \param[in] fixed_isn the Initial Sequence Number to use, if set (otherwise uses a random ISN)
TCPSender::TCPSender(const size_t capacity, const uint16_t retx_timeout, const std::optional<WrappingInt32> fixed_isn)
    : _isn(fixed_isn.value_or(WrappingInt32{random_device()()}))
    , _initial_retransmission_timeout{retx_timeout}
    , _stream(capacity) {}

uint64_t TCPSender::bytes_in_flight() const { 
    uint64_t result = 0;
    bool isFirstSegQualified = true; 
    for (TCPSegment segment : _segments_sent_not_acked) {
        uint64_t segmentSeqno = unwrap(segment.header().seqno, _isn, _next_seqno);
        if (segmentSeqno >= _next_seqno ) {
            if (isFirstSegQualified) {
                result += segment.length_in_sequence_space() - (_next_seqno - segmentSeqno);
                isFirstSegQualified = false;
            } else {
                result += segment.length_in_sequence_space();
            }
        }  
    }
    return result;
}

void TCPSender::fill_window() { 
    TCPSegment sendingSeg; 
    if (windowSize == 0) {
        // ? I should send a single byte segment
        // ? temporarily leave it
    }
    // window is not empty (windowSize != 0)
    if (isSynSent == false) {
        if (_stream.buffer_empty()) {
            // syn not be sent and _stream is empty : close state and _stream has nothing to send
            return;
        } else {
            // close state but start to send things.
            sendingSeg.header().syn = true;
            isSynSent = true;
            while (windowSize != 0) {
                sendingSeg.payload = Buffer()
            }
        }
    }
}


//! \param ackno The remote receiver's ackno (acknowledgment number)
//! \param window_size The remote receiver's advertised window size
void TCPSender::ack_received(const WrappingInt32 ackno, const uint16_t window_size) {
    uint64_t ack_64 = unwrap(ackno, _isn, received_ack);
    // if ack_64 is invalid
    if (ack_64 <= received_ack || ack_64 >= _next_seqno) {
        return;
    }
    received_ack = ack_64;
    windowSize = window_size;
    // erase acked segments
    for (auto iter = _segments_sent_not_acked.begin(); iter != _segments_sent_not_acked.end(); iter++) {
        if (ack_64 > unwrap(iter->header().seqno, _isn, received_ack) + iter->length_in_sequence_space() - 1) {
            _segments_sent_not_acked.erase(iter);
        }
    }
    fill_window();
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void TCPSender::tick(const size_t ms_since_last_tick) { DUMMY_CODE(ms_since_last_tick); }

unsigned int TCPSender::consecutive_retransmissions() const { return {}; }

void TCPSender::send_empty_segment() {}
