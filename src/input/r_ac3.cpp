/*
 * mkvmerge -- utility for splicing together matroska files
 * from component media subtypes
 *
 * Distributed under the GPL
 * see the file COPYING for details
 * or visit http://www.gnu.org/copyleft/gpl.html
 *
 * $Id$
 *
 * AC3 demultiplexer module
 *
 * Written by Moritz Bunkus <moritz@bunkus.org>.
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

extern "C" {
#include <avilib.h>
}

#include "common.h"
#include "error.h"
#include "r_ac3.h"
#include "p_ac3.h"

int
ac3_reader_c::probe_file(mm_io_c *mm_io,
                         int64_t size,
                         int64_t probe_size,
                         int num_headers) {
  return (find_valid_headers(mm_io, probe_size, num_headers) != -1) ? 1 : 0;
}

ac3_reader_c::ac3_reader_c(track_info_c *nti)
  throw (error_c):
  generic_reader_c(nti) {
  int pos;

  try {
    mm_io = new mm_file_io_c(ti->fname);
    size = mm_io->get_size();
    chunk = (unsigned char *)safemalloc(4096);
    if (mm_io->read(chunk, 4096) != 4096)
      throw error_c("ac3_reader: Could not read 4096 bytes.");
    mm_io->setFilePointer(0, seek_beginning);
  } catch (exception &ex) {
    throw error_c("ac3_reader: Could not open the source file.");
  }
  pos = find_ac3_header(chunk, 4096, &ac3header);
  if (pos < 0)
    throw error_c("ac3_reader: No valid AC3 packet found in the first "
                  "4096 bytes.\n");
  bytes_processed = 0;
  ti->id = 0;                   // ID for this track.
  if (verbose)
    mxinfo(FMT_FN "Using the AC3 demultiplexer.\n", ti->fname.c_str());
}

ac3_reader_c::~ac3_reader_c() {
  delete mm_io;
  safefree(chunk);
}

void
ac3_reader_c::create_packetizer(int64_t) {
  if (NPTZR() != 0)
    return;
  add_packetizer(new ac3_packetizer_c(this, ac3header.sample_rate,
                                      ac3header.channels, ac3header.bsid, ti));
  mxinfo(FMT_TID "Using the AC3 output module.\n", ti->fname.c_str(),
         (int64_t)0);
}

file_status_t
ac3_reader_c::read(generic_packetizer_c *,
                   bool) {
  int nread;

  nread = mm_io->read(chunk, 4096);
  if (nread <= 0) {
    PTZR0->flush();
    return file_status_done;
  }

  memory_c mem(chunk, nread, false);
  PTZR0->process(mem);
  bytes_processed += nread;

  return file_status_moredata;
}

int
ac3_reader_c::get_progress() {
  return 100 * bytes_processed / size;
}

void
ac3_reader_c::identify() {
  mxinfo("File '%s': container: AC3\nTrack ID 0: audio (AC3)\n",
         ti->fname.c_str());
}

int
ac3_reader_c::find_valid_headers(mm_io_c *mm_io,
                                 int64_t probe_range,
                                 int num_headers) {
  unsigned char *buf;
  int pos, nread;

  try {
    mm_io->setFilePointer(0, seek_beginning);
    buf = (unsigned char *)safemalloc(probe_range);
    nread = mm_io->read(buf, probe_range);
    pos = find_consecutive_ac3_headers(buf, nread, num_headers);
    safefree(buf);
    mm_io->setFilePointer(0, seek_beginning);
    return pos;
  } catch (...) {
    return -1;
  }
}
