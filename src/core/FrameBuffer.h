#pragma once
#include "../objects/Color.h"
#include <vector>

namespace rt{

    class FrameBuffer {
        std::vector<Color> pixels;
        const int WIDTH;
        const int HEIGHT;
    
    public:
        class ConstIterator {
            const Color* ptr;
            int index;
            size_t cols;
    
        public:
            ConstIterator(const Color* dataPointer, size_t startIndex, int columns);
    
            const Color& operator*() const;
    
            ConstIterator& operator++();
    
            bool operator!=(const ConstIterator& other) const;
    
            int row() const;
            int col() const;
        };
    
        class Iterator {
            Color* ptr;
            size_t index;
            int cols;
    
        public:
            Iterator(Color* dataPointer, size_t startIndex, int columns);
    
            Color& operator*() const;
    
            Iterator& operator++();
    
            bool operator!=(const Iterator& other) const;
    
            int row() const;
            int col() const;
        };
    
        FrameBuffer(int width, int height);
    
        const Color* operator[](int desiredHeight) const;
        Color* operator[](int desiredHeight);
    
    
        [[nodiscard]] ConstIterator begin() const;
    
        [[nodiscard]] ConstIterator end() const;
    
        Iterator begin();
    
        Iterator end();
    
    };

}//rt
