#include "byte_stream.hh"

// using namespace std;

ByteStream::ByteStream( uint64_t capacity )
  : capacity_( capacity ), error_( false ), buffer_(), bytes_pushed_( 0 ), bytes_popped_( 0 ), closed_( false )
{}

void Writer::push( std::string data )
{
  // (void)data; // Your code here.
  if ( closed_ ) {
    return;
  }

  const size_t n = std::min<size_t>( available_capacity(), data.size() );
  buffer_.append( data.data(), n );

  bytes_pushed_ += static_cast<uint64_t>( n );
}

void Writer::close()
{
  // Your code here.
  closed_ = true;
}

bool Writer::is_closed() const
{
  return closed_; // Your code here.
}

uint64_t Writer::available_capacity() const
{
  return capacity_ - buffer_.size(); // Your code here.
}

uint64_t Writer::bytes_pushed() const
{
  return bytes_pushed_; // Your code here.
}

std::string_view Reader::peek() const
{
  return std::string_view( buffer_.data(), buffer_.size() ); // Your code here.
}

void Reader::pop( uint64_t len )
{
  // (void)len; // Your code here.
  const size_t n = std::min<size_t>( len, buffer_.size() );
  buffer_.erase( 0, n );
  bytes_popped_ += n;
}

bool Reader::is_finished() const
{
  return closed_ && buffer_.size() == 0; // Your code here.
}

uint64_t Reader::bytes_buffered() const
{
  return buffer_.size(); // Your code here.
}

uint64_t Reader::bytes_popped() const
{
  return bytes_popped_; // Your code here.
}
