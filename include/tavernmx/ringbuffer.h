#pragma once
#include <array>
#include <concepts>
#include <iterator>
#include <memory>

namespace tavernmx
{
    /**
     * @brief Specialized ring buffer where the tail is always the oldest item
     * inserted (i.e. new items are inserted at the head).
     * @tparam T Type to contain. Must be copy-constructible.
     * @tparam Capacity Maximum number of elements in the ring buffer. Note that the maximum
     * accessible elements will be 1 less than this number.
     */
    template <typename T, size_t Capacity>
        requires std::copy_constructible<T>
    class RingBuffer
    {
    public:
        /**
         * @brief Bi-directional iterator for RingBuffer<T, Capacity>.
         */
        struct Iterator
        {
            using iterator_category = std::bidirectional_iterator_tag;
            using difference_type = std::size_t;
            using value_type = T;
            using pointer = value_type*;
            using reference = value_type&;

            Iterator(RingBuffer* buffer, size_t pos)
                : _buffer{ buffer }, _pos{ pos } {
            };

            reference operator*() const { return *_buffer->_data[_pos]; }
            pointer operator->() { return _buffer->_data[_pos].get(); }

            Iterator& operator++() {
                if (++this->_pos == this->_buffer->capacity()) {
                    this->_pos = 0;
                }
                return *this;
            }

            Iterator operator++(int) {
                Iterator tmp = *this;
                ++(*this);
                return tmp;
            }

            Iterator& operator--() {
                if (this->_pos == 0) {
                    this->_pos = this->_buffer->capacity() - 1;
                } else {
                    --this->_pos;
                }
                return *this;
            }

            Iterator operator--(int) {
                Iterator tmp = *this;
                --(*this);
                return tmp;
            }

            bool operator==(const Iterator& other) {
                return this->_buffer == other._buffer && this->_pos == other._pos;
            };

            bool operator!=(const Iterator& other) {
                return this->_buffer != other._buffer || this->_pos != other._pos;
            };

        private:
            RingBuffer* _buffer;
            size_t _pos;
        };

        /**
         * @brief Create a new RingBuffer<T, Capacity>.
         */
        RingBuffer() = default;

        /**
         * @brief Return a pointer to the oldest item in the buffer.
         * @return Pointer of type T. This will be nullptr if the container is empty.
         */
        T* tail() const {
            if (this->empty()) {
                return nullptr;
            }
            return this->_data[this->_tail].get();
        }

        /**
         * @brief Returns the number of elements that the container has currently allocated
         * space for.
         * @return Capacity of the currently allocated storage.
         */
        constexpr size_t capacity() const {
            return Capacity;
        }

        /**
         * @brief Checks if the container has no elements, i.e. whether begin() == end().
         * @return true if the container is empty, otherwise false.
         */
        bool empty() const {
            return this->_head == this->_tail;
        }

        /**
         * @brief Checks if the container is full, i.e. whether capacity() == size().
         * @return true if the container is full, otherwise false.
         */
        bool full() const {
            return (this->_head + 1) % this->capacity() == this->_tail;
        }

        /**
         * @brief Insert an element at the head of the container. \p value will be copied.
         * @param value Element value to insert.
         */
        void insert(const T& value) {
            this->_data[this->_head] = std::make_unique<T>(value);
            this->increment();
        }

        /**
         * @brief Insert an element at the head of the container. \p value will be moved.
         * @param value Element value to insert.
         */
        void insert(T&& value) {
            this->_data[this->_head] = std::make_unique<T>(std::move(value));
            this->increment();
        }

        /**
         * @brief Insert an element at the head of the container. The container takes ownership
         * of \p pointer.
         * @param pointer Pointer to element value to insert.
         */
        void insert(std::unique_ptr<T> pointer) {
            this->_data[this->_head] = std::move(pointer);
            this->increment();
        }

        /**
         * @brief Erases all elements from the container. After this call, size() returns zero.
         */
        void reset() {
            for (auto& elem : this->_data) {
                elem.reset();
            }
            this->_head = this->_tail;
        }

        /**
         * @brief Returns the number of elements in the container.
         * @return
         * @note size() will return the true number of elements allocated in the container.
         * However, due to how the tail/head are managed, the true number of accessible elements
         * will not exceed capacity() - 1.
         */
        size_t size() const {
            if (this->full()) {
                return this->capacity();
            }
            if (this->_head >= this->_tail) {
                return this->_head - this->_tail;
            }
            return this->capacity() + this->_head - this->_tail;
        }

        /**
         * @brief Returns an iterator to the first element of the container.
         * @return Iterator to the first element.
         */
        auto begin() {
            return Iterator{ this, this->_tail };
        }

        /**
         * @brief Returns an iterator to the element following the last element of the container.
         * @return Iterator to the element following the last element.
         */
        auto end() {
            return Iterator{ this, this->_head };
        }

        /**
         * @brief Returns a reverse iterator to the first element of the reversed container. It
         * corresponds to the last element of the non-reversed container.
         * @return Reverse iterator to the first element.
         */
        auto rbegin() {
            return std::reverse_iterator{ this->end() };
        }

        /**
         * @brief Returns a reverse iterator to the element following the last element of the reversed
         * container. It corresponds to the element preceding the first element of the non-reversed container.
         * @return Reverse iterator to the element following the last element.
         */
        auto rend() {
            return std::reverse_iterator{ this->begin() };
        }

    private:
        std::array<std::unique_ptr<T>, Capacity> _data{};
        size_t _head{ 0 };
        size_t _tail{ 0 };

        void increment() {
            this->_head = (this->_head + 1) % this->capacity();
            if (this->_tail == this->_head) {
                this->_tail = (this->_tail + 1) % this->capacity();
            }
        };
    };
}
