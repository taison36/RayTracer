#include "FrameBuffer.h"

namespace rt {

FrameBuffer::FrameBuffer(int width, int height)
    : pixels(height * width), WIDTH(width), HEIGHT(height) {}

const Color* FrameBuffer::operator[](int desiredHeight) const {
    return &pixels[desiredHeight * WIDTH];
}

Color* FrameBuffer::operator[](int desiredHeight) {
    return &pixels[desiredHeight * WIDTH];
}

FrameBuffer::ConstIterator::ConstIterator(const Color * dataPointer, size_t startIndex, int columns)
    : ptr(dataPointer),
      index(startIndex),
      cols(columns) {}

const Color& FrameBuffer::ConstIterator::operator*() const {
    return ptr[index];
}

FrameBuffer::ConstIterator& FrameBuffer::ConstIterator::operator++() {
    ++index;
    return *this;
}

bool FrameBuffer::ConstIterator::operator!=(const ConstIterator& other) const {
    return index != other.index;
}

int FrameBuffer::ConstIterator::row() const {
    return index / cols;
}
int FrameBuffer::ConstIterator::col() const {
    return index % cols;
}

FrameBuffer::Iterator::Iterator(Color* dataPointer, size_t startIndex, int columns)
    : ptr(dataPointer),
      index(startIndex),
      cols(columns) {}

Color& FrameBuffer::Iterator::operator*() const{
    return ptr[index];
}

FrameBuffer::Iterator& FrameBuffer::Iterator::operator++() {
    ++index;
    return *this;
}

bool FrameBuffer::Iterator::operator!=(const Iterator& other) const {
    return index != other.index;
}

int FrameBuffer::Iterator::row() const {
    return index / cols;
}

int FrameBuffer::Iterator::col() const {
    return index % cols;
}

FrameBuffer::ConstIterator FrameBuffer::begin() const {
    return {pixels.data(), 0, HEIGHT};
}

FrameBuffer::ConstIterator FrameBuffer::end() const {
    return {pixels.data(), pixels.size(), HEIGHT};
}

FrameBuffer::Iterator FrameBuffer::begin() {
    return {pixels.data(), 0, HEIGHT};
}

FrameBuffer::Iterator FrameBuffer::end() {
    return {pixels.data(), pixels.size(), HEIGHT};
}

} //rt
