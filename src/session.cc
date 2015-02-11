/**
 * Multiparty Off-the-Record Messaging library
 * Copyright (C) 2014, eQualit.ie
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of version 3 of the GNU Lesser General
 * Public License as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */


#include <assert.h>

#include "src/session.h"
#include "src/exceptions.h"
#include <stdlib.h>

void MessageDigest::update(std::string new_message) {
  UNUSED(new_message);
  return;
}

static void cb_send_heartbeat(evutil_socket_t fd, short what, void *arg) {
  np1secSession* session = (static_cast<np1secSession*>(arg));
  session->send("Heartbeat", PURE_META_MESSAGE);
  session->start_heartbeat_timer();
}

static void cb_ack_not_received(evutil_socket_t fd, short what, void *arg) {
  // Construct message for ack
  np1secSession* session = (static_cast<np1secSession*>(arg));
  session->send("Where is my ack?", PURE_META_MESSAGE);
}

static void cb_send_ack(evutil_socket_t fd, short what, void *arg) {
  // Construct message with p.id
  np1secSession* session = (static_cast<np1secSession*>(arg));
  session->send("ACK", PURE_META_MESSAGE);
}

np1secSession::np1secSession(np1secUserState *us)
  :myself(us->username())
{
  throw std::invalid_argument("Default constructor should not be used.");
}

/**
 * This constructor should be only called when the session is generated
 * to join. That's why all participant are not authenticated.
 */
np1secSession::np1secSession(np1secUserState *us, std::string room_name,
                             std::vector<UnauthenticatedParticipant>participants_in_the_room) : us(us), room_name(room_name), participants_in_the_room(participants_in_the_room),   myself(us->username())
{
}

/**
 * it should be invoked only once to compute the session id
 * if one need session id then they need a new session
 *
 * @return return true upon successful computation
 */
bool np1secSession::compute_session_id()
{
  //sanity check: You can only compute session id once
  assert(not session_id.size());

  if (peers.size() == 0) //nothing to compute
    return false;

  /**
   * Generate Session ID
   */

  //TODO:: Bill
  //session_id = Hash of (U1,ehpmeral1, U2);
  return true;

}

/**
 * Received the pre-processed message and based on the state
 * of the session decides what is the appropriate action
 *
 * @param receive_message pre-processed received message handed in by receive function
 *
 * @return true if state has been change 
 */
bool np1secSession::state_handler(np1secMessage receivd_message)
{
  switch(my_state) {
    case np1secSession::NONE:
      //This probably shouldn't happen, if a session has
      //no state state_handler shouldn't be called.
      //The receive_handler of the user_state should call
      //approperiate inition of a session of session less
      //message
      throw  np1secSessionStateException();
      break;
        
    case np1secSession::JOIN_REQUESTED: //The thread has requested to join by sending ephemeral key
      //Excepting to receive list of current participant
      break;
    case np1secSession::REPLIED_TO_NEW_JOIN: //The thread has received a join from a participant replied by participant list
      break;
    case np1secSession::GROUP_KEY_GENERATED: //The thread has computed the session key and has sent the conformation
      break;
    case np1secSession::IN_SESSION: //Key has been confirmed
      break;
    case np1secSession::UPDATED_KEY: //all new shares has been received and new key has been generated: no more send possible
      break;
    case np1secSession::LEAVE_REQUESTED: //Leave requested by the thread: waiting for final transcirpt consitancy check
      break;
    case np1secSession::FAREWELLED: //LEAVE is received from another participant and a meta message for transcript consistancy and new shares has been sent
      break;
    case np1secSession::DEAD: //Won't accept receive or sent messages, possibly throw up
      break;
    default:
      return false;
  };
  
}

bool np1secSession::join(LongTermIDKey long_term_id_key) {

  //We need to generate our ephemerals anyways
  if (!cryptic.init()) {
    return false;
  }
  myself.ephemeral_key = cryptic.get_ephemeral_pub_key();

  //we add ourselves to the (authenticated) participant list
  peers.push_back(myself);

  //if nobody else is in the room have nothing to do more than
  //just computing the session_id
  if (participants_in_the_room.size()== 1) {
    assert(this->compute_session_id());
         
  }
  else {
    
  }
  
  us->ops->send_bare(room_name, us->username(), "testing 123", NULL);
  return true;
}

bool np1secSession::accept(std::string new_participant_id) {
  UNUSED(new_participant_id);
  return true;
}

bool np1secSession::farewell(std::string leaver_id) {
  UNUSED(leaver_id);
  return true;
}

void np1secSession::start_heartbeat_timer() {
  struct event *timer_event;
  struct timeval ten_seconds = {10, 0};
  struct event_base *base = event_base_new();

  timer_event = event_new(base, -1, EV_TIMEOUT, &cb_send_heartbeat, this);
  event_add(timer_event, &ten_seconds);

  event_base_dispatch(base);
}

void np1secSession::start_ack_timers() {
  struct event *timer_event;
  struct timeval ten_seconds = {10, 0};
  struct event_base *base = event_base_new();

  for (std::vector<std::string>::iterator it = peers.begin();
       it != peers.end();
       ++it) {
    timer_event = event_new(base, -1, EV_TIMEOUT, &cb_ack_not_received, this);
    awaiting_ack[*it] = timer_event;
    event_add(awaiting_ack[*it], &ten_seconds);
  }

  event_base_dispatch(base);
}

void np1secSession::start_receive_ack_timer(std::string sender_id) {
  struct event *timer_event;
  struct timeval ten_seconds = {10, 0};
  struct event_base *base = event_base_new();

  //msg = otrl_base64_otr_encode((unsigned char*)combined_content.c_str(),
  //                             combined_content.size());
  //(us->ops->send_bare)(room_name, myself.id, msg, static_cast<void*>(us));

  timer_event = event_new(base, -1, EV_TIMEOUT, &cb_send_ack, this);
  acks_to_send[sender_id] = timer_event;
  event_add(awaiting_ack[sender_id], &ten_seconds);
  event_base_dispatch(base);
}

void np1secSession::stop_timer_send() {
  for (std::map<std::string, struct event*>::iterator
       it = acks_to_send.begin();
       it != acks_to_send.end();
       ++it) {
    event_free(it->second);
    acks_to_send.erase(it);
  }
}

void np1secSession::stop_timer_receive(std::string acknowledger_id) {
  event_free(awaiting_ack[acknowledger_id]);
  awaiting_ack.erase(acknowledger_id);
}

void np1secSession::add_message_to_transcript(std::string message,
                                        uint32_t message_id) {
  HashBlock* hb;
  std::stringstream ss;
  std::string pointlessconversion;

  ss << transcript_chain.rbegin()->second;
  ss >> pointlessconversion;
  pointlessconversion += ":O3" + message;

  compute_message_hash(*hb, pointlessconversion);

  transcript_chain[message_id] = hb;
}

bool np1secSession::send(std::string message, np1secMessageType message_type) {
  HashBlock* transcript_chain_hash = transcript_chain.rbegin()->second;
  // TODO(bill)
  // Add code to check message type and get
  // meta load if needed
  np1secLoadFlag meta_load_flag = NO_LOAD;
  std::string meta_load = NULL;
  np1secMessage outbound(session_id, us->username(),
                         message, message_type,
                         transcript_chain_hash,
                         meta_load_flag, meta_load,
                         peers, cryptic);

  // As we're sending a new message we are no longer required to ack
  // any received messages
  stop_timer_send();

  if (message_type == USER_MESSAGE) {
    // We create a set of times for all other peers for acks we expect for
    // our sent message
    start_ack_timers();
  }

  // us->ops->send_bare(room_name, outbound);
  return true;
}

np1secMessage np1secSession::receive(std::string raw_message) {
  HashBlock* transcript_chain_hash = transcript_chain.rbegin()->second;
  np1secMessage received_message(raw_message, cryptic);

  if (*transcript_chain_hash == received_message.transcript_chain_hash) {
    add_message_to_transcript(received_message.user_message,
                        received_message.message_id);
    // Stop awaiting ack timer for the sender
    stop_timer_receive(received_message.sender_id);

    // Start an ack timer for us so we remember to say thank you
    // for the message
    start_receive_ack_timer(received_message.sender_id);

  } else {
    // The hash is a lie!
  }
  received_message.message_type = np1secMessage::USER_MESSAGE;
  received_message.user_message = decrypted_message;

  return received_message;
}

np1secSession::~np1secSession() {
  return;
}
