#include <sys/stat.h>
#include <sys/mman.h>
#include <unistd.h>
#include "itch_messages.hpp"
#include "order_book.hpp"
#include "matching_engine.hpp"

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
    // uint16_t aapl_locate = 0;
    // bool have_aapl = false;
    // OrderBook book;
    // uint64_t aapl_messages = 0;

    // needed to locate a specific stock's book.
    // locate is a small int, hashmap works fine for +8000 symbols.
    std::unordered_map<uint16_t, OrderBook> books; 

    uint64_t book_messages = 0; // all message types mentionned
    uint64_t symbols_seen = 0; // counts R messages
    uint64_t missing_references = 0;
    //uint64_t invariant_fails = 0;
    uint64_t crossed_count = 0;
    uint64_t locked_count = 0;
    std::unordered_map<uint16_t, bool> trading;

    struct CrossEvent {
        char     type;
        uint64_t timestamp_ns;
        uint64_t order_ref;
        uint32_t bid;
        uint32_t ask;
    };
    std::unordered_map<uint16_t, CrossEvent> pending_cross; // locate -> the crossing event we're waiting to explain
    uint64_t logged_examples = 0;
    const uint64_t MAX_LOG = 20;

    struct LastExec{
        uint32_t price;
        char side;
        uint64_t ts;
        int chain_len;
    };
    std::unordered_map<uint16_t, LastExec> last_exec; //one active chain per symbol

    struct RefillSample{
        uint16_t locate;
        uint64_t dt_ns;
        int chain_len;
    };
    std::vector<RefillSample> refill_samples;



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


        if(type == 'S'){
            auto s = itch::SystemEvent::decode(body);
            if(s.event_code == 'Q'){
                //market_open = true;
            }
        }
        else if(type == 'H'){
            auto h = itch::StockTradingAction::decode(body);
            trading[h.stock_locate] = (h.trading_state == 'T');
        }

        //find AAPL's locate from Stock Directory
        // doing books[r.stock_locate] will create an empty book when needed.
        if (type == 'R') {
            auto r = itch::StockDirectory::decode(body);
            books.try_emplace(r.stock_locate);
            ++symbols_seen;
            // exactly 8 chars: 'A' 'A' 'P' 'L' ' ' ' ' ' ' ' '
            // if (std::memcmp(r.stock.data(), "AAPL    ", 8) == 0) {
            //     aapl_locate = r.stock_locate;
            //     have_aapl   = true;
            //     std::cout << "AAPL locate = " << aapl_locate << "\n";
            // }
        }
        //keep only AAPL order messages 
        else if(type == 'A' || type == 'F'|| type == 'E' || type == 'C' || type == 'X' || type == 'D'|| type == 'U'){
            uint16_t locate = itch::be16(body + 1);
            OrderBook& book = books[locate];

            // If this locate crossed on a previous message, this is the "next" message, log it.
            auto pend = pending_cross.find(locate);
            if (pend != pending_cross.end() && logged_examples < MAX_LOG) {
                uint64_t this_ts = itch::be48(body + 5);
                uint64_t this_ref = itch::be64(body + 11);
                std::cerr << "CROSS locate=" << locate
                        << " first=" << pend->second.type
                        << " ts=" << pend->second.timestamp_ns
                        << " ref=" << pend->second.order_ref
                        << " bid=" << pend->second.bid << " ask=" << pend->second.ask
                        << " | next=" << static_cast<char>(type)
                        << " ts=" << this_ts
                        << " ref=" << this_ref
                        << " dt_ns=" << (this_ts - pend->second.timestamp_ns)
                        << "\n";
                ++logged_examples;
                pending_cross.erase(pend);
            }

            ++book_messages;
                //also need to decode + book.add / reduce/ delete/ replace
                switch (type) {
                    //record the raw dt always, but only treat it as chain progress 
                    //(and only keep tracking) if it matches; otherwise the chain is broken:
                    case 'A': {
                        auto m = itch::AddOrder::decode(body);
                        auto le = last_exec.find(locate);
                        if(le != last_exec.end()){
                            uint64_t ts = itch::be48(body+5);
                            uint64_t dt = ts - le->second.ts;
                            bool same_level = (le->second.price == m.price && le->second.side == m.side);
                            if(same_level){
                                le->second.chain_len += 1;
                                refill_samples.push_back({locate, dt, le->second.chain_len});
                            } else{
                                refill_samples.push_back({locate, dt, 0}); // unrelated Add, record dt for the distribution, chain broken
                                last_exec.erase(le); // nothing left to continue until the next E
                            }
                        }

                        book.add(m.order_ref, m.side, m.price, m.shares);
                        break;
                    }
                    case 'F': {
                        auto m = itch::AddOrderMPID::decode(body);
                        auto le = last_exec.find(locate);
                        if(le != last_exec.end()){
                            uint64_t ts = itch::be48(body+5);
                            uint64_t dt = ts - le->second.ts;
                            bool same_level = (le->second.price == m.price && le->second.side == m.side);
                            if(same_level){
                                le->second.chain_len += 1;
                                refill_samples.push_back({locate, dt, le->second.chain_len});
                            } else{
                                refill_samples.push_back({locate, dt, 0}); // unrelated Add, record dt for the distribution, chain broken
                                last_exec.erase(le); // nothing left to continue until the next E
                            }
                        }
                        book.add(m.order_ref, m.side, m.price, m.shares);
                        break;
                    }
                    case 'X': {
                        auto m = itch::OrderCancel::decode(body);
                        if (!book.reduce(m.order_ref, m.cancelled_shares)) ++missing_references;
                        break;
                    }
                    //reset the chain if this execution doesn't continue the price/side already being tracked:
                    case 'E': {
                        auto m = itch::OrderExecuted::decode(body);
                        uint32_t px; char sd;
                        if(book.price_of(m.order_ref, px, sd)){
                            uint64_t ts = itch::be48(body+5);
                            auto le = last_exec.find(locate);
                            if(le != last_exec.end() && le->second.price == px && le->second.side == sd){
                                le->second.ts = ts; // Same level continuing
                            } else{
                                last_exec[locate] = {px,sd,ts,0}; // first time or different level, new chain_len
                            }
                        }
                        if (!book.reduce(m.order_ref, m.executed_shares)) ++missing_references;
                        break;
                    }
                    case 'C': {
                        auto m = itch::OrderExecutedWithPrice::decode(body);
                        uint32_t px; char sd;
                        if(book.price_of(m.order_ref, px, sd)){
                            uint64_t ts = itch::be48(body+5);
                            auto le = last_exec.find(locate);
                            if(le != last_exec.end() && le->second.price == px && le->second.side == sd){
                                le->second.ts = ts; // Same level continuing
                            } else{
                                last_exec[locate] = {px,sd,ts,0}; // first time or different level, new chain_len
                            }
                        }
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

                
                //Light check instead.
                if(trading[locate]){
                    uint32_t bid = 0, ask = 0;
                    if(book.best_bid(bid) && book.best_ask(ask)){
                        if(bid > ask){
                            ++crossed_count;
                            if(!pending_cross.count(locate)){
                                uint64_t ts = itch::be48(body + 5);
                                uint64_t ref = itch::be64(body + 11);
                                pending_cross[locate] = CrossEvent{static_cast<char>(type), ts, ref, bid, ask};
                            }
                        }
                        if(bid == ask) ++locked_count;
                    }
                }

                //Removing this to run it faster.
                // if(!book.check_invariants()){
                //     ++invariant_fails;
                //     if(invariant_fails == 1){
                //         uint32_t bid = 0, ask = 0;
                //         bool high_bid = book.best_bid(bid);
                //         bool high_ask = book.best_ask(ask);

                //         std::cerr << "First invariant fail at message type = " << static_cast<char>(type)
                //                   << " bid = " << (high_bid? std::to_string(bid) : "none")
                //                   << " ask = " << (high_ask? std::to_string(ask) : "none")
                //                   << "\n";
                        
                //     }
                // }
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


    //std::cout << "AAPL messages kept: " <<aapl_messages << "\n";

    std::cout << "Symbols (R): " << symbols_seen << "\n";
    std::cout << "Books seen: " << books.size() << "\n";
    std::cout << "Book messages applied: " << book_messages << "\n";
    std::cout << "Missing order refs: " <<missing_references << "\n";
    //std::cout << "Invariant failures: " << invariant_fails << "\n";
    std::cout << "Crossed counted: " << crossed_count << "\n";
    std::cout << "Locked counted: " << locked_count << "\n";


    FILE* out = std::fopen("refill_samples.csv","w");
    if(out){
        std::fprintf(out, "locate, dt_ns, chain_len\n");
        for(const auto& r: refill_samples){
            std::fprintf(out, "%u, %llu, %d \n", r.locate, (unsigned long long) r.dt_ns, r.chain_len);

        }
        std::fclose(out);
    }
    std::cout << "Refill samples recorded: " << refill_samples.size() << "\n";

    std::cout << data[0] << "\n";
    munmap(data, size);
    fclose(file);
    return 0;

}