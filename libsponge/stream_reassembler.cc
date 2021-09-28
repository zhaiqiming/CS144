#include "stream_reassembler.hh"

#include <iostream>

// Dummy implementation of a stream reassembler.

// For Lab 1, please replace with a real implementation that passes the
// automated checks run by `make check_lab1`.

// You will need to add private members to the class declaration in `stream_reassembler.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

StreamReassembler::StreamReassembler(const size_t capacity) : _output(capacity), _capacity(capacity) {
    _buffer = std::list<data_node>(_capacity, data_node());
}

//! \details This function accepts a substring (aka a segment) of bytes,
//! possibly out-of-order, from the logical stream, and assembles any newly
//! contiguous substrings and writes them into the output stream in order.
void StreamReassembler::push_substring(const string &data, const size_t index, const bool eof) {
    // max_index
    size_t max_index = _output.bytes_written() + _capacity - _output.buffer_size() - 1;
    if (index > max_index) { return; }

    // set data_node
    std::list<data_node>::iterator it = _buffer.begin();
    if (index >= _output.bytes_written()) { advance(it, index - _output.bytes_written()); }
    
    // empty eof
    if (data.size() == 0) {
        if (!eof)
            return;
        if (index == _output.bytes_written()) {
            _output.end_input();
        } else if (index > _output.bytes_written()) {
            (--it)->_eof = true;
        }
        return;
    }

    // save data
    size_t start_index = index < _output.bytes_written() ? _output.bytes_written() : index;
    for (size_t i = start_index; i < index + data.size() && i <= max_index; i++) {
        if (i == index + data.size() - 1 && eof) {
            it->_eof = true;
        }
        it->_ch = data[i - index];
        it->_vaild = true;
        it++;
    }
    
    // try to Reassemble and write
    it = _buffer.begin();
    string str_insert = "";
    while (it != _buffer.end() && it->_vaild) {
        if (it->_eof) {
            _output.end_input();
        }
        str_insert += (it++)->_ch;
    }
    _output.write(str_insert);

    // update list
    size_t num = distance(_buffer.begin(), it);
    for (size_t i = 0; i < num; i++) { _buffer.pop_front(); }
    _buffer.resize(_buffer.size() + num, data_node());
}

size_t StreamReassembler::unassembled_bytes() const {
    size_t num = 0;
    for (auto &item : _buffer) {
        if (item._vaild)
            num++;
    }
    return num;
}

bool StreamReassembler::empty() const { return unassembled_bytes() == 0; }
