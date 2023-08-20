#include "tcp_receiver.hh"

// Dummy implementation of a TCP receiver

// For Lab 2, please replace with a real implementation that passes the
// automated checks run by `make check_lab2`.

void TCPReceiver::segment_received(const TCPSegment &seg) {
    if (!isSynSet) {
        if (!seg.header().syn)
            return;
        isn = WrappingInt32(seg.header().seqno);
        isSynSet = true;
    }
    // remember : segStream may not come in order. So it's unacceptable to 
    // increase streamIndex by seg.length_in_sequence_space when seg comes.
    uint64_t streamIndex =
        unwrap(seg.header().seqno, isn, _reassembler.stream_out().bytes_written()) -
        1 + seg.header().syn;
    _reassembler.push_substring(seg.payload().copy(), streamIndex, seg.header().fin);
}

std::optional<WrappingInt32> TCPReceiver::ackno() const {
    if (!isSynSet)
        return std::nullopt;
    uint64_t abs_ack_no = _reassembler.stream_out().bytes_written() + 1;
    if (_reassembler.stream_out().input_ended())
        ++abs_ack_no;
    return WrappingInt32(isn) + abs_ack_no;
}

size_t TCPReceiver::window_size() const { return _capacity - _reassembler.stream_out().buffer_size(); }

