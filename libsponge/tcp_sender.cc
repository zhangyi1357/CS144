#include "tcp_sender.hh"

#include "parser.hh"
#include "tcp_config.hh"

#include <cassert>
#include <cstddef>
#include <iostream>
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
    , _timer(_initial_retransmission_timeout)
    , _stream(capacity) {}

uint64_t TCPSender::bytes_in_flight() const { return _next_seqno - _last_ackno; }

void TCPSender::fill_window() {
    if (!_syn_sent) {
        TCPSegment seg;
        seg.header().syn = true;
        _send_segment(seg);
        _syn_sent = true;
        return;
    }
    size_t window_size = (_window_size == 0) ? 1 : _window_size;
    while (!_fin_sent && window_size > bytes_in_flight()) {
        TCPSegment seg;
        const size_t segment_size = min(TCPConfig::MAX_PAYLOAD_SIZE, window_size - bytes_in_flight());
        seg.payload() = Buffer(_stream.read(segment_size));
        if (_stream.eof() && seg.length_in_sequence_space() < window_size) {
            seg.header().fin = true;
            _fin_sent = true;
        }
        if (seg.length_in_sequence_space() == 0)
            return;
        _send_segment(seg);
    }
}

//! \param ackno The remote receiver's ackno (acknowledgment number)
//! \param window_size The remote receiver's advertised window size
void TCPSender::ack_received(const WrappingInt32 ackno, const uint16_t window_size) {
    uint64_t abs_ackno = unwrap(ackno, _isn, _next_seqno);
    // ignore ackno if it is out of range
    if (abs_ackno > _next_seqno)
        return;

    _window_size = window_size;
    if (abs_ackno <= _last_ackno)
        return;
    // received a new ackno
    _last_ackno = abs_ackno;
    _timer.set_timeout(_initial_retransmission_timeout);
    _consecutive_retransmissions = 0;

    while (!_outstanding_segments.empty()) {
        const auto &seg = _outstanding_segments.front();
        const uint64_t seg_end_index = unwrap(seg.header().seqno + seg.length_in_sequence_space(), _isn, _next_seqno);
        if (seg_end_index <= _last_ackno) {
            _outstanding_segments.pop();
        } else {
            break;
        }
    }

    // fill_window();

    if (_outstanding_segments.empty()) {
        _timer.stop();
    } else {
        _timer.reset_and_run();
    }
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void TCPSender::tick(const size_t ms_since_last_tick) {
    if (_timer.running()) {
        _timer.tick(ms_since_last_tick);
        if (_timer.expired() && !_outstanding_segments.empty()) {
            _segments_out.push(_outstanding_segments.front());
            if (_window_size) {
                _consecutive_retransmissions += 1;
                _timer.set_timeout(pow(2, _consecutive_retransmissions) * _initial_retransmission_timeout);
            }
            _timer.reset_and_run();
        }
    }
}

unsigned int TCPSender::consecutive_retransmissions() const { return _consecutive_retransmissions; }

void TCPSender::send_empty_segment() {
    TCPSegment seg;
    seg.header().seqno = wrap(_next_seqno, _isn);
    _segments_out.push(seg);
}

void TCPSender::_send_segment(TCPSegment seg) {
    seg.header().seqno = wrap(_next_seqno, _isn);
    _segments_out.push(seg);
    _outstanding_segments.push(seg);
    _next_seqno += seg.length_in_sequence_space();
    if (!_timer.running()) {
        _timer.reset_and_run();
    }
}
