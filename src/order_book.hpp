#pragma once

#include <order.hpp>

#include <cstdint>
#include <deque>
#include <map>
#include <unordered_map>

// struct Order{
//     uint64_t    id;         //order reference
//     char        side;       //'B' = buy
//     uint32_t    price;      // ITCH integer price (is 1/10000 of a dollar)
//     uint32_t    quantity;   // shares still resting on the book

// };

/*
4 most imporant invariants for a valid LOB.
No Crossed Markets: The highest buy price (Best Bid) must always be strictly lower than the lowest sell price (Best Ask).
Price Level Volume: The total shares listed at any price level must exactly equal the sum of the shares of all individual orders resting at that price level.
Idempotent Order ID Mapping: Every active Order ID must map to exactly one price level, one side (buy/sell), and one stock ID.
Non-Negative States: Order sizes, share volumes, and price level counts can never drop below zero.

=======================================================================
*/
// Price level is: one price -> a line of order IDs at that price :)
// use c++ api std::map<> with price as key, and a queue of order IDs std::deque<> as values.
// std::map is also sorted so helps us find best bid/ask way easier later on.
// reminder: bid = highest price (last key in map), ask = lowest price (first key in map).

class OrderBook{
public: 
    
    void add(uint64_t id, char side, uint32_t price, uint32_t quantity);
    bool delete_order(uint64_t id);
    bool reduce(uint64_t id, uint32_t shares);
    bool replace(uint64_t old_id, uint64_t new_id, uint32_t new_price, uint32_t new_quantity);

    bool best_bid(uint32_t& price) const; // Highest buy price
    bool best_ask(uint32_t& price) const; // Lowest sell price.
    bool check_invariants() const;        // Checks if book is in a bad state.

    bool price_of(uint64_t id, uint32_t& price, char& side) const;



private: 
    std::unordered_map<uint64_t, Order> orders_;

    std::map<uint32_t, std::deque<uint64_t>> bids_;

    std::map<uint32_t, std::deque<uint64_t>> asks_;

    //allows picking the right price level.
    std::map<uint32_t, std::deque<uint64_t>>& book_side(char side){
        return side =='B' ? bids_ : asks_;
    }

    //delete helper to pull an ID out of its price queue and if price level is empty we erase it.
    //Also it's O(n) now, I might optimize by using Linked Lists like wkselph advised.
    void remove_from_level(char side, uint32_t price, uint64_t id){
        auto& levels = book_side(side);
        auto ite = levels.find(price);
        if(ite == levels.end()){
            return;
        }
        
        // reference to the whole queue of orders waiting at this price so I can modify it.
        auto& q = ite->second;
        for(auto i = q.begin(); i != q.end(); ++i){
            if(*i == id){
                q.erase(i);
                break;
            }
        }
        if(q.empty()){
            levels.erase(ite);
        }
    }

};
/*
Let's visualize it rq:
add 3 orders:
order_:
    101 -> {Buy, $150, 200 shares}
    102 -> {Buy, $150, 50 shares}
    201 -> {Sell, $151, 100 shares}

bids_: 
    150 -> [101, 102] (101 is first in, so also first out, DNF).

asks_:
    151 -> [201]

*/


/*
Make an Order{id, side, price, quantity}
Put it in orders_ under the id
Go to correct side map: bids_ or asks_
Find/Create the price level for price
push_back(id), new order goes to the end of the line
*/
void OrderBook::add(uint64_t id, char side, uint32_t price, uint32_t quantity){
    orders_[id] = Order{id, side, price, quantity};
    book_side(side)[price].push_back(id);
}

//For 'D' messages
bool OrderBook::delete_order(uint64_t id){
    auto ite = orders_.find(id);
    if (ite == orders_.end()){
        return false;
    }

    const Order& o = ite->second;
    remove_from_level(o.side, o.price, id);
    orders_.erase(ite);

    return true;
}

//reducing resting size
bool OrderBook::reduce(uint64_t id, uint32_t shares){
    auto ite = orders_.find(id);
    if(ite == orders_.end()) return false;

    auto& it = ite->second.quantity;


    if(shares >= it){
        return delete_order(id);
    }

    it -= shares;
    return true;
}


/*
Look up the old order and find its side
Delete old ID completely
Add new ID with new price/quantity, at the same side and at the back of new price line.
*/
bool OrderBook::replace(uint64_t old_id, uint64_t new_id, uint32_t new_price, uint32_t new_quantity){

    auto ite = orders_.find(old_id);
    if(ite == orders_.end()) return false;

    char side = ite->second.side; // save it before deleting
    delete_order(old_id);
    add(new_id, side, new_price, new_quantity);
    return true;
}

bool OrderBook::best_bid(uint32_t& price) const{
    if(bids_.empty()) return false;
    price = bids_.rbegin()->first;
    return true;
}

bool OrderBook::best_ask(uint32_t& price) const{
    if(asks_.empty()) return false;
    price = asks_.begin()->first;
    return true;
}


/*
1) no empty shelves left behind (should never happen if delete is correct)
2) every order qty must be > 0 (0 should have been deleted)
3) book must not be crossed when both sides exist
*/
bool OrderBook::check_invariants() const{
    //1
    for( const auto& [px, q]: bids_){
        if(q.empty()) return false;
    }
    for( const auto& [px, q]: asks_){
        if(q.empty()) return false;
    }

    //2
    for(const auto& [id, o]: orders_){
        (void)id;
        if(o.quantity == 0 ){
            return false;
        }
    }

    //3
    uint32_t bid = 0, ask = 0;
    if(best_bid(bid) && best_ask(ask)){
        if(bid >= ask) return false; // either crossed or locked.(crossed = bid >= ask)(locked = bid == ask).
    }
    return true;
}

bool OrderBook::price_of(uint64_t id, uint32_t& price, char& side) const{
    auto ite = orders_.find(id);
    if(ite == orders_.end()) return false;
    price = ite->second.price;
    side = ite->second.side;
    return true;
}