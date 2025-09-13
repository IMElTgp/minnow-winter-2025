#include "tcp_receiver.hh"
#include "debug.hh"

using namespace std;

void TCPReceiver::receive( TCPSenderMessage message )
{
  // if RST sign of the incoming msg is true, set RST
  if ( message.RST ) {
    has_RST_ = true;
    // set error for an RST
    reassembler_.writer().set_error();
    return;
  }
  // if seen SYN with ISN sign not set, set ISN sign and ISN
  if ( !isn_set_ ) {
    if ( message.SYN ) {
      ISN_ = message.seqno;
      isn_set_ = true;
    } else {
      // skip packets without SYN while ISN not set (these packets aren't for us now)
      return;
    }
  }
  // stream_index excludes SYN, thus minus 1
  // bytes_pushed_ is a stream index, thus plus 1 for absolute seqno
  // if the packet owns a SYN sign, plus 1 to skip it
  uint64_t stream_index = message.seqno.unwrap( ISN_, 1 + reassembler_.writer().bytes_pushed() ) - 1 + message.SYN;

  reassembler_.insert( stream_index, message.payload, message.FIN );
}

TCPReceiverMessage TCPReceiver::send() const
{
  TCPReceiverMessage msg;
  // process ackno, next index wanted
  if ( !isn_set_ ) {
    msg.ackno = nullopt;
  } else {
    // includes SYN(+1)
    uint64_t ack_abs = reassembler_.writer().bytes_pushed() + 1;
    if ( reassembler_.writer().is_closed() ) {
      // includes FIN
      ack_abs++;
    }

    // wrap into seqno (from absolute seqno)
    msg.ackno = Wrap32::wrap( ack_abs, ISN_ );
  }
  // process window_size
  // cut off to 65535, which is determined by TCP
  msg.window_size = min<uint64_t>( ( 1ULL << 16 ) - 1, reassembler_.writer().available_capacity() );
  // process RST
  // wtf is set_error()
  msg.RST = has_RST_ || reassembler_.writer().has_error();

  return msg;
}
