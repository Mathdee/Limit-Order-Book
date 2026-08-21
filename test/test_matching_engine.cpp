#include "matching_engine.hpp"
#include <iostream>
#include <cassert>

void test_full_fill_single_order(){
    
    MatchingEngine engine;

    //Rest a sell order first: 100 shares at price 100.
    auto fills1 = engine.submit(1, 'S', 100, 100);
    assert(fills1.empty()); // nothing can match against yet, it should just rest.

    // Now a buy comes in willing to pay 100 for 100 shares.
    auto fills2 = engine.submit(2, 'B', 100, 100);

    assert(fills2.size() == 1);         // Exactly one trade happened
    assert(fills2[0].incoming_id == 2); // the buyer
    assert(fills2[0].resting_id == 1);  // matched against the resting seller
    assert(fills2[0].price == 100);     // traded at resting order's price
    assert(fills2[0].quantity == 100);  // fully filled

    uint32_t bid, ask;
    assert(!engine.best_bid(bid));      // nothing resting on the bid side anymore
    assert(!engine.best_ask(ask));      // nothing resting on the ask side anymore

    std::cout << "Tests Passed \n";
}

// 1 - Partial Fills
void test_incoming_partial_fill(){
    MatchingEngine e;
    e.submit(1, 'S', 100, 200);
    auto fills = e.submit(2, 'B', 100, 50);
    assert(fills.size() == 1 && fills[0].quantity == 50);
    uint32_t ask;
    e.best_ask(ask);
    assert(ask == 100);
    std::cout << "test_incoming_partial_fill PASSED!! \n";
}

void test_resting_partial_consumption(){
    MatchingEngine e;
    e.submit(1, 'S', 100, 50);
    auto fills = e.submit(2, 'B', 100, 200);
    assert(fills.size() == 1 && fills[0].quantity == 50);
    uint32_t ask;
    assert(!e.best_ask(ask));
    uint32_t bid;
    e.best_bid(bid);
    assert(bid == 100);
    std::cout << "test_resting_partial_consumption PASSED!! \n";
}

//2 - Sweeping Multiple Price Levels
void test_sweep_multiple_levels(){
    MatchingEngine e;
    e.submit(1, 'S', 100, 50);
    e.submit(2, 'S', 101, 50);

    auto fills = e.submit(3, 'B', 101, 100);
    assert(fills.size() == 2);
    assert(fills[0].price == 100 && fills[1].price == 101);
    std::cout << "test_sweep_multiple_levels PASSED \n";
}

// 3 - Time Priority
void test_time_priority_same_price(){
    MatchingEngine e;
    e.submit(1, 'S', 100, 50);
    e.submit(2, 'S', 100, 50);

    auto fills = e.submit(3, 'B', 100, 50);
    assert(fills.size() == 1 && fills[0].resting_id == 1);

    std::cout << "test_time_priority_same_price PASSED!! \n";
}

// 4 - No Match
void test_no_match_price_too_low(){
    MatchingEngine e;
    e.submit(1, 'S', 100, 50);
    auto fills = e.submit(2, 'B', 99, 50);
    assert(fills.empty());
    uint32_t bid;
    e.best_bid(bid);
    assert(bid == 99);
    std::cout << "test_no_match_price_too_low PASSED!! \n";
}

void test_empty_book_rests_immediately(){
    MatchingEngine e;
    auto fills = e.submit(1, 'B', 100, 50);
    assert(fills.empty());
    uint32_t bid;
    e.best_bid(bid);
    assert(bid == 100);
    std::cout << "test_empty_book_rests_immediately PASSED!! \n";
}


// 5 - exact exhaustion
void test_exact_exhaustion_removes_level(){
    MatchingEngine e;
    e.submit(1, 'S', 100, 50);
    auto fills = e.submit(2, 'B', 100, 50);
    assert(fills.size() == 1 && fills[0].quantity == 50);
    uint32_t ask;
    assert(!e.best_ask(ask));
    std::cout << "test_exact_exhaustion_removes_level PASSED \n";
}

// 6 - Cancel
void test_cancel_prevents_match(){
    MatchingEngine e;
    e.submit(1, 'S', 100, 50);
    bool ok = e.cancel(1);
    assert(ok);

    auto fills = e.submit(2, 'B', 100, 50);
    assert(fills.empty());
    std::cout << " test_cancel_prevents_match PASSED!! \n";
}

void test_cancel_unknown_id_fails(){
    MatchingEngine e;
    assert(!e.cancel(999));
    std::cout<< "test_cancel_unknown_id_fails PASSED!! \n";
}

// 7 - Replace Loses Priority
void test_replace_loses_time_priority(){
    MatchingEngine e;
    e.submit(1, 'S', 100, 50);
    e.submit(2, 'S', 100, 50);
    e.replace(1, 3, 100, 50);

    auto fills = e.submit(4, 'B', 100, 50);
    assert(fills.size() == 1 && fills[0].resting_id == 2);
    std::cout << "test_replace_loses_time_priority PASSED!! \n";
}

// 8 - Basic Invariant Sanity Check After A Sequence
void test_book_never_crosses_after_sequence(){
    MatchingEngine e;
    e.submit(1, 'B', 100, 50);
    e.submit(2, 'S', 100, 50);
    e.submit(3, 'B', 100, 50);
    e.submit(4, 'S', 100, 50);
    uint32_t bid, ask;
    if(e.best_bid(bid) && e.best_ask(ask)){
        assert(bid < ask);
    }

    std::cout << "test_book_never_crosses_after_sequence PASSED!! \n";
}


int main() {
    test_full_fill_single_order();
    test_incoming_partial_fill();
    test_resting_partial_consumption();
    test_sweep_multiple_levels();
    test_time_priority_same_price();
    test_no_match_price_too_low();
    test_empty_book_rests_immediately();
    test_exact_exhaustion_removes_level();
    test_cancel_prevents_match();
    test_cancel_unknown_id_fails();
    test_replace_loses_time_priority();
    test_book_never_crosses_after_sequence();
    std::cout << "All tests passed.\n";
    return 0;
}