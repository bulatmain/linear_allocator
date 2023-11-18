#ifndef CUST_ALLOCATOR_HPP
#define CUST_ALLOCATOR_HPP

#include <iostream>
#include <exception>

#ifndef DEFAULT_CA_MEMORY_CAPCACITY
#define DEFAULT_CA_MEMORY_CAPCACITY int(10e6)
#endif

namespace lab5 {

    template <typename T, std::size_t CAPACITY = DEFAULT_CA_MEMORY_CAPCACITY>
    class cust_allocator {

    protected:

        void* begin;
        void* end;

    public:
        const std::size_t capacity = CAPACITY;
        using value_type = T;
        using pointer = T*;
        using const_pointer = const T*;
        using size_type = std::size_t;

        template <class U>
        struct rebind {
            using other = cust_allocator<U, CAPACITY>;
        };

        cust_allocator();
        cust_allocator(cust_allocator&& other) noexcept;

        cust_allocator& operator= (cust_allocator&& other) noexcept;

        ~cust_allocator() noexcept;

        T* allocate(std::size_t n);

        void deallocate(T* p, std::size_t n) noexcept;

    private:
        void move(cust_allocator&& other) noexcept;

        cust_allocator(const cust_allocator&);
        cust_allocator& operator=(const cust_allocator&);

        T* allocate_in_free_block(void* it, std::size_t n) noexcept;

        void squeeze_blocks(void* p) noexcept;

    };


    struct block;

    struct block {

        struct block_data {
            void* p;
            std::size_t size;
        };

        block_data data;
        bool is_busy;
        void* next;

        block(void* p, std::size_t size, bool is_busy = false, void* next = nullptr) 
            : data({ p, size }), is_busy(is_busy), next(next) {}

    };



    template <typename T, std::size_t CAPACITY>
    cust_allocator<T, CAPACITY>::cust_allocator() {
        begin = ::operator new(capacity);
        end = (char*)begin + capacity; 
        new(begin) block((char*)begin + sizeof(block), capacity - sizeof(block), false, end);
    }

    template <typename T, std::size_t CAPACITY>
    void cust_allocator<T, CAPACITY>::move(cust_allocator&& other) noexcept {
        begin = other.begin;
        end = other.end;
        other.begin = other.end = nullptr;
    }

    template <typename T, std::size_t CAPACITY>
    cust_allocator<T, CAPACITY>::cust_allocator(cust_allocator&& other) noexcept {
        move(std::move(other));
    }

    template <typename T, std::size_t CAPACITY>
    cust_allocator<T, CAPACITY>& cust_allocator<T, CAPACITY>::operator= (cust_allocator&& other) noexcept {
        move(std::move(other));
    }

    template <typename T, std::size_t CAPACITY>
    cust_allocator<T, CAPACITY>::~cust_allocator() noexcept {
        ::operator delete(begin);
        begin = end = nullptr;
    }

    template <typename T, std::size_t CAPACITY>
    T* cust_allocator<T, CAPACITY>::allocate(std::size_t n) {
        if (n == 0) {
            return nullptr;
        }
        void* it = begin;
        void* result = nullptr;

        while (it != end) {
            // std::cout << it << " " << reinterpret_cast<block*>(it)->data.size << std::endl;
            if (reinterpret_cast<block*>(it)->is_busy) {
                it = reinterpret_cast<block*>(it)->next;
                continue;
            } 

            squeeze_blocks(it);

            if (n * sizeof(T) < reinterpret_cast<block*>(it)->data.size) {
                result = allocate_in_free_block(it, n);
                break;
            } else {
                it = reinterpret_cast<block*>(it)->next;
            }
        }

        // while (it != end) {
        //     // std::cout << it << " " << reinterpret_cast<block*>(it)->data.size << std::endl;
        //     it = reinterpret_cast<block*>(it)->next;
        // }

        // std::cout << "Allocate in [" << it << "; " << reinterpret_cast<block*>(it)->next << "], n: " << n << std::endl;

        if (result == nullptr) {
            throw std::bad_alloc();
        }

        return reinterpret_cast<T*>(result);
    }


    template <typename T, std::size_t CAPACITY>
    void cust_allocator<T, CAPACITY>::deallocate(T* p, std::size_t n) noexcept {
        void* it = reinterpret_cast<char*>(p) - sizeof(block);
        void* next = reinterpret_cast<block*>(it)->next;
        std::size_t size = (char*)next - (char*)it - sizeof(block);
        
        new(it) block(
            p,
            size,
            false,
            next
        );
        // std::cout << "Deallocate in [" << it << "; " << next << "], n: " << n << std::endl;
    }

    template <typename T>
    bool fits_in(void* p, void* q, std::size_t n);

    template <typename T>
    void split_on_two(void* p, void* q, std::size_t n);

    template <typename T, std::size_t CAPACITY>
    T* cust_allocator<T, CAPACITY>::allocate_in_free_block(void* it, std::size_t n) noexcept {
        void *p = it, *q = reinterpret_cast<block*>(it)->next;

        // std::cout << "p: " << p << " q: " << q << std::endl;

        if (fits_in<T>(p, q, n)) {
            split_on_two<T>(p, q, n);    
        } else {
            new(p) block(
                (char*)p + sizeof(block),
                (char*)q - (char*)p - sizeof(block),
                true,
                q
            );
            // std::cout << "New block: [" << p << "; " << q << "] with size " << (char*)q - (char*)p - sizeof(block) << "\n";
        }

        void* data_pointer = reinterpret_cast<block*>(p)->data.p;
        return reinterpret_cast<T*>(data_pointer);
    }

    template <typename T>
    bool fits_in(void* p, void* q, std::size_t n) {
        std::size_t size_to_be_put = 2 * sizeof(block) + n * sizeof(T);
        std::size_t available_size = std::size_t((char*)q - (char*)p);
        // std::cout << "Size to be put: " << size_to_be_put << " and available: " << available_size << std::endl;
        return size_to_be_put <= available_size;
    }

    template <typename T>
    void split_on_two(void* p, void* q, std::size_t n) {
        std::size_t lsz = n * sizeof(T);
        void* r = (char*)p + sizeof(block) + lsz;
        std::size_t rsz = (char*)q - (char*)r - sizeof(block);

        new(p) block(
            (char*)p + sizeof(block),
            lsz,
            true,
            r
        );
        new(r) block(
            (char*)r + sizeof(block),
            rsz,
            false,
            q
        );

        // std::cout << "Block [" << p << "; " << q << "] splited by "
        //           << "[" << p << "; " << r << "] with size " << lsz 
        //           << " and [" << r << "; " << q << "] with size " << rsz << "\n";
    }

    template <typename T, std::size_t CAPACITY>
    void cust_allocator<T, CAPACITY>::squeeze_blocks(void* p) noexcept {
        void* it = p;
        while (it != end && !reinterpret_cast<block*>(it)->is_busy) {
            it = reinterpret_cast<block*>(it)->next;
        }
        if (it == p || it == reinterpret_cast<block*>(p)->next) {
            return;
        } else {
            new(p) block(
                (char*)p + sizeof(block), 
                (char*)it - (char*)p - sizeof(block), 
                false, 
                it
            );
        }
        // std::cout << "Squeezed to [" << p << "; " << it << "]\n";
        // std::cout << "Size of new block: " <<  (char*)it - (char*)p - sizeof(block) << std::endl;
    }

    template<class T, class U>
    bool operator==(const cust_allocator<T>&, const cust_allocator<T>&) {
        return true;
    }

    template<class T, class U>
    bool operator!=(const cust_allocator<T>&, const cust_allocator<T>&) {
        return false;
    }

};

#endif