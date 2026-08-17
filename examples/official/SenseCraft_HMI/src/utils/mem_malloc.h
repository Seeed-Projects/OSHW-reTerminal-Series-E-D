// utils/mem_malloc.h

#ifndef MEM_MALLOC_H
#define MEM_MALLOC_H

#include <cstddef>          // for std::size_t
#include <new>              // for std::bad_alloc
#include <esp_heap_caps.h>  // for heap_caps_malloc

namespace util {

template <typename T>
struct psram_allocator {
    using value_type = T;

    psram_allocator() = default;

    template <typename U>
    constexpr psram_allocator(const psram_allocator<U>&) noexcept {}

    
    T* allocate(std::size_t n) {
        if (n > std::size_t(-1) / sizeof(T)) {
            throw std::bad_alloc();
        }
        
        void* ptr = heap_caps_malloc(n * sizeof(T), MALLOC_CAP_SPIRAM);
        if (!ptr) {
            throw std::bad_alloc();
        }
        return static_cast<T*>(ptr);
    }


    void deallocate(T* p, std::size_t /*n*/) noexcept {
        heap_caps_free(p);
    }
};

template <typename T, typename U>
bool operator==(const psram_allocator<T>&, const psram_allocator<U>&) {
    return true;
}

template <typename T, typename U>
bool operator!=(const psram_allocator<T>&, const psram_allocator<U>&) {
    return false;
}

} // namespace util

#endif // MEM_MALLOC_H