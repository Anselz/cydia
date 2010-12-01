// Copyright Notice (GNU Affero GPL) {{{
/* Cyndir - (Awesome) Memory Mapped Dictionary
 * Copyright (C) 2010  Jay Freeman (saurik)
*/

/*
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 * 
 * You should have received a copy of the GNU Affero General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/
// }}}

#ifndef CYTORE_HPP
#define CYTORE_HPP

#include <fcntl.h>

#include <sys/mman.h>
#include <sys/stat.h>

#include <cstdio>
#include <cstdlib>

#include <errno.h>
#include <stdint.h>
#include <unistd.h>

#define _assert(test) do \
    if (!(test)) { \
        fprintf(stderr, "_assert(%d:%s)@%s:%u[%s]\n", errno, #test, __FILE__, __LINE__, __FUNCTION__); \
        exit(-1); \
    } \
while (false)

namespace Cytore {

static const uint32_t Magic = 'cynd';

struct Header {
    uint32_t magic_;
    uint32_t version_;
    uint32_t size_;
    uint32_t reserved_;
};

struct Block {
};

template <typename Target_>
class Offset {
  private:
    uint32_t offset_;

  public:
    Offset() :
        offset_(0)
    {
    }

    Offset(uint32_t offset) :
        offset_(offset)
    {
    }

    Offset &operator =(uint32_t offset) {
        offset_ = offset;
        return *this;
    }

    uint32_t GetOffset() const {
        return offset_;
    }

    bool IsNull() const {
        return offset_ == 0;
    }
};

template <typename Type_>
static _finline Type_ Round(Type_ value, Type_ size) {
    Type_ mask(size - 1);
    return value + mask & ~mask;
}

template <typename Base_>
class File {
  private:
    static const unsigned Shift_ = 17;
    static const size_t Block_ = 1 << Shift_;
    static const size_t Mask_ = Block_ - 1;

  private:
    int file_;
    std::vector<uint8_t *> blocks_;

    Header &Header_() {
        return *reinterpret_cast<Header *>(blocks_[0]);
    }

    uint32_t &Size_() {
        return Header_().size_;
    }

    void Map_(size_t size) {
        size_t before(blocks_.size() * Block_);
        size_t extend(size - before);

        void *data(mmap(NULL, extend, PROT_READ | PROT_WRITE, MAP_FILE | MAP_SHARED, file_, before));
        for (size_t i(0); i != extend >> Shift_; ++i)
            blocks_.push_back(reinterpret_cast<uint8_t *>(data) + Block_ * i);
    }

    void Truncate_(size_t capacity) {
        capacity = Round(capacity, Block_);
        ftruncate(file_, capacity);
        Map_(capacity);
    }

  public:
    File() :
        file_(-1)
    {
    }

    File(const char *path) :
        file_(-1)
    {
        Open(path);
    }

    ~File() {
        // XXX: this object is never deconstructed. if it were, this should unmap the memory
        close(file_);
    }

    size_t Capacity() const {
        return blocks_.size() * Block_;
    }

    void Open(const char *path) {
        _assert(file_ == -1);
        file_ = open(path, O_RDWR | O_CREAT | O_EXLOCK, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);

        struct stat stat;
        fstat(file_, &stat);

        size_t core(sizeof(Header) + sizeof(Base_));

        size_t size(stat.st_size);
        if (size == 0) {
            Truncate_(core);
            Header_().magic_ = Magic;
            Size_() = core;
        } else {
            _assert(size >= core);
            Truncate_(size);
            _assert(Header_().magic_ == Magic);
            _assert(Header_().version_ == 0);
        }
    }

    void Reserve(size_t capacity) {
        if (capacity <= Capacity())
            return;
        blocks_.pop_back();
        Truncate_(capacity);
    }

    template <typename Target_>
    Target_ &Get(uint32_t offset) {
        return *reinterpret_cast<Target_ *>(blocks_[offset >> Shift_] + (offset & Mask_));
    }

    template <typename Target_>
    Target_ &Get(Offset<Target_> &ref) {
        return Get<Target_>(ref.GetOffset());
    }

    Base_ *operator ->() {
        return &Get<Base_>(sizeof(Header));
    }

    template <typename Target_>
    Offset<Target_> New(size_t extra = 0) {
        size_t size(sizeof(Target_) + extra);
        size = Round(size, sizeof(uintptr_t));
        Reserve(Size_() + size);
        uint32_t offset(Size_());
        Size_() += size;
        return Offset<Target_>(offset);
    }
};

}

#endif//CYTORE_HPP