#pragma once

#include <order.hpp>
#include <cstdint>
#include <deque>
#include <map>
#include <unordered_map>
#include <vector>


/*
Holds order's current state, quantity is mutable so when order is partially filled
it doesn't create a new Order but quantity is reduced instead.
*/

/*
This just records an event, created once everytime a trade happens inside submit().
*/
struct Fill{
    uint64_t incoming_id;
    uint64_t resting_id;
    uint32_t price;
    uint32_t quantity;
};


class MatchingEngine{
public:
    // Takes four pieces of an incoming Order.
    // Returns every fill that happened as a result of this order.
    // If vector.empty() == true. nothing was matched.
    std::vector<Fill> submit(uint64_t id, char side, uint32_t price, uint32_t quantity){
        std::vector<Fill> fills;
        auto& opposite = (side == 'B') ? asks_ : bids_; //if someone is buying it looks at what people are selling for, vice-versa.

        while(quantity > 0 && !opposite.empty()){
            auto level_ite = opposite.begin();
            uint32_t level_price = level_ite->first;
            
            bool acceptable = (side == 'B') ? (price >= level_price) : (price <= level_price);
            if(!acceptable){
                break;
            }

            auto& queue = level_ite->second;
            uint64_t resting_id = queue.front();
            Order& resting = orders_[resting_id];

            uint32_t traded = std::min(quantity, resting.quantity);
            fills.push_back({id, resting_id, level_price, traded});

            quantity -= traded;         // function's local parameter tracking how much of the incoming order is unfilled.
            resting.quantity -= traded; // actual stored order's remaining size.
            
            //When the resting order is consumed it needs to disappear.
            if(resting.quantity == 0){
                queue.pop_front();
                orders_.erase(resting_id);
                if(queue.empty()){
                    opposite.erase(level_ite);
                }
            }
        }

        /*
        If there is still quantity after the loop ends(fully filled or ran out of acceptable price levels)
        It rests in book as a new resting order on its own side(buy order rests among the bids, not the asks)
        pushing it back puts it at the end of queue at its price, since it just arrived it has no claim to time priority.
        */
        if(quantity >0 ){
            orders_[id] = Order{id, side, price, quantity};
            auto& own_side = (side == 'B') ? bids_ : asks_;
            own_side[price].push_back(id);
        }
        
        return fills;
    }

    /*
    Cancel(id): find the order id, find its side and price. remove it from that price levels queue.
    If price level empty, clean it up and remove it from the lookup table.
    */
    bool cancel(uint64_t id){
        auto ite = orders_.find(id);
        if(ite == orders_.end()){
            return false;
        }

        const Order& order = ite->second;
        auto& side_map = (order.side == 'B')? bids_:asks_;

        auto level_ite = side_map.find(order.price);
        if(level_ite != side_map.end()){
            auto& queue = level_ite->second;
            for(auto bye = queue.begin(); bye != queue.end(); ++bye){
                if(*bye == id){
                    queue.erase(bye);
                    break;
                }
            }
            if (queue.empty()){
                side_map.erase(level_ite);
            }
        }

        orders_.erase(ite);
        return true;
    }

    /*
    Replace(old_id, new_id, new_price, new_quantity): delete the old order in full.
    Add brand new order that loses its place in line, to the back of the queue.
    */
    bool replace(uint64_t old_id, uint64_t new_id, uint32_t new_price, uint32_t new_quantity){
        auto ite = orders_.find(old_id);
        if(ite == orders_.end()){
            return false;
        }

        char side = ite->second.side;
        if(!cancel(old_id)){
            return false;
        }

        orders_[new_id] = Order{new_id, side, new_price, new_quantity};
        auto& side_map = (side == 'B') ? bids_ : asks_;
        side_map[new_price].push_back(new_id);

        return true;
    }



    //Helper functions:
    // return false if there is nothing to report (empty book), else write value in ref param and return true.
    //bids_ uses .rbegin() because highest bid is better.
    // asks_ uses .begin() because lowest ask is better.
    //To remember: bid = highest buy price
    //             ask = lowest sell price

    bool best_bid(uint32_t& p)const {
        if(bids_.empty()) return false;
        p = bids_.rbegin()->first;
        return true;
    }

    bool best_ask(uint32_t& p) const{
        if(asks_.empty()) return false;
        p = asks_.begin()->first;
        return true;
    }





    


private:

    /*
        Same structure as OrderBook in order_book.hpp, 
        A lookup table from order ID to the order itself + two sorted maps( price -> queue of order IDs waiting at that price)
        for bids and asks.

    */
    std::unordered_map<uint64_t, Order> orders_;
    std::map<uint32_t, std::deque<uint64_t>> bids_;
    std::map<uint32_t, std::deque<uint64_t>> asks_;

};