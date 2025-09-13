#include "reassembler.hh"
#include "debug.hh"

// using namespace std;

void Reassembler::insert( uint64_t first_index, std::string data, bool is_last_substring )
{
  // if the segment is the EOF segment, set eof_index_
  if ( is_last_substring ) {
    eof_index_ = first_index + data.size();
    if ( data.size() == 0 && first_index == next_index ) {
      output_.writer().close();
      return;
    }
  }
  // if the index of incoming segment is totally ahead of next_index, drop it
  if ( first_index + data.size() <= next_index ) {
    return;
  }
  // if the segment is overwhelming the ByteStream buffer, cut off the rest
  uint64_t off = ( first_index > next_index ) ? 0 : ( next_index - first_index );
  uint64_t definite_border = next_index + output_.writer().available_capacity();
  if ( definite_border <= first_index + off ) {
    return;
  }
  std::string str_to_append
    = data.substr( off, std::min<uint64_t>( data.size() - off, definite_border - first_index - off ) );
  // insert str_to_append to Reassembler::pending
  if ( !str_to_append.size() ) {
    return;
  }
  uint64_t key = std::max<uint64_t>( first_index, next_index );
  if ( pending.find( key ) == pending.end() ) {
    pending[key] = str_to_append;
    pending_count_ += str_to_append.size();
  } else {
    std::string origin_str = pending[key];
    // if first_index > next_index: first_index_of_ori == first_index_of_toappend
    // elif first_index <= next_index: begin_of_ori == begin_of_toappend
    // thus origin_str and str_to_append are aligned by head
    if ( origin_str.size() >= str_to_append.size() ) {
      pending[key] = origin_str;
    } else {
      pending[key] = str_to_append;
      pending_count_ += str_to_append.size() - origin_str.size();
    }
  }

  // Merge between keys
  // the left
  auto it = pending.lower_bound( key );

  while ( it != pending.begin() && it != pending.end() ) {
    auto prev = std::prev( it );
    const uint64_t tail_index = prev->first + prev->second.size();
    if ( tail_index >= it->first ) {
      // merge
      const uint64_t merge_len
        = tail_index >= it->first + it->second.size() ? 0 : it->second.size() - tail_index + it->first;
      if ( merge_len == 0 ) {
        pending_count_ -= it->second.size();
        pending.erase( it );
        it = prev;
        continue;
      }
      prev->second
        += it->second.substr( static_cast<size_t>( tail_index - it->first ), static_cast<size_t>( merge_len ) );
      // split pending_count_ += merge_len - it->second.size() in case unsigned integers overflow
      pending_count_ += merge_len;
      pending_count_ -= it->second.size();
      // delete
      pending.erase( it );
      it = prev;
    } else {
      break;
    }
  }

  // the right
  auto next = std::next( it );
  while ( next != pending.end() && it != pending.end() ) {
    // next = std::next( next );
    if ( next == pending.end() ) {
      break;
    }
    // const uint64_t definite_border = next_index + output_.writer().available_capacity();
    if ( next->first >= definite_border )
      break;
    const uint64_t head_index = next->first;
    const uint64_t R = it->first + it->second.size();
    if ( head_index <= R ) {
      // merge
      const uint64_t right_border = std::min<uint64_t>( next->first + next->second.size(), definite_border );
      const uint64_t merge_len = right_border > R ? right_border - R : 0;
      if ( merge_len == 0 ) {
        pending_count_ -= next->second.size();
        // delete current `next` and move forward
        auto temp = next;
        next = std::next( next );
        pending.erase( temp );
        continue;
      }
      it->second += next->second.substr( static_cast<size_t>( R - head_index ), static_cast<size_t>( merge_len ) );
      pending_count_ += merge_len;
      pending_count_ -= next->second.size();
      // delete current `next` and move forward
      auto temp = next;
      next = std::next( temp );
      pending.erase( temp );
    } else {
      break;
    }
  }

  // Attempting to commit segments
  for ( it = pending.begin(); it != pending.end(); ) {
    if ( it->first > next_index ) {
      break;
    }
    if ( !it->second.size() ) {
      it = pending.erase( it );
      continue;
    }
    output_.writer().push( it->second );
    next_index += it->second.size();
    // prevent from visiting invalid iterator
    pending_count_ -= it->second.size();
    it = pending.erase( it );
  }

  // deal with EOF
  if ( eof_index_ > 0 && next_index == eof_index_ ) {
    output_.writer().close();
  }
}

// How many bytes are stored in the Reassembler itself?
// This function is for testing only; don't add extra state to support it.
uint64_t Reassembler::count_bytes_pending() const
{
  return pending_count_;
}
