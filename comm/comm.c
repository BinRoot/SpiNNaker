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

#define NORTH_ARRIVED      (1 << NORTH)
#define SOUTH_ARRIVED      (1 << SOUTH)
#define NONE_ARRIVED       0

#define CORE_TO_NORTH(core)        (core + 1)
#define CORE_TO_SOUTH(core)        (core - 1)

#define CHIP_TO_NORTH(chip)     (chip + 1)
#define CHIP_TO_SOUTH(chip)     (chip - 1)
#define IS_NORTHERNMOST_CHIP(x, y) ((chip_map[x][y] & (1 << NORTH)) != 0)
#define IS_SOUTHERNMOST_CHIP(x, y) ((chip_map[x][y] & (1 << SOUTH)) != 0)
#define NORTHERNMOST_CORE(core)    (((core - 1) | 0x3) + 1)
#define SOUTHERNMOST_CORE(core)    (((core - 1) & 0xc) + 1)



#define DONT_ROUTE_KEY     0xffff
#define ROUTING_KEY(chip, core)    ((chip << 5) | core)
#define ROUTE_TO_CORE(core)        (1 << (core + 6))
#define ROUTE_TO_LINK(link)        (1 << link)
#define NORTH_LINK                 2
#define SOUTH_LINK                 5

// ---
// variables
// ---
uint coreID;
uint chipID;
uint my_chip, my_x, my_y;
uint init_arrived;

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

#define IS_NORTHERNMOST_CORE(core) (((core - 1) & 0x3) == 0x3)
#define IS_SOUTHERNMOST_CORE(core) (((core - 1) & 0x3) == 0x0)
#define IS_NORTHERNMOST_CHIP(x, y) ((chip_map[x][y] & (1 << NORTH)) != 0)
#define IS_SOUTHERNMOST_CHIP(x, y) ((chip_map[x][y] & (1 << SOUTH)) != 0)


uint my_key;
uint north_key;
uint south_key;
sdp_msg_t my_msg;
uint my_route = 0;  // where I send my data

uint route_from_north = FALSE;
uint route_from_south = FALSE;


void routing_table_init () {
  my_key = ROUTING_KEY(chipID, coreID);
  init_arrived = NONE_ARRIVED;

  if (IS_NORTHERNMOST_CORE(coreID)) {
    if (IS_NORTHERNMOST_CHIP(my_x, my_y)) {
      // don't send packets north
      // don't expect packets from north
      north_key = DONT_ROUTE_KEY;
      init_arrived |= NORTH_ARRIVED;
    } else {
      // send packets to chip to the north
      my_route |= ROUTE_TO_LINK(NORTH_LINK);
      // expect packets from chip to the north (southernmost core)
      route_from_north = TRUE;
      north_key = ROUTING_KEY(CHIP_TO_NORTH(chipID), SOUTHERNMOST_CORE(coreID));
    }
  } else {
    // expect packets from north
    north_key = ROUTING_KEY(chipID, CORE_TO_NORTH(coreID));
    // send to north core
    my_route |= ROUTE_TO_CORE(CORE_TO_NORTH(coreID));
  }

  if (IS_SOUTHERNMOST_CORE(coreID)) {
    if (IS_SOUTHERNMOST_CHIP(my_x, my_y)) {
      // don't send packets south
      // don't expect packets from south
      south_key = DONT_ROUTE_KEY;
      init_arrived |= SOUTH_ARRIVED;
    } else {
      // send packets to chip to the south
      my_route |= ROUTE_TO_LINK(SOUTH_LINK);
      // expect packets from chip to the south (northernmost core)
      route_from_south = TRUE;
      south_key = ROUTING_KEY(CHIP_TO_SOUTH(chipID), NORTHERNMOST_CORE(coreID));
    }
  } else {
    // expect packets from south
    south_key = ROUTING_KEY(chipID, CORE_TO_SOUTH(coreID));
    // send to south core
    my_route |= ROUTE_TO_CORE(CORE_TO_SOUTH(coreID));
  }


  spin1_set_mc_table_entry((6 * coreID), // entry
                     my_key,             // key
                     0xffffffff,         // mask
                     my_route            // route
                    );

  /* set MC routing table entries to get packets from neighbour chips */
  /* north */
  if (route_from_north)
  {
    spin1_set_mc_table_entry((6 * coreID) + 1,     // entry
                             north_key,            // key
                             0xffffffff,           // mask
                             ROUTE_TO_CORE(coreID) // route
                            );
  }

  /* south */
  if (route_from_south)
  {
    spin1_set_mc_table_entry((6 * coreID) + 2,     // entry
                             south_key,            // key
                             0xffffffff,           // mask
                             ROUTE_TO_CORE(coreID) // route
                            );
  }


}

uint data_to_send = 1;

// Callback MC_PACKET_RECEIVED
void receive_data (uint key, uint payload) {
  io_printf (IO_STD, "Received data (%u, %u)\n", key, payload);

  data_to_send = payload + 1;
}

// Callback TIMER_TICK
void update (uint a, uint b) {
  io_printf (IO_STD, "tick %d\n", data_to_send);

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
  routing_table_init ();

  // Initialize SDP message buffer
  sdp_init ();

  // go
  spin1_start();
}
