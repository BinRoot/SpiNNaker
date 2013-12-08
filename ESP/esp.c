/*
 * This simulation demos using the routing table to send packets between chips.
 * The NW, SE, and SW chips are all sending a packet to the corresponding core on the NE chip.
 * On receiving a packet, the core simply prints an acknowledgement.
 */

#include "spin1_api.h"

#define NORTH              3
#define SOUTH              2
#define EAST               1
#define WEST               0

#define NECh ((1 << NORTH) | (1 << EAST))
#define NWCh ((1 << NORTH) | (1 << WEST))
#define SECh ((1 << SOUTH) | (1 << EAST))
#define SWCh ((1 << SOUTH) | (1 << WEST))

#define InCh ((NWCh) | (SECh) | (SWCh))
#define OutCh (NECh)

#define NUMBER_OF_XCHIPS 2
#define NUMBER_OF_YCHIPS 2
#define STOP_KEY       ROUTING_KEY(0, 17)
#define CMD_MASK       0xfffffe1f
#define NONE_ARRIVED       0
#define ROUTING_KEY(chip, core)    ((chip << 5) | core)
#define ROUTE_TO_CORE(core)        (1 << (core + 6))

#define ROUTE_TO_LINK(link)        (1 << link)
#define EAST_LINK                  0
#define NORTH_EAST_LINK            1
#define NORTH_LINK                 2
#define WEST_LINK                  3
#define SOUTH_WEST_LINK            4
#define SOUTH_LINK                 5

#define CHIP_ADDR(x, y)      ((x << 8) | y)

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

// routing table variables
uint my_key;
uint my_route = 0; 

uint coreID;
uint chipID;
uint my_chip, my_x, my_y;

void routing_table_init () {
  my_key = ROUTING_KEY(chipID, coreID);

  /*
   * The NE Chip will receive input from all the other chips.
   * We want the routing table to get messages from cores on input chips to their corresponding core on the output chip.
   * In other words, messages from cores 4 in chips (0,0) (0,1) (1,0) should be sent to core 4 in (1,1).
   */

  if (my_x == 1 && my_y == 1) { // Output (NECh)
    spin1_set_mc_table_entry((6*coreID) + 1, // entry
			     ROUTING_KEY(CHIP_ADDR(0, 0), coreID),
			     0xffffffff, // mask
			     ROUTE_TO_CORE(coreID) // route
			     );
    spin1_set_mc_table_entry((6*coreID) + 2, // entry
			     ROUTING_KEY(CHIP_ADDR(0, 1), coreID),
			     0xffffffff, // mask
			     ROUTE_TO_CORE(coreID) // route
			     );
    spin1_set_mc_table_entry((6*coreID) + 3, // entry
			     ROUTING_KEY(CHIP_ADDR(1, 0), coreID),
			     0xffffffff, // mask
			     ROUTE_TO_CORE(coreID) // route
			     );
  }
  else { // Input
    if ((my_x == 0) && (my_y == 0)) { // if SWCh
      my_route |= ROUTE_TO_LINK(NORTH_EAST_LINK);
    }
    else if ((my_x == 0) && (my_y == 1)) { // if NWCh
      my_route |= ROUTE_TO_LINK(EAST_LINK);
    }
    else if ((my_x == 1) && (my_y == 0)) { // if SECh
      my_route |= ROUTE_TO_LINK(NORTH_LINK);
    }

    spin1_set_mc_table_entry((6 * coreID), // entry
			     my_key, // key
			     0xffffffff, // mask
			     my_route // route
			     );
  } 
}

void receive_data (uint key, uint payload) {
  io_printf (IO_STD, "got response! (%u, %u)\n", key, payload);
}

void send_first_value (uint a, uint b) {
  spin1_send_mc_packet(my_key, 0, WITH_PAYLOAD);
}

void c_main (void) {
  // get this core's ID
  coreID = spin1_get_core_id();
  chipID = spin1_get_chip_id();

  // get this chip's coordinates for core map
  my_x = chipID >> 8;
  my_y = chipID & 0xff;
  my_chip = (my_x * NUMBER_OF_YCHIPS) + my_y;

  // set the core map for the simulation
  spin1_application_core_map(NUMBER_OF_XCHIPS, 
			     NUMBER_OF_YCHIPS, 
			     core_map);

  io_printf (IO_STD, "%u\n", coreID, my_x, my_y);

  // register callbacks
  spin1_callback_on (MC_PACKET_RECEIVED, receive_data, 0);
  
  // initialize routing tables
  routing_table_init();

  // kick-start the update process
  spin1_schedule_callback(send_first_value, 0, 0, 3);

  // go
  spin1_start();

  io_printf (IO_STD, "done\n");
}
