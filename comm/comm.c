#include <sark.h>
#include <spin1_api.h>

// ---
// Routing table constants and macro definitions
// ---
#define NORTH              3
#define SOUTH              2
#define EAST               1
#define WEST               0

#define NECh ((1 << NORTH) | (1 << EAST))
#define NWCh ((1 << NORTH) | (1 << WEST))
#define SECh ((1 << SOUTH) | (1 << EAST))
#define SWCh ((1 << SOUTH) | (1 << WEST))

#define ROUTING_KEY(chip, core)    ((chip << 5) | core)

// ---
// variables
// ---
uint coreID;
uint chipID;
uint my_chip, my_x, my_y;

#define MAP_2x2_on_4       TRUE

#define TIMER_TICK_PERIOD  25000

#ifdef MAP_2x2_on_4
  #define NUMBER_OF_XCHIPS 2
  #define NUMBER_OF_YCHIPS 2
  uint core_map[NUMBER_OF_XCHIPS][NUMBER_OF_YCHIPS] =
  {
    {0x1fffe, 0x1fffe},
    {0x1fffe, 0x1fffe}
  };
  uint chip_map[NUMBER_OF_XCHIPS][NUMBER_OF_YCHIPS] =
  {
    {SWCh, NWCh},
    {SECh, NECh}
  };
#endif

uint my_key;
sdp_msg_t my_msg;

void routing_table_init () {
  my_key = ROUTING_KEY(chipID, coreID);
}

// Callback MC_PACKET_RECEIVED
void receive_data (uint key, uint payload) {
  io_printf (IO_STD, "Received data (%u, %u)\n", key, payload);
}

// Callback TIMER_TICK
void update (uint a, uint b) {
  int data_to_send = 7;

  // send data to neighbor(s)
  spin1_send_mc_packet(my_key, data_to_send, WITH_PAYLOAD);

  // report data to host
  if (leadAp) {
    // copy temperatures into msg buffer and set message length
    uint len = 16 * sizeof(uint);
    spin1_memcpy (my_msg.data, (void *) data_to_send, len);
    my_msg.length = sizeof (sdp_hdr_t) + sizeof (cmd_hdr_t) + len;

    // and send SDP message!
    (void) spin1_send_sdp_msg (&my_msg, 100); // 100ms timeout
  }
}

// Callback SDP_PACKET_RX
void host_data (uint mailbox, uint port) {
  io_printf (IO_STD, "Received packet from host");
}


void sdp_init () {
  // fill in SDP destination fields
  my_msg.tag = 1;                    // IPTag 1
  my_msg.dest_port = PORT_ETH;       // Ethernet
  my_msg.dest_addr = sv->dbg_addr;   // Root chip

  // fill in SDP source & flag fields
  my_msg.flags = 0x07;
  my_msg.srce_port = coreID;
  my_msg.srce_addr = sv->p2p_addr;
}


void c_main (void) {
  io_printf (IO_STD, "Starting communication test.\n");
  
  // get this core's ID
  coreID = spin1_get_core_id();
  chipID = spin1_get_chip_id();
  
  // get this chip's coordinates for core map
  my_x = chipID >> 8;
  my_y = chipID & 0xff;
  my_chip = (my_x * NUMBER_OF_YCHIPS) + my_y;

  // operate only if in core map!
  if ((core_map[my_x][my_y] & (1 << coreID)) == 0) {
    io_printf (IO_STD, "Stopping comm. (Not in core map)\n");
    return;
  }

      // set the core map for the simulation
  spin1_application_core_map(NUMBER_OF_XCHIPS, NUMBER_OF_YCHIPS, core_map);

  spin1_set_timer_tick (TIMER_TICK_PERIOD);
  
  // register callbacks
  spin1_callback_on (MC_PACKET_RECEIVED, receive_data, 0);
  spin1_callback_on (TIMER_TICK, update, 0);
  spin1_callback_on (SDP_PACKET_RX, host_data, 0);

  // Initialize routing tables
  // routing_table_init ();

  // Initialize SDP message buffer
  sdp_init ();

}
