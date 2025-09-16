#include <algorithm>
#include <chrono>
#include <functional>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <stdexcept>
#include <thread>
#include <type_traits>
#include <vector>
#include <windows.h>

// ======================= ParallelProcessor =======================
// - 고정 타입에 묶이지 않고 함수형 스타일의 parallel_map 제공
// - 입력은 임의 접근(iterator) 컨테이너(예: std::vector) 가정
template <typename T>
class ParallelProcessor {
public:
    explicit ParallelProcessor(std::size_t threads)
        : threads_(threads ? threads : std::thread::hardware_concurrency()) {}

    // 컨테이너 전체를 받아 결과 벡터 반환
    template <typename Func>
    auto parallel_map(const std::vector<T>& input, Func f) const
        -> std::vector<std::invoke_result_t<Func, T>> {
        using Out = std::invoke_result_t<Func, T>;

        const std::size_t n = input.size();
        std::vector<Out> output(n);
        if (n == 0) return output;

        const std::size_t num_threads = std::min<std::size_t>(threads_, n);
        const std::size_t block = n / num_threads;
        const std::size_t rem   = n % num_threads;

        std::vector<std::thread> workers;
        workers.reserve(num_threads);

        std::size_t beginIndex = 0;
        for (std::size_t t = 0; t < num_threads; ++t) {
            const std::size_t count = block + (t < rem ? 1 : 0);
            const std::size_t start = beginIndex;
            const std::size_t end   = start + count;

            workers.emplace_back([&, start, end, f] {
                for (std::size_t i = start; i < end; ++i) {
                    output[i] = f(input[i]);
                }
            });

            beginIndex = end;
        }
        for (auto& th : workers) th.join();
        return output;
    }

private:
    std::size_t threads_;
};
// ===================== end ParallelProcessor =====================

// 단일 스레드 버전(성능 비교용)
template <typename InT, typename Func>
auto sequential_map(const std::vector<InT>& input, Func f)
    -> std::vector<std::invoke_result_t<Func, InT>> {
    using Out = std::invoke_result_t<Func, InT>;
    std::vector<Out> output(input.size());
    for (std::size_t i = 0; i < input.size(); ++i) output[i] = f(input[i]);
    return output;
}

int main() {
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
    using std::cout;
    using std::endl;

    // 100만 픽셀 값 생성: 0,1,2,...,999999
    const std::size_t N = 1'000'000;
    std::vector<int> pixelData(N);
    std::iota(pixelData.begin(), pixelData.end(), 0);

    // 4개 스레드로 ParallelProcessor 생성
    ParallelProcessor<int> processor(4);

    // --- 1) 밝기 증가 필터 (clamp 0..255)
    auto brighten = [](int p) {
        int v = p + 50;
        if (v > 255) v = 255;
        if (v < 0) v = 0;
        return v;
    };

    // 성능 측정: 단일 스레드
    auto t0 = std::chrono::high_resolution_clock::now();
    auto bright_single = sequential_map(pixelData, brighten);
    auto t1 = std::chrono::high_resolution_clock::now();

    // 성능 측정: 병렬
    auto t2 = std::chrono::high_resolution_clock::now();
    auto bright_parallel = processor.parallel_map(pixelData, brighten);
    auto t3 = std::chrono::high_resolution_clock::now();

    auto seq_ms = std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();
    auto par_ms = std::chrono::duration_cast<std::chrono::milliseconds>(t3 - t2).count();
    double speedup = par_ms ? static_cast<double>(seq_ms) / par_ms : 0.0;

    // --- 2) 픽셀을 문자열로 변환
    auto pixelStrings = processor.parallel_map(pixelData, [](int p) {
        return std::string("pixel_") + std::to_string(p);
    });

    // --- 3) 픽셀 제곱 (범위 커지므로 64비트로)
    auto squaredPixels = processor.parallel_map(pixelData, [](int p) {
        return static_cast<long long>(p) * static_cast<long long>(p);
    });

    // ------------------- 출력 (예제 이미지 형식) -------------------
    cout << "// brightenedImage 결과\n";
    cout << "brightenedImage[0]   = "  << bright_parallel[0]        << "  // 0 + 50\n";
    cout << "brightenedImage[1]   = "  << bright_parallel[1]        << "  // 1 + 50\n";
    cout << "brightenedImage[100] = "  << bright_parallel[100]      << "  // 100 + 50\n";
    cout << "brightenedImage[999999] = " << bright_parallel[999999] << " // min(255, 999999 + 50)\n\n";

    cout << "// pixelStrings 결과\n";
    cout << "pixelStrings[0]   = \""   << pixelStrings[0]   << "\"\n";
    cout << "pixelStrings[1]   = \""   << pixelStrings[1]   << "\"\n";
    cout << "pixelStrings[100] = \""   << pixelStrings[100] << "\"\n\n";

    cout << "// squaredPixels 결과\n";
    cout << "squaredPixels[0]  = " << squaredPixels[0]  << "\n";
    cout << "squaredPixels[1]  = " << squaredPixels[1]  << "\n";
    cout << "squaredPixels[10] = " << squaredPixels[10] << "\n\n";

    cout << "// 성능 측정 결과 출력\n";
    cout << "Processing " << N << " elements with 4 threads\n";
    cout << "Sequential time: ~" << seq_ms << "ms\n";
    cout << "Parallel time:   ~" << par_ms << "ms\n";
    cout << "Speedup: " << std::fixed << std::setprecision(1) << speedup << "x\n";

    return 0;
}
