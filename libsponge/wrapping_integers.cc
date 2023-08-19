#include "wrapping_integers.hh"

// Dummy implementation of a 32-bit wrapping integer

// For Lab 2, please replace with a real implementation that passes the
// automated checks run by `make check_lab2`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

//! Transform an "absolute" 64-bit sequence number (zero-indexed) into a WrappingInt32
//! \param n The input absolute 64-bit sequence number
//! \param isn The initial sequence number
WrappingInt32 wrap(uint64_t n, WrappingInt32 isn) {
    // result - isn + 2>>32 * x = n
    uint32_t result;
    n = n & 0x00000000FFFFFFFF;
    result = n + isn.raw_value();
    // ^ is XOR operator!! not exponent operator!!
    // result = n + (2 ^ 32) + isn.raw_value();

    return WrappingInt32{result};
}

//! Transform a WrappingInt32 into an "absolute" 64-bit sequence number (zero-indexed)
//! \param n The relative sequence number
//! \param isn The initial sequence number
//! \param checkpoint A recent absolute 64-bit sequence number
//! \returns the 64-bit sequence number that wraps to `n` and is closest to `checkpoint`
//!
//! \note Each of the two streams of the TCP connection has its own ISN. One stream
//! runs from the local TCPSender to the remote TCPReceiver and has one ISN,
//! and the other stream runs from the remote TCPSender to the local TCPReceiver and
//! has a different ISN.
uint64_t unwrap(WrappingInt32 n, WrappingInt32 isn, uint64_t checkpoint) {
    // n -isn + (1u << 32) * x = result; |result - checkpoint| < (1u << 32)
    uint64_t result = n - isn >= 0 ? n - isn : n - isn + (1ul << 32);
    uint64_t checkpoint_high = checkpoint & 0xFFFFFFFF00000000;
    uint64_t checkpoint_low = checkpoint & 0x00000000FFFFFFFF;
    if (result - checkpoint_low > (1u << 31) && result - checkpoint_low <= (1ul << 32)) {
        if (checkpoint_high != 0) {
        result += checkpoint_high - (1ul << 32);
        } else {
        result += checkpoint_high;
        }
    } else if (result - checkpoint_low <= (1ul << 31)) {
        result += checkpoint_high;
    } else if (checkpoint_low - result <= (1ul << 31)) {
        result += checkpoint_high;
    } else {
        result += checkpoint_high + (1ul << 32);
    }

    return result;
}
