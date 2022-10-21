#ifndef _QUEUE_H_
#define _QUEUE_H_

#include <vector>

template<class T>
class Queue {
public:
    Queue():size_(10),readPtr_(0),writePtr_(0)
    {
        content_ = new T[size_ + 1];
    }

    Queue(int size):readPtr_(0),writePtr_(0)
    {
        size_ = size;
        content_ = new T[size_ + 1];
    }

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

private:
    int size_;
    int readPtr_;
    int writePtr_;
    T *content_;
};

#endif

