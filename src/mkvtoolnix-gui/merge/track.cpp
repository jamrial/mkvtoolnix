#include "common/common_pch.h"

#include "common/iso639.h"
#include "common/list_utils.h"
#include "common/strings/editing.h"
#include "mkvtoolnix-gui/merge/mkvmerge_option_builder.h"
#include "mkvtoolnix-gui/merge/mux_config.h"
#include "mkvtoolnix-gui/merge/source_file.h"
#include "mkvtoolnix-gui/merge/track.h"
#include "mkvtoolnix-gui/util/config_file.h"
#include "mkvtoolnix-gui/util/settings.h"

#include <QVariant>

namespace mtx { namespace gui { namespace Merge {

using namespace mtx::gui;

Track::Track(SourceFile *file,
             Track::Type type)
  : m_file{file}
  , m_type{type}
{
}

Track::~Track() {
}

bool
Track::isType(Type type)
  const {
  return type == m_type;
}

bool
Track::isAudio()
  const {
  return Audio == m_type;
}

bool
Track::isVideo()
  const {
  return Video == m_type;
}

bool
Track::isSubtitles()
  const {
  return Subtitles == m_type;
}

bool
Track::isButtons()
  const {
  return Buttons == m_type;
}

bool
Track::isChapters()
  const {
  return Chapters == m_type;
}

bool
Track::isGlobalTags()
  const {
  return GlobalTags == m_type;
}

bool
Track::isTags()
  const {
  return Tags == m_type;
}

bool
Track::isAttachment()
  const {
  return Attachment == m_type;
}

bool
Track::isAppended()
  const {
  return !m_file ? false : m_file->m_appended;
}

bool
Track::isRegular()
  const {
  return isAudio() || isVideo() || isSubtitles() || isButtons();
}

bool
Track::isPropertySet(QString const &property)
  const {
  if (!m_properties.contains(property))
    return false;

  auto var = m_properties.value(property);

  return var.isNull()                            ? false
       : !var.isValid()                          ? false
       : var.canConvert(QMetaType::QVariantList) ? !var.toList().isEmpty()
       : var.canConvert(QMetaType::QString)      ? !var.toString().isEmpty()
       :                                           true;
}

void
Track::setDefaults() {
  if (!isRegular())
    return;

  auto &settings = Util::Settings::get();

  if (isAudio() && settings.m_setAudioDelayFromFileName)
    m_delay = extractAudioDelayFromFileName();

  m_forcedTrackFlag            = m_properties.value("forced_track").toBool() ? 1 : 0;
  m_forcedTrackFlagWasSet      = m_forcedTrackFlag == 1;
  m_defaultTrackFlagWasSet     = m_properties.value("default_track").toBool();
  m_defaultTrackFlagWasPresent = m_properties.contains("default_track");
  m_name                       = m_properties.value("track_name").toString();
  m_nameWasPresent             = !m_name.isEmpty();
  m_cropping                   = m_properties.value("cropping").toString();
  m_aacSbrWasDetected          = m_properties.value("aac_is_sbr").toString().contains(QRegExp{"1|true"});
  m_stereoscopy                = m_properties.contains("stereo_mode") ? m_properties.value("stereo_mode").toUInt() + 1 : 0;
  m_characterSet               = m_properties.value("text_subtitles").toBool() && m_file && (m_file->m_type != FILE_TYPE_MATROSKA) ? settings.m_defaultSubtitleCharset : Q("");

  auto language = m_properties.value("language").toString();
  if (   language.isEmpty()
      || (   (Util::Settings::SetDefaultLanguagePolicy::IfAbsentOrUndetermined == settings.m_whenToSetDefaultLanguage)
          && (language == Q("und"))))
    language = isAudio()     ? settings.m_defaultAudioTrackLanguage
             : isVideo()     ? settings.m_defaultVideoTrackLanguage
             : isSubtitles() ? settings.m_defaultSubtitleTrackLanguage
             :                 Q("");

  auto idx = map_to_iso639_2_code(to_utf8(language), true);
  if (0 <= idx)
    m_language = to_qs(g_iso639_languages[idx].iso639_2_code);

  QRegExp re_displayDimensions{"^(\\d+)x(\\d+)$"};
  if (-1 != re_displayDimensions.indexIn(m_properties.value("display_dimensions").toString())) {
    m_displayWidth  = re_displayDimensions.cap(1);
    m_displayHeight = re_displayDimensions.cap(2);
  }

  if (   !settings.m_enableMuxingTracksByLanguage
      || !mtx::included_in(m_type, Video, Audio, Subtitles)
      || ((Video     == m_type) && settings.m_enableMuxingAllVideoTracks)
      || ((Audio     == m_type) && settings.m_enableMuxingAllAudioTracks)
      || ((Subtitles == m_type) && settings.m_enableMuxingAllSubtitleTracks))
    m_muxThis = true;

  else {
    auto language = m_language.isEmpty() ? Q("und") : m_language;
    m_muxThis     = settings.m_enableMuxingTracksByTheseLanguages.contains(language);
  }
}

QString
Track::extractAudioDelayFromFileName()
  const {
  QRegExp re{"delay\\s+(-?\\d+)", Qt::CaseInsensitive};
  if (-1 == re.indexIn(m_file->m_fileName))
    return "";
  return re.cap(1);
}

void
Track::saveSettings(Util::ConfigFile &settings)
  const {
  MuxConfig::saveProperties(settings, m_properties);

  QStringList appendedTracks;
  for (auto &track : m_appendedTracks)
    appendedTracks << QString::number(reinterpret_cast<qulonglong>(track));

  settings.setValue("objectID",                   reinterpret_cast<qulonglong>(this));
  settings.setValue("appendedTo",                 reinterpret_cast<qulonglong>(m_appendedTo));
  settings.setValue("appendedTracks",             appendedTracks);
  settings.setValue("type",                       m_type);
  settings.setValue("id",                         static_cast<qulonglong>(m_id));
  settings.setValue("muxThis",                    m_muxThis);
  settings.setValue("setAspectRatio",             m_setAspectRatio);
  settings.setValue("defaultTrackFlagWasSet",     m_defaultTrackFlagWasSet);
  settings.setValue("defaultTrackFlagWasPresent", m_defaultTrackFlagWasPresent);
  settings.setValue("forcedTrackFlagWasSet",      m_forcedTrackFlagWasSet);
  settings.setValue("aacSbrWasDetected",          m_aacSbrWasDetected);
  settings.setValue("nameWasPresent",             m_nameWasPresent);
  settings.setValue("fixBitstreamTimingInfo",     m_fixBitstreamTimingInfo);
  settings.setValue("name",                       m_name);
  settings.setValue("codec",                      m_codec);
  settings.setValue("language",                   m_language);
  settings.setValue("tags",                       m_tags);
  settings.setValue("delay",                      m_delay);
  settings.setValue("stretchBy",                  m_stretchBy);
  settings.setValue("defaultDuration",            m_defaultDuration);
  settings.setValue("timecodes",                  m_timecodes);
  settings.setValue("aspectRatio",                m_aspectRatio);
  settings.setValue("displayWidth",               m_displayWidth);
  settings.setValue("displayHeight",              m_displayHeight);
  settings.setValue("cropping",                   m_cropping);
  settings.setValue("characterSet",               m_characterSet);
  settings.setValue("additionalOptions",          m_additionalOptions);
  settings.setValue("defaultTrackFlag",           m_defaultTrackFlag);
  settings.setValue("forcedTrackFlag",            m_forcedTrackFlag);
  settings.setValue("stereoscopy",                m_stereoscopy);
  settings.setValue("naluSizeLength",             m_naluSizeLength);
  settings.setValue("cues",                       m_cues);
  settings.setValue("aacIsSBR",                   m_aacIsSBR);
  settings.setValue("reduceAudioToCore",          m_reduceAudioToCore);
  settings.setValue("compression",                m_compression);
  settings.setValue("size",                       static_cast<qulonglong>(m_size));
  settings.setValue("attachmentDescription",      m_attachmentDescription);
}

void
Track::loadSettings(MuxConfig::Loader &l) {
  MuxConfig::loadProperties(l.settings, m_properties);

  auto objectID = l.settings.value("objectID").toULongLong();
  if ((0 >= objectID) || l.objectIDToTrack.contains(objectID))
    throw InvalidSettingsX{};

  l.objectIDToTrack[objectID]  = this;
  m_type                       = static_cast<Type>(l.settings.value("type").toInt());
  m_id                         = l.settings.value("id").toULongLong();
  m_muxThis                    = l.settings.value("muxThis").toBool();
  m_setAspectRatio             = l.settings.value("setAspectRatio").toBool();
  m_defaultTrackFlagWasSet     = l.settings.value("defaultTrackFlagWasSet").toBool();
  m_defaultTrackFlagWasPresent = l.settings.value("defaultTrackFlagWasPresent").toBool() || m_defaultTrackFlagWasSet;
  m_forcedTrackFlagWasSet      = l.settings.value("forcedTrackFlagWasSet").toBool();
  m_aacSbrWasDetected          = l.settings.value("aacSbrWasDetected").toBool();
  m_name                       = l.settings.value("name").toString();
  m_nameWasPresent             = l.settings.value("nameWasPresent").toBool();
  m_fixBitstreamTimingInfo     = l.settings.value("fixBitstreamTimingInfo").toBool();
  m_codec                      = l.settings.value("codec").toString();
  m_language                   = l.settings.value("language").toString();
  m_tags                       = l.settings.value("tags").toString();
  m_delay                      = l.settings.value("delay").toString();
  m_stretchBy                  = l.settings.value("stretchBy").toString();
  m_defaultDuration            = l.settings.value("defaultDuration").toString();
  m_timecodes                  = l.settings.value("timecodes").toString();
  m_aspectRatio                = l.settings.value("aspectRatio").toString();
  m_displayWidth               = l.settings.value("displayWidth").toString();
  m_displayHeight              = l.settings.value("displayHeight").toString();
  m_cropping                   = l.settings.value("cropping").toString();
  m_characterSet               = l.settings.value("characterSet").toString();
  m_additionalOptions          = l.settings.value("additionalOptions").toString();
  m_defaultTrackFlag           = l.settings.value("defaultTrackFlag").toInt();
  m_forcedTrackFlag            = l.settings.value("forcedTrackFlag").toInt();
  m_stereoscopy                = l.settings.value("stereoscopy").toInt();
  m_naluSizeLength             = l.settings.value("naluSizeLength").toInt();
  m_cues                       = l.settings.value("cues").toInt();
  m_aacIsSBR                   = l.settings.value("aacIsSBR").toInt();
  m_reduceAudioToCore          = l.settings.value("reduceAudioToCore").toBool();
  m_compression                = static_cast<Compression>(l.settings.value("compression").toInt());
  m_size                       = l.settings.value("size").toULongLong();
  m_attachmentDescription      = l.settings.value("attachmentDescription").toString();

  if (   (TypeMin > m_type)        || (TypeMax < m_type)
      || (CompMin > m_compression) || (CompMax < m_compression))
    throw InvalidSettingsX{};
}

void
Track::fixAssociations(MuxConfig::Loader &l) {
  if (isRegular() && isAppended()) {
    auto appendedToID = l.settings.value("appendedTo").toULongLong();
    if ((0 >= appendedToID) || !l.objectIDToTrack.contains(appendedToID))
      throw InvalidSettingsX{};
    m_appendedTo = l.objectIDToTrack.value(appendedToID);
  }

  m_appendedTracks.clear();
  for (auto &appendedTrackID : l.settings.value("appendedTracks").toStringList()) {
    if (!l.objectIDToTrack.contains(appendedTrackID.toULongLong()))
      throw InvalidSettingsX{};
    m_appendedTracks << l.objectIDToTrack.value(appendedTrackID.toULongLong());
  }
}

std::string
Track::debugInfo()
  const {
  return (boost::format("%1%/%2%:%3%@%4%") % static_cast<unsigned int>(m_type) % m_id % m_codec % this).str();
}

void
Track::buildMkvmergeOptions(MkvmergeOptionBuilder &opt)
  const {
  ++opt.numTracksOfType[m_type];

  if (!m_muxThis || (m_appendedTo && !m_appendedTo->m_muxThis))
    return;

  auto sid = QString::number(m_id);
  opt.enabledTrackIds[m_type] << sid;

  if (isAudio()) {
    if (m_aacSbrWasDetected || (0 != m_aacIsSBR))
      opt.options << Q("--aac-is-sbr") << Q("%1:%2").arg(sid).arg((1 == m_aacIsSBR) || ((0 == m_aacIsSBR) && m_aacSbrWasDetected) ? 1 : 0);

    if (m_reduceAudioToCore)
      opt.options << Q("--reduce-to-core") << sid;

  } else if (isVideo()) {
    if (!m_cropping.isEmpty())
      opt.options << Q("--cropping") << Q("%1:%2").arg(sid).arg(m_cropping);

  } else if (isSubtitles()) {
    if (!m_characterSet.isEmpty())
      opt.options << Q("--sub-charset") << Q("%1:%2").arg(sid).arg(m_characterSet);

  } else if (isChapters()) {
    if (!m_characterSet.isEmpty())
      opt.options << Q("--chapter-charset") << m_characterSet;

    if (!m_language.isEmpty())
      opt.options << Q("--chapter-language") << m_language;

    return;

  } else if (isTags() || isGlobalTags() || isAttachment())
    return;

  if (!m_appendedTo) {
    opt.options << Q("--language") << Q("%1:%2").arg(sid).arg(m_language);

    if (m_cues) {
      auto cues = 1 == m_cues ? Q(":iframes")
                : 2 == m_cues ? Q(":all")
                :               Q(":none");
      opt.options << Q("--cues") << (sid + cues);
    }

    if (!m_name.isEmpty() || m_nameWasPresent)
      opt.options << Q("--track-name") << Q("%1:%2").arg(sid).arg(m_name);

    if (0 != m_defaultTrackFlag)
      opt.options << Q("--default-track") << Q("%1:%2").arg(sid).arg(m_defaultTrackFlag == 1 ? Q("yes") : Q("no"));

    if (m_forcedTrackFlagWasSet != !!m_forcedTrackFlag)
      opt.options << Q("--forced-track") << Q("%1:%2").arg(sid).arg(m_forcedTrackFlag == 1 ? Q("yes") : Q("no"));

    if (!m_tags.isEmpty())
      opt.options << Q("--tags") << Q("%1:%2").arg(sid).arg(m_tags);

    if (m_setAspectRatio && !m_aspectRatio.isEmpty())
      opt.options << Q("--aspect-ratio") << Q("%1:%2").arg(sid).arg(m_aspectRatio);

    else if (!m_setAspectRatio && !m_displayHeight.isEmpty() && !m_displayWidth.isEmpty())
      opt.options << Q("--display-dimensions") << Q("%1:%2x%3").arg(sid).arg(m_displayWidth).arg(m_displayHeight);

    if (m_stereoscopy)
      opt.options << Q("--stereo-mode") << Q("%1:%2").arg(sid).arg(m_stereoscopy - 1);

    if (m_naluSizeLength)
      opt.options << Q("--nalu-size-length") << Q("%1:%2").arg(sid).arg(m_naluSizeLength);

    if (m_compression)
      opt.options << Q("--compression") << Q("%1:%2").arg(sid).arg(1 == m_compression ? Q("none") : Q("zlib"));
  }

  if (!m_delay.isEmpty() || !m_stretchBy.isEmpty()) {
    auto arg = Q("%1:%2").arg(sid).arg(m_delay.isEmpty() ? Q("0") : m_delay);

    if (!m_stretchBy.isEmpty()) {
      arg += Q(",%1").arg(m_stretchBy);
      if (!m_stretchBy.contains('/'))
        arg += Q("/1");
    }

    opt.options << Q("--sync") << arg;
  }

  if (!m_defaultDuration.isEmpty()) {
    auto unit = m_defaultDuration.contains(QRegExp{"[ip]$"}) ? Q("") : Q("fps");
    opt.options << Q("--default-duration") << Q("%1:%2%3").arg(sid).arg(m_defaultDuration).arg(unit);
  }

  if (!m_timecodes.isEmpty())
    opt.options << Q("--timecodes") << Q("%1:%2").arg(sid).arg(m_timecodes);

  if (m_fixBitstreamTimingInfo)
    opt.options << Q("--fix-bitstream-timing-information") << Q("%1:1").arg(sid);

  auto additionalOptions = Q(strip_copy(to_utf8(m_additionalOptions)));
  if (!additionalOptions.isEmpty())
    opt.options += additionalOptions.replace(Q("<TID>"), sid).split(QRegExp{" +"});
}

QString
Track::nameForType()
  const {
  return isAudio()      ? QY("Audio")
       : isVideo()      ? QY("Video")
       : isSubtitles()  ? QY("Subtitles")
       : isButtons()    ? QY("Buttons")
       : isAttachment() ? QY("Attachment")
       : isChapters()   ? QY("Chapters")
       : isTags()       ? QY("Tags")
       : isGlobalTags() ? QY("Global tags")
       :                   Q("INTERNAL ERROR");
}

}}}
