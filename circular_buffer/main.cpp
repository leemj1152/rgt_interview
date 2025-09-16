#include <iostream>
#include <memory>
#include <stdexcept>
#include <iterator>
#include <algorithm>
#include <numeric>
#include <iomanip>
#include <windows.h>

// ======================= CircularBuffer =======================
template <typename T>
class CircularBuffer {
public:
    explicit CircularBuffer(std::size_t capacity)
        : cap_(capacity), data_(std::make_unique<T[]>(capacity)),
          head_(0), tail_(0), size_(0) {
        if (cap_ == 0) throw std::invalid_argument("capacity must be > 0");
    }

    void push_back(const T& item) {
        data_[tail_] = item;
        if (size_ == cap_) {
            head_ = (head_ + 1) % cap_; // overwrite 정책(가장 오래된 것 버림)
        } else {
            ++size_;
        }
        tail_ = (tail_ + 1) % cap_;
    }

    void pop_front() {
        if (empty()) throw std::out_of_range("pop_front on empty buffer");
        head_ = (head_ + 1) % cap_;
        --size_;
    }

    T&       front()       { if (empty()) throw std::out_of_range("front on empty"); return data_[head_]; }
    const T& front() const { if (empty()) throw std::out_of_range("front on empty"); return data_[head_]; }

    T&       back()        { if (empty()) throw std::out_of_range("back on empty");  return data_[(tail_ + cap_ - 1) % cap_]; }
    const T& back()  const { if (empty()) throw std::out_of_range("back on empty");  return data_[(tail_ + cap_ - 1) % cap_]; }

    std::size_t size()     const noexcept { return size_; }
    std::size_t capacity() const noexcept { return cap_;  }
    bool        empty()    const noexcept { return size_ == 0; }

    // ---- STL forward iterator (const/non-const) ----
    template <bool IsConst>
    class Iter {
        using Buf = std::conditional_t<IsConst, const CircularBuffer, CircularBuffer>;
        using Ref = std::conditional_t<IsConst, const T&, T&>;
        using Ptr = std::conditional_t<IsConst, const T*, T*>;
    public:
        using iterator_category = std::forward_iterator_tag;
        using value_type        = T;
        using difference_type   = std::ptrdiff_t;
        using reference         = Ref;
        using pointer           = Ptr;

        Iter(Buf* buf, std::size_t idx, std::size_t left) : buf_(buf), idx_(idx), left_(left) {}
        reference operator*() const { return buf_->data_[idx_]; }
        pointer   operator->() const { return &buf_->data_[idx_]; }
        Iter& operator++() { if (left_) { idx_ = (idx_ + 1) % buf_->cap_; --left_; } return *this; }
        Iter operator++(int) { auto tmp = *this; ++(*this); return tmp; }
        friend bool operator==(const Iter& a, const Iter& b){ return a.buf_==b.buf_ && a.idx_==b.idx_ && a.left_==b.left_; }
        friend bool operator!=(const Iter& a, const Iter& b){ return !(a==b); }
    private:
        Buf* buf_; std::size_t idx_; std::size_t left_;
    };

    using iterator       = Iter<false>;
    using const_iterator = Iter<true>;
    iterator begin()             { return iterator(this, head_, size_); }
    iterator end()               { return iterator(this, (head_ + size_) % cap_, 0); }
    const_iterator begin() const { return cbegin(); }
    const_iterator end()   const { return cend();   }
    const_iterator cbegin() const{ return const_iterator(this, head_, size_); }
    const_iterator cend()   const{ return const_iterator(this, (head_ + size_) % cap_, 0); }

    // 디버깅/출력용: 내부 인덱스 순서(raw order)로 접근
    T at_index(std::size_t i) const { return data_[i]; } // i는 0..cap_-1
    std::size_t head_index() const { return head_; }

private:
    std::size_t cap_;
    std::unique_ptr<T[]> data_;
    std::size_t head_;  // front
    std::size_t tail_;  // next write
    std::size_t size_;
};
// ===================== end CircularBuffer =====================

// ---------------------- Demo (요구 예제 출력) ----------------------
int main() {
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
    CircularBuffer<double> tempBuffer(5);

    // 원본 센서 데이터 추가
    tempBuffer.push_back(23.5);
    tempBuffer.push_back(24.1);
    tempBuffer.push_back(23.8);
    tempBuffer.push_back(25.2);
    tempBuffer.push_back(24.7);
    // 버퍼가 가득 찬 뒤 추가 → 가장 오래된 값(23.5) 오버라이트
    tempBuffer.push_back(26.1);

    // 1) "버퍼 내용 (인덱스 순서)" : 내부 배열 순서 그대로 출력
    std::cout << u8"버퍼 내용 (인덱스 순서): [";
    for (std::size_t i = 0; i < tempBuffer.capacity(); ++i) {
        std::cout << std::fixed << std::setprecision(1) << tempBuffer.at_index(i);
        if (i + 1 != tempBuffer.capacity()) std::cout << ", ";
    }
    std::cout << "]\n";

    // 2) begin()부터 순회(가장 오래된 것부터 최신 순)
    std::cout << u8"begin()부터 순회 시: ";
    bool first = true;
    for (auto v : tempBuffer) {
        if (!first) std::cout << ", ";
        std::cout << std::fixed << std::setprecision(1) << v;
        first = false;
    }
    std::cout << u8" (가장 오래된 것부터)\n\n";

    // 3) 메타 정보 및 통계(STL 사용)
    std::cout << "tempBuffer.size()    = " << tempBuffer.size() << "\n";
    std::cout << "tempBuffer.capacity()= " << tempBuffer.capacity() << "\n";
    std::cout << "tempBuffer.empty()   = " << std::boolalpha << tempBuffer.empty() << "\n";

    double maxTemp = *std::max_element(tempBuffer.begin(), tempBuffer.end());
    double sumTemp = std::accumulate(tempBuffer.begin(), tempBuffer.end(), 0.0);
    double avgTemp = sumTemp / static_cast<double>(tempBuffer.size());

    std::cout << "maxTemp = " << std::fixed << std::setprecision(1) << maxTemp << "\n";
    std::cout << "avgTemp = " << std::fixed << std::setprecision(2) << avgTemp << "\n";

    std::cout << u8"tempBuffer.front() = " << std::fixed << std::setprecision(1) << tempBuffer.front() << "  // 가장 오래된 데이터\n";
    std::cout << u8"tempBuffer.back()  = " << std::fixed << std::setprecision(1) << tempBuffer.back()  << "  // 가장 최근 데이터\n";
    return 0;
}
