#include "waitgroup.h"
#include <stdexcept>
#include <utility>

// 平台相关的 CPU 暂停指令，用于自适应自旋
#if defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || defined(_M_IX86)
#include <immintrin.h>
#define WG_CPU_PAUSE() _mm_pause()
#elif defined(__aarch64__) || defined(__arm__)
#define WG_CPU_PAUSE() __asm__ __volatile__("yield" ::: "memory")
#else
#define WG_CPU_PAUSE() ((void)0)
#endif

static constexpr int kMaxSpinIterations = 64;

WaitGroup::WaitGroup(WaitGroup&& other) noexcept
    : counter_(other.counter_.load(std::memory_order_acquire))
{
    // 必须用 acq_rel，确保移动后的状态对其他线程立即可见
    other.counter_.store(0, std::memory_order_release);
}

void WaitGroup::Add(int delta) {
    int old_counter = counter_.fetch_add(delta, std::memory_order_acq_rel);
    int new_counter = old_counter + delta;

    if (new_counter < 0) {
        // ⚠️ 关键修复：不要尝试 store(old_counter) 回滚！
        // 并发场景下多个协程同时触发负数，各自的回滚会互相覆盖导致计数器永久损坏。
        // Go 的做法是直接 panic，C++ 中我们抛出异常，由调用方保证 "Done 次数不超过 Add" 的契约。
        throw std::runtime_error("sync: negative WaitGroup counter");
    }

    if (new_counter == 0) {
        // 计数器归零，唤醒所有等待者
        // 注意：这里不需要持锁 notify_all，但为了配合 cv_.wait 的 predicate 检查，
        // 持锁可以避免 lost wakeup 问题（libgo 的 co_condition_variable 行为可能与 std 不同）
        std::unique_lock<co_mutex> lock(mtx_);
        cv_.notify_all();
    }
}

void WaitGroup::Done() {
    Add(-1);
}

void WaitGroup::Wait() {
    // 自适应自旋快速路径：分级退避避免总线风暴
    for (int i = 0; i < kMaxSpinIterations; ++i) {
        if (counter_.load(std::memory_order_acquire) == 0) {
            return;
        }

        // 分级退避策略
        if (i < 8) {
            WG_CPU_PAUSE();           // 前8次：单次 pause，低延迟
        }
        else if (i < 32) {
            WG_CPU_PAUSE();           // 中间24次：单次 pause
            WG_CPU_PAUSE();           // 双次 pause，增加退避间隔
        }
        else {
            WG_CPU_PAUSE();           // 后32次：四次 pause，最大退避
            WG_CPU_PAUSE();
            WG_CPU_PAUSE();
            WG_CPU_PAUSE();
        }
    }

    // CV 慢速路径
    std::unique_lock<co_mutex> lock(mtx_);
    cv_.wait(lock, [this] {
        return counter_.load(std::memory_order_relaxed) == 0;
        });
}

// ✅ Go() 方法移至 .cpp 中实现，便于包含日志头文件和异常处理
void WaitGroup::Go(std::function<void()> f) {
    Add(1);
    go[this, f = std::move(f)]() {
        // RAII Guard 确保任何退出路径都会调用 Done()
        struct Guard {
            WaitGroup* _wg;
            ~Guard() { _wg->Done(); }
        } guard{ this };

        try {
            f();
        }
        catch (const std::exception& e) {
            // ⚠️ C++ 协程没有 Go 的 panic 传播机制
            // 未捕获异常会导致 libgo 调度器 UB 或静默丢失错误
            // 这里必须记录日志，Guard 仍会确保 Done() 被调用
            fprintf(stderr, "[WaitGroup::Go] task exception: %s\n", e.what());
        }
        catch (...) {
            fprintf(stderr, "[WaitGroup::Go] task threw unknown exception\n");
        }
    };
}