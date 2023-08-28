#include "tcp_sender.hh"

#include "tcp_config.hh"

#include <random>

// Dummy implementation of a TCP sender

// For Lab 3, please replace with a real implementation that passes the
// automated checks run by `make check_lab3`.

template <typename... Targs>
void DUMMY_CODE(Targs &&.../* unused */) {}

using namespace std;

//! \param[in] capacity the capacity of the outgoing byte stream
//! \param[in] retx_timeout the initial amount of time to wait before retransmitting the oldest outstanding segment
//! \param[in] fixed_isn the Initial Sequence Number to use, if set (otherwise uses a random ISN)
TCPSender::TCPSender(const size_t capacity, const uint16_t retx_timeout, const std::optional<WrappingInt32> fixed_isn)
    : _isn(fixed_isn.value_or(WrappingInt32{random_device()()}))
    , _initial_retransmission_timeout{retx_timeout}
    , RTO(retx_timeout)
    , _stream(capacity)
    , received_ack(0)
    , windowSize(1) {}

uint64_t TCPSender::bytes_in_flight() const {
    uint64_t result = 0;
    // bool isFirstSegQualified = true;
    for (TCPSegment segment : _segments_sent_not_acked) {
        // uint64_t segmentSeqno = unwrap(segment.header().seqno, _isn, _next_seqno);
        result += segment.length_in_sequence_space();
        // if (segmentSeqno < _next_seqno) {
        //     if (isFirstSegQualified) {
        //         result += segment.length_in_sequence_space() - (_next_seqno - segmentSeqno);
        //         isFirstSegQualified = false;
        //     } else {
        //         result += segment.length_in_sequence_space();
        //     }
        // }
    }
    return result;
}

void TCPSender::fill_window() {
    TCPSegment sendingSeg;
    if (isSynSent) {
        if (windowSize == 0) {
            // ? I should send a single byte segment
            // ? temporarily leave it
            // sending a useless single Byte segment
            isEmptyWindow = true;
            windowSize = 1;
        }
    }
    // window is not empty (windowSize != 0)
    // if (isSynSent == false) {
    //     if (_stream.buffer_empty()) {
    //         // syn not be sent and _stream is empty : close state and _stream has nothing to send
    //         return;
    //     }
    // }
    // fill in the sendingSeg (to fill the window)
    while (windowSize > bytes_in_flight() && !isFinSent) {
        if (isSynSent == false) {
            // close state but start to send things.
            sendingSeg.header().syn = true;
            isSynSent = true;
        } else {
            // open state and have sent something. This time send something not ended.
            sendingSeg.header().syn = false;
        }

        size_t payloadSize =
            min(windowSize - bytes_in_flight(), min(_stream.buffer_size(), TCPConfig::MAX_PAYLOAD_SIZE));
        sendingSeg.payload() = Buffer(_stream.read(payloadSize));
        if (_stream.input_ended() && _stream.eof() && payloadSize + sendingSeg.header().syn < windowSize && !isFinSent) {
            sendingSeg.header().fin = true;
            isFinSent = true;
        } else {
            sendingSeg.header().fin = false;
        }

        if (sendingSeg.length_in_sequence_space() == 0) {
            break;
        }
        sendingSeg.header().seqno = wrap(_next_seqno, _isn);
        _next_seqno += sendingSeg.header().syn + sendingSeg.header().fin + sendingSeg.payload().size();
        _segments_out.push(sendingSeg);
        _segments_sent_not_acked.push_back(sendingSeg);
    }
    if (isEmptyWindow) {
        windowSize = 0;
    }
}

//! \param ackno The remote receiver's ackno (acknowledgment number)
//! \param window_size The remote receiver's advertised window size
void TCPSender::ack_received(const WrappingInt32 ackno, const uint16_t window_size) {
    if (!isSynSent) {
        return;
    }
    uint64_t ack_64 = unwrap(ackno, _isn, _next_seqno);
    // if ack_64 is invalid
    TCPSegment firstUnackedOutgoingSeg = _segments_sent_not_acked.front();
    if (window_size > 0) {
        isEmptyWindow = false;
    }
    if (ack_64 < unwrap(firstUnackedOutgoingSeg.header().seqno + firstUnackedOutgoingSeg.length_in_sequence_space() - 1, _isn, _next_seqno) || ack_64 > _next_seqno) {
        windowSize = window_size;
        return;
    }
    received_ack = ack_64;
    windowSize = window_size;
    bool isSuccessEraseOutStandingData = false;
    // erase acked segments
    auto iter = _segments_sent_not_acked.begin();
    while (iter != _segments_sent_not_acked.end()) {
        if (ack_64 > unwrap(iter->header().seqno, _isn, received_ack) + iter->length_in_sequence_space() - 1) {
            iter = _segments_sent_not_acked.erase(iter);
            isSuccessEraseOutStandingData = true;
            time_count = 0;
        } else {
            iter++;
        }
    }
    // for (auto iter = _segments_sent_not_acked.begin(); iter != _segments_sent_not_acked.end(); iter++) {
    //     if (ack_64 > unwrap(iter->header().seqno, _isn, received_ack) + iter->length_in_sequence_space() - 1) {
    //         _segments_sent_not_acked.erase(iter);
    //         isSuccessEraseOutStandingData = true;
    //     }
    // }
    if (isSuccessEraseOutStandingData) {
        RTO = _initial_retransmission_timeout;
        retransmission_count = 0;
    }
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void TCPSender::tick(const size_t ms_since_last_tick) {
    time_count += ms_since_last_tick;
    // timer expire
    if (time_count >= RTO) {
        if (!_segments_sent_not_acked.empty()) {
            _segments_out.push(_segments_sent_not_acked.front());
            if (!isEmptyWindow) {
                RTO *= 2;
                ++retransmission_count;
            }
        }
        time_count = 0;
    }
}

unsigned int TCPSender::consecutive_retransmissions() const { return retransmission_count; }

void TCPSender::send_empty_segment() {
    TCPSegment emptySeg;
    emptySeg.header().seqno = wrap(_next_seqno, _isn);
    emptySeg.header().syn = false;
    emptySeg.header().fin = false;
    emptySeg.payload() = {};
    _segments_out.push(emptySeg);
}
