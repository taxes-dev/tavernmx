#pragma once
#include <mutex>
#include <optional>
#include <stack>

namespace tavernmx
{
    /**
     * @brief Thread-safe implementation of LIFO data structure. (Wraps std::stack<T, std::dequeue<T>>.)
     * @tparam T The type of the stored elements.
     * @note All container operations rely on locking a mutex. Copying/moving requires locking
     * on both containers in the operation.
     */
    template <typename T>
    class ThreadSafeStack
    {
    public:
        /**
         * @brief Creates a new, empty container.
         */
        ThreadSafeStack() noexcept = default;

        virtual ~ThreadSafeStack() = default;

        ThreadSafeStack(const ThreadSafeStack& other) noexcept {
            *this = other;
        }

        ThreadSafeStack(ThreadSafeStack&& other) noexcept {
            *this = std::move(other);
        };

        ThreadSafeStack& operator=(const ThreadSafeStack& other) noexcept {
            if (this != &other) {
                std::lock_guard<std::mutex> guard1{ this->_mutex };
                std::lock_guard<std::mutex> guard2{ other._mutex };
                this->_stack = other._stack;
            }
            return *this;
        }

        ThreadSafeStack& operator=(ThreadSafeStack&& other) noexcept {
            if (this != &other) {
                std::lock_guard<std::mutex> guard1{ this->_mutex };
                std::lock_guard<std::mutex> guard2{ other._mutex };
                this->_stack = std::move(other._stack);
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
            return this->_stack.emplace(std::forward<Args...>(args...));
        }

        /**
         * @brief Checks if the underlying container has no elements.
         * @return true if the underlying container is empty, otherwise false.
         */
        bool empty() const {
            std::lock_guard guard{ this->_mutex };
            return this->_stack.empty();
        }

        /**
         * @brief Returns a reference to the top element in the stack.
         * @return Reference to the last element.
         */
        T& top() {
            std::lock_guard guard{ this->_mutex };
            return this->_stack.top();
        }

        /**
         * @brief Returns a const reference to the top element in the stack.
         * @return Reference to the last element.
         */
        const T& top() const {
            std::lock_guard guard{ this->_mutex };
            return this->_stack.top();
        }

        /**
         * @brief Removes the top element from the stack, if possible, and returns it.
         * @return A std::optional<T> containing the element popped off the stack, or nothing
         * if the underlying container is empty.
         */
        std::optional<T> pop() {
            std::lock_guard guard{ this->_mutex };
            if (this->_stack.empty()) {
                return {};
            }
            T returned = std::move(this->_stack.top());
            this->_stack.pop();
            return returned;
        }

        /**
         * @brief Pushes the given element \p item to the top of the stack.
         * @param item The value of the element to push.
         */
        void push(const T& item) {
            std::lock_guard guard{ this->_mutex };
            this->_stack.push(item);
        }

        /**
         * @brief Pushes the given element \p item to the top of the stack.
         * @param item The value of the element to push.
         */
        void push(T&& item) {
            std::lock_guard guard{ this->_mutex };
            this->_stack.push(std::move(item));
        }

        /**
         * @brief Returns the number of elements in the underlying container.
         * @return The number of elements in the container.
         */
        size_t size() const {
            std::lock_guard guard{ this->_mutex };
            return this->_stack.size();
        }

    private:
        mutable std::mutex _mutex{};
        std::stack<T> _stack{};
    };
}
