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
 * Subripper subtitle reader
 *
 * Written by Moritz Bunkus <moritz@bunkus.org>.
 */

#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include "pr_generic.h"
#include "r_srt.h"
#include "subtitles.h"
#include "matroska.h"

using namespace std;

#define iscolon(s) (*(s) == ':')
#define iscomma(s) (*(s) == ',')
#define istwodigits(s) (isdigit(*(s)) && isdigit(*(s + 1)))
#define isthreedigits(s) (isdigit(*(s)) && isdigit(*(s + 1)) && \
                          isdigit(*(s + 2)))
#define isarrow(s) (!strncmp((s), " --> ", 5))
#define istimecode(s) (istwodigits(s) && iscolon(s + 2) && \
                        istwodigits(s + 3) && iscolon(s + 5) && \
                        istwodigits(s + 6) && iscomma(s + 8) && \
                        isthreedigits(s + 9))
#define issrttimecode(s) (istimecode(s) && isarrow(s + 12) && \
                           istimecode(s + 17))

int
srt_reader_c::probe_file(mm_text_io_c *mm_io,
                         int64_t) {
  string s;
  int64_t dummy;

  try {
    mm_io->setFilePointer(0, seek_beginning);
    s = mm_io->getline();
    strip(s);
    if (!parse_int(s, dummy))
      return 0;
    s = mm_io->getline();
    if ((s.length() < 29) || !issrttimecode(s.c_str()))
      return 0;
    s = mm_io->getline();
    mm_io->setFilePointer(0, seek_beginning);
  } catch (exception &ex) {
    return 0;
  }
  return 1;
}

srt_reader_c::srt_reader_c(track_info_c *nti)
  throw (error_c):
  generic_reader_c(nti) {

  try {
    mm_io = new mm_text_io_c(new mm_file_io_c(ti->fname));
    if (!srt_reader_c::probe_file(mm_io, 0))
      throw error_c("srt_reader: Source is not a valid SRT file.");
    ti->id = 0;                 // ID for this track.
  } catch (exception &ex) {
    throw error_c("srt_reader: Could not open the source file.");
  }
  if (verbose)
    mxinfo(FMT_FN "Using the SRT subtitle reader.\n", ti->fname.c_str());
  parse_file();
}

srt_reader_c::~srt_reader_c() {
  delete mm_io;
}

void
srt_reader_c::create_packetizer(int64_t) {
  bool is_utf8;

  if (NPTZR() != 0)
    return;

  is_utf8 = mm_io->get_byte_order() != BO_NONE;
  add_packetizer(new textsubs_packetizer_c(this, MKV_S_TEXTUTF8, NULL, 0,
                                           true, is_utf8, ti));
  mxinfo(FMT_TID "Using the text subtitle output module.\n", ti->fname.c_str(),
         (int64_t)0);
}

#define STATE_INITIAL         0
#define STATE_SUBS            1
#define STATE_SUBS_OR_NUMBER  2
#define STATE_TIME            3

void
srt_reader_c::parse_file() {
  int64_t start, end, previous_start;
  char *chunk;
  string s, subtitles;
  int state, i, line_number;
  bool non_number_found, timecode_warning_printed;

  start = 0;
  end = 0;
  previous_start = 0;
  timecode_warning_printed = false;
  state = STATE_INITIAL;
  line_number = 0;
  subtitles = "";
  while (1) {
    if (!mm_io->getline2(s))
      break;
    line_number++;
    strip(s);

    if (s.length() == 0) {
      if ((state == STATE_INITIAL) || (state == STATE_TIME))
        continue;
      state = STATE_SUBS_OR_NUMBER;
      if (subtitles.length() > 0)
        subtitles += "\n";
      subtitles += "\n";
      continue;
    }

    if (state == STATE_INITIAL) {
      non_number_found = false;
      for (i = 0; i < s.length(); i++)
        if (!isdigit(s[i])) {
          mxwarn(FMT_FN "Error in line %d: expected subtitle number "
                 "and found some text.\n", ti->fname.c_str(), line_number);
          non_number_found = true;
          break;
        }
      if (non_number_found)
        break;
      state = STATE_TIME;

    } else if (state == STATE_TIME) {
      if ((s.length() < 29) || !issrttimecode(s.c_str())) {
        mxwarn(FMT_FN "Error in line %d: expected a SRT timecode "
               "line but found something else. Aborting this file.\n",
               ti->fname.c_str(), line_number);
        break;
      }

      if (subtitles.length() > 0) {
        strip(subtitles, true);
        subs.add(start, end, subtitles.c_str());
      }

      // 00:00:00,000 --> 00:00:00,000
      // 01234567890123456789012345678
      //           1         2
      chunk = safestrdup(s.c_str());
      chunk[2] = 0;
      chunk[5] = 0;
      chunk[8] = 0;
      chunk[12] = 0;
      chunk[19] = 0;
      chunk[22] = 0;
      chunk[25] = 0;
      chunk[29] = 0;

      start = atol(chunk) * 3600000 + atol(&chunk[3]) * 60000 +
        atol(&chunk[6]) * 1000 + atol(&chunk[9]);
      start *= 1000000;
      end = atol(&chunk[17]) * 3600000 + atol(&chunk[20]) * 60000 +
        atol(&chunk[23]) * 1000 + atol(&chunk[26]);
      end *= 1000000;

      if (!timecode_warning_printed && (start < previous_start)) {
        mxwarn(FMT_FN "Warning in line %d: The start timecode is smaller "
               "than that of the previous entry. All entries from this file "
               "will be sorted by their start time.\n", ti->fname.c_str(),
               line_number);
        timecode_warning_printed = true;
      }
      previous_start = start;

      safefree(chunk);

      subtitles = "";
      state = STATE_SUBS;

    } else if (state == STATE_SUBS) {
      if (subtitles.length() > 0)
        subtitles += "\n";
      subtitles += s;

    } else {
      non_number_found = false;
      for (i = 0; i < s.length(); i++)
        if (!isdigit(s[i])) {
          non_number_found = true;
          break;
        }

      if (!non_number_found)
        state = STATE_TIME;
      else {
        if (subtitles.length() > 0)
          subtitles += "\n";
        subtitles += s;
      }
    }
  }

  if (subtitles.length() > 0) {
    strip(subtitles, true);
    subs.add(start, end, subtitles.c_str());
  }

  subs.sort();
}

file_status_e
srt_reader_c::read(generic_packetizer_c *,
                   bool) {
  if (subs.empty())
    return FILE_STATUS_DONE;

  subs.process((textsubs_packetizer_c *)PTZR0);

  return subs.empty() ? FILE_STATUS_DONE : FILE_STATUS_MOREDATA;
}

int
srt_reader_c::get_progress() {
  int num_entries;

  num_entries = subs.get_num_entries();
  if (num_entries == 0)
    return 100;
  return 100 * subs.get_num_processed() / num_entries;
}

void
srt_reader_c::identify() {
  mxinfo("File '%s': container: SRT\nTrack ID 0: subtitles (SRT)\n",
         ti->fname.c_str());
}
