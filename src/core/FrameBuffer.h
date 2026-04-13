#pragma once
#include "../objects/UtilObjects.h"
#include <cstdint>
#include <vector>

namespace rt{

    class FrameBuffer {
        std::vector<Color> pixels;
    public:
        const uint32_t COLS;
        const uint32_t ROWS;

        class ConstIterator {
            const Color* ptr;
            uint32_t index;
            size_t cols;
    
        public:
            ConstIterator(const Color* dataPointer, size_t startIndex, uint32_t columns);
    
            const Color& operator*() const;
    
            ConstIterator& operator++();
    
            bool operator!=(const ConstIterator& other) const;
    
            int row() const;
            int col() const;
        };
    
        class Iterator {
            Color* ptr;
            size_t index;
            uint32_t cols;
    
        public:
            Iterator(Color* dataPointer, size_t startIndex, uint32_t columns);
    
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
