#include "byte_stream.hh"

// Dummy implementation of a flow-controlled in-memory byte stream.

// For Lab 0, please replace with a real implementation that passes the
// automated checks run by `make check_lab0`.

// You will need to add private members to the class declaration in `byte_stream.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

ByteStream::ByteStream(const size_t capacity) {
    _capacity = capacity;
    _buffer_size = 0;
    _buffer.clear();
    _bytes_written = 0;
    _bytes_read = 0;
    _input_ended = false;
}

size_t ByteStream::write(const string &data) {
    // DUMMY_CODE(data);
    size_t temp = _buffer_size;
    for(auto ch : data) {
        if(temp == _capacity) break;
        _buffer.emplace_back(ch);
        ++temp;
    }
    int num = temp - _buffer_size;
    _bytes_written += num;
    _buffer_size = temp;
    return num;
}

//! \param[in] len bytes will be copied from the output side of the buffer
string ByteStream::peek_output(const size_t len) const {
    // DUMMY_CODE(len);
    const size_t peek_length = len > _buffer_size ? _buffer_size : len;
    list<char>::const_iterator it = _buffer.begin();
    advance(it, peek_length);
    return string(_buffer.begin(), it);
}

//! \param[in] len bytes will be removed from the output side of the buffer
void ByteStream::pop_output(const size_t len) {
    for(size_t i = 0; i < len && _buffer_size > 0; i++){
        ++_bytes_read;
        --_buffer_size;
        _buffer.pop_front();
    }
}

//! Read (i.e., copy and then pop) the next "len" bytes of the stream
//! \param[in] len bytes will be popped and returned
//! \returns a string
std::string ByteStream::read(const size_t len) {
    string ans = peek_output(len);
    pop_output(len);
    return ans;
}

void ByteStream::end_input() { _input_ended = true; }

bool ByteStream::input_ended() const { return _input_ended; }

size_t ByteStream::buffer_size() const { return _buffer_size; }

bool ByteStream::buffer_empty() const { return _buffer_size == 0; }

bool ByteStream::eof() const { return input_ended() && buffer_empty(); }

size_t ByteStream::bytes_written() const { return _bytes_written; }

size_t ByteStream::bytes_read() const { return _bytes_read; }

size_t ByteStream::remaining_capacity() const { return _capacity - _buffer_size; }
