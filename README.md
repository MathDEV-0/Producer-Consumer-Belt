Producer-Consumer Simulation with Bounded Buffer
================================================

Description
--------------

This project simulates a classic producer-consumer problem using POSIX threads (pthreads) in C. It models a mechanical conveyor belt system where:

-   3 producers generate products with random quality (B = Good, M = Bad)

-   2 consumers remove products from the belt for processing

-   Buffer capacity is limited to 3 products

-   Producers have longer processing times than consumers, creating pressure on the buffer

The simulation demonstrates proper thread synchronization using mutexes and condition variables to prevent race conditions, data corruption, and ensure correct product flow.

* * * * *

Project Structure
---------------------

```
producer_consumer_buffer.c
├── Product struct
├── Buffer management (insert/remove)
├── Producer threads (3)
├── Consumer threads (2)
├── Statistics collection
└── Signal handler (Ctrl+C for stats)
```

* * * * *

Compilation and Execution
----------------------------

### Compile:

```
gcc -pthread producer_consumer_buffer.c -o producer_consumer
```

### Run:


```
./producer_consumer
```

### Stop (show statistics):

Press `Ctrl+C` to display final statistics.

* * * * *

Sample Output
----------------


```
Producer[0] produced product[1] (B), buffer count=1, created at 1700000000.123456789
Producer[1] produced product[2] (M), buffer count=2, created at 1700000001.234567890
Consumer[0] consumed product[1] (B), buffer count=1, created at 1700000000.123456789, consumed at 1700000002.345678901
Producer[2] produced product[3] (B), buffer count=2, created at 1700000003.456789012
...
```


* * * * *

Implementation Details
-------------------------

### Product Structure

```
typedef struct {
    short int id;                    // Unique product identifier
    char quality;                    // B (Good) or M (Bad)
    short int type;                  // Reserved for future extensions
    struct timespec time_create;     // Creation timestamp
    struct timespec time_out;        // Consumption timestamp
} Product;
```
### Buffer Implementation

The buffer is a fixed-size array with capacity `BUFFER_LEN = 3`. Each product occupies one position. When a product is removed, its ID is set to 0 to mark the slot as empty.

### Producer Threads

-   Generate products with random quality (50% B, 50% M)

-   Acquire mutex before accessing buffer

-   Wait if buffer is full (`pthread_cond_wait`)

-   Insert product and signal consumers

-   Sleep 2-5 seconds (simulating production time)

### Consumer Threads

-   Acquire mutex before accessing buffer

-   Wait if buffer is empty (`pthread_cond_wait`)

-   Remove product and calculate time on belt

-   Classify product by quality

-   Signal producers and release mutex

-   Sleep 1-2 seconds (simulating processing time)

* * * * *

What Happens Without Synchronization?
----------------------------------------

### Example 1: Without Mutex

If we remove all mutex locks, multiple threads can access the buffer simultaneously, causing:

```
// WITHOUT MUTEX - RACE CONDITION
void* p_func_no_mutex(void *args) {
    while(1) {
        Product p = createProduct();

        // NO MUTEX - multiple threads can enter here!
        while(count == BUFFER_LEN) { /* wait */ }

        // PROBLEM: count could be modified by another thread
        // between check and insertion
        total_produced++;
        insertProduct(p);  // May overwrite existing product!

        // count may become inconsistent
    }
}
```

Consequences:

-   Race conditions: Two producers may insert into the same buffer slot

-   Data corruption: Products may be overwritten before consumption

-   Inconsistent count: `count` variable may not reflect actual buffer state

-   Lost products: Products may disappear without being consumed

-   Duplicate consumption: Same product may be consumed multiple times

Example of corrupted output:

```
Producer[0] produced product[1] (B), buffer count=1
Producer[1] produced product[2] (M), buffer count=2
Consumer[0] consumed product[1] (B)  // OK
Consumer[1] consumed product[1] (B)  // WRONG! Product already consumed
Producer[0] produced product[3] (B), buffer count=2
Consumer[0] consumed product[3] (B)  // Works sometimes
Consumer[1] consumed product[0] (?)  // Reading uninitialized data!
```
* * * * *

### Example 2: Without Condition Variables

If we remove condition variables and use busy-waiting:

```
// WITHOUT CONDITION VARIABLES - BUSY WAITING
void* p_func_no_cond(void *args) {
    while(1) {
        Product p = createProduct();

        pthread_mutex_lock(&mutex);
        // BUSY WAIT - consumes CPU unnecessarily
        while(count == BUFFER_LEN) {
            pthread_mutex_unlock(&mutex);
            usleep(1000);  // Still wastes CPU cycles
            pthread_mutex_lock(&mutex);
        }

        total_produced++;
        insertProduct(p);
        pthread_mutex_unlock(&mutex);

        // NO SIGNAL to consumers - they must poll!
        sleep(rand() % 4 + 2);
    }
}
```

Consequences:

-   CPU waste: Threads constantly loop checking condition

-   Starvation: Consumers may never know when products are available

-   Inefficiency: High CPU usage with no productive work

-   Delayed response: Threads may take milliseconds to detect changes

-   Increased latency: Products may sit in buffer longer

* * * * *

Why Mutex + Condition Variables Are Essential
------------------------------------------------

### Correct Synchronization Pattern:

```
// Producer
pthread_mutex_lock(&mutex);
while(count == BUFFER_LEN) {
    pthread_cond_wait(&not_full, &mutex);  // Atomically release mutex and wait
}
// Critical section: buffer access
insertProduct(p);
pthread_cond_signal(&not_empty);  // Wake up waiting consumers
pthread_mutex_unlock(&mutex);
```

Benefits:

1.  Mutual exclusion: Only one thread accesses buffer at a time

2.  No busy waiting: Threads sleep when conditions aren't met

3.  Atomic operations: Condition wait releases mutex atomically

4.  Correct signaling: Consumers are notified immediately when products arrive

5.  Consistent state: `count` always reflects actual buffer contents

* * * * *

Testing Guide
----------------

### Test 1: Basic Functionality

Run the program and observe:

-   Products are created and consumed continuously

-   Buffer count never exceeds capacity (3)

-   Buffer count never goes negative

### Test 2: Without Mutex

Ryb the file with the sufix "_no_mutex".

Expected behavior:

- The `count` variable (responsible for tracking the number of items in the buffer) becomes inconsistent.
- The value of `count` may become negative or not reflect the actual state of the buffer.
- Consumers may consume the same item simultaneously (duplicate consumption).
- Producers may overwrite positions simultaneously (inconsistent production).
- The overall state of the system becomes unreliable.
### Test 3: Without Condition Variables

Run the code with the sufix "_mutex":

Expected behavior:

-   Program runs but with higher CPU usage

-   Responses may be slower

-   No crashes, but inefficiency is visible via `top` command

### Test 4: Timing Analysis

Observe production vs consumption rates:

-   Producers sleep 2-5 seconds

-   Consumers sleep 1-2 seconds

-   Buffer should frequently become full or empty

-   Demonstrates synchronization necessity

* * * * *

Statistics Output
--------------------

When pressing `Ctrl+C`, the program displays:

```
=== STATISTICS ===
Total produced: 47
Total consumed: 42
Total in-process: 3
Average time on belt: 2.345678 s
Total good quality: 21
Total bad quality: 21
Total discarded: 2
```

Metrics explained:

-   Total produced: Products created by producers

-   Total consumed: Products removed by consumers

-   Total in-process: Products currently in buffer

-   Average time on belt: Mean time from creation to consumption

-   Total good/bad: Quality distribution

-   Total discarded: Products lost due to buffer overflow

* * * * *

Key Learning Points
----------------------

1.  Thread synchronization is critical for shared resource access

2.  Mutex provides mutual exclusion, preventing race conditions

3.  Condition variables enable efficient waiting without CPU waste

4.  Producer-consumer pattern is fundamental in concurrent programming

5.  Buffer capacity creates natural pressure that tests synchronization

6.  Timing differences between threads reveal synchronization issues
