# Table of Contents

- [July 5 2026](#july-5-2026)
- [July 6 2026](#july-6-2026)
- [August 7 2026](#august-7-2026)
- [August 10 2026](#august-10-2026)
- [August 12 2026](#august-12-2026)
- [August 17 2026](#august-17-2026)
- [August 18 2026](#august-18-2026)


---

# July 5 2026

I read the blog posts of `wkselph` on the waybackMachine: <https://web.archive.org/web/20110219155647/http://howtohft.wordpress.com/author/howtohft/>

## What I learned

We should separate the Trading models from the Trading System:

### Trading Model

- Contains all of the trading Inteligence.
- Decides things like:
  - Should I buy or sell?
  - What price should I quote?
  - How much inventory should I hold?
  - Should I cancel an order?
- The Model should contain anything you want to change or experiment with.

### Trading System

- This is the Infrastructure.
- Provides services such as:
  - receiving market data.
  - maintaining the order book.
  - sending orders.
  - networking.
  - logging.
  - databases & more...
- This is the Operating system of the Trading Models, it doesnt decide but provides the tools for the Models to trade.

### Why is it advised to seperate them?

- New strategies are constantly being developed, if infrastructure is mixed with trading logic, every new strategy means changing the entire system.
- We should be able to swap the Trading Models without having to rewrite the system.

Diagram:

```text
Trading System
    ^
    |
API/ Interface
    ^
    |
Trading Model
```

The LOB is updated one message at a time, because that's how the feed arrives. However, the trading model shouldn't necessarily react after every individual message, because several sequential messages may represent a single logical market event.

- Which is why a LOB is not updated one event at a time.
- The Model needs a way to recognize that messages belong to a same event and make decisions aftwerwards.

The Trading System needs clean interfaces:

- It shouldn't have to worry about:
  - Sockets
  - FIX messages
  - Exchange protocol
  - Networking

## Learned about OMS (Order Management Sysytem)

It manages the lifecyle of an order and is usually the intermediary between the MODEL and the EXCHANGE.

It tracks:

- pending orders.
- live orders.
- incomplete orders.
- cancelled orders.
- rejected orders.
- filled orders.

Saves the trading state.

The OMS receives the model's request (e.g., "I want +300shares"), it then figures out wether to submit, modify, cancel.

So what I understand is that the Model specifies the Intentions and the OMS keeps track of the data and what is needed.

This is useful when the market conditions change before an order finishes and the model now requests -"I want 100 shares".

- The OMS will automatically cancel unnecessay orders, replace prices and update quantities.
- So the Model never blocks or waits.

I also learned that if there are independent models actively trading, they all use the OMS as an intermediary.
So if one wants to buy 100 shares of NVIDIA and the other wants to sell 100 shares of NVIDIA:

- Without an OMS: we will have 2 unnecessay trades.
- With an OMS: it will see that the net position is 100-100 = 0 and will not do anything.

Another use for it: if all independent models quote Buy for the same share, the OMS will combine them.

## LOB vs OMS

I am building a LOB (Limit Order Book), but reading about this allowed me to deeped my understanding of the overall system the LOB is apart of.

I now know that the LOB is more a mirror of the market's state while the OMS is a mirror of our state.

| A LOB is more:       | An OMS is more:                |
| -------------------- | ------------------------------ |
| What orders exist?   | What orders have I submitted?  |
| Best bid?            | Which are pending?             |
| Best ask?            | Which are filled?              |
| Market Depth?        | What inventory do I have?      |
| Recent Executions?   | What am I trying to achieve?   |

So they solve different problems.

SOOOOOO, an OMS is crucial to keep track of its trading state and react fast enough to have a succesful strategy.

The Architecture would look something like this: (PS: I know some parts are missing, but I prefer to not get sidetracked too much.)

```text
            Model A                Model B
               |                    |
               |                    |
               |                    |
                --------------------
                          |
                         API
                          v
              --------------------------
              |                        |
              |      CONNECTIVITY      |
              |          OMS           |
              |    MARKET DATA HANDLER |
              |          LOB           |
              |                        |
              --------------------------
                        |
                        |
                 Market Data Feed
                        |
                        |
                        v
                    EXCHANGE
```

## WHAT I LEARNED ABOUT BUILDING A FAST LIMIT ORDER BOOK

KEY TAKEAWAY after reading: Always optimize and design the LOB around operations you will perform the most.

A LOB has three main operations: add, cancel, execute. (all should aim to be implemented in O(1) time)

- **add**: places an order at the end of a list of orders to be executed at a particular limit price
- **cancel**: removes an order from anywhere in the book
- **execute**: execute removes the head of the FIFO queue at the best bid or best ask. (the inside of the book is defined as the oldest buy order at the highest buying price and the oldest sell order at the lowest selling price)

Because each operation has a unique Orderid , the best way to track them is a hash table (`std::unordered_map<>`).

Trading Models will ask: "what are the best bid and offer?", "how much volume is there between prices A and B?" or "what is order X's current position in the book?".

So we need data structures that make these queries fast.

All DataStuctures that will be needed and for what:

- Binary Search Tree: bids (sorted by price)
- Binary Search Tree: asks (sorted by price)

Each Node in the trees represent one price-level, the LIMIT.
Inside each LIMIT there's a FIFO doubly-linked list of all orders at that price.

There are 3 main objects I'll have to build, based on `wkselph`'s advice.

- **The Order**: Stores information about an individual order.
  - Contains:
    - Order ID
    - Buy/Sell
    - Shares
    - Limit Price
    - Entry Time
    - Event Time
    - Previous Order
    - Next Order
    - Pointer to its parent Limit
- **The Limit**: Represents one price level.
  - Contains:
    - Price
    - Number of Orders
    - Total Volume
    - FIFO queue of Orders
    - Parent/Left/Right pointers (BST)
- **The Book**:
  - Contains:
    - Buy Tree
    - Sell Tree
    - Pointer to Highest Bid
    - Pointer to Lowest Ask

I'll need to maintain two hashmaps:

- One for Order (O(1) lookup for cancels and execute) and one for Limit (O(1), find a price level).

From what `wkselph` has given me, I think most operations (e.g., Add (existing price), Cancel, Execute, Get Volume at Price, Best Bid/Ask) will have O(1) time complexity. Except Add new price (O(log M)) because inserting a new price level requires traversing the BST, giving O(log M) if balanced. to find where this new price belongs.

- M = Nb of occupied price levels.
- N = Total Orders.

M (number of price levels) is usually much smaller than N (number of orders), which is why organizing the book by price levels is efficient.

While reading I asked myself why use a Doubly-Linked list, and why seperate both Trees.

- **Why DLL?**: Orders at the same price level are FIFO, based on time.
  - Adding to tail is O(1);
  - Removing is O(1);
  - Execute from head is O(1);
  - So basically the best outcome we could hope for.
- **Why seperate both BST?**: Apparently it make it easier to find the Best Bid and Best Ask, with the pointers in Book object.

Also always make sure the BST is balanced or time complexity will be O(M) worse than O(log M).

`wkselph` also said that if we need new price insertion to be O(1) we can use an array instead of a BST, but then that adds its own performance issues.

Couple values to keep in mind:

- individual messages average about 20 bytes.
- Bursts of 100,000–200,000 messages per second.
- 20+ gigabytes/day of ITCH data with spikes of 3 megabytes/second or more.

---

# July 6 2026

Looked into ITCH protocol from: <https://databento.com/microstructure/itch> and
<https://www.nasdaqtrader.com/content/technicalsupport/specifications/dataproducts/NQTVITCHSpecification.pdf>

The ITCH file is a binary file, each byte is data encoding numbers, prices, orderIDS and more...
The parser code I have written today allows us to read the binary file efficiently.

There are 2 ways to receive the data:

- **Buffered reading**: The program reads small parts of the file in memory as it needs them.
- **Memory-mapped file**: The Operating System makes it look like the file is already in memory, the program can access it like a normal array.

Also learned that the messages in ITCH are encoded like this `[2 bytes = length N] [N bytes = message body]`.

So going through the file looks like:

1. read the 2 bytes, thats the length.
2. the next `length` bytes are the message.
3. the first byte gives us the message type.
4. then move pointer by `lenght` bytes forward.
5. Keep doing this until we reach the end.

So first coding today, `parser.cpp`.

I am going with the Memory-mapped file approach because I don't have to worry about buffer management and chunk loading, the file is directly parsed, and the OS handles all the loading and caching for us, which makes it more efficient for a +7GB file.

CPP functions I used:

- `std::fopen()` allowed me to open my ITCH file. // `fclose()`: when we're done with it we close it.
- `fstat(file, &struct)`: assigns the metadata of that file (size, permissions, timestamp, type) to a declared struct. `struct.st_size;` returns size; (TO CALL IT: `#include <sys/stat.h>`)
- `mmap()`: maps the file directly into your process's virtual memory so you can access it like a normal byte array.
  - `munmap()`: removes the file from memory.

Comparison of both approaches:

```text
The mmap version I implemented:            The buffered reader implementation:
mmap entire file                                          read 16 MB
 ↓                                                            ↓ 
pos = 0                                                     parse
 ↓                                                            ↓ 
parse until pos == file_size                          carry leftover bytes
                                                              ↓ 
                                                        read next 16 bytes
                                                              ↓ 
                                                            parse
```

Parser Flow:

```text
open()
 ↓ 
fstat()
 ↓ 
mmap()
 ↓ 
  pos = 0
    ↓ 
 [read length]
 [read type]
 [count]
 [advance]
    ↓ 
 end of file
    ↓   
  munmap()
    ↓ 
  close()
```

So what I built today:

- [X] Open file
- [X] Get size
- [X] mmap file
- [X] Walk messages with pos
- [X] Count message types
- [X] Benchmark messages/sec

OUTPUT first version (dry-run):

```text
mathdee@LaptopMathijs:/mnt/c/Users/faila/OneDrive/Documents/Project SPSC RIngBuffer/lob-repo$ ./build/parser
J: 34
Q: 17836
C: 99917
I: 4024315
K: 3
V: 1
P: 1218602
E: 5722824
F: 1485888
X: 2787676
U: 21639067
D: 114360997
A: 117145568
L: 215161
Y: 9013
H: 8966
R: 8906
S: 6
Total messages: 268744780
Messages/sec: 4.03391e+06
```

Time is pretty low for a dry run, the culprit is most likely: `std::unordered_map<char, uint64_t> counts;`.
First of all its in the HEAP so scattered nodes, each access is probably a cache miss, AND it looks up total_messages nb of times.

### FIX I HAVE IN MIND

- Use a flat array of size 256, everytime we see a type, `array[type]++`;
- Then we return those that have `array[type] > 0`;
- 256 indexes in `uint64_t` so the total bytes is 2048 bytes = 2 KB -> FITS inside L1 cache which has 32KB of space.

Memory layout comparison:

```text
Before: unordered_map                  After: uint64_t counts[256]

unordered_map                          Stack:
     |                                 +----------------+
     v                                 | counts[0]      |
  heap nodes                           | counts[1]      |
     |                                 | counts[2]      |
     v                                 | ...            |
 [key][value][pointer]                 | counts[255]    |
                                       +----------------+

  Cache misses on every lookup         Everything contiguous in L1 cache
```

| Metric          | Before (`unordered_map`) | After (`counts[256]`) |
| --------------- | ------------------------ | --------------------- |
| Messages/sec    | 4.03391e+06              | 3.61513e+07           |
| Improvement     |                          | **x9**                |


# August 7 2026

Pffff, pretty long break I took, let's not do that again !!

First thing I did, re-read: https://web.archive.org/web/20110219163448/http://howtohft.wordpress.com/2011/02/15/how-to-build-a-fast-limit-order-book/

rewriting what I forgot:
  The LOB has to be perfect and an optimized data structure because its the primary source of market information for trading models.

  Total volume of data per day is around 30+ GB nowadays, probably will be more with the markets looking into expanding to 23hour days.
  There are 3 main operations the LOB needs : add, cancel, execute. (need them all in O(1) for speed).
  
  According to wkselph, the most seen operations are add and cancel, so the focus will be more on making sure both are optimized and fastest if we have to choose what operations we have to allocate in L1 cache.

  Quick recap of what the 3 operations do:
  - add: places an order at the end of a list of orders to be executed at a particular limit price.
  - cancel: removes an order from anywhere in the book.
  - execute: removes an order from the inside of the book, inside of the book is the oldest buy order at the highest buying price and the oldest sell order at the lowest selling price.

  So each operation is keyed of an id number, so according to wkselph the hash table would make the most sense because the data structure allows for unique keys and each key pointing is to a non-unique value.

  The rest is same thing I wrote down July 5th.(going to read that again).

SOOOOOO, quick refresh on the specs, going to write what each symbol means:
   - A: This adds a new anonymous visible limit order to the book with a unique tracking ID.
   - F: same as A but limit order is visible, and includes the firm's specific MPID attribution.
   - E: This reduces an order's resting size after a partial/full execution at its original displayed price.
   - C: This logs partial/full execution of a resting order at a price different from its original display price.
   - X: This reduces the visible size of a resting order due by N to a partial cancellation.
   - D: This removes a resting order entirely from the book because it was fully canceled or deleted.
   - U: This replaces an existing order with a new tracking ID to update its prize or share size in an instant.


So what am I building now then:
  today:
[X] - OrderBook: unordered_map<order_ref->Order> + map of price levels ( bids/asks with FIFO queues).
[X] - Operations: add(A/F), cancel(X), delete(D), replace(U), execute(E/C).
[X] - Filter one symbol (AAPL via stock_locate from R), i'll ignore the rest.
[X] - Connect to the parser: decode the messaged and update book.
  other day:
[ ] - Debugging
[ ] - Running a full day on AAPL

notes for building:
  - each order looks like this: "ID42, buy, 100 shares at 150$"
  - the price levels are shelves each containing a list of identical buy prices, FIFO layout so first order in the shelve gets served first.
  - lookup binder allows for quick order lookup because operations only give us the ID, not the price.
  - Why am I using two maps for bids and asks instead of one? Because they will now sort in opposite directions for their best bid(highest buy price) and best ask (lowest sell price).
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
        151 -> [201].

    */
## What I built (so I can rebuild from scratch)
### 1) OrderBook data layout (`src/order_book.hpp`)
An order is just: id, side ('B'/'S'), price (ITCH int), quantity.
The book keeps the SAME order in 2 places:
  - orders_: unordered_map<id -> Order>  → fast find by ID (cancels/executes only give ID)
  - bids_ / asks_: map<price -> deque of ids> → shelves sorted by price, FIFO line at each price
Why 2 maps not 1: best bid = highest buy, best ask = lowest sell. Easier separate.
helper book_side('B'/'S') picks bids_ or asks_.
### 2) Ops (bookkeeping only, NO matching yet)
  - add: put in orders_, push_back id on that price shelf
  - delete_order (D): find order, remove id from its shelf, erase from orders_. If shelf empty, erase price level.
  - reduce (X/E/C): shrink qty. if shares >= qty → delete fully. Same function for cancel and execute for now.
  - replace (U): NOT an edit. Save side from old order FIRST, delete old id, add new id at BACK of queue (loses time priority). U message does not send side.
remove_from_level scans the deque for the id (O(n) for now, linked list later if I care).
### 3) AAPL filter in parser
R messages map stock_locate → ticker. Ticker is 8 chars space-padded so compare to "AAPL    " (4 spaces).
I saved aapl_locate. For A/F/E/C/X/D/U, read locate from body+1, skip if not AAPL.
Got: AAPL locate = 13
### 4) Wire parser → book
After filter passes, switch on type:
  A/F → book.add
  X/E/C → book.reduce
  D → book.delete_order
  U → book.replace (OrderReplace: orig_order_ref, new_order_ref, price, shares)
C++ tip that bit me: each case that does `auto m = ...` needs its own `{ }` or all the m's fight each other.
### Results after full day
  AAPL messages kept: 1512179
  Missing order refs: 0
So every E/X/D/U for AAPL hit an order I actually had. That's already a strong check.
### Rebuild checklist
1. Order + OrderBook with orders_/bids_/asks_
2. add / remove_from_level / delete_order / reduce / replace
3. In parser loop: find AAPL from R, filter by locate
4. switch decode → book calls
5. count missing_references, want 0
6. next: assert best bid < best ask after every update




# August 10 2026
What I still have to do:
[ ] - Debugging
[ ] - Running a full day on AAPL
Quick recap until now: 
- Learned the format of ITCH and I wrote a parser that reads the whole file and counts the message types.
- Built a hash-map that tracks orders through their IDs, and also put orders on 2 different price levels for buy and sell.
- Implemented operations: the big three - Add, Cancel & Execute. Also, Delete and Replace.
- I filtered Nasdaq ITCH data to isolate "AAPL" by its unique barcode which is 13, allows it to ignore all the rest for easier debugging. From the millions of messages counted we succesfully parsed the +1.5million order messages from APPLE without a single error.

What am I doing today:
- after every AAPL message, check if top buy < top sell, orders are of valid size.

I already got missing order refs: 0, so I didnt lose track of IDs
Now we just have to make sure that every time best bid < best ask to make sure that our operations didn't mess with the prices.
Implemented an invariant counter instead of writing asserts, reason is that if something fails only at the open I can still finish the whole day and also know how many fails I caused. Goal is a count of 0 before the day ends.


Encountered some issues in writing the methods, didn't call by reference so after calling methods in `check_invariants()`, they wouldn't save to bid because they would be local and get deleted after the function went out of scope.

Succesful results for today :).
Missing order refs: 0
Invariant failures: 0
Means that it all checks out normally and the next step is going from only AAPL to all stocks in the NASDAQ ITCH data.

Methods implemented today:
`bool best_bid(uint32_t& price) const;`
`bool best_ask(uint32_t& price) const;`
`bool check_invariants() const;`


# August 12 2026

So what did I already do?
- Wrote the parser, counted +260Million messages.
- focused on Apple stocks and implemented the add, cancel, delete, replace and execute operations.
- Ran a full day on AAPL and got 0 missing IDs and 0 invariant failures.


Today I'm going to run the full day with all stocks, not only "AAPL". 
And a thing I should add for today to make sure everything stays valid, for every execute and cancel I need to check,
if it exists in book, its first in line at its price level, its the best price on that side.

Ok so I am tracking R to see how many messages have been applied, also because it's running the whole file now instead of only one stock I wont check for all invariants every single time, I'll keep it simple and check only for best bid < best ask, should be faster because its a two map lookup.

Ok after first run : `cmake --build build && ./build/parser`.
The output is:
```Python
crossed book locate = 6286 type = A bid = 14200 ask = 13000
A : 117145568
C : 99917
D : 114360997
E : 5722824
F : 1485888
H : 8966
I : 4024315
J : 34
K : 3
L : 215161
P : 1218602
Q : 17836
R : 8906
S : 6
U : 21639067
V : 1
X : 2787676
Y : 9013
Total messages: 268744780
Messages/sec: 1.59463e+06
Symbols (R): 8906
Books seen: 8906
Book messages applied: 263241937
Missing order refs: 0
Invariant failures: 8649
```

Used command : `/usr/bin/time -v ./build/parser 2>&1 | tail -n 30`
Need to see if I am leaking orders, first step to identifying where the issue comes from.
The output for that was:
```Python
Messages/sec: 1.55693e+06
Symbols (R): 8906
Books seen: 8906
Book messages applied: 263241937
Missing order refs: 0
Invariant failures: 8649

        Command being timed: "./build/parser"
        User time (seconds): 159.45
        System time (seconds): 12.29
        Percent of CPU this job got: 99%
        Elapsed (wall clock) time (h:mm:ss or m:ss): 2:52.67
        Average shared text size (kbytes): 0
        Average unshared data size (kbytes): 0
        Average stack size (kbytes): 0
        Average total size (kbytes): 0
        Maximum resident set size (kbytes): 5945284
        Average resident set size (kbytes): 0
        Major (requiring I/O) page faults: 3
        Minor (reclaiming a frame) page faults: 179896
        Voluntary context switches: 43
        Involuntary context switches: 725
        Swaps: 0
        File system inputs: 16099137
        File system outputs: 0
        Socket messages sent: 0
        Socket messages received: 0
        Signals delivered: 0
        Page size (bytes): 4096
        Exit status: 0
```

Okkkk, so what can I make of these numberrrss.
First we have what we saw earlier, 8649 Invariant Failures in 8906 books. ~97% failure, which is too much.
But we have 0 missing order refs so that means that every message operation found its order.
Parsing isn't the issue here and hashing neither then.
Hmmmmm, so the issue happens after, maybe it's how I manipulate the book?

This many failures is probably a one time event happening in most books because it doesnt happen for every message.
In `parcer.cpp` -  ++invariant_fails; happens at each bid >= ask.
I wrote before that books fail invariants when they are crosses ( bid < ask>) or locked ( bid == ask).
So havind bid >= ask will fire at crossed and at locks. 

Quick note:
 - `Crossed = bid > ask` — genuinely invalid, someone's buy price is higher than someone's sell price, the exchange should have matched them.
 - `Locked = bid == ask` — buy and sell sitting at the exact same price. This is not invalid. Locked markets happen constantly in real exchange data, especially around the open, on thin/illiquid names, or with pegged/mid orders. NASDAQ ITCH books lock all the time without anything being wrong.

Crosses can be a bug, locks aren't bugs usually. So I need to track both separately to be able to identify the issue.
Wrote the changed code in `parser.cpp`.
```CPP
  uint32_t bid = 0, ask = 0;
  if(book.best_bid(bid) && book.best_ask(ask)){
      if(bid < ask) ++crossed_count;
      if(bid == ask) ++locked_count;
  }
```
### Results:
8649 Invariant Failures separate into: 
 - Crossed counted: 8579
 - Locked counted: 70

Ok, so I do not think that my code is the issue because the failures happen at every symbol cleanly, so its not random. So the issue would then be the data?
Sooo, What is true about every stock, every day, that I haven't taken into account yet, that would be where the solution is normally.
NASDAQ ITCH also saves the `pre-market`, where orders are already coming in and being added to the book, but nothing is getting matched or cleared. The orders pile up waiting for the market to open.
Now once it opens, NASDAQ runs `opening cross`, which looks at all the piled up orders and it matches them all together. It is only after that that the book behaves correctly and bid < ask.

And this would explain why nearly every symbol(~96%) shows a crossed book once, right before the market opens.
So the way to do so would be to start checking for invariants after the market opens. To do so we must track the message 'S' which is System event messages and then look for the message 'Q' which represents "Start of Market Hours" so only when market_open = true; can we check for invariants.

I implemented it in `parser.cpp`. Now run it again and see, the crossed count should drastically reduce.
### Results:
 - Missing order refs: 0
 - Crossed counted: 8535
 - Locked counted: 70
Still nothing, After reading into it, this is actually just what a raw, unfiltered ITCH feed genuinely looks like, the exchange reports the order landing on the book and its correcting execution as two separate sequential messages, it checks book validity in the split second between them, which is why I get a  "crossed" state that was never real from the exchange's point of view. 
I am going to assume it is because my parser is just fast enough to be able to catch the feed mid-event.




# August 17 2026

Ayeee, another day, another line of code to write am I right, heheheh ._.

Last time I ended on assuming things but I thought about it and because I have time to check it and I do not know nearly enough to assume things I am going to test it to make sure I understand what is happening.

8535 books ended up `crossed` (best bid > best ask) which should never happen in a healthy book.
I need to make sure that this isn't a bug or if it's how the exchange data looks.

read this: `https://softwareengineering.stackexchange.com/questions/412908/what-makes-a-before-after-vs-only-before-approach-to-logging-more-effective`

The before/after logger will only turn on when something triggers it and it will record the next order that is logged.
First I implemented `pending_cross` an unordered_map from stock symbol to the details of the crossing event -> what message caused it, what time(nanoseconds), order ID?, bad bid/ask?
So every time yhe book detects a cross (bid > ask), I save the symbol in `pending_cross` only if it wasn't already there. This allows me to only save the first cross for that symbol and not every one after.

I then check at the top of the message-processing loop if the symbol is sitting in the waiting room?
Because this message is the very next thing that happened to the symbol after it got flagged I print both events and remove them from `pending_cross` so I don't log it again.

The idea is to catch the issue, then catch whatever comes after and see if the next order is fixing it or making it worse.

The reason I did this is because I thought that maybe the exchange sends order landed and order got executed as two separate messages and that my program was so fast that it checked the book state in the very tiny gap between them. And if that were true, the crossing message is `A` and the very next message for that symbol is `E`/`C`/`X` that fixes it, ~seconds later.

### Results after running code:
```Python
    ref=10234516 dt_ns=2893879
    CROSS locate=7598 first=A ts=34201487889885 ref=10234516 bid=65100 ask=65000 | next=A ts=34201487911324 ref=10234520 dt_ns=21439
    CROSS locate=7598 first=A ts=34201487911324 ref=10234520 bid=65100 ask=65000 | next=A ts=34201487912386 ref=10234524 dt_ns=1062
    CROSS locate=6286 first=A ts=34201538619358 ref=7968431 bid=14400 ask=13000 | next=A ts=34201538644300 ref=7968439 dt_ns=24942
    CROSS locate=6286 first=A ts=34201538644300 ref=7968439 bid=14400 ask=13000 | next=A ts=34201538646880 ref=7968443 dt_ns=2580
    CROSS locate=6286 first=A ts=34201538646880 ref=7968443 bid=15100 ask=13000 | next=A ts=34201538661324 ref=7968451 dt_ns=14444
```

Here I see that as speculated, the crossing message was `A`. However so was the next message, this happened 5 times in a row. New unrelated orders just keep landing on top of it which directly ends my assumption because nothing is fixing itself. The book is just crossed while new orders pile on top.
Also something else I spotted is the time, when I convert `34201487889885` nanoseconds into the time of day I get `09:30:01` for all of them with just microsecond gaps. This shows that it's all happening right as the market opens and not throughout the day.

So, after doing more reading into NASDAQ ITCH, I realised that each individual stock runs its own auction at 9:30 AM which is when all the pre-market piled-up orders get matched against each other in one batch. So we have to start looking when a specific stock is done with their auction and starts trading normally. the message type `H` is for the stock trading actions and thus we need to look for the flag `T` which stands for "now trading".

My issue was that I checked `market_open` one time for all stocks, but instead I need to check if each stock has finished their own auction. 

## So let's implement that too.
Had to implement a new decode struct in `itch_messages.hpp` for `H`, switched `bool market_open = false` with `std::unordered_map<uint16_t, bool> trading;` to track individual stocks.
Added a new `else if(type == 'H){}` that sets true or false based on `h.trading_state == 'T'`

Now time to rerun and see what we come up with. Normally I should have 0 crossed.

### Results:
```Python
    CROSS locate=8900 first=C ts=36022633521012 ref=10303868 bid=147500 ask=10000 | next=C ts=36022633526282 ref=10303880 dt_ns=5270
    CROSS locate=8900 first=C ts=36022633526282 ref=10303880 bid=147500 ask=10000 | next=C ts=36022633526540 ref=10303864 dt_ns=258
    CROSS locate=8900 first=C ts=36022633526540 ref=10303864 bid=147500 ask=10000 | next=C ts=36022633528571 ref=10303860 dt_ns=2031
    CROSS locate=1334 first=C ts=36287932322086 ref=39643245 bid=7900 ask=7700 | next=C ts=36287932322640 ref=42907457 dt_ns=554
    CROSS locate=7241 first=C ts=36657018494017 ref=57550236 bid=184800 ask=160000 | next=C ts=36657018494251 ref=59836540 dt_ns=234
    CROSS locate=7241 first=C ts=36657018494251 ref=59836540 bid=180000 ask=160000 | next=C ts=36657018495449 ref=56247932 dt_ns=1198
    CROSS locate=7241 first=C ts=36657018504747 ref=56458584 bid=176000 ask=160000 | next=C ts=36657018505941 ref=57347400 dt_ns=1194
    CROSS locate=7241 first=C ts=36657018505941 ref=57347400 bid=176000 ask=160000 | next=C ts=36657018506198 ref=57672948 dt_ns=257
    CROSS locate=7241 first=C ts=36657018506198 ref=57672948 bid=176000 ask=160000 | next=C ts=36657018507337 ref=57736116 dt_ns=1139
    CROSS locate=7241 first=C ts=36657018507337 ref=57736116 bid=176000 ask=160000 | next=C ts=36657018508540 ref=58788348 dt_ns=1203
    CROSS locate=7241 first=C ts=36657018508540 ref=58788348 bid=176000 ask=160000 | next=C ts=36657018530068 ref=58912508 dt_ns=21528
    CROSS locate=7241 first=C ts=36657018530068 ref=58912508 bid=175600 ask=160000 | next=C ts=36657018531155 ref=59131588 dt_ns=1087
    CROSS locate=7241 first=C ts=36657018531155 ref=59131588 bid=175000 ask=160000 | next=C ts=36657018532358 ref=57926808 dt_ns=1203
```
 - `Crossed counted: 634`
 - `Locked counted: 8`

So we removed 7901 crossed counts. A 92.5% decrease which is good. 
Looking at the timestamps, they are around `10:00 AM` which is a whole 30 minutes after each individual auction end. Thus, opening-auction problem I tried to solved was solved !!!!!.

The current crossed message type is `C` which stands for Order Executed with Price.

I looked more into it and foudd this article: `https://onlinelibrary.wiley.com/doi/full/10.1111/jfir.12414`.

So basically what it is:
 - An `iceberg/reserve` order is one large order where we can only see a small portion of it in the public order book. The rest is hidden reserve quantity.
 - Looks like this:
   - Total Order = 100,000 shares
   - Visible = 1,000 shares
   - Hidden = 99,000 shares
 - When the 1,000 visible shares are consumed the exchange replenishes the visible quantity from the hidden reserve. Repeats until hidden is empty.
 - The visible quantity can be randomized, isn't always the same amount.

Need to check if the remaining 634 crosses are caused by iceberg/reserves orders.
To do so we need to look for a pattern that would allow me to assure that the leftover crosses are consistent with reserve/iceberg replenishemnt.
Pattern: execution -> new order at same price -> execution -> replenishment
Doing this check will allow me to infer it from the pattern but I won't be able to 100% prove that the hidden portion exists only from ITCH.

## Proving the Iceberg Theory
To actually test this, I stopped trying to guess a cutoff time and just logged the raw time deltas (`dt_ns`) and "chain lengths" (how many times the exact same price/side got executed and replenished back to back) to a CSV to see what the distribution actually looked like. 

If this was just random humans or external algos reacting to a trade, there would be a wide spread of times due to network jitter, and the chains would get interrupted constantly.

### Results from the CSV:
* **Total "Refills" (Chain > 0)**: 236,401
* **Median Refill Time**: 82.3 µs
* **Max Chain Length**: 500 (for locate 4169)

The histogram showed a massive, extremely tight cluster of refill times between 10µs and 100µs. That is practically instantaneous. 

The chains confirm the mechanism. A chain of 500 means the exact same price and side got hit and replenished 500 times in a row without a single external order landing elsewhere to break the sequence. 

THUS, Humans or external algorithms cannot perfectly time 500 consecutive sub-100µs round-trips over a network. "I won't be able to 100% prove that the hidden portion exists only from ITCH., executing a hardcoded `if (reserve > 0) { replenish(); }` hot path entirely in memory. 

ITCH will never give me a boolean `is_iceberg` flag because that defeats the whole point of a hidden reserve. But the results I got, but the sub-100µs latencies and the unbroken 500-length chains are strong, hard-to-explain-otherwise evidence that this is what's happening, and it accounts for the residual crosses.

Pff, long and hard but we got there.


# August 18 2026

Today, I am going to implement the matching engine. It does the opposite of `OrderBook`, it makes decisions. I will feed it a new order, and it has to figure out, right now, using only the orders already resting in the book: does this new order trade against anyone? With whom? How much? At what price? And if there's anything left over, where does it go?

Watched: `https://www.youtube.com/watch?v=NH1Tta7purM`

The main rule -> price-time priority.
When three people have sell orders `{100:$10.00, 200:$10.00, 50:$9.99}` that are resting and someone submits a buy order `{120:$10.01}`:
 - Price priority: First the buyer buys the best available price even if it was placed later, which is all of the 50:$9.99 order.
 - Time priority: Second, the buyer still neds 70 shares to buy, it will always buy them from the oldest resting order which is `{100:$10.00}`.

 The leftover is the 100-70 = 30 shares that the buyer bought. Order should have 30 shares now and keep same price and time priority.

 Created the `matching_engine.hpp` file. Contains the submit() logic.
 What it does:
  - Look at best price level on the ask side.
  - Is that ask price <= what the buyer wants to pay? if not, stop.
  - If yes look at front of price level deque, oldest resting order.
  - Match as much as possible: `std::min(incoming remaining, resting order's remaining)`
  - Record a Fill. Reduce's both sides remaining quantity by the matched amount.
  - When resting order hits 0, remove it from the book.
  - If incoming order wants more, go back to first step because there can be more at the same price level.
  - If incoming order is at 0, it stops.
  - If all compatible price levels are at 0 and the incoming order still has quantity left, rest remainder in book as a new order.

All this is mainly for buy orders. However sell orders are the same, only difference is check the bid side, match while bid price >= what the seller wants.

Separated `struct Order{};` into its own file so `matching_engine.hpp` and `order_book.hpp` can both call it.
## Connecting the Matching Engine & Unit Testing

After writing `matching_engine.hpp`, I needed to hook it up to a buildable target. Up to this point, `CMakeLists.txt` only handled the `parser` executable, so I set up a dedicated target just for running tests.

I used `add_library(itch_headers INTERFACE)`, which acts as an interface target carrying `target_include_directories`. Any target linking against it inherits `src/` on its include path—allowing `parser.cpp` and test files to use `#include "matching_engine.hpp"` directly without relative path workarounds (`../`).

Added to `CMakeLists.txt`:

```cmake
add_executable(test_matching_engine test/test_matching_engine.cpp)
target_link_libraries(test_matching_engine PRIVATE itch_headers)

```

Running `cmake --build build` now produces both executables.

---

## First Test: Full Match & Exhaustion

The initial test in `test/test_matching_engine.cpp` covers a complete fill between one resting order and one incoming order. Rather than asserting loosely with `fills.size() == 1`, the test explicitly verifies the full match payload:

* `incoming_id`
* `resting_id`
* `price`
* `quantity`
* Post-trade state: `best_bid` and `best_ask` are both empty.

```text
Tests Passed

```

The pipeline verified end-to-end: CMake target $\to$ compiler $\to$ assertions $\to$ matching engine logic.

---

## Implementing `cancel()` and `replace()`

Before writing the remaining test suite, I added two core methods required to test cancel-execute races and priority loss on replacement:

* **`cancel(id)`**: Mirrors the existing `OrderBook::delete_order` logic. Locates the order, identifies its side and price level, removes it from the level's `std::deque`, cleans up empty price levels, and erases the entry from `orders_`.
* **`replace(old_id, new_id, new_price, new_quantity)`**: Handles replacement semantics matching ITCH `U` messages (cancel + new insert rather than an in-place update).
* The side must be cached in a local variable **before** calling `cancel(old_id)`, as the record in `orders_` is destroyed during cancellation.
* Constructs a new `Order` and calls `push_back` on the designated price level, placing it at the tail of the queue and resetting time priority even if the price remains unchanged.
* Reuses `cancel()` internally to avoid duplicating order removal logic.



Rerunning the initial test confirmed zero regressions.

---

## Targeted Test Suite

Instead of writing repetitive cases, the test suite was split into 8 specific behavioral categories across 12 total tests:

1. **Partial fills** (both incoming and resting partial executions)
2. **Multi-level sweeps**
3. **Time priority at identical price levels**
4. **No-match scenarios / resting on an empty book**
5. **Exact level exhaustion and memory cleanup**
6. **Cancellations** (preventing subsequent matches and handling unknown IDs)
7. **Order replacement priority resets**
8. **Invariants** (ensuring the book never crosses after a sequence)

---

## Test Assertion Bug & Fix

During multi-level sweep testing, an assertion failed:

```text
test_matching_engine: .../test/test_matching_engine.cpp:62: void test_sweep_multiple_levels(): Assertion `fills[0].price == 100 && fills[0].price == 101' failed.
Aborted (core dumped)

```

**Root cause:** A copy-paste error in the test assertion itself evaluated `fills[0]` twice against two distinct values instead of inspecting `fills[1]`.

**Fix:**

```cpp
assert(fills[0].price == 100 && fills[1].price == 101);

```

---

## Final Results

```text
Tests Passed 
test_incoming_partial_fill PASSED!!
test_resting_partial_consumption PASSED!!
test_sweep_multiple_levels PASSED
test_time_priority_same_price PASSED!!
test_no_match_price_too_low PASSED!!
test_empty_book_rests_immediately PASSED!!
test_exact_exhaustion_removes_level PASSED
test_cancel_prevents_match PASSED!!
test_cancel_unknown_id_fails PASSED!!
test_replace_loses_time_priority PASSED!!
test_book_never_crosses_after_sequence PASSED!!
All tests passed.

```

**Summary:** 12/12 tests passing across all critical matching invariants, FIFO priority guarantees, and order lifecycle edge cases.

```

```