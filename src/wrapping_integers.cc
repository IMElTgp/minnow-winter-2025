#include "wrapping_integers.hh"
#include "debug.hh"
#include <algorithm>

using namespace std;

Wrap32 Wrap32::wrap( uint64_t n, Wrap32 zero_point )
{
  return Wrap32 {
    static_cast<uint32_t>( ( n + static_cast<uint64_t>( zero_point.raw_value_ ) ) % ( 1ULL << 32 ) ) };
}

uint64_t Wrap32::unwrap( Wrap32 zero_point, uint64_t checkpoint ) const
{
  uint64_t offset = ( raw_value_ - zero_point.raw_value_ ) & 0xFFFFFFFFULL;
  uint64_t checkpoint_base = ( checkpoint & ~0xFFFFFFFFULL );
  uint64_t candidate = checkpoint_base + offset;
  uint64_t best = candidate;
  uint64_t best_dist = ( best > checkpoint ) ? ( best - checkpoint ) : ( checkpoint - best );

  uint64_t cands[2]
    = { candidate + ( 1ULL << 32 ), ( candidate >= ( 1ULL << 32 ) ? candidate - ( 1ULL << 32 ) : candidate ) };

  for ( uint64_t c : cands ) {
    uint64_t dist = ( c > checkpoint ) ? ( c - checkpoint ) : ( checkpoint - c );
    if ( dist < best_dist ) {
      best = c;
      best_dist = dist;
    }
  }
  return best;
}
