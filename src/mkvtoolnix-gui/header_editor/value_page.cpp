#include "common/common_pch.h"

#include <QCheckBox>
#include <QFrame>
#include <QGridLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

#include "common/ebml.h"
#include "common/qt.h"
#include "mkvtoolnix-gui/forms/header_editor/value_page.h"
#include "mkvtoolnix-gui/header_editor/tab.h"
#include "mkvtoolnix-gui/header_editor/value_page.h"
#include "mkvtoolnix-gui/util/widget.h"

namespace mtx { namespace gui { namespace HeaderEditor {

using namespace mtx::gui;

ValuePage::ValuePage(Tab &parent,
                     PageBase &topLevelPage,
                     EbmlMaster &master,
                     EbmlCallbacks const &callbacks,
                     ValueType valueType,
                     translatable_string_c const &title,
                     translatable_string_c const &description)
  : PageBase{parent, title}
  , m_master(master)
  , m_callbacks(callbacks)
  , m_description{description}
  , m_valueType{valueType}
  , m_element{find_ebml_element_by_id(&m_master, m_callbacks.GlobalId)}
  , m_present{!!m_element}
  , m_topLevelPage(topLevelPage)
{
}

ValuePage::~ValuePage() {
}

void
ValuePage::init() {
  m_lTitle = new QLabel{this};
  m_lTitle->setWordWrap(true);

  auto line = new QFrame{this};
  line->setFrameShape(QFrame::HLine);
  line->setFrameShadow(QFrame::Sunken);

  m_lTypeLabel    = new QLabel{this};
  m_lType         = new QLabel{this};

  auto sizePolicy = QSizePolicy{QSizePolicy::Expanding, QSizePolicy::Preferred};
  sizePolicy.setHorizontalStretch(0);
  sizePolicy.setVerticalStretch(0);
  sizePolicy.setHeightForWidth(m_lType->sizePolicy().hasHeightForWidth());
  m_lType->setSizePolicy(sizePolicy);

  if (!m_description.get_untranslated().empty()) {
    m_lDescriptionLabel = new QLabel{this};
    m_lDescription      = new QLabel{this};
    sizePolicy.setHeightForWidth(m_lDescription->sizePolicy().hasHeightForWidth());
    m_lDescription->setSizePolicy(sizePolicy);
    m_lDescription->setWordWrap(true);
  }

  m_lStatusLabel  = new QLabel{this};
  m_lStatus       = new QLabel{this};
  m_cbAddOrRemove = new QCheckBox{this};

  if (m_present) {
    auto semantic = find_ebml_semantic(KaxSegment::ClassInfos, m_callbacks.GlobalId);
    if (semantic && semantic->Mandatory)
      m_cbAddOrRemove->setEnabled(false);
  }

  sizePolicy.setHeightForWidth(m_lStatus->sizePolicy().hasHeightForWidth());
  m_lStatus->setSizePolicy(sizePolicy);
  m_lStatus->setWordWrap(true);

  if (m_present) {
    m_lOriginalValueLabel = new QLabel{this};
    m_lOriginalValue      = new QLabel{this};
  }

  m_lValueLabel = new QLabel{this};
  m_input       = createInputControl();
  m_input->setEnabled(m_present);

  m_bReset = new QPushButton{this};

  // ----------------------------------------------------------------------

  auto statusLayout = new QVBoxLayout{};
  statusLayout->addWidget(m_lStatus);
  statusLayout->addWidget(m_cbAddOrRemove);

  auto row        = 0;
  auto gridLayout = new QGridLayout{};
  gridLayout->addWidget(m_lTypeLabel,            row,   0, 1, 1);
  gridLayout->addWidget(m_lType,                 row++, 1, 1, 1);

  if (!m_description.get_untranslated().empty()) {
    gridLayout->addWidget(m_lDescriptionLabel,   row,   0, 1, 1, Qt::AlignTop);
    gridLayout->addWidget(m_lDescription,        row++, 1, 1, 1, Qt::AlignTop);
  }

  gridLayout->addWidget(m_lStatusLabel,          row,   0, 1, 1, Qt::AlignTop);
  gridLayout->addLayout(statusLayout,            row++, 1, 1, 1, Qt::AlignTop);

  if (m_present) {
    gridLayout->addWidget(m_lOriginalValueLabel, row,   0, 1, 1);
    gridLayout->addWidget(m_lOriginalValue,      row++, 1, 1, 1);
  }

  gridLayout->addWidget(m_lValueLabel,           row,   0, 1, 1);
  gridLayout->addWidget(m_input,                 row++, 1, 1, 1);

  auto resetLayout = new QHBoxLayout{};
  resetLayout->addItem(new QSpacerItem{0, 0, QSizePolicy::Expanding, QSizePolicy::Minimum});
  resetLayout->addWidget(m_bReset);

  auto widgetLayout = new QVBoxLayout{this};
  widgetLayout->setContentsMargins(-1, 0, 0, 0);
  widgetLayout->addWidget(m_lTitle);
  widgetLayout->addWidget(line);
  widgetLayout->addLayout(gridLayout);
  widgetLayout->addItem(new QSpacerItem{0, 0, QSizePolicy::Minimum, QSizePolicy::Expanding});
  widgetLayout->addLayout(resetLayout);

  // ----------------------------------------------------------------------

  connect(m_cbAddOrRemove, &QCheckBox::toggled,   this, &ValuePage::onAddOrRemoveChecked);
  connect(m_bReset,        &QPushButton::clicked, this, &ValuePage::onResetClicked);

  // ----------------------------------------------------------------------

  m_parent.appendPage(this, m_topLevelPage.m_pageIdx);

  m_topLevelPage.m_children << this;

  retranslateUi();
}

void
ValuePage::retranslateUi() {
  if (!m_bReset)
    return;

  m_lTitle->setText(Q(m_title.get_translated()));

  auto type = ValueType::AsciiString     == m_valueType ? QY("ASCII string (no special chars like Umlaute etc)")
            : ValueType::String          == m_valueType ? QY("String")
            : ValueType::UnsignedInteger == m_valueType ? QY("Unsigned integer")
            : ValueType::Float           == m_valueType ? QY("Floating point number")
            : ValueType::Binary          == m_valueType ? QY("Binary (displayed as hex numbers)")
            : ValueType::Bool            == m_valueType ? QY("Boolean (yes/no, on/off etc)")
            :                                             QY("unknown");

  m_lTypeLabel->setText(QY("Type:"));
  m_lType->setText(type);

  if (!m_description.get_translated().empty()) {
    m_lDescriptionLabel->setText(QY("Description:"));
    m_lDescription->setText(Q(m_description.get_translated()));
  }

  m_lStatusLabel->setText(QY("Status:"));
  if (m_present) {
    auto semantic = find_ebml_semantic(KaxSegment::ClassInfos, m_callbacks.GlobalId);
    if (semantic && semantic->Mandatory)
      m_lStatus->setText(Q("%1 %2").arg(QY("This element is currently present in the file.")).arg(QY("It cannot be removed because it is a mandatory header field.")));
    else
      m_lStatus->setText(Q("%1 %2").arg(QY("This element is currently present in the file.")).arg(QY("You can let the header editor remove the element from the file.")));

    m_lValueLabel->setText(QY("Current value:"));
    m_cbAddOrRemove->setText(QY("Remove element"));

    m_lOriginalValueLabel->setText(QY("Original value:"));
    m_lOriginalValue->setText(originalValueAsString());

  } else {
    m_lStatus->setText(Q("%1 %2").arg(QY("This element is not currently present in the file.")).arg(QY("You can let the header editor add the element to the file.")));
    m_lValueLabel->setText(QY("New value:"));
    m_cbAddOrRemove->setText(QY("Add element"));

  }

  m_bReset->setText(QY("&Reset this value"));
  Util::setToolTip(m_bReset, QY("Resets the header value on this page to how it's saved in the file."));
}

void
ValuePage::onResetClicked() {
  resetValue();
  m_cbAddOrRemove->setChecked(false);
  m_input->setEnabled(m_present);
}

void
ValuePage::onAddOrRemoveChecked() {
  m_input->setEnabled(willBePresent());
}

bool
ValuePage::willBePresent()
  const {
  return (!m_present &&  m_cbAddOrRemove->isChecked())
      || ( m_present && !m_cbAddOrRemove->isChecked());
}

bool
ValuePage::hasThisBeenModified()
  const {
  return m_cbAddOrRemove->isChecked() || (currentValueAsString() != originalValueAsString());
}

bool
ValuePage::validateThis()
  const {
  if (!m_input->isEnabled())
    return true;
  return validateValue();
}

void
ValuePage::modifyThis() {
  if (!hasThisBeenModified())
    return;

  if (m_present && m_cbAddOrRemove->isChecked()) {
    for (auto i = 0u; m_master.ListSize() > i; ++i) {
      if (m_master[i]->Generic().GlobalId != m_callbacks.GlobalId)
        continue;

      auto e = m_master[i];
      delete e;

      m_master.Remove(i);

      break;
    }

    return;
  }

  if (!m_present) {
    m_element = &m_callbacks.Create();
    m_master.PushElement(*m_element);
  }

  copyValueToElement();
}

}}}
