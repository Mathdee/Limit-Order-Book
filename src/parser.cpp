#include <sys/stat.h>
#include <sys/mman.h>
#include <unistd.h>
#include "itch_messages.hpp"

#include <unordered_map>
#include <iostream>
#include <cstdio>
#include <filesystem>
#include <cstdint>
#include <array>


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

    std::cout << data[0] << "\n";
    munmap(data, size);
    fclose(file);
    return 0;

}