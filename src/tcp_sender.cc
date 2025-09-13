#include "tcp_sender.hh"
#include "debug.hh"
#include "tcp_config.hh"

using namespace std;

// This function is for testing only; don't add extra state to support it.
uint64_t TCPSender::sequence_numbers_in_flight() const
{
  size_t total = 0;
  for ( const auto& seg : segments_in_flight_ ) {
    total += seg.sequence_length();
  }
  return total;
}

// This function is for testing only; don't add extra state to support it.
uint64_t TCPSender::consecutive_retransmissions() const
{
  return consecutive_retransmissions_;
}

void TCPSender::push( const TransmitFunction& transmit )
{
  uint64_t sending_window_border = max_ackno_ + sending_window_;
  uint64_t left_space = sending_window_border >= next_seqno_ ? sending_window_border - next_seqno_ : 0;

  if ( sending_window_ == 0 ) {
    // zero-window test msg
    // pretend to be 1
    TCPSenderMessage msg;
    msg.seqno = Wrap32::wrap( next_seqno_, isn_ );

    if ( !syn_sent_ ) {
      msg.SYN = true;
      syn_sent_ = true;
    } else if ( !fin_sent_ && input_.reader().is_finished() ) {
      msg.FIN = true;
      fin_sent_ = true;
    } else if ( !input_.reader().is_finished() && max_ackno_ > 0 && segments_in_flight_.empty() ) {
      // send an 1-byte msg only when no seg is in flight and max_ackno_ isn't 0 (updated at least once)
      read( input_.reader(), 1, msg.payload );
    }
    // avoid sending empty msg
    if ( !msg.SYN && !msg.FIN && msg.payload.empty() ) {
      return;
    }
    // update next_seqno_ (seqno of the next expected seg)
    next_seqno_ += msg.sequence_length();
    // once sent, add it into the in-flight queue
    segments_in_flight_.emplace_back( msg );
    // start timing
    if ( !timer_running_ ) {
      timer_running_ = true;
      retransmit_timer_ = 0;
      consecutive_retransmissions_ = 0;
    }

    transmit( msg );
    return;
  }
  // try emptying the sending window
  while ( left_space > 0 ) {
    TCPSenderMessage msg;
    msg.seqno = Wrap32::wrap( next_seqno_, isn_ );
    // 1) segment with SYN=true
    if ( !syn_sent_ ) {
      msg.SYN = true;
      // carrys as much payload as it can
      uint64_t max_payload = min( TCPConfig::MAX_PAYLOAD_SIZE, left_space > 0 ? left_space - 1 : 0 );
      if ( max_payload > 0 && !input_.reader().is_finished() ) {
        read( input_.reader(), max_payload, msg.payload );
      }
      // if no FIN sent, while writer closed and buffer cleared (and left space allows), try carry FIN=true as well
      if ( !fin_sent_ && input_.reader().is_finished() && left_space >= msg.payload.size() + 2 ) {
        msg.FIN = true;
        fin_sent_ = true;
      }
      // avoid sending empty seg
      if ( msg.sequence_length() == 0 ) {
        break;
      }
      // NOTICE THE RST TESTCASE
      if ( input_.reader().has_error() ) {
        msg.RST = true;
      }

      transmit( msg );
      // process seqno and timer
      next_seqno_ += msg.sequence_length();
      segments_in_flight_.emplace_back( msg );
      if ( !timer_running_ ) {
        timer_running_ = true;
        retransmit_timer_ = 0;
        consecutive_retransmissions_ = 0;
      }
      // process left_space
      if ( left_space >= msg.sequence_length() ) {
        left_space -= msg.sequence_length();
      } else {
        left_space = 0;
      }
      syn_sent_ = true;
    } else if ( input_.reader().is_finished() && left_space >= 1 && !fin_sent_ ) {
      // 2) segment with FIN=true (for corner case)
      msg.FIN = true;
      // process left_space
      if ( left_space >= msg.sequence_length() ) {
        left_space -= msg.sequence_length();
      } else {
        left_space = 0;
      }
      // avoid sending empty seg
      if ( msg.sequence_length() == 0 ) {
        break;
      }
      // NOTICE THE RST TESTCASE
      if ( input_.reader().has_error() ) {
        msg.RST = true;
      }

      transmit( msg );
      // process seqno and timer
      next_seqno_ += msg.sequence_length();
      segments_in_flight_.emplace_back( msg );
      if ( !timer_running_ ) {
        timer_running_ = true;
        retransmit_timer_ = 0;
        consecutive_retransmissions_ = 0;
      }
      // as we've sent FIN, set the fin_sent_ sign
      fin_sent_ = true;
    } else if ( !input_.reader().is_finished() ) {
      // 3) average case
      // length we read from ByteStream
      uint64_t read_len = min( left_space, TCPConfig::MAX_PAYLOAD_SIZE );
      read( input_.reader(), read_len, msg.payload );
      // try carry FIN with data for efficiency (while testcases actually demanded)
      if ( !fin_sent_ && left_space >= msg.payload.size() + 1 && input_.reader().is_finished() ) {
        msg.FIN = true;
        fin_sent_ = true;
      }
      // avoid sending empty seg
      if ( !msg.payload.size() && !msg.SYN && !msg.FIN ) {
        break;
      }
      // NOTICE THE RST TESTCASE
      if ( input_.reader().has_error() ) {
        msg.RST = true;
      }

      transmit( msg );
      segments_in_flight_.emplace_back( msg );
      // process timer
      if ( !timer_running_ ) {
        timer_running_ = true;
        retransmit_timer_ = 0;
        consecutive_retransmissions_ = 0;
      }
      // process seqno and left space
      next_seqno_ += msg.sequence_length();
      left_space = ( left_space >= msg.sequence_length() ) ? left_space - msg.sequence_length() : 0;
    } else {
      // ignore remaining cases for they shouldn't happen
      break;
    }
  }
}

TCPSenderMessage TCPSender::make_empty_message() const
{
  TCPSenderMessage msg;
  msg.SYN = msg.FIN = false;
  msg.seqno = Wrap32::wrap( next_seqno_, isn_ );
  msg.payload = "";
  // NOTICE THE RST TESTCASE
  if ( input_.reader().has_error() ) {
    msg.RST = true;
  }
  return msg;
}

void TCPSender::receive( const TCPReceiverMessage& msg )
{
  // NOTICE THE RST TESTCASE
  if ( msg.RST ) {
    input_.set_error();
    return;
  }
  // if the msg has no valid ackno, update the sending window and drop this msg
  if ( !msg.ackno ) {
    sending_window_ = msg.window_size;
    return;
  }
  // NOTE THAT ACKNO IS OPTIONAL, SO USE `->` INSTEAD
  uint64_t ack_abs = msg.ackno->unwrap( isn_, max_ackno_ );
  // if the absolute ackno is invalid (ahead of all recorded seqnos), drop it
  if ( ack_abs > next_seqno_ ) {
    return;
  }
  sending_window_ = msg.window_size;
  // use advertise to manage exponential backoff (if zero-window, skip the backoff)
  zero_win_advertise_ = sending_window_ == 0;
  // new incoming ack
  if ( ack_abs > max_ackno_ ) {
    max_ackno_ = ack_abs;
    uint64_t start_abs = 0;

    if ( segments_in_flight_.size() ) {
      // seqno of the front (use this (plus sequence_length()) to describe the seqno of the next acked seg)
      start_abs = segments_in_flight_.front().seqno.unwrap( isn_, max_ackno_ );
    }
    // clear acked segs
    while ( segments_in_flight_.size()
            && start_abs + segments_in_flight_.front().sequence_length() <= max_ackno_ ) {
      segments_in_flight_.pop_front();
      if ( segments_in_flight_.size() ) {
        start_abs = segments_in_flight_.front().seqno.unwrap( isn_, max_ackno_ );
      }
    }
    // when encountering acks, reset RTO_ and timer
    RTO_ = initial_RTO_ms_;
    consecutive_retransmissions_ = 0;
    retransmit_timer_ = 0;
    timer_running_ = segments_in_flight_.size() ? true : false;
  }
}

void TCPSender::tick( uint64_t ms_since_last_tick, const TransmitFunction& transmit )
{
  // works only when timer is running and segments_in_flight_ is not empty
  if ( timer_running_ && segments_in_flight_.size() ) {
    retransmit_timer_ += ms_since_last_tick;

    if ( retransmit_timer_ >= RTO_ ) {
      // retransmit
      const auto& segment_to_retransmit = segments_in_flight_.front();
      transmit( segment_to_retransmit );
      retransmit_timer_ = 0;
      timer_running_ = true;
      // exponential backoff (only when the window isn't zero-length)
      if ( zero_win_advertise_ )
        return;
      // double the RTO
      RTO_ *= 2;
      // increse the consecutive retransmission counter (here only for test)
      consecutive_retransmissions_++;
    }
  }
}
