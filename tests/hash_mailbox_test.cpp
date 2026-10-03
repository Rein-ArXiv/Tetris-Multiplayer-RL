#include "net/hash_mailbox.h"
#include <cstdio>
#include <cstdlib>
#define REQUIRE(x) do { if (!(x)) { std::fprintf(stderr,"line %d: %s\n",__LINE__,#x); std::abort(); } } while(false)

int main() {
    net::HashExchange exchange;
    net::HashComparison comparison{99, 11, 22};
    // Remote advancement cannot erase an earlier mismatch awaiting local work.
    REQUIRE(exchange.record_remote(600, 9) == net::HashPut::stored);
    REQUIRE(exchange.record_remote(1200, 22) == net::HashPut::stored);
    REQUIRE(exchange.record_local(1200, 22) == net::HashPut::stored);
    REQUIRE(!exchange.poll(comparison) && comparison.tick == 99);
    REQUIRE(exchange.record_local(600, 11) == net::HashPut::stored);
    REQUIRE(exchange.poll(comparison) && comparison.tick == 600);
    REQUIRE(comparison.local == 11 && comparison.remote == 9);
    REQUIRE(exchange.poll(comparison) && comparison.tick == 1200);
    REQUIRE(comparison.local == 22 && comparison.remote == 22);
    REQUIRE(!exchange.poll(comparison) && comparison.tick == 1200);

    exchange.clear();
    net::HashSample sample{99, 88};
    REQUIRE(!exchange.latest_remote(sample) && sample.tick == 99 && sample.hash == 88);
    REQUIRE(exchange.record_remote(0, 0) == net::HashPut::invalid);
    REQUIRE(exchange.record_remote(601, 0) == net::HashPut::invalid);
    for (unsigned i = 1; i <= 8; ++i)
        REQUIRE(exchange.record_remote(i * 600, 0) == net::HashPut::stored);
    REQUIRE(exchange.record_remote(5400, 0) == net::HashPut::too_far);
    REQUIRE(exchange.record_remote(600, 0) == net::HashPut::duplicate);
    REQUIRE(exchange.record_remote(600, 1) == net::HashPut::conflict);
    REQUIRE(exchange.record_local(600, 0) == net::HashPut::stored);
    REQUIRE(exchange.poll(comparison) && comparison.local == 0 && comparison.remote == 0);
    REQUIRE(exchange.record_remote(600, 1) == net::HashPut::stale);
    REQUIRE(exchange.record_remote(5400, 0) == net::HashPut::stored);
}
