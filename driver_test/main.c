#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include <net/if.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>

#include <linux/can.h>
#include <linux/can/raw.h>

int main(void)
{
  int s;
  int nbytes;
  struct sockaddr_can addr;
  struct can_frame frame;
  struct ifreq ifr;

  const char *ifname = "can0";

  if((s = socket(PF_CAN, SOCK_RAW, CAN_RAW)) < 0) {
    perror("Error while opening socket");
    return -1;
  }

  strcpy(ifr.ifr_name, ifname);
  ioctl(s, SIOCGIFINDEX, &ifr);

  addr.can_family  = AF_CAN;
  addr.can_ifindex = ifr.ifr_ifindex;

  printf("%s at index %d\n", ifname, ifr.ifr_ifindex);

  if(bind(s, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
    perror("Error in socket bind");
    return -2;
  }

  frame.can_id  = 0x123;
  frame.can_dlc = 2;
  frame.data[0] = 0x11;
  frame.data[1] = 0x22;

  nbytes = write(s, &frame, sizeof(struct can_frame));

  printf("Wrote %d bytes\n", nbytes);

  while (1) {
    struct can_frame framein;

    // Read in a CAN frame
    int numBytes = read(s, &framein, CANFD_MTU);
    switch (numBytes) {
    case CAN_MTU:
      if(framein.can_id & 0x80000000)
	printf("Received %u byte payload; canid 0x%lx (EXT)\n",
	       framein.can_dlc, framein.can_id & 0x7FFFFFFF);
      else
	printf("Received %u byte payload; canid 0x%lx\n", framein.can_dlc, framein.can_id);
      break;
    case CANFD_MTU:
      // TODO: Should make an example for CAN FD
      break;
    case -1:
      // Check the signal value on interrupt
      //if (EINTR == errno)
      //  continue;

      // Delay before continuing
      sleep(1);
    default:
      continue;
    }
  }

  return 0;
}
