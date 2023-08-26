#include "tcp_connection.hh"

#include <iostream>

// Dummy implementation of a TCP connection

// For Lab 4, please replace with a real implementation that passes the
// automated checks run by `make check`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

size_t TCPConnection::remaining_outbound_capacity() const { return _sender.stream_in().remaining_capacity();
 }

size_t TCPConnection::bytes_in_flight() const { return _sender.bytes_in_flight(); }

size_t TCPConnection::unassembled_bytes() const { return _receiver.unassembled_bytes(); }

size_t TCPConnection::time_since_last_segment_received() const { return time_count; }

void TCPConnection::segment_received(const TCPSegment &seg) {
    time_count = 0;
    toSendEmptySeg = seg.length_in_sequence_space(); // if seg occupy any sequence number(including syn and fin)
    // (but exclude any another segment flags, so we use `length_in_sequence_space()`), we need to send empty segment

    if (isSynSent == false) {
        if (seg.header().syn == false) {
            return;
        } else {
            connect();
        }
    }

    if (seg.header().rst == true) {
        _sender.stream_in().set_error();
        _receiver.stream_out().set_error();
        isActive = false;
        // ! I don't know how to "kill the connection permantly"
        return;
    }
    _receiver.segment_received(seg); 


    if (seg.header().ack == true) {
        _sender.ack_received(seg.header().ackno, seg.header().win);
        _sender.fill_window();
        // if any segment will be sent, then empty segment shouldn't be sent
        if (!_sender.segments_out().empty()) {
            toSendEmptySeg = false;
        }
    }



    if (seg.header().fin == true) {
        if (_sender.getIsFinSent() == false) {
            _linger_after_streams_finish = false;
        }
    }

    if (TCPState::state_summary(_receiver) == TCPReceiverStateSummary::FIN_RECV && TCPState::state_summary(_sender) == TCPSenderStateSummary::FIN_ACKED && _linger_after_streams_finish == false) {
        isActive = false;
    }

    if (toSendEmptySeg) {
        _sender.send_empty_segment();
    }
    sendSegToRealOut();
}

bool TCPConnection::active() const { return isActive; }

size_t TCPConnection::write(const string &data) { 
    size_t writtenBytes = _sender.stream_in().write(data);
    _sender.fill_window();
    sendSegToRealOut();
    return writtenBytes;
}

//! \param[in] ms_since_last_tick number of milliseconds since the last call to this method
void TCPConnection::tick(const size_t ms_since_last_tick) { 
    time_count += ms_since_last_tick;
    _sender.tick(ms_since_last_tick);
    sendSegToRealOut();
    if (_sender.consecutive_retransmissions() > TCPConfig::MAX_RETX_ATTEMPTS) {
        // ? need to be done : abort the connection

        // send a reset segment(an empty segment with RST flag set) to the peer
        TCPSegment resetSegment{};
        resetSegment.header().rst = true;
        _segments_out.push(resetSegment);
    }
    // ? need to be done : end the connection cleanly
    if (_linger_after_streams_finish) {
        if (time_count >= 10 * _cfg.rt_timeout) {
            if (TCPState::state_summary(_sender) == TCPSenderStateSummary::FIN_ACKED && TCPState::state_summary(_receiver) == TCPReceiverStateSummary::FIN_RECV) {
                isActive = false;
            }
        }
    }

}

void TCPConnection::end_input_stream() { 
    _sender.stream_in().end_input();
    _sender.fill_window();
    sendSegToRealOut();
}

void TCPConnection::connect() { 
    _sender.fill_window();
    isActive = true;
    isSynSent = true;
    sendSegToRealOut();
}

TCPConnection::~TCPConnection() {
    try {
        if (active()) {
            cerr << "Warning: Unclean shutdown of TCPConnection\n";

            // Your code here: need to send a RST segment to the peer
        }
    } catch (const exception &e) {
        std::cerr << "Exception destructing TCP FSM: " << e.what() << std::endl;
    }
}

void TCPConnection::sendSegToRealOut() {
    while (!_sender.segments_out().empty()) {
        // ? this needs to be done
        TCPSegment sendingSeg = _sender.segments_out().front();
        if (_receiver.ackno().has_value()) {
            sendingSeg.header().ack = true;
            sendingSeg.header().ackno = _receiver.ackno().value();
            sendingSeg.header().win = _receiver.window_size();
        }
        _sender.segments_out().pop();
        _segments_out.push(sendingSeg);
    }
    return;
}
