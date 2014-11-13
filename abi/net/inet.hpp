/** Common utilities for internetworking */
#ifndef INET_HPP
#define INET_HPP

#include <delegate>
#include <net/class_packet.hpp>

namespace net {

  /** Upstream delegate */
  typedef delegate<int(std::shared_ptr<Packet>)> upstream;

  /** Downstream delegate 

      @note We used to have meta-info like destination mac, dest.IP etc. as 
      parameters. When we remove this we get a weakness; it's possible to 
      pass packets without the proper parameters. On the upside; much cleaner
      interfaces.      
      
   */
  typedef upstream downstream;
  

  /** Compute the internet checksum for the buffer / buffer part provided */
  uint16_t checksum(uint16_t* buf, uint32_t len); 
  
}

#endif
