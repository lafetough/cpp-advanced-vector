#pragma once
#include <cassert>
#include <cstdlib>
#include <new>
#include <utility>
#include <memory>
#include <iostream>

template <typename T>
class RawMemory {
public:
    RawMemory() = default;

    explicit RawMemory(size_t capacity)
        : buffer_(Allocate(capacity))
        , capacity_(capacity) {
    }

    RawMemory(RawMemory&& other) noexcept
        :buffer_(other.buffer_), capacity_(other.capacity_)
    {
        other.buffer_ = nullptr;
        other.capacity_ = 0;
    }

    RawMemory& operator=(RawMemory&& rhs) noexcept
    {
        buffer_ = rhs.buffer_;
        capacity_ = rhs.capacity_;
        rhs.capacity_ = 0;
        rhs.buffer_ = nullptr;
        return *this;
    }

    RawMemory& operator=(const RawMemory&) = delete;
    RawMemory(const RawMemory&) = delete;

    ~RawMemory() {
        Deallocate(buffer_);
    }

    T* operator+(size_t offset) noexcept {
        assert(offset <= capacity_);
        return buffer_ + offset;
    }

    const T* operator+(size_t offset) const noexcept {
        return const_cast<RawMemory&>(*this) + offset;
    }

    const T& operator[](size_t index) const noexcept {
        return const_cast<RawMemory&>(*this)[index];
    }

    T& operator[](size_t index) noexcept {
        assert(index < capacity_);
        return buffer_[index];
    }

    void Swap(RawMemory& other) noexcept {
        std::swap(buffer_, other.buffer_);
        std::swap(capacity_, other.capacity_);
    }

    const T* GetAddress() const noexcept {
        return buffer_;
    }

    T* GetAddress() noexcept {
        return buffer_;
    }

    size_t Capacity() const {
        return capacity_;
    }

private:
    // Выделяет сырую память под n элементов и возвращает указатель на неё
    static T* Allocate(size_t n) {
        return n != 0 ? static_cast<T*>(operator new(n * sizeof(T))) : nullptr;
    }

    // Освобождает сырую память, выделенную ранее по адресу buf при помощи Allocate
    static void Deallocate(T* buf) noexcept {
        operator delete(buf);
    }

    T* buffer_ = nullptr;
    size_t capacity_ = 0;
};

template <typename T>
class Vector {
public:

    Vector() = default;

    explicit Vector(size_t size)
        : data_(size)
        , size_(size)  //
    {
        std::uninitialized_value_construct_n(data_.GetAddress(), size);
    }

    Vector(const Vector& other)
        : data_(other.size_)
        , size_(other.size_)
    {
        std::uninitialized_copy_n(other.data_.GetAddress(), other.size_, data_.GetAddress());
    }

    Vector(Vector&& other) noexcept
        :data_(std::move(other.data_)), size_(std::move(other.size_))
    {}

    Vector& operator=(const Vector& rhs) {
        if (this != &rhs) {
            if (rhs.size_ > data_.Capacity()) {
                Vector rhs_copy(rhs);
                Swap(rhs_copy);
            }
            else {
                if (rhs.size_ >= size_) {
                    for (size_t i = 0; i != size_; ++i) {
                        data_[i] = rhs.data_[i];
                    }
                    std::uninitialized_copy_n(rhs.data_.GetAddress(), rhs.size_ - size_, data_.GetAddress() + size_);
                }
                else {
                    for (size_t i = 0; i != rhs.size_; ++i) {
                        data_[i] = rhs.data_[i];
                    }
                    std::destroy_n(data_.GetAddress() + rhs.size_, size_ - rhs.size_);
                }
                size_ = rhs.size_;
            }
        }
        return *this;
    }
    Vector& operator=(Vector&& rhs) noexcept {
        if (this != &rhs) {
            data_ = std::move(rhs.data_);
            size_ = std::move(rhs.size_);
        }
        return *this;
    }

    void Swap(Vector& other) noexcept {
        data_.Swap(other.data_);
        std::swap(size_, other.size_);
    }

    using iterator = T*;
    using const_iterator = const T*;

    iterator begin() noexcept {
        return data_.GetAddress();
    }
    iterator end() noexcept {
        return data_.GetAddress() + size_;
    }
    const_iterator begin() const noexcept {
        return data_.GetAddress();
    }
    const_iterator end() const noexcept {
        return data_.GetAddress() + size_;
    }
    const_iterator cbegin() const noexcept {
        return data_.GetAddress();
    }
    const_iterator cend() const noexcept {
        return data_.GetAddress() + size_;
    }

    void Reserve(size_t new_capacity) {
        if (new_capacity <= data_.Capacity()) {
            return;
        }
        RawMemory<T> new_data(new_capacity);

        if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
            std::uninitialized_move_n(data_.GetAddress(), size_, new_data.GetAddress());
        }
        else {
            std::uninitialized_copy_n(data_.GetAddress(), size_, new_data.GetAddress());
        }
        std::destroy_n(data_.GetAddress(), size_);
        data_.Swap(new_data);
    }

    void Resize(size_t new_size) {
        if (new_size == size_) {
            return;
        }

        if (new_size > size_) {
            this->Reserve(new_size);
            std::uninitialized_value_construct_n(data_.GetAddress() + size_, new_size - size_);
        }
        else {
            std::destroy_n(data_.GetAddress() + new_size, size_ - new_size);
        }
        size_ = new_size;
    }

    void PushBack(const T& value) {
        if (size_ == Capacity()) {
            RawMemory<T> new_memory = NewExpendedMem();
            new(new_memory.GetAddress() + size_) T(value);
            Realloc(new_memory);
        }
        else {
            new(data_.GetAddress() + size_) T(value);
        }
        ++size_;
    }
    void PushBack(T&& value) {
        if (size_ == Capacity()) {
            RawMemory<T> new_memory = NewExpendedMem();
            new(new_memory.GetAddress() + size_) T(std::move(value));
            Realloc(new_memory);
        }
        else {
            new(data_.GetAddress() + size_) T(std::move(value));
        }
        ++size_;
    }

    void PopBack() noexcept {
        std::destroy_at(data_.GetAddress() + size_ - 1);
        --size_;
    }

    template<typename... Args>
    T& EmplaceBack(Args&&... args) {

        T* emplaced;

        if (size_ == Capacity()) {
            RawMemory<T> new_memory = NewExpendedMem();
            emplaced = new(new_memory.GetAddress() + size_) T(std::forward<Args>(args)...);
            Realloc(new_memory);
        }
        else {
            emplaced = new(data_.GetAddress() + size_) T(std::forward<Args>(args)...);
        }
        ++size_;
        return *emplaced;
    }

    template<typename... Args>
    iterator Emplace(const_iterator pos, Args&&... args) {
        if (pos == end()) {
            return &EmplaceBack(std::forward<Args>(args)...);
        }

        auto iter = const_cast<iterator>(pos);

        if (size_ == Capacity()) {
            size_t offset = static_cast<size_t>(iter - begin());
            RawMemory<T> new_data = NewExpendedMem();

            auto temp = new(new_data.GetAddress() + offset) T(std::forward<Args>(args)...);

            if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
                std::uninitialized_move(begin(), iter, new_data.GetAddress());
                std::uninitialized_move(iter, end(), new_data.GetAddress() + offset + 1);
            }
            else {
                std::uninitialized_copy(begin(), iter, new_data.GetAddress());
                std::uninitialized_copy(iter, end(), new_data.GetAddress() + offset + 1);
            }
            std::destroy_n(begin(), size_);
            data_.Swap(new_data);
            iter = temp;

        }
        else {
            auto temp = T(std::forward<Args>(args)...);
            new(end()) T(std::forward<T>(*(end() - 1)));
            std::move_backward(iter, end() - 1, end());
            *iter = std::move(temp);
        }
        ++size_;
        return iter;
    }

    iterator Insert(const_iterator pos, const T& value) {
        return Emplace(pos, value);
    }
    iterator Insert(const_iterator pos, T&& value) {
        return Emplace(pos, std::move(value));
    }

    iterator Erase(const_iterator pos) {
        iterator iter = const_cast<iterator>(pos);
        std::move(iter + 1, end(), iter);
        std::destroy_at(end() - 1);
        --size_;
        return iter;
    }

    size_t Size() const noexcept {
        return size_;
    }

    size_t Capacity() const noexcept {
        return data_.Capacity();
    }

    const T& operator[](size_t index) const noexcept {
        return const_cast<Vector&>(*this)[index];
    }

    T& operator[](size_t index) noexcept {
        assert(index < size_);
        return data_[index];
    }

    ~Vector() {

        if (data_.GetAddress() != nullptr) {
            std::destroy_n(data_.GetAddress(), size_);
        }
    }

private:
    RawMemory<T> data_;
    size_t size_ = 0;

    void Realloc(RawMemory<T>& new_data) {
        if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
            std::uninitialized_move_n(data_.GetAddress(), size_, new_data.GetAddress());
        }
        else {
            std::uninitialized_copy_n(data_.GetAddress(), size_, new_data.GetAddress());
        }
        std::destroy_n(data_.GetAddress(), size_);
        data_.Swap(new_data);
    }

    RawMemory<T> NewExpendedMem() {
        size_t new_cap = size_ == 0 ? 1 : size_ * 2;
        RawMemory<T> new_memory(new_cap);
        return new_memory;
    }

};