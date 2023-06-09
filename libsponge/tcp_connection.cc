#include "tcp_connection.hh"

#include "tcp_config.hh"
#include "tcp_segment.hh"

#include <cstdint>
#include <iostream>
#include <limits>

// Dummy implementation of a TCP connection

// For Lab 4, please replace with a real implementation that passes the
// automated checks run by `make check`.

template <typename... Targs>
void DUMMY_CODE(Targs &&.../* unused */) {}

using namespace std;

size_t TCPConnection::remaining_outbound_capacity() const { return _sender.stream_in().remaining_capacity(); }

size_t TCPConnection::bytes_in_flight() const { return _sender.bytes_in_flight(); }

size_t TCPConnection::unassembled_bytes() const { return _receiver.unassembled_bytes(); }

size_t TCPConnection::time_since_last_segment_received() const { return _time_last_segment_received; }

void TCPConnection::_send_and_reset_connection() {
    TCPSegment sending_seg;
    sending_seg.header().rst = true;
    _segments_out.push(sending_seg);
    _reset_connection();
}

void TCPConnection::_reset_connection() {
    _sender.stream_in().set_error();
    _receiver.stream_out().set_error();
    _active = false;
}

void TCPConnection::_set_segment_ackno_and_win(TCPSegment &sending_seg) {
    if (_receiver.ackno().has_value()) {
        sending_seg.header().ack = true;
        sending_seg.header().ackno = _receiver.ackno().value();
    }
    // send maximum window size if the window size is larger than the maximum window size
    sending_seg.header().win = min(_receiver.window_size(), static_cast<size_t>(std::numeric_limits<uint16_t>::max()));
}

bool TCPConnection::_send_all_segments() {
    bool sent = false;
    while (!_sender.segments_out().empty()) {
        TCPSegment sending_seg = _sender.segments_out().front();
        _sender.segments_out().pop();
        _set_segment_ackno_and_win(sending_seg);
        _segments_out.push(sending_seg);
        sent = true;
    }
    return sent;
}

void TCPConnection::segment_received(const TCPSegment &seg) {
    // deal with the first segment
    if (!_syn_get && !seg.header().syn)
        return;
    if (seg.header().syn)
        _syn_get = true;

    // update the time last segment received
    _time_last_segment_received = 0;

    // check the rst flag
    if (seg.header().rst) {
        _reset_connection();
        return;
    }

    // give the segment to the receiver
    _receiver.segment_received(seg);

    // check the linger state
    if (_receiver.stream_out().eof() && !_sender.stream_in().eof()) {
        _linger_after_streams_finish = false;
    }

    // give the segment to the sender() if ack is set
    if (seg.header().ack) {
        _sender.ack_received(seg.header().ackno, seg.header().win);
        _sender.fill_window();
        _send_all_segments();
    }

    // react to segment that has data
    if (seg.length_in_sequence_space()) {
        _sender.fill_window();
        bool sent = _send_all_segments();
        // send at least one segment if the segment occupies any sequence number
        if (!sent) {
            _sender.send_empty_segment();
            _send_all_segments();
        }
    }

    // deal with "keep alive" segment
    if (_receiver.ackno().has_value() && seg.length_in_sequence_space() == 0 &&
        seg.header().seqno == _receiver.ackno().value() - 1) {
        _sender.send_empty_segment();
        _send_all_segments();
    }
}

bool TCPConnection::active() const { return _active; }

size_t TCPConnection::write(const string &data) {
    if (data.size() == 0)
        return 0;
    size_t bytes_write = _sender.stream_in().write(data);
    _sender.fill_window();
    _send_all_segments();
    return bytes_write;
}

//! \param[in] ms_since_last_tick number of milliseconds since the last call to this method
void TCPConnection::tick(const size_t ms_since_last_tick) {
    // increase the time last segment received
    _time_last_segment_received += ms_since_last_tick;

    // tick the sender
    _sender.tick(ms_since_last_tick);

    // retransmit the segment if there is one
    if (!_sender.segments_out().empty()) {
        TCPSegment retxSeg = _sender.segments_out().front();
        _sender.segments_out().pop();
        _set_segment_ackno_and_win(retxSeg);
        // abort the connection
        if (_sender.consecutive_retransmissions() > _cfg.MAX_RETX_ATTEMPTS) {
            _sender.stream_in().set_error();
            _receiver.stream_out().set_error();
            retxSeg.header().rst = true;
            _active = false;
        }
        _segments_out.push(retxSeg);
    }

    // if prerequisites are met, set the connection to inactive
    if (_sender.stream_in().eof() && _sender.bytes_in_flight() == 0 && _receiver.stream_out().input_ended()) {
        if (!_linger_after_streams_finish || time_since_last_segment_received() >= 10 * _cfg.rt_timeout) {
            _active = false;
        }
    }
}

void TCPConnection::end_input_stream() {
    _sender.stream_in().end_input();
    _sender.fill_window();
    _send_all_segments();
}

void TCPConnection::connect() {
    _syn_get = true;
    // send the syn segment
    _sender.fill_window();
    _send_all_segments();
}

TCPConnection::~TCPConnection() {
    try {
        if (active()) {
            cerr << "Warning: Unclean shutdown of TCPConnection\n";

            // need to send a RST segment to the peer
            _send_and_reset_connection();
        }
    } catch (const exception &e) {
        std::cerr << "Exception destructing TCP FSM: " << e.what() << std::endl;
    }
}