#include <benchmark/benchmark.h>

#include "benchmark.hpp"
#include "fixtures/viper_fixture.hpp"
#include "fixtures/dram_map_fixture.hpp"
#include "fixtures/faster_fixture.hpp"
#include "fixtures/pmem_kv_fixture.hpp"

using namespace viper::kv_bm;

constexpr size_t VAR_SIZES_NUM_REPETITIONS = 1;
constexpr double VAR_SIZES_SCALE_FACTOR = 1.0;
constexpr size_t VAR_SIZES_PREFILL_SIZE = 0.1 * (1000l * 1000 * 1000) * VAR_SIZES_SCALE_FACTOR;
constexpr size_t VAR_SIZES_INSERT_SIZE = VAR_SIZES_PREFILL_SIZE / 2;
constexpr size_t VAR_SIZES_NUM_FINDS = 5'000'000 * VAR_SIZES_SCALE_FACTOR;

#define GENERAL_ARGS \
            ->Repetitions(VAR_SIZES_NUM_REPETITIONS) \
            ->Iterations(1) \
            ->Unit(BM_TIME_UNIT) \
            ->UseRealTime() \
            ->ThreadRange(1, NUM_MAX_THREADS) \
            ->Threads(24)

#define DEFINE_BM_INTERNAL(fixture, method, KS, VS) \
        BENCHMARK_TEMPLATE_DEFINE_F(fixture, method ##_ ##KS ##_ ##VS,  \
                                    std::string, std::string, viper::internal::KeyCompare<std::string>)(benchmark::State& state) { \
            bm_##method(state, *this, KS, VS); \
        } \
        BENCHMARK_REGISTER_F(fixture, method ##_ ##KS ##_ ##VS) GENERAL_ARGS

#define DEFINE_BM(fixture, KS, VS) \
        DEFINE_BM_INTERNAL(fixture, get, KS, VS) \
            ->Args({VAR_SIZES_PREFILL_SIZE / (KS + VS), VAR_SIZES_NUM_FINDS}); \
        DEFINE_BM_INTERNAL(fixture, insert, KS, VS) \
            ->Args({VAR_SIZES_PREFILL_SIZE / (KS + VS), VAR_SIZES_INSERT_SIZE / (KS + VS)}); \

#define DEFINE_ALL_BMS(fixture) \
        DEFINE_BM(fixture, 10, 50); \
        DEFINE_BM(fixture, 100, 250)

inline void bm_insert(benchmark::State& state, BaseFixture& fixture, size_t key_size, size_t value_size) {
    const uint64_t num_total_prefill = state.range(0);
    const uint64_t num_total_inserts = state.range(1);

    set_cpu_affinity(state.thread_index);

    if (is_init_thread(state)) {
        const size_t num_total_strings = num_total_prefill + num_total_inserts;
        fixture.generate_strings(num_total_strings, key_size, value_size);
        fixture.InitMap(num_total_prefill);
    }

    const uint64_t num_inserts_per_thread = num_total_inserts / state.threads;
    const uint64_t start_idx = (state.thread_index * num_inserts_per_thread) + num_total_prefill;
    const uint64_t end_idx = start_idx + num_inserts_per_thread;

    for (auto _ : state) {
        fixture.setup_and_insert(start_idx, end_idx);

    }

    state.SetItemsProcessed(num_inserts_per_thread);

    if (is_init_thread(state)) {
        fixture.DeInitMap();
    }
}

inline void bm_get(benchmark::State& state, BaseFixture& fixture, size_t key_size, size_t value_size) {
    const uint64_t num_total_prefill = state.range(0);
    const uint64_t num_total_finds = state.range(1);

    set_cpu_affinity(state.thread_index);

    if (is_init_thread(state)) {
        const size_t num_total_strings = num_total_prefill;
        fixture.generate_strings(num_total_strings, key_size, value_size);
        fixture.InitMap(num_total_prefill);
    }

    const uint64_t num_finds_per_thread = num_total_finds / state.threads;
    const uint64_t start_idx = 0;
    const uint64_t end_idx = num_total_prefill - state.threads;

    for (auto _ : state) {
        fixture.setup_and_find(start_idx, end_idx, num_finds_per_thread);
    }

    state.SetItemsProcessed(num_finds_per_thread);

    if (is_init_thread(state)) {
        fixture.DeInitMap();
    }
}

//DEFINE_ALL_BMS(DramMapFixture);
DEFINE_ALL_BMS(ViperFixture);
//DEFINE_ALL_BMS(NvmFasterFixture);
//DEFINE_ALL_BMS(PmemHybridFasterFixture);
//DEFINE_ALL_BMS(PmemKVFixture);



int main(int argc, char** argv) {
    std::string exec_name = argv[0];
//    const std::string arg = get_output_file("kv_size/kv_size");
//    return bm_main({exec_name, arg});
    return bm_main({exec_name});
}