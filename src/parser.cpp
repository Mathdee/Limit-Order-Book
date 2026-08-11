#include <sys/stat.h>
#include <sys/mman.h>
#include <unistd.h>
#include "itch_messages.hpp"
#include "order_book.hpp"

#include <unordered_map>
#include <iostream>
#include <cstdio>
#include <filesystem>
#include <cstdint>
#include <array>
#include <cstring>
#include <chrono>


int main(){

    std::cout << std::filesystem::current_path() << '\n';
    FILE* file = std::fopen(
        "/home/mathdee/itch_data/12302019.NASDAQ_ITCH50",
        "rb"
    );

    if(!file){

        std::perror("File not returned");
        return 1;
    }
    int fd = fileno(file);

    struct stat fileInfo;
    fstat(fd, &fileInfo); 

    size_t size = fileInfo.st_size;

    

    uint8_t* data = static_cast<uint8_t*>(
        mmap(
            nullptr,     // let OS choose address.
            size,        // number of bytes on map
            PROT_READ,   // only reading
            MAP_PRIVATE, // changes are not written back
            fd,
            0            // offset from beginning of file
        )
    );

    if(data == MAP_FAILED){
        std::perror("mmap failed");
        fclose(file);
        return 1;
    }

    
    uint64_t counts[256] = {0};

    uint64_t total_messages = 0;
    size_t pos = 0;

     //AAPL locate, state
    uint16_t aapl_locate = 0;
    bool have_aapl = false;
    OrderBook book;
    uint64_t aapl_messages = 0;

    uint64_t missing_references = 0;
    uint64_t invariant_fails = 0;

    auto start = std::chrono::high_resolution_clock::now();
    while(pos +2 <= size){

        uint16_t length = itch::be16(data + pos);
        if(pos + 2 + length > size){
            std::cerr << "Corrupt message at: " << pos << "\n";
            break;
        }

        uint8_t* body = data + pos + 2;

        uint8_t type = body[0];
        counts[type]++;
        total_messages++;

        //find AAPL's locate from Stock Directory
        if (type == 'R') {
            auto r = itch::StockDirectory::decode(body);
            // exactly 8 chars: 'A' 'A' 'P' 'L' ' ' ' ' ' ' ' '
            if (std::memcmp(r.stock.data(), "AAPL    ", 8) == 0) {
                aapl_locate = r.stock_locate;
                have_aapl   = true;
                std::cout << "AAPL locate = " << aapl_locate << "\n";
            }
        }
        //keep only AAPL order messages 
        else if(type == 'A' || type == 'F'|| type == 'E' || type == 'C' || type == 'X' || type == 'D'|| type == 'U'){
            uint16_t locate = itch::be16(body + 1);
            if(have_aapl && locate == aapl_locate){
                ++aapl_messages;
                //also need to decode + book.add / reduce/ delete/ replace
                switch (type) {
                    case 'A': {
                        auto m = itch::AddOrder::decode(body);
                        book.add(m.order_ref, m.side, m.price, m.shares);
                        break;
                    }
                    case 'F': {
                        auto m = itch::AddOrderMPID::decode(body);
                        book.add(m.order_ref, m.side, m.price, m.shares);
                        break;
                    }
                    case 'X': {
                        auto m = itch::OrderCancel::decode(body);
                        if (!book.reduce(m.order_ref, m.cancelled_shares)) ++missing_references;
                        break;
                    }
                    case 'E': {
                        auto m = itch::OrderExecuted::decode(body);
                        if (!book.reduce(m.order_ref, m.executed_shares)) ++missing_references;
                        break;
                    }
                    case 'C': {
                        auto m = itch::OrderExecutedWithPrice::decode(body);
                        if (!book.reduce(m.order_ref, m.executed_shares)) ++missing_references;
                        break;
                    }
                    case 'D': {
                        auto m = itch::OrderDelete::decode(body);
                        if (!book.delete_order(m.order_ref)) ++missing_references;
                        break;
                    }
                    case 'U': {
                        auto m = itch::OrderReplace::decode(body);
                        if (!book.replace(m.orig_order_ref, m.new_order_ref, m.price, m.shares))
                            ++missing_references;
                        break;
                    }
                    default:
                        break;
                }

                if(!book.check_invariants()){
                    ++invariant_fails;
                    if(invariant_fails == 1){
                        uint32_t bid = 0, ask = 0;
                        bool high_bid = book.best_bid(bid);
                        bool high_ask = book.best_ask(ask);

                        std::cerr << "First invariant fail at message type = " << static_cast<char>(type)
                                  << " bid = " << (high_bid? std::to_string(bid) : "none")
                                  << " ask = " << (high_ask? std::to_string(ask) : "none")
                                  << "\n";
                        
                    }
                }
            }
        }

        pos += 2 + length;
    }

    auto end = std::chrono::high_resolution_clock::now();

    double seconds = std::chrono::duration<double>(end- start).count();

    
    for(int i = 0; i< 256; ++i){
        if(counts[i] > 0){
            std::cout << static_cast<char>(i) << " : " << counts[i] << "\n";
        }
    }
    
    std::cout<< "Total messages: " << total_messages << "\n";

    std::cout<< "Messages/sec: " << total_messages / seconds << "\n";


    std::cout << "AAPL messages kept: " <<aapl_messages << "\n";
    std::cout << "Missing order refs: " <<missing_references << "\n";
    std::cout << "Invariant failures: " << invariant_fails << "\n";

    std::cout << data[0] << "\n";
    munmap(data, size);
    fclose(file);
    return 0;

}