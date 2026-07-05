#pragma once

// itch_messages.hpp
//
// Decoded structs for the 9 Nasdaq TotalView-ITCH 5.0 message types we need:
//   S  System Event
//   R  Stock Directory
//   A  Add Order (no MPID)
//   F  Add Order with MPID Attribution
//   E  Order Executed
//   C  Order Executed with Price
//   X  Order Cancel
//   D  Order Delete
//   U  Order Replace
//
// File format recap:
//   [2-byte big-endian length][message body of that many bytes]
//   message body: [1-byte type][fields...]
//   ALL multi-byte integers in the file are big-endian.
//
// Each struct has a static decode(const uint8_t* msg) that takes a pointer to
// the first byte of the message body (the type byte).  Offsets below are
// relative to that pointer.

#include <array>
#include <cstdint>
#include <cstring>

namespace itch {

// ============================================================================
// Big-endian read helpers
// ============================================================================
//
// We deliberately avoid reinterpret_cast + __attribute__((packed)) structs.
// Shift-and-OR is fully portable, avoids undefined behaviour from strict
// aliasing violations, and compiles to a single bswap instruction on x86-64.

inline uint16_t be16(const uint8_t* p) noexcept {
    return static_cast<uint16_t>(
        (static_cast<uint16_t>(p[0]) << 8) |
         static_cast<uint16_t>(p[1]));
}

inline uint32_t be32(const uint8_t* p) noexcept {
    return (static_cast<uint32_t>(p[0]) << 24) |
           (static_cast<uint32_t>(p[1]) << 16) |
           (static_cast<uint32_t>(p[2]) <<  8) |
            static_cast<uint32_t>(p[3]);
}

inline uint64_t be64(const uint8_t* p) noexcept {
    return (static_cast<uint64_t>(p[0]) << 56) |
           (static_cast<uint64_t>(p[1]) << 48) |
           (static_cast<uint64_t>(p[2]) << 40) |
           (static_cast<uint64_t>(p[3]) << 32) |
           (static_cast<uint64_t>(p[4]) << 24) |
           (static_cast<uint64_t>(p[5]) << 16) |
           (static_cast<uint64_t>(p[6]) <<  8) |
            static_cast<uint64_t>(p[7]);
}

// ITCH timestamps are 6 bytes (48-bit nanoseconds since midnight).
// We read them into the high 48 bits of a uint64_t.
inline uint64_t be48(const uint8_t* p) noexcept {
    return (static_cast<uint64_t>(p[0]) << 40) |
           (static_cast<uint64_t>(p[1]) << 32) |
           (static_cast<uint64_t>(p[2]) << 24) |
           (static_cast<uint64_t>(p[3]) << 16) |
           (static_cast<uint64_t>(p[4]) <<  8) |
            static_cast<uint64_t>(p[5]);
}

// ============================================================================
// Message type constants — the single byte that identifies each message
// ============================================================================

namespace MsgType {
    inline constexpr char SYSTEM_EVENT         = 'S';
    inline constexpr char STOCK_DIRECTORY      = 'R';
    inline constexpr char ADD_ORDER            = 'A';
    inline constexpr char ADD_ORDER_MPID       = 'F';
    inline constexpr char ORDER_EXECUTED       = 'E';
    inline constexpr char ORDER_EXECUTED_PRICE = 'C';
    inline constexpr char ORDER_CANCEL         = 'X';
    inline constexpr char ORDER_DELETE         = 'D';
    inline constexpr char ORDER_REPLACE        = 'U';
}

// ============================================================================
// Every ITCH message body begins with the same 11 bytes:
//
//   [0]      type          (char)
//   [1..2]   stock_locate  (uint16 BE) — maps stock ticker to a small int
//   [3..4]   tracking_num  (uint16 BE) — for Nasdaq's own audit trail
//   [5..10]  timestamp_ns  (uint48 BE) — nanoseconds since midnight
//
// stock_locate is your fast key: the parser will build a table
// stock_locate -> symbol string from the R (Stock Directory) messages.
// ============================================================================

// ============================================================================
// S — System Event Message  (body: 12 bytes)
// ============================================================================
//
// Marks lifecycle milestones for the trading day.
// event_code values you'll actually care about:
//   'O'  Start of Messages   (first message of the day)
//   'S'  Start of System Hours
//   'Q'  Start of Market Hours  ← use this to ignore pre-market executions
//   'M'  End of Market Hours    ← use this to ignore post-market executions
//   'E'  End of System Hours
//   'C'  End of Messages    (last message of the day)

struct SystemEvent {
    uint16_t stock_locate;
    uint16_t tracking_num;
    uint64_t timestamp_ns;
    char     event_code;        // see table above

    // msg[0]  = 'S'
    // msg[11] = event_code
    static SystemEvent decode(const uint8_t* msg) noexcept {
        return {
            be16(msg + 1),
            be16(msg + 3),
            be48(msg + 5),
            static_cast<char>(msg[11])
        };
    }
};

// ============================================================================
// R — Stock Directory Message  (body: 39 bytes)
// ============================================================================
//
// Sent at the start of each day for every listed symbol.
// Gives you the mapping: stock_locate (uint16) ↔ ticker string (8 chars).
// Build a lookup table from these so you can name every subsequent message.

struct StockDirectory {
    uint16_t            stock_locate;
    uint16_t            tracking_num;
    uint64_t            timestamp_ns;
    std::array<char, 8> stock;              // ASCII, right-padded with spaces
    char                market_category;    // 'Q'=Nasdaq, 'N'=NYSE, etc.
    char                financial_status;
    uint32_t            round_lot_size;
    char                round_lots_only;    // 'Y' / 'N'
    char                issue_classification;
    std::array<char, 2> issue_sub_type;
    char                authenticity;       // 'P'=live production, 'T'=test
    char                short_sale_threshold;
    char                ipo_flag;
    char                luld_ref_price_tier;
    char                etp_flag;
    uint32_t            etp_leverage_factor;
    char                inverse_indicator;

    // Offsets:
    //  [0]      type 'R'
    //  [1..2]   stock_locate
    //  [3..4]   tracking_num
    //  [5..10]  timestamp_ns
    //  [11..18] stock (8 bytes)
    //  [19]     market_category
    //  [20]     financial_status
    //  [21..24] round_lot_size
    //  [25]     round_lots_only
    //  [26]     issue_classification
    //  [27..28] issue_sub_type
    //  [29]     authenticity
    //  [30]     short_sale_threshold
    //  [31]     ipo_flag
    //  [32]     luld_ref_price_tier
    //  [33]     etp_flag
    //  [34..37] etp_leverage_factor
    //  [38]     inverse_indicator
    static StockDirectory decode(const uint8_t* msg) noexcept {
        StockDirectory r{};
        r.stock_locate         = be16(msg + 1);
        r.tracking_num         = be16(msg + 3);
        r.timestamp_ns         = be48(msg + 5);
        std::memcpy(r.stock.data(), msg + 11, 8);
        r.market_category      = static_cast<char>(msg[19]);
        r.financial_status     = static_cast<char>(msg[20]);
        r.round_lot_size       = be32(msg + 21);
        r.round_lots_only      = static_cast<char>(msg[25]);
        r.issue_classification = static_cast<char>(msg[26]);
        r.issue_sub_type[0]    = static_cast<char>(msg[27]);
        r.issue_sub_type[1]    = static_cast<char>(msg[28]);
        r.authenticity         = static_cast<char>(msg[29]);
        r.short_sale_threshold = static_cast<char>(msg[30]);
        r.ipo_flag             = static_cast<char>(msg[31]);
        r.luld_ref_price_tier  = static_cast<char>(msg[32]);
        r.etp_flag             = static_cast<char>(msg[33]);
        r.etp_leverage_factor  = be32(msg + 34);
        r.inverse_indicator    = static_cast<char>(msg[38]);
        return r;
    }
};

// ============================================================================
// A — Add Order, no MPID  (body: 36 bytes)
// ============================================================================
//
// An order has been accepted and added to Nasdaq's book.
// This is the primary message you'll feed into your LOB.
//
// price is in units of 1/10,000 of a dollar (4 implied decimal places).
//   e.g. price = 100000 → $10.00
//        price = 1234567 → $123.4567
// Use price_dollars() for human-readable display; keep price as uint32_t
// internally (integer arithmetic is faster and exact).

struct AddOrder {
    uint16_t            stock_locate;
    uint16_t            tracking_num;
    uint64_t            timestamp_ns;
    uint64_t            order_ref;      // globally unique order ID for this day
    char                side;           // 'B' = buy, 'S' = sell
    uint32_t            shares;         // number of shares
    std::array<char, 8> stock;          // ticker, right-padded with spaces
    uint32_t            price;          // 1/10000 dollar units

    // Offsets:
    //  [0]      type 'A'
    //  [1..2]   stock_locate
    //  [3..4]   tracking_num
    //  [5..10]  timestamp_ns
    //  [11..18] order_ref  (8 bytes)
    //  [19]     side
    //  [20..23] shares
    //  [24..31] stock  (8 bytes)
    //  [32..35] price
    static AddOrder decode(const uint8_t* msg) noexcept {
        AddOrder r{};
        r.stock_locate = be16(msg + 1);
        r.tracking_num = be16(msg + 3);
        r.timestamp_ns = be48(msg + 5);
        r.order_ref    = be64(msg + 11);
        r.side         = static_cast<char>(msg[19]);
        r.shares       = be32(msg + 20);
        std::memcpy(r.stock.data(), msg + 24, 8);
        r.price        = be32(msg + 32);
        return r;
    }

    double price_dollars() const noexcept { return price / 10000.0; }
};

// ============================================================================
// F — Add Order with MPID Attribution  (body: 40 bytes)
// ============================================================================
//
// Same as AddOrder but with a 4-byte Market Participant ID appended.
// For the LOB you can treat 'F' exactly like 'A'; the MPID is optional info.

struct AddOrderMPID {
    uint16_t            stock_locate;
    uint16_t            tracking_num;
    uint64_t            timestamp_ns;
    uint64_t            order_ref;
    char                side;
    uint32_t            shares;
    std::array<char, 8> stock;
    uint32_t            price;
    std::array<char, 4> attribution;   // MPID, e.g. "GSCO", "MLCO"

    // Offsets: same as AddOrder through [35], then:
    //  [36..39] attribution
    static AddOrderMPID decode(const uint8_t* msg) noexcept {
        AddOrderMPID r{};
        r.stock_locate = be16(msg + 1);
        r.tracking_num = be16(msg + 3);
        r.timestamp_ns = be48(msg + 5);
        r.order_ref    = be64(msg + 11);
        r.side         = static_cast<char>(msg[19]);
        r.shares       = be32(msg + 20);
        std::memcpy(r.stock.data(), msg + 24, 8);
        r.price        = be32(msg + 32);
        std::memcpy(r.attribution.data(), msg + 36, 4);
        return r;
    }

    double price_dollars() const noexcept { return price / 10000.0; }
};

// ============================================================================
// E — Order Executed  (body: 31 bytes)
// ============================================================================
//
// A resting order (previously added with A/F) was partially or fully filled.
// This is your GROUND TRUTH for validation:
//   - order_ref identifies WHICH resting order was hit
//   - executed_shares is how many shares were filled this time
//   - match_number links this execution to the corresponding trade report
//
// NOTE: 'E' does NOT tell you the execution price — use the order's resting
// price (from the original A/F message). If Nasdaq traded at a different
// price (e.g. opening cross), use the 'C' message instead.

struct OrderExecuted {
    uint16_t stock_locate;
    uint16_t tracking_num;
    uint64_t timestamp_ns;
    uint64_t order_ref;         // which resting order got hit
    uint32_t executed_shares;   // shares filled in this execution event
    uint64_t match_number;      // ties to the trade report

    // Offsets:
    //  [0]      type 'E'
    //  [1..2]   stock_locate
    //  [3..4]   tracking_num
    //  [5..10]  timestamp_ns
    //  [11..18] order_ref
    //  [19..22] executed_shares
    //  [23..30] match_number
    static OrderExecuted decode(const uint8_t* msg) noexcept {
        return {
            be16(msg + 1),
            be16(msg + 3),
            be48(msg + 5),
            be64(msg + 11),
            be32(msg + 19),
            be64(msg + 23)
        };
    }
};

// ============================================================================
// C — Order Executed with Price  (body: 36 bytes)
// ============================================================================
//
// Like 'E', but the execution happened at a price DIFFERENT from the order's
// resting price (e.g. the opening/closing cross, or a trade-through).
// Use execution_price for the actual fill price, not the order's price.
// printable == 'N' means this is a non-displayed fill (dark pool / auction).

struct OrderExecutedWithPrice {
    uint16_t stock_locate;
    uint16_t tracking_num;
    uint64_t timestamp_ns;
    uint64_t order_ref;
    uint32_t executed_shares;
    uint64_t match_number;
    char     printable;         // 'Y' = tape-eligible print, 'N' = not
    uint32_t execution_price;   // 1/10000 dollar units

    // Offsets:
    //  [0]      type 'C'
    //  [1..2]   stock_locate
    //  [3..4]   tracking_num
    //  [5..10]  timestamp_ns
    //  [11..18] order_ref
    //  [19..22] executed_shares
    //  [23..30] match_number
    //  [31]     printable
    //  [32..35] execution_price
    static OrderExecutedWithPrice decode(const uint8_t* msg) noexcept {
        OrderExecutedWithPrice r{};
        r.stock_locate    = be16(msg + 1);
        r.tracking_num    = be16(msg + 3);
        r.timestamp_ns    = be48(msg + 5);
        r.order_ref       = be64(msg + 11);
        r.executed_shares = be32(msg + 19);
        r.match_number    = be64(msg + 23);
        r.printable       = static_cast<char>(msg[31]);
        r.execution_price = be32(msg + 32);
        return r;
    }

    double execution_price_dollars() const noexcept { return execution_price / 10000.0; }
};

// ============================================================================
// X — Order Cancel  (body: 23 bytes)
// ============================================================================
//
// A PARTIAL cancel: cancelled_shares is the number of shares removed,
// NOT the remaining quantity.  The order stays in the book with
//   new_qty = original_qty - sum_of_all_X_cancelled_shares - sum_of_all_E_executed_shares
// Track remaining quantity on your Order struct, not here.

struct OrderCancel {
    uint16_t stock_locate;
    uint16_t tracking_num;
    uint64_t timestamp_ns;
    uint64_t order_ref;
    uint32_t cancelled_shares;  // shares removed from this order's quantity

    // Offsets:
    //  [0]      type 'X'
    //  [1..2]   stock_locate
    //  [3..4]   tracking_num
    //  [5..10]  timestamp_ns
    //  [11..18] order_ref
    //  [19..22] cancelled_shares
    static OrderCancel decode(const uint8_t* msg) noexcept {
        return {
            be16(msg + 1),
            be16(msg + 3),
            be48(msg + 5),
            be64(msg + 11),
            be32(msg + 19)
        };
    }
};

// ============================================================================
// D — Order Delete  (body: 19 bytes)
// ============================================================================
//
// Remove order_ref entirely from the book.
// After a 'D' you will NEVER see another message referencing this order_ref.
// Safe to erase from your hash map.

struct OrderDelete {
    uint16_t stock_locate;
    uint16_t tracking_num;
    uint64_t timestamp_ns;
    uint64_t order_ref;

    // Offsets:
    //  [0]      type 'D'
    //  [1..2]   stock_locate
    //  [3..4]   tracking_num
    //  [5..10]  timestamp_ns
    //  [11..18] order_ref
    static OrderDelete decode(const uint8_t* msg) noexcept {
        return {
            be16(msg + 1),
            be16(msg + 3),
            be48(msg + 5),
            be64(msg + 11)
        };
    }
};

// ============================================================================
// U — Order Replace  (body: 35 bytes)
// ============================================================================
//
// *** CRITICAL subtlety — a classic bug source ***
//
// 'U' is NOT an in-place edit.  It is atomically:
//   1. Delete  orig_order_ref  from the book entirely.
//   2. Insert  new_order_ref   with new shares/price at the BACK of its
//      price level queue — it LOSES time priority.
//
// orig_order_ref is gone after this message; new_order_ref is the live order.
// The side and stock are INHERITED from orig_order_ref (not repeated here),
// so your code must look them up from the original order before deleting it.

struct OrderReplace {
    uint16_t stock_locate;
    uint16_t tracking_num;
    uint64_t timestamp_ns;
    uint64_t orig_order_ref;    // order being replaced — REMOVE this
    uint64_t new_order_ref;     // replacement order   — ADD this at queue back
    uint32_t shares;            // new quantity (NOT a delta)
    uint32_t price;             // new price (1/10000 dollar units)

    // Offsets:
    //  [0]      type 'U'
    //  [1..2]   stock_locate
    //  [3..4]   tracking_num
    //  [5..10]  timestamp_ns
    //  [11..18] orig_order_ref
    //  [19..26] new_order_ref
    //  [27..30] shares
    //  [31..34] price
    static OrderReplace decode(const uint8_t* msg) noexcept {
        return {
            be16(msg + 1),
            be16(msg + 3),
            be48(msg + 5),
            be64(msg + 11),
            be64(msg + 19),
            be32(msg + 27),
            be32(msg + 31)
        };
    }

    double price_dollars() const noexcept { return price / 10000.0; }
};

} // namespace itch
