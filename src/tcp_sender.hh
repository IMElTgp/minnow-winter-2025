#pragma once

#include "byte_stream.hh"
#include "tcp_receiver_message.hh"
#include "tcp_sender_message.hh"

#include <deque>
#include <functional>

class TCPSender
{
public:
  // retransmit timer
  uint64_t retransmit_timer_ = 0;
  bool timer_running_ = false;
  // consecutive retransmissions
  uint64_t consecutive_retransmissions_ = 0;
  // next seqno to send
  uint64_t next_seqno_ = 0;
  // RTO
  uint64_t RTO_ = 0;
  // number of segment(s) to be sent
  // if 0, pretend to be 1 and send a testing segment
  uint64_t sending_window_ = 0;
  // biggest ackno received
  uint64_t max_ackno_ = 0;
  // segments-in-flight(sent but not acked yet)
  // maybe std::queue is enough?
  std::deque<TCPSenderMessage> segments_in_flight_;
  // judge if SYN and FIN have benn sent (for zero-window test segment)
  bool syn_sent_ = false;
  bool fin_sent_ = false;
  bool zero_win_advertise_ = false;
  /* Construct TCP sender with given default Retransmission Timeout and possible ISN */
  TCPSender( ByteStream&& input, Wrap32 isn, uint64_t initial_RTO_ms )
    : retransmit_timer_( 0 )
    , timer_running_( false )
    , consecutive_retransmissions_( 0 )
    , next_seqno_( 0 )
    , RTO_( initial_RTO_ms )
    , sending_window_( 0 )
    , max_ackno_( 0 )
    , segments_in_flight_()
    , syn_sent_( false )
    , fin_sent_( false )
    , zero_win_advertise_( false )
    , input_( std::move( input ) )
    , isn_( isn )
    , initial_RTO_ms_( initial_RTO_ms )
  {}

  /* Generate an empty TCPSenderMessage */
  TCPSenderMessage make_empty_message() const;

  /* Receive and process a TCPReceiverMessage from the peer's receiver */
  void receive( const TCPReceiverMessage& msg );

  /* Type of the `transmit` function that the push and tick methods can use to send messages */
  using TransmitFunction = std::function<void( const TCPSenderMessage& )>;

  /* Push bytes from the outbound stream */
  void push( const TransmitFunction& transmit );

  /* Time has passed by the given # of milliseconds since the last time the tick() method was called */
  void tick( uint64_t ms_since_last_tick, const TransmitFunction& transmit );

  // Accessors
  uint64_t sequence_numbers_in_flight() const;  // For testing: how many sequence numbers are outstanding?
  uint64_t consecutive_retransmissions() const; // For testing: how many consecutive retransmissions have happened?
  const Writer& writer() const { return input_.writer(); }
  const Reader& reader() const { return input_.reader(); }
  Writer& writer() { return input_.writer(); }

private:
  Reader& reader() { return input_.reader(); }

  ByteStream input_;
  Wrap32 isn_;
  uint64_t initial_RTO_ms_;
};
