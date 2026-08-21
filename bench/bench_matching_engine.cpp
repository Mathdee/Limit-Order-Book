#include<matching_engine.hpp>

#include <chrono>
#include <vector>
#include <algorithm>
#include <iostream>
#include <random>

int main(){
    MatchingEngine engine;
    const int N = 1'000'000;
    std::vector<uint64_t> latencies_ns;
    latencies_ns.reserve(N);

    std::mt19937 rng(42); //fixed seed, can reproduce results
    std::uniform_int_distribution<uint32_t> price_dist(9900, 10100);
    std::uniform_int_distribution<uint32_t> quantity_distance(1, 500);

    auto overall_start = std::chrono::high_resolution_clock::now();

    for(int i = 0; i < N; ++i){
        char side = (i % 2 == 0) ? 'B' : 'S';
        uint32_t price = price_dist(rng);
        uint32_t quantity = quantity_distance(rng);

        auto t0 = std::chrono::high_resolution_clock::now();
        engine.submit(i, side, price, quantity);
        auto t1 = std::chrono::high_resolution_clock::now();

        latencies_ns.push_back(std::chrono::duration_cast<std::chrono::nanoseconds>(t1-t0).count());

    }

    auto overall_end = std::chrono::high_resolution_clock::now();
    double seconds = std::chrono::duration<double>(overall_end - overall_start).count();

    std::sort(latencies_ns.begin(), latencies_ns.end());
    uint64_t p50 = latencies_ns[N * 50 / 100];
    uint64_t p99 = latencies_ns[N * 99 / 100];
    uint64_t p999 = latencies_ns[N * 999 / 1000];


    std::cout << "Orders submitted: " << N << "\n";
    std::cout << "Total Time (s): " << seconds << "\n";
    std::cout << "Orders/sec: " << N / seconds << "\n";
    std::cout << "P50 latency (ns): " << p50 << "\n";
    std::cout << "P99 latency (ns): " << p99 << "\n";
    std::cout << "P999 latency (ns): " << p999 << "\n";

    return  0;
}