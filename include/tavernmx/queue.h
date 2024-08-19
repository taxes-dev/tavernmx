#pragma once
#include <mutex>
#include <optional>
#include <queue>

namespace tavernmx
{
    /**
     * @brief Thread-safe implementation of FIFO data structure. (Wraps std::queue<T, std::dequeue<T>>.)
     * @tparam T The type of the stored elements.
     * @note All container operations rely on locking a mutex. Copying/moving requires locking
     * on both containers in the operation.
     */
    template <typename T>
    class ThreadSafeQueue
    {
        // Implementation partially based on: https://codetrips.com/2020/07/26/modern-c-writing-a-thread-safe-queue/comment-page-1/
    public:
        /**
         * @brief Creates a new, empty container.
         */
        ThreadSafeQueue() noexcept = default;

        virtual ~ThreadSafeQueue() = default;

        ThreadSafeQueue(const ThreadSafeQueue& other) noexcept {
            *this = other;
        }

        ThreadSafeQueue(ThreadSafeQueue&& other) noexcept {
            *this = std::move(other);
        };

        ThreadSafeQueue& operator=(const ThreadSafeQueue& other) noexcept {
            if (this != &other) {
                std::lock_guard<std::mutex> guard1{ this->_mutex };
                std::lock_guard<std::mutex> guard2{ other._mutex };
                this->_queue = other._queue;
            }
            return *this;
        }

        ThreadSafeQueue& operator=(ThreadSafeQueue&& other) noexcept {
            if (this != &other) {
                std::lock_guard<std::mutex> guard1{ this->_mutex };
                std::lock_guard<std::mutex> guard2{ other._mutex };
                this->_queue = std::move(other._queue);
            }
            return *this;
        };

        /**
         * @brief Pushes a new element to the end of the queue. The element is constructed in-place.
         * @tparam Args arguments to forward to the constructor of the element
         * @param args arguments to forward to the constructor of the element
         * @return The value or reference inserted into the container.
         */
        template <class... Args>
        decltype(auto) emplace(Args&&... args) {
            std::lock_guard guard{ this->_mutex };
            return this->_queue.emplace(std::forward<Args...>(args...));
        }

        /**
         * @brief Checks if the underlying container has no elements.
         * @return true if the underlying container is empty, otherwise false.
         */
        bool empty() const {
            std::lock_guard guard{ this->_mutex };
            return this->_queue.empty();
        }

        /**
         * @brief Returns a reference to the first element in the queue.
         * @return Reference to the first element.
         */
        T& front() {
            std::lock_guard guard{ this->_mutex };
            return this->_queue.front();
        }

        /**
         * @brief Returns a const reference to the first element in the queue.
         * @return Reference to the first element.
         */
        const T& front() const {
            std::lock_guard guard{ this->_mutex };
            return this->_queue.front();
        }

        /**
         * @brief Removes an element from the front of the queue, if possible, and returns it.
         * @return A std::optional<T> containing the element popped off the queue, or nothing
         * if the underlying container is empty.
         */
        std::optional<T> pop() {
            std::lock_guard guard{ this->_mutex };
            if (this->_queue.empty()) {
                return std::nullopt;
            }
            T returned = std::move(this->_queue.front());
            this->_queue.pop();
            return returned;
        }

        /**
         * @brief Pushes the given element \p item to the end of the queue.
         * @param item The value of the element to push.
         */
        void push(const T& item) {
            std::lock_guard guard{ this->_mutex };
            this->_queue.push(item);
        }

        /**
         * @brief Pushes the given element \p item to the end of the queue.
         * @param item The value of the element to push.
         */
        void push(T&& item) {
            std::lock_guard guard{ this->_mutex };
            this->_queue.push(std::move(item));
        }

        /**
         * @brief Returns the number of elements in the underlying container.
         * @return The number of elements in the container.
         */
        size_t size() const {
            std::lock_guard guard{ this->_mutex };
            return this->_queue.size();
        }

    private:
        mutable std::mutex _mutex{};
        std::queue<T> _queue{};
    };
}
