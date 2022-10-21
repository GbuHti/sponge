#ifndef _QUEUE_H_
#define _QUEUE_H_

#include <vector>
#include <iostream>

template<class T>
class Queue {
public:
    Queue():size_(10),readPtr_(0),writePtr_(0),eof_(false),content_(nullptr)
    {
        content_ = new T[size_ + 1];
    }

    Queue(int size):size_(size),readPtr_(0),writePtr_(0),eof_(false),content_(nullptr)
    {
        content_ = new T[size_ + 1];
    }

    Queue(const Queue<T>&)=delete;
    Queue<T>& operator=(const Queue<T>&)=delete;

    ~Queue()
    {
        delete []content_;
    }

    T pop()
    {
        T tmp{};
        if (this->empty() == false) {
            tmp = content_[readPtr_];
            readPtr_ = (readPtr_ + 1) % (size_ + 1);
        }
        return tmp;
    }

    T peek(int relativePos)
    {
        int newPos = (readPtr_ + relativePos) % (size_ + 1);
        return content_[newPos];
    }

    void push(T t)
    {
        if (this->full() == false) {
            content_[writePtr_] = t;
            writePtr_ = (writePtr_ + 1) % (size_ + 1);
        }
    }

    bool empty()
    {
        if (readPtr_ == writePtr_) {
            return true;
        } else {
            return false;
        }
    }

    bool full()
    {
        if ((writePtr_ + 1) % (size_+1) == readPtr_) {
            return true;
        } else {
            return false;
        }
    }

    int buffer_size()
    {
        if (full()) {
            return size_;
        } else if (empty()) {
            return 0;
        } else {
            std::cout << "W " << writePtr_ << "R " << readPtr_ << std::endl;
            return writePtr_ <= readPtr_ ? writePtr_ + size_ + 1 - readPtr_ : writePtr_ - readPtr_;
        }
    }

    void set_eof()
    {
        eof_ = writePtr_;
    }

    bool eof()
    {
        return readPtr_ == eof_;
    }

private:
    int size_;
    int readPtr_;
    int writePtr_;
    int eof_;
    T *content_;
};

#endif

