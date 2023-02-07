#include "byte_stream.hh"

// Dummy implementation of a flow-controlled in-memory byte stream.

// For Lab 0, please replace with a real implementation that passes the
// automated checks run by `make check_lab0`.

// You will need to add private members to the class declaration in `byte_stream.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&.../* unused */) {}

using namespace std;

ByteStream::ByteStream(const size_t capacity) : m_capacity(capacity), m_buffer(capacity, '\0') {}

size_t ByteStream::write(const string &data) {
    size_t len = data.size();
    size_t bytes_to_write = min(len, remaining_capacity());
    for (size_t i = 0; i < bytes_to_write; ++i) {
        m_buffer[(m_write_idx + i) % m_capacity] = data[i];
    }
    m_write_idx += bytes_to_write;
    return bytes_to_write;
}

//! \param[in] len bytes will be copied from the output side of the buffer
string ByteStream::peek_output(const size_t len) const {
    size_t bytes_to_peek = min(len, buffer_size());
    string result(bytes_to_peek, '\0');
    for (size_t i = 0; i < bytes_to_peek; ++i) {
        result[i] = m_buffer[(m_read_idx + i) % m_capacity];
    }
    return result;
}

//! \param[in] len bytes will be removed from the output side of the buffer
void ByteStream::pop_output(const size_t len) {
    size_t bytes_to_pop = min(len, buffer_size());
    m_read_idx += bytes_to_pop;
}

//! Read (i.e., copy and then pop) the next "len" bytes of the stream
//! \param[in] len bytes will be popped and returned
//! \returns a string
std::string ByteStream::read(const size_t len) {
    size_t bytes_to_read = min(len, buffer_size());
    string result(bytes_to_read, '\0');
    for (size_t i = 0; i < bytes_to_read; ++i) {
        result[i] = m_buffer[(m_read_idx + i) % m_capacity];
    }
    m_read_idx += bytes_to_read;
    return result;
}

void ByteStream::end_input() { m_end_input = true; }

bool ByteStream::input_ended() const { return m_end_input; }

size_t ByteStream::buffer_size() const { return m_write_idx - m_read_idx; }

bool ByteStream::buffer_empty() const { return m_write_idx == m_read_idx; }

bool ByteStream::eof() const { return (m_end_input && buffer_empty()) ? true : false; }

size_t ByteStream::bytes_written() const { return m_write_idx; }

size_t ByteStream::bytes_read() const { return m_read_idx; }

size_t ByteStream::remaining_capacity() const { return m_capacity - buffer_size(); }
