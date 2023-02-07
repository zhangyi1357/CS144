#include "tcp_receiver.hh"

#include "wrapping_integers.hh"

#include <optional>

// Dummy implementation of a TCP receiver

// For Lab 2, please replace with a real implementation that passes the
// automated checks run by `make check_lab2`.

template <typename... Targs>
void DUMMY_CODE(Targs &&.../* unused */) {}

using namespace std;

void TCPReceiver::segment_received(const TCPSegment &seg) {
    if (_state == TCPReceiverState::LISTEN && seg.header().syn) {
        // the first arriving segment with the SYN flag set
        _state = TCPReceiverState::SYN_RECV;
        _isn = seg.header().seqno;
        _checkpoint = 1;
    }
    if (_state == TCPReceiverState::SYN_RECV) {
        // push any data to StreamReassembler
        const uint64_t absolute_seqno = unwrap(seg.header().seqno, _isn, _checkpoint) + (seg.header().syn);
        const uint64_t stream_index = absolute_seqno - 1;
        _reassembler.push_substring(seg.payload().copy(), stream_index, seg.header().fin);
        _checkpoint = _reassembler.stream_out().bytes_written() + 1;  // + 1 for SYN flag
        if (_reassembler.stream_out().input_ended()) {
            _state = TCPReceiverState::FIN_RECV;
            _checkpoint += 1;  // + 1 for FIN flag
        }
    }
}

optional<WrappingInt32> TCPReceiver::ackno() const {
    if (_state == TCPReceiverState::LISTEN)
        return std::nullopt;
    return wrap(_checkpoint, _isn);
}

size_t TCPReceiver::window_size() const { return _reassembler.stream_out().remaining_capacity(); }
