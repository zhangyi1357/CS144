#include "stream_reassembler.hh"

#include <cstddef>
#include <cstdint>
#include <exception>
#include <iostream>
#include <netinet/in.h>
#include <random>

// Dummy implementation of a stream reassembler.

// For Lab 1, please replace with a real implementation that passes the
// automated checks run by `make check_lab1`.

// You will need to add private members to the class declaration in `stream_reassembler.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&.../* unused */) {}

using namespace std;

StreamReassembler::StreamReassembler(const size_t capacity)
    : _buffer(capacity, '\0'), _used(capacity, false), _output(capacity), _capacity(capacity) {}

//! \details This function accepts a substring (aka a segment) of bytes,
//! possibly out-of-order, from the logical stream, and assembles any newly
//! contiguous substrings and writes them into the output stream in order.
void StreamReassembler::push_substring(const string &data, const size_t index, const bool eof) {
    if (eof)
        _eof_index = index + data.size();
    if (_index >= _eof_index)
        _output.end_input();
    if (index <= _index) {
        if (data.size() < (_index - index))  // must be checked, because len is unsigned
            return;
        size_t len = min(data.size() - (_index - index), _capacity);  // discard the bytes out of capacity
        size_t bytes_output = _output.write(data.substr(_index - index, len));
        _set_unused(bytes_output);
        _assemble();
        if (_index >= _eof_index)
            _output.end_input();
        return;
    }
    size_t buffer_index = index, end = min(_eof_index, _index + _capacity);
    size_t data_index = 0, data_size = data.size();
    while (buffer_index < end && data_index < data_size) {
        _buffer[buffer_index % _capacity] = data[data_index];
        _used[buffer_index % _capacity] = true;
        ++buffer_index;
        ++data_index;
    }
}

void StreamReassembler::_set_unused(const size_t bytes) {
    for (size_t i = 0; i < bytes; ++i) {
        _used[(_index + i) % _capacity] = false;
    }
    _index += bytes;
}

void StreamReassembler::_assemble() {
    size_t end_of_used = _index;
    while (end_of_used < _eof_index && _used[end_of_used % _capacity]) {
        ++end_of_used;
    }

    // std::cout << "_assemble " << _index - original_index << " bytes" << std::endl;
    size_t bytes_output = 0;
    if (_index == end_of_used) {
        return;
    } else if (end_of_used % _capacity > _index % _capacity) {
        bytes_output += _output.write(_buffer.substr(_index % _capacity, end_of_used - _index));
    } else {
        bytes_output += _output.write(_buffer.substr(_index % _capacity, _capacity - _index % _capacity));
        bytes_output += _output.write(_buffer.substr(0, end_of_used % _capacity));
    }
    _set_unused(bytes_output);
    if (_index >= _eof_index)
        _output.end_input();
}

size_t StreamReassembler::unassembled_bytes() const {
    size_t result = 0;
    size_t end = min(_eof_index, _index + _capacity);
    for (size_t i = _index; i < end; ++i) {
        if (_used[i])
            ++result;
    }
    return result;
}

bool StreamReassembler::empty() const { return unassembled_bytes() == 0; }
