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
    _q = new Queue<char>(_capacity);
}

ByteStream::~ByteStream(){
    delete _q;
}

size_t ByteStream::write(const string &data) {
    size_t size{};
    for (char i : data) {
        if (!_q->full()) {
            _q->push(i);
            _bytesWriten++;
            size++;
        }
    }
    return size;
}

//! \param[in] len bytes will be copied from the output side of the buffer
string ByteStream::peek_output(const size_t len) const {
    string tmp{};
    for (size_t i = 0; i < len; i++) {
        tmp += _q->peek(static_cast<int>(i));
    }
    return tmp;
}

//! \param[in] len bytes will be removed from the output side of the buffer
void ByteStream::pop_output(const size_t len)
{
    string tmp{};
    for (size_t i = 0; i < len; i++) {
        tmp += _q->pop();
        _bytesRead++;
    }
}

//! Read (i.e., copy and then pop) the next "len" bytes of the stream
//! \param[in] len bytes will be popped and returned
//! \returns a string
std::string ByteStream::read(const size_t len) {
    string tmp{};
    for (size_t i = 0; i < len; i++) {
        if (!_q->empty()) {
            tmp += _q->pop();
            _bytesRead++;
        }
    }
    return tmp;
}

void ByteStream::end_input() { _eof = true; _q->set_eof();}

bool ByteStream::input_ended() const { return _eof;}

size_t ByteStream::buffer_size() const { return _q->buffer_size();}

bool ByteStream::buffer_empty() const { return _q->empty();}

bool ByteStream::eof() const { return _eof && _q->eof(); }

size_t ByteStream::bytes_written() const { return _bytesWriten; }

size_t ByteStream::bytes_read() const { return _bytesRead; }

size_t ByteStream::remaining_capacity() const { return _capacity - buffer_size(); }
