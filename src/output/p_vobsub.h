/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL v2
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   class definition for the VobSub output module

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef MTX_P_VOBSUB_H
#define MTX_P_VOBSUB_H

#include "common/common_pch.h"

#include "common/compression.h"
#include "merge/generic_packetizer.h"

class vobsub_packetizer_c: public generic_packetizer_c {
public:
  vobsub_packetizer_c(generic_reader_c *reader, track_info_c &ti);
  virtual ~vobsub_packetizer_c();

  virtual int process(packet_cptr packet) override;
  virtual void set_headers() override;

  virtual translatable_string_c get_format_name() const override {
    return YT("VobSub");
  }

  virtual connection_result_e can_connect_to(generic_packetizer_c *src, std::string &error_message) override;

protected:
  virtual void after_packet_timestamped(packet_t &packet) override;
};

#endif // MTX_P_VOBSUB_H
