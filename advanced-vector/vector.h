#pragma once
#include <cassert>
#include <cstdlib>
#include <new>
#include <utility>
#include <memory>

template <typename T>
class RawMemory {
public:
    RawMemory() = default;
    RawMemory(const RawMemory&) = delete;
    RawMemory& operator=(const RawMemory& rhs) = delete;

    explicit RawMemory(size_t capacity)
        : buffer_(Allocate(capacity))
        , capacity_(capacity) {
    }

    ~RawMemory() {
        Deallocate(buffer_);
    }

    T* operator+(size_t offset) noexcept {
        // Разрешается получать адрес ячейки памяти, следующей за последним элементом массива
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

    explicit Vector(size_t size) : data_(size) {
        std::uninitialized_value_construct_n(data_.GetAddress(), size);
        size_ = size;
    }

    Vector(const Vector& other) : data_(other.size_) {
        std::uninitialized_copy_n(other.data_.GetAddress(), other.size_, data_.GetAddress());
        size_ = other.size_;
    }

    Vector(Vector&& other) noexcept {
        this->Swap(other);
    }

        Vector<T>& operator=(const Vector& rhs) {
        if (this != &rhs) {
            if (rhs.size_ > data_.Capacity()) {
                Vector<T> rhs_copy(rhs);
                this->Swap(rhs_copy);
            } else {
                for (size_t i = 0; i < std::min(rhs.size_, size_); ++i) {
                    data_[i] = rhs.data_[i];
                }
                if (rhs.size_ < size_) {
                    std::destroy_n(data_.GetAddress() + rhs.size_, size_ - rhs.size_);
                } else {
                    std::uninitialized_copy_n(rhs.data_.GetAddress() + size_, rhs.size_ - size_, data_.GetAddress() + size_);
                }
                size_ = rhs.size_;
            }
        }
        return *this;
    }

    Vector& operator=(Vector&& rhs) noexcept {
        if (this != &rhs) {
            this->Swap(rhs);
        }
        return *this;
    }

    using iterator = T*;
    using const_iterator = const T*;
    

    const_iterator cbegin() const noexcept {
        return data_.GetAddress();
    }
    const_iterator cend() const noexcept {
        return (data_.GetAddress() + size_);
    }
    iterator begin() noexcept {
        return data_.GetAddress();
    }
    iterator end() noexcept {
        return data_.GetAddress() + size_;
    }
    const_iterator begin() const noexcept {
        return cbegin();
    }
    const_iterator end() const noexcept {
        return cend();
    }

    void Swap(Vector& other) noexcept {
        data_.Swap(other.data_);
        std::swap(size_, other.size_);
    }


    void Reserve(size_t new_capacity) {
        if (new_capacity <= data_.Capacity()) {
            return;
        }
        RawMemory<T> new_data{new_capacity};

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
        Reserve(new_size);
        if (size_ < new_size) {
            std::uninitialized_value_construct_n(data_.GetAddress() + size_, new_size - Size());
        }
        if (size_ > new_size) {
            std::destroy_n(data_.GetAddress() + new_size, size_ - new_size);
        }
        size_ = new_size;
    }    

    template <typename... Args>
    T& EmplaceBack(Args&&... args){
        if (size_ == Capacity()) {
            RawMemory<T> new_data(Capacity() == 0 ? 1 : size_ * 2);
            new (new_data + size_) T(std::forward<Args>(args)...);
            if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
                std::uninitialized_move_n(data_.GetAddress(), size_, new_data.GetAddress());
            }
            else {
                std::uninitialized_copy_n(data_.GetAddress(), size_, new_data.GetAddress());
            }
            std::destroy_n(data_.GetAddress(), size_);
            data_.Swap(new_data);
        } else {
            new (data_ + size_) T(std::forward<Args>(args)...);
        }
        size_++;
        return *(data_.GetAddress() + size_ - 1);
    }

    template <typename F>
    void PushBack(F&& value) {
        EmplaceBack(std::forward<F>(value));
    }

    void PopBack() {
        std::destroy_at(data_.GetAddress() + size_ - 1);
        --size_;
    }

    template <typename... Args>
    iterator Emplace(const_iterator position, Args&&... args){
        if (position == cend()) {
            EmplaceBack(std::forward<Args>(args)...);
            return end() - 1;
        }
        size_t index = position - begin();
        if (size_ < Capacity()){
            return EmplaceEnoughtCapacity(index, std::forward<Args>(args)...);
        }
        else {
            return EmplaceNoCapacity(index, std::forward<Args>(args)...);
        }
    }

    iterator Insert(const_iterator position, const T& value){
        return Emplace(position, value);
    }

    iterator Insert(const_iterator position, T&& value){
        return Emplace(position, std::move(value));
    }

    iterator Erase(const_iterator position) {
        size_t distance = position - begin();
        std::move(begin() + distance + 1, end(), begin() + distance);
        std::destroy_at(data_.GetAddress() + size_ - 1);
        --size_;
        return data_.GetAddress() + distance;
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
        std::destroy_n(data_.GetAddress(), size_);
    }

private:

    static void DestroyN(T* buf, size_t n) noexcept {
        for (size_t i = 0; i != n; ++i) {
            Destroy(buf + i);
        }
    }


    static void CopyConstruct(T* buf, const T& elem) {
        new (buf) T(elem);
    }


    static void Destroy(T* buf) noexcept {
        buf->~T();
    }

    template <typename... Args>
    iterator EmplaceEnoughtCapacity(size_t index, Args&&... args){
        new (data_.GetAddress() + size_) T(std::move(data_[size_ - 1]));
        std::move_backward(data_.GetAddress() + index, end() - 1, end());
        data_[index] = T(std::forward<Args>(args)...);
        ++size_;
        return data_.GetAddress() + index;
    }

    template <typename... Args>
    iterator EmplaceNoCapacity(size_t index, Args&&... args){
        RawMemory<T> new_data(Capacity() == 0 ? 1 : size_ * 2);
        if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
            new (new_data.GetAddress() + index) T(std::forward<Args>(args)...);
            std::uninitialized_move_n(begin(), index, new_data.GetAddress());
            std::uninitialized_move_n(begin() + index, size_ - index, new_data.GetAddress() + index + 1);
        }
        else {
            new (new_data.GetAddress() + index) T(std::forward<Args>(args)...);
            std::uninitialized_copy_n(begin(), index, new_data.GetAddress());
            std::uninitialized_copy_n(begin() + index, size_ - index, new_data.GetAddress() + index + 1);
        }
        std::destroy_n(data_.GetAddress(), size_);
        data_.Swap(new_data);
        ++size_;
        return data_.GetAddress() + index;
    }


    RawMemory<T> data_;
    size_t size_ = 0;
};