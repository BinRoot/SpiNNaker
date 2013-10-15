#include <sark.h>

void c_main (void) {
  uint id = sark_chip_id();
  uint x = id >> 8;
  uint y = id & 255;
  uint c = sark_core_id();
  io_printf (IO_STD, "(%u, %u, %u)\n", x, y, c);

  sdp_msg_t msg;
  
  io_printf (IO_STD, "p2p_addr: %u, %u\n", 
	     sv->p2p_addr,
	     sv->p2p_dims);

  //  uint success = sark_msg_send(msg, 1000);
}
