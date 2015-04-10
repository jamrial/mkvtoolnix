#include "common/common_pch.h"

#include <QDropEvent>
#include <QMimeData>

#include "mkvtoolnix-gui/util/files_drag_drop_handler.h"

namespace mtx { namespace gui { namespace Util {

FilesDragDropHandler::FilesDragDropHandler(Mode mode)
  : QObject{}
  , m_mode{mode}
{
}

bool
FilesDragDropHandler::handle(QDropEvent *event) {
  event->ignore();

  if (!event->mimeData()->hasUrls())
    return false;

  if (Mode::Remember == m_mode)
    m_fileNames = QStringList{};

  for (auto const &url : event->mimeData()->urls())
    if (!url.isLocalFile())
      return false;
    else if (Mode::Remember == m_mode)
      m_fileNames << url.toLocalFile();

  if (Mode::Remember == m_mode)
    event->acceptProposedAction();

  return true;
}

QStringList const &
FilesDragDropHandler::getFileNames()
  const {
  return m_fileNames;
}

}}}
