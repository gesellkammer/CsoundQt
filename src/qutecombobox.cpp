/***************************************************************************
 *   Copyright (C) 2008 by Andres Cabrera                                  *
 *   mantaraya36@gmail.com                                                 *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 3 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA.              *
 ***************************************************************************/
#include "qutecombobox.h"

QuteComboBox::QuteComboBox(QWidget *parent) : QuteWidget(parent)
{
  m_widget = new MyQComboBox(this);
  connect(((MyQComboBox *)m_widget), SIGNAL(popUpMenu(QPoint)), this, SLOT(popUpMenu(QPoint)));
//   ((QComboBox *)m_widget)->setIcon(icon);
//   connect((QComboBox *)m_widget, SIGNAL(released()), this, SLOT(buttonReleased()));
}

QuteComboBox::~QuteComboBox()
{
}

void QuteComboBox::setValue(double value)
{
  qDebug("QuteComboBox::setValue %i", (int) value);
  // setValue sets the current index of the ioMenu
  ((QComboBox *)m_widget)->setCurrentIndex((int) value);
  m_value = ((QComboBox *)m_widget)->currentIndex();  //This confines the value to valid indices
}

double QuteComboBox::getValue()
{
  // Returns the current index
  return (float) ((QComboBox *)m_widget)->currentIndex();
}

void QuteComboBox::setSize(int size)
{
  m_size = size;
}

QString QuteComboBox::getWidgetLine()
{
  QString line = "ioMenu {" + QString::number(x()) + ", " + QString::number(y()) + "} ";
  line += "{"+ QString::number(width()) +", "+ QString::number(height()) +"} ";
  line += QString::number(((QComboBox *)m_widget)->currentIndex()) + " ";
  line += QString::number(m_size) + " ";
  line += "\"" + itemList() + "\" ";
  line += m_name;
  return line;
}

void QuteComboBox::applyProperties()
{
  setText(text->text());
  //TODO set size for Menu widget
//   setSize
  setWidgetGeometry(xSpinBox->value(), ySpinBox->value(), wSpinBox->value(), hSpinBox->value());
  QuteWidget::applyProperties();  //Must be last to make sure the widgetsChanged signal is last
}

void QuteComboBox::contextMenuEvent(QContextMenuEvent* event)
{
  qDebug("QuteComboBox::contextMenuEvent");
  QuteWidget::contextMenuEvent(event);
}

void QuteComboBox::createPropertiesDialog()
{
  QuteWidget::createPropertiesDialog();
  QLabel *label = new QLabel(dialog);
  //TODO add size selection for combo box
//   label->setText("Size");
//   label->setAlignment(Qt::AlignRight|Qt::AlignVCenter);
//   layout->addWidget(label, 4, 0, Qt::AlignRight|Qt::AlignVCenter);
// //   label->setFrameStyle(QFrame::Panel | QFrame::Sunken);
//   typeComboBox = new QComboBox(dialog);
//   typeComboBox->addItem("event");
//   typeComboBox->addItem("value");
//   typeComboBox->addItem("pictevent");
//   typeComboBox->addItem("pictvalue");
//   typeComboBox->addItem("pict");
//   typeComboBox->setCurrentIndex(typeComboBox->findText(m_type));
//   layout->addWidget(typeComboBox, 4, 1, Qt::AlignLeft|Qt::AlignVCenter);

  label = new QLabel(dialog);
  label->setText("Items (separated by commas):");
  label->setAlignment(Qt::AlignRight|Qt::AlignVCenter);
  layout->addWidget(label, 5, 0, Qt::AlignRight|Qt::AlignVCenter);
  text = new QLineEdit(dialog);
  text->setText(itemList());
  layout->addWidget(text, 5,1,1,3, Qt::AlignLeft|Qt::AlignVCenter);
  text->setMinimumWidth(320);
}

void QuteComboBox::setText(QString text)
{
  ((QComboBox *)m_widget)->clear();
  QStringList items = text.split(",");
  foreach (QString item, items) {
    ((QComboBox *)m_widget)->addItem(item);
  }
}

QString QuteComboBox::itemList()
{
  QString list = "";
  for (int i = 0; i < ((QComboBox *)m_widget)->count(); i++) {
    list += ((QComboBox *)m_widget)->itemText(i) + ",";
  }
  list.chop(1); //remove last comma
  return list;
}

void QuteComboBox::popUpMenu(QPoint pos)
{
  QuteWidget::popUpMenu(pos);
}
