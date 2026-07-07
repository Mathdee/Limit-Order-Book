# Table of Contents

- [July 5 2026](#july-5-2026)
- [July 6 2026](#july-6-2026)

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
