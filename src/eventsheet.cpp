/*
    Copyright (C) 2009 Andres Cabrera
    mantaraya36@gmail.com

    This file is part of QuteCsound.

    QuteCsound is free software; you can redistribute it
    and/or modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    QuteCsound is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with Csound; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
    02111-1307 USA
*/

#include "eventsheet.h"
#include "liveeventframe.h"

#include <QMenu>
#include <QDir>
#include <QContextMenuEvent>
#include <QLabel>
#include <QVBoxLayout>
#include <QLineEdit>
#include <QDialog>
#include <QDoubleSpinBox>
#include <QApplication>
#include <QClipboard>

#include <QFile>
#include <QMessageBox>

// For rand() function
#include <cstdlib>
// Only for debug
#include <QtCore>

class OneValueDialog: public QDialog
{
public:
  OneValueDialog(QWidget *parent, QString label, double defaultValue = 0.0):
      QDialog(parent)
  {
    l = new QVBoxLayout(this);
    lab= new QLabel(label, this);
    box = new QDoubleSpinBox(this);
    box->setRange(-999999, 999999);
    box->setDecimals(8);
    box->setValue(defaultValue);
    box->selectAll();
    l->addWidget(lab);
    l->addWidget(box);
    connect(box, SIGNAL(editingFinished()), this, SLOT(accept ()) );

  }

  double value() { return box->value();}

  QVBoxLayout *l;
  QLabel *lab;
  QDoubleSpinBox *box;
};

class ThreeValueDialog: public QDialog
{
public:
  ThreeValueDialog(QWidget *parent, QStringList labels, QVector<double> defaultValues):
      QDialog(parent)
  {
    l = new QVBoxLayout(this);
    lab1= new QLabel(labels[0], this);
    box1 = new QDoubleSpinBox(this);
    box1->setRange(-999999, 999999);
    box1->setDecimals(8);
    box1->setValue(defaultValues[0]);
    box1->selectAll();
    lab2= new QLabel(labels[1], this);
    box2 = new QDoubleSpinBox(this);
    box2->setRange(-999999, 999999);
    box2->setDecimals(8);
    box2->setValue(defaultValues[1]);
    l->addWidget(lab1);
    l->addWidget(box1);
    l->addWidget(lab2);
    l->addWidget(box2);
    if (labels[2] != "") {
      lab3= new QLabel(labels[2], this);
      box3 = new QDoubleSpinBox(this);
      box3->setRange(-999999, 999999);
      box3->setDecimals(8);
      box3->setValue(defaultValues[2]);
      l->addWidget(lab3);
      l->addWidget(box3);
    }
//    connect(box1, SIGNAL(editingFinished()), this, SLOT(accept ()) );
//    connect(box2, SIGNAL(editingFinished()), this, SLOT(accept ()) );
    connect(box3, SIGNAL(editingFinished()), this, SLOT(accept ()) );
  }

  double value1() { return box1->value();}
  double value2() { return box2->value();}
  double value3() { return box3->value();} // Be careful, this is not checked!
  QVBoxLayout *l;
  QLabel *lab1;
  QDoubleSpinBox *box1;
  QLabel *lab2;
  QDoubleSpinBox *box2;
  QLabel *lab3;
  QDoubleSpinBox *box3;

protected:
  virtual void keyPressEvent (QKeyEvent * event) {
    if (event->key() == Qt::Key_Escape) {
      this->reject();
    }
    else if (event->key() == Qt::Key_Return) {
      if (box1->hasFocus())
        box2->setFocus(Qt::TabFocusReason);
      else if (box2->hasFocus())
        box3->setFocus(Qt::TabFocusReason);
    }
    else {
      QDialog::keyPressEvent(event);  // Otherwise propagate event
    }
  }
};

EventSheet::EventSheet(QWidget *parent) : QTableWidget(parent)
{
//  qDebug() << "EventSheet::EventSheet";
  this->setRowCount(10);
  this->setColumnCount(6);
  columnNames << tr("Event") << "p1 (instr)" << "p2 (start)" << "p3 (dur)" << "p4" << "p5";
  this->setHorizontalHeaderLabels(columnNames);
  this->setColumnWidth(0, 50);
  this->setColumnWidth(1, 70);
  this->setColumnWidth(2, 70);
  this->setColumnWidth(3, 70);
  this->setColumnWidth(4, 50);
  this->setColumnWidth(5, 50);

  m_name = "Events";
  createActions();
  connect(this, SIGNAL(itemSelectionChanged()), this, SLOT(selectionChanged()));
  // a bit of a hack to ensure that manual changes to the sheet are stored in the
  // undo history. This seems better than calling markHistory() when a cell
  // changes because large operations like add or subractract will produce
  // many steps in the history
  connect(this, SIGNAL(cellDoubleClicked(int, int)), this, SLOT(cellDoubleClickedSlot(int, int)));
  connect(this, SIGNAL(cellChanged (int, int)), this, SLOT(cellChangedSlot(int, int)));

  loopTimer.setSingleShot(true);
  connect(&loopTimer, SIGNAL(timeout()), this, SLOT(sendEvents()));

  builtinScripts << ":/python/sort_by_start.py";
  converterScripts << ":/python/cps2mid.py" << ":/python/mid2cps.py" << ":/python/cps2pch.py" << ":/python/pch2cps.py";
  testScripts <<  ":/python/test/python_test.py" << ":/python/test/tk_test.py";

  noHistoryChange = 0;
}

EventSheet::~EventSheet()
{
}

QString EventSheet::getPlainText(bool scaleTempo)
{
  QString t = "";
  for (int i = 0; i < this->rowCount(); i++) {
    t += getLine(i, scaleTempo) + "\n";  // Don't scale by default
  }
  t.chop(1);
//  qDebug() << " EventSheet::getPlainText   " << t;
  return t;
}

QString EventSheet::getLine(int number, bool scaleTempo, bool storeNumber, bool preprocess, double startOffset)
{
  QString line = "";
  bool instrEvent = false;
  bool comment = false;
  for (int i = 0; i < this->columnCount(); i++) {
    QTableWidgetItem * item = this->item(number, i);
    if (item != 0) { // Item is not empty
      if (i == 0 && (item->data(Qt::DisplayRole).toString() == "i") ) {
        instrEvent = true;  // Check if event is intrument note to allow scale by tempo
      }
      if (instrEvent && i == 1 && storeNumber) { // append current instrument number to active note list
        bool ok = false;
        double instrNum = item->data(Qt::DisplayRole).toDouble(&ok);
        if (ok && !activeInstruments.contains(instrNum))
          activeInstruments.append(instrNum);
      }
      if (preprocess && item->data(Qt::DisplayRole).toString() == ".") { // Carry value from above
        QString cellText = ".";
        int row = number;
        while (row >= 0 && cellText == "." && cellText != "") {
          item = this->item(row, i);
          if (item != 0 ) {
            cellText = item->data(Qt::DisplayRole).toString();
          }
          else {
            cellText = "";
          }
          row--;
        }
      }
      if (scaleTempo && instrEvent && (i == 2 || i == 3) ) { // Scale tempo only for pfields p2 and p3
        bool ok = false;
        double value = item->data(Qt::DisplayRole).toDouble(&ok);
        if (ok) {
          if (i == 2) { // Add start offset to p2 before scaling
            value += startOffset;
          }
          value = value * (60.0/m_tempo);
          line += QString::number(value, 'f', 8);;
        }
        else {
          line += item->data(Qt::DisplayRole).toString();
        }
      }
      else {  // All other p-fields that don't require tempo scaling
        QString cellText = item->data(Qt::DisplayRole).toString();
        if (comment) {
          bool dataRemaining = false;  // Check if remaining cells have data
          for (int j = i + 1; j < this->columnCount(); j++) {
            QTableWidgetItem * checkItem = this->item(number, j);
            if (checkItem != 0 && checkItem->data(Qt::DisplayRole).toString() != "") {
              dataRemaining = true;
              break;
            }
          }
          if (dataRemaining || cellText != "")
            line += ";" + cellText;
        }
        else if (cellText.startsWith(';')) {
          comment = true; // is a comment from now on
          line += cellText;
        }
        else {
          line += cellText;
        }
      }
      // Then add white space separation
      QString space = item->data(Qt::UserRole).toString();
      if (!space.isEmpty()) { // Separataion is stored in UserRole of items
        line += space;
      }
      else {
        line += " ";
      }
    }
    else if (comment) { // empty cell part of a comment
      // Do nothing
    }
  }
  return line;
}

void EventSheet::setFromText(QString text, int rowOffset, int columnOffset, int numRows, int numColumns, bool noHistoryMark)
{
  // Separataion is stored in UserRole of items
  // remember to treat comments and formulas properly
  QStringList lines = text.split("\n");
  int nRows = numRows == 0 ? lines.size() : numRows;  // 0 pastes only available data
  nRows = (numRows == -1  && nRows <  this->rowCount()) ?  this->rowCount() : nRows; //-1 pastes all of numColumns even if not in data
  if (this->rowCount() <nRows + rowOffset) {
    this->setRowCount(nRows + rowOffset);
  }
  for (int i = 0; i < nRows; i++) {
    if (nRows != 0 && i >= nRows) {  // Only paste up to a certain number of rows if not 0
      break;
    }
    QString line = "";
    if (i < lines.size()) {
      line = lines[i].trimmed(); //Remove whitespace from start and end
    }
    QList<QPair<QString, QString> > fields = parseLine(line);
    int nColumns = numColumns == 0 ? fields.size() : numColumns;
    nColumns = (numColumns == -1 && nColumns <  this->columnCount()) ?  this->columnCount() : nColumns;
    if (this->columnCount() < nColumns + columnOffset) {
      this->setColumnCount(nColumns + columnOffset);
    }
    for (int j = 0; j < nColumns; j++) {
      if (nColumns != 0 && j >= nColumns) {  // Only paste up to a certain number of columns if not 0
        break;
      }
      QTableWidgetItem * item = this->item(i + rowOffset, j + columnOffset);
      if (item == 0) {
        item = new QTableWidgetItem();
        noHistoryChange = (noHistoryMark ? 1: 0);
        this->setItem(i + rowOffset, j + columnOffset, item);
      }
      if (j < fields.size()) {
        noHistoryChange = (noHistoryMark ? 1: 0);
        item->setData(Qt::DisplayRole, fields[j].first);
        noHistoryChange = (noHistoryMark ? 1: 0);
        item->setData(Qt::UserRole, fields[j].second);
      }
      else {
        noHistoryChange = (noHistoryMark ? 1: 0);
        item->setData(Qt::DisplayRole, "");
        noHistoryChange = (noHistoryMark ? 1: 0);
        item->setData(Qt::UserRole, "");
      }
    }
  }
  if (this->rowCount() == 0)
    this->setRowCount(1);
}

void EventSheet::setTempo(double value)
{
//  qDebug() << "EventSheet::setTempo " << value;
  m_tempo = value;
}

void EventSheet::setLoopLength(double value)
{
//  qDebug() << "EventSheet::setLoopLength " << value;
  m_loopLength = value;
}

void EventSheet::sendEvents()
{
  QModelIndexList list;
  QList<int> selectedRows;
  if (m_looping && sender() != sendEventsAct) {
    double time = 1000.0 * m_loopLength * 60.0 /m_tempo;
    qDebug() << " EventSheet::sendEvents() " << time;
    loopTimer.start(time);
    list = loopList;
  }
  else {
    list = this->selectedIndexes();
  }
  for (int i = 0; i < list.size(); i++) {
    if (!selectedRows.contains(list[i].row()) ) {
      selectedRows.append(list[i].row());
    }
  }
  for (int i = 0; i < selectedRows.size(); i++) {
//    double number = 0.0;
    emit sendEvent(getLine(selectedRows[i], true, true, true));  // With tempo scaling
  }
}

void EventSheet::sendEventsOffset()
{
  QModelIndexList list;
  QList<int> selectedRows;
  list = this->selectedIndexes();
  for (int i = 0; i < list.size(); i++) {
    if (!selectedRows.contains(list[i].row()) ) {
      selectedRows.append(list[i].row());
    }
  }
  double minTime = 999999999999999.0;
  bool hasMin = false;
  for (int i = 0; i < selectedRows.size(); i++) {
    QTableWidgetItem * item = this->item(selectedRows[i], 2);
    if (item != 0 && item->data(Qt::DisplayRole).canConvert(QVariant::Double)) {
      bool ok = false;
      double n = item->data(Qt::DisplayRole).toDouble(&ok);
      if (ok && n < minTime) {
        minTime = n;
        hasMin = true;
      }
    }
  }
  if (!hasMin)
    minTime = 0.0;
  for (int i = 0; i < selectedRows.size(); i++) {
//    double number = 0.0;
    emit sendEvent(getLine(selectedRows[i], true, true, true, -minTime));  // With tempo scaling
  }
}

void EventSheet::loopEvents()
{
  for (int i = 0; i < loopList.size(); i++) {
    QTableWidgetItem * item = this->item(loopList[i].row(), loopList[i].column());
    if (item == 0) {
        item = new QTableWidgetItem();
        this->setItem(loopList[i].row(), loopList[i].column(), item );
      }
    item->setBackground(QBrush());
  }
  m_looping = true;
  loopList = this->selectedIndexes();
  for (int i = 0; i < loopList.size(); i++) {
    QTableWidgetItem * item = this->item(loopList[i].row(), loopList[i].column());
    if (item == 0) {
        item = new QTableWidgetItem();
        this->setItem(loopList[i].row(), loopList[i].column(), item );
      }
    item->setBackground(QBrush(Qt::green));
  }
  sendEvents();
}

void EventSheet::stopAllEvents()
{
  m_looping = false;

  for (int i = 0; i < loopList.size(); i++) {
    QTableWidgetItem * item = this->item(loopList[i].row(), loopList[i].column());
    if (item == 0) {
        item = new QTableWidgetItem();
        this->setItem(loopList[i].row(), loopList[i].column(), item );
      }
    item->setBackground(QBrush());
  }
  loopTimer.stop();
  // TODO is it necessary to protect this data from threaded access?
  while (!activeInstruments.isEmpty()) {
    QString event = "i -";
    event += QString::number(activeInstruments.takeFirst(), 'f', 10);
    event += " 0 1";
    emit sendEvent(event);
  }
}

void EventSheet::del()
{
  QModelIndexList list = this->selectedIndexes();
  for (int i = 0; i < list.size(); i++) {
    QTableWidgetItem * item = this->item(list[i].row(), list[i].column());
    if (item != 0) { // Item is not empty
      delete item;
    }
  }
  markHistory();
}

void EventSheet::cut()
{
  copy(true);
  markHistory();
}

void EventSheet::copy(bool cut)
{
  QModelIndexList list = this->selectedIndexes();
  QList<int> selectedRows;
  QList<int> selectedColumns;
  for (int i = 0; i < list.size(); i++) { // Get list of selected rows
    if (!selectedRows.contains(list[i].row()) ) {
      selectedRows.append(list[i].row());
    }
    if (!selectedColumns.contains(list[i].column()) ) {
      selectedColumns.append(list[i].column());
    }
  }
  QString text = "";
  for (int i = 0; i < selectedRows.size(); i++) {
    QString line = "";
    for (int j = 0; j < selectedColumns.size(); j++) {
      QTableWidgetItem * item = this->item(selectedRows[i], selectedColumns[j]);
      if (item != 0) { // Item is not empty
        line += item->data(Qt::DisplayRole).toString();
        QString space = item->data(Qt::UserRole).toString();
        if (!space.isEmpty())
          line += space;
        else
          line += " ";
        if (cut)
          delete item;
      }
    }
    text += line + "\n";
  }
  text.chop(1); // remove last line break
  qApp->clipboard()->setText(text);
}

void EventSheet::paste()
{
  qDebug() << "EventSheet::paste() text = " << qApp->clipboard()->text();
  QModelIndexList list = this->selectedIndexes();
  QList<int> selectedRows;
  QList<int> selectedColumns;
  int rowCount, columnCount;
  int lowestRow = 99999, lowestColumn = 99999;
  for (int i = 0; i < list.size(); i++) { // Get list of selected rows
    if (!selectedRows.contains(list[i].row()) ) {
      selectedRows.append(list[i].row());
      if (list[i].row() < lowestRow) {
        lowestRow = list[i].row();
      }
    }
    if (!selectedColumns.contains(list[i].column()) ) {
      selectedColumns.append(list[i].column());
      if (list[i].column() < lowestColumn) {
        lowestColumn = list[i].column();
      }
    }
  }
  rowCount = selectedRows.size();
  columnCount = selectedColumns.size();
  if (rowCount <= 1 && columnCount <= 1) {
    rowCount = columnCount = 0;
  }
  setFromText(qApp->clipboard()->text(), lowestRow, lowestColumn, rowCount, columnCount, true);
  markHistory();
}

void EventSheet::undo()
{
  qDebug() << "EventSheet::undo() " << historyIndex;
  if (historyIndex > 0) {
    historyIndex--;
    setFromText(history[historyIndex], 0,0,rowCount(),columnCount(),true);
  }
}

void EventSheet::redo()
{
  qDebug() << "EventSheet::redo() " << historyIndex << history.size();
  if (historyIndex < history.size() - 1) {
    historyIndex++;
    setFromText(history[historyIndex], 0,0,rowCount(),columnCount(),true);
  }
}

void EventSheet::markHistory()
{
  QString text = getPlainText();
  if (history.isEmpty()) {
    history << "";
    historyIndex = 0;
  }
  if (history[historyIndex] != text) {
    if (! history[historyIndex].isEmpty())
      historyIndex++;
//    if (historyIndex >= QUTE_MAX_UNDO) {
//      history.pop_front();
//      historyIndex--;
//    }
    if (history.size() != historyIndex + 1)
      history.resize(historyIndex + 1);
    history[historyIndex] = text;
//    qDebug() << "EventSheet::markHistory "<< historyIndex << " ....."  << text;
  }
}

void EventSheet::clearHistory()
{
  QString text = getPlainText();
//  qDebug() << "EventSheet::clearHistory() " << text;
  history.clear();
  historyIndex = 0;
}

void EventSheet::setScriptDirectory(QString dir)
{
  scriptDir = dir;
  if (!scriptDir.endsWith("/"))
    scriptDir += "/";
}

void EventSheet::subtract()
{
  OneValueDialog d(this, tr("Subtract"));
  d.exec();
  if (d.result() == QDialog::Accepted) {
    this->add(- d.value());
  }
}

void EventSheet::add()
{
  OneValueDialog d(this, tr("Add"));
  d.exec();
  if (d.result() == QDialog::Accepted) {
    this->add(d.value());
  }
}

void EventSheet::multiply()
{
  OneValueDialog d(this, tr("Multiply by"));
  d.exec();
  if (d.result() == QDialog::Accepted) {
    this->multiply(d.value());
  }
}

void EventSheet::divide()
{
  OneValueDialog d(this, tr("Divide by"));
  d.exec();
  if (d.result() == QDialog::Accepted) {
    this->divide(d.value());
  }
}

void EventSheet::randomize()
{
  QStringList labels;
  labels << tr("Minimum") << tr("Maximum") << tr("Mode: 0=decimals 1=Integers only");
  QVector<double> defaultValues;
  defaultValues << 0.0 << 1.0 << 0.0;
  ThreeValueDialog d(this, labels, defaultValues);
  d.box3->setDecimals(0);
  d.exec();
  if (d.result() == QDialog::Accepted) {
    this->randomize(d.value1(), d.value2(), d.value3());
  }
}

void EventSheet::reverse()
{
  QModelIndexList list = this->selectedIndexes();
  if (list.size() < 2)
    return;
  QList<int> selectedColumns;
  for (int i = 0; i < list.size(); i++) {
    if (!selectedColumns.contains(list[i].column()) ) {
      selectedColumns.append(list[i].column());
    }
  }
  int numRows = list.size() / selectedColumns.size();
  if (numRows < 2)
    return;

  QVector<QVariant> elements;
  elements.resize(numRows);
  for (int i = 0; i < selectedColumns.size(); i++) {
    for (int j = 0; j < numRows; j++) {
      QTableWidgetItem * item = this->item(list[(i*numRows) + j].row(), list[(i*numRows) + j].column());
      if (item != 0) {
        elements[numRows - j - 1] = item->data(Qt::DisplayRole);
      }
      else {
        elements[numRows - j - 1] = QVariant();
      }
    }
    for (int j = 0; j < numRows; j++) {
      QTableWidgetItem * item = this->item(list[(i*numRows) + j].row(), list[(i*numRows) + j].column());
      if (item == 0) {
        item = new QTableWidgetItem();
        this->setItem(list[(i*numRows) + j].row(), list[(i*numRows) + j].column(), item );
      }
      item->setData(Qt::DisplayRole, elements[j] );
    }
  }
}

void EventSheet::shuffle()
{
  OneValueDialog d(this, tr("Iterations"));
  d.box->setValue(10.0);
  d.box->setDecimals(0);
  d.box->selectAll();
  d.exec();
  if (d.result() == QDialog::Accepted) {
    this->shuffle(d.value());
  }
}

//void EventSheet::mirror()
//{
//
//}

void EventSheet::rotate()
{
  OneValueDialog d(this, tr("Rotate by"));
  d.box->setValue(1.0);
  d.box->setDecimals(0);
  d.exec();
  if (d.result() == QDialog::Accepted) {
    this->rotate(d.value());
  }
}

void EventSheet::fill()
{
  QStringList labels;
  labels << tr("From") << tr("To") << tr("Slope (1=Linear)");
  QVector<double> defaultValues;
  defaultValues << 1.0 << 5.0 << 1.0;
  ThreeValueDialog d(this, labels, defaultValues);
  connect(d.box1, SIGNAL(valueChanged (double)),
          d.box2, SLOT(setValue(double)));
  d.exec();
  if (d.result() == QDialog::Accepted) {
    this->fill(d.value1(), d.value2(), d.value3());
  }
}

void EventSheet::runScript()
{
  runScript(static_cast<QAction *>(sender())->data().toString());
}

QString EventSheet::generateDataText()
{
  QModelIndexList list = this->selectedIndexes();
  int minRow = 999999, minCol = 999999, maxRow = -1, maxCol = -1;
  for (int i = 0; i < list.size(); i++) { // First traverse to find size
    if (list[i].row() > maxRow) {
      maxRow = list[i].row();
    }
    if (list[i].row() < minRow) {
      minRow = list[i].row();
    }
    if (list[i].column() > maxCol) {
      maxCol = list[i].column();
    }
    if (list[i].column() < minCol) {
      minCol = list[i].column();
    }
  }
  QString data = "[ ";
  for (int i = minRow; i <= maxRow; i++) {
    data += "( ";
    for (int j = minCol; j <= maxCol; j++) {
      QTableWidgetItem * item = this->item(i, j);
      if ( item == 0) {
        data += "''";
      }
      else {
        bool ok;
        item->data(Qt::DisplayRole).toString().toDouble(&ok);
        if (ok) {
          data += item->data(Qt::DisplayRole).toString();
        }
        else {
          data += "'" + item->data(Qt::DisplayRole).toString() + "'";
        }
      }
      data += ", ";
    }
    data.chop(2);
    data += " ),\n";
  }
  data.chop(2);
  data += " ]";

  QString data_all = "[ ";
  for (int i = 0; i < this->rowCount(); i++) {
    data_all += "( ";
    for (int j = 0; j < this->columnCount() ; j++) {
      QTableWidgetItem * item = this->item(i, j);
      if ( item == 0) {
        data_all += "''";
      }
      else {
        bool ok;
        item->data(Qt::DisplayRole).toString().toDouble(&ok);
//        qDebug() << item->data(Qt::DisplayRole).typeName() << item;
        if (ok) {
          data_all += item->data(Qt::DisplayRole).toString();
        }
        else {
          data_all += "'" + item->data(Qt::DisplayRole).toString() + "'";
        }
      }
      data_all += ", ";
    }
    data_all.chop(2);
    data_all += " ),\n";
  }
  data_all.chop(2);
  data_all += " ]";

  QString text;
  text += "row = " + QString::number(minRow) + "\n";
  text += "col = " + QString::number(minCol) + "\n";
  text += "num_rows = " + QString::number(maxRow - minRow + 1) + "\n";
  text += "num_cols = " + QString::number(maxCol - minCol + 1) + "\n";
  text += "total_rows = " + QString::number(this->rowCount()) + "\n";
  text += "total_cols = " + QString::number(this->columnCount ()) + "\n";

  text += "data = " + data + "\n";
  text += "data_all = " + data_all + "\n";
  return text;
}

void EventSheet::runScript(QString name)
{
//  qDebug() << "EventSheet::runScript " << name;

  QString outFileName = "qutesheet_out_data.txt";
  QDir oldDir = QDir::current();
  QDir tempDir(QDir::tempPath());
  QString subDir = "QCS-" + QString::number(qrand());
  while (!tempDir.mkdir(subDir))
    subDir = "QCS-" + QString(qrand());
  tempDir.cd(subDir);
  QDir::setCurrent(tempDir.absolutePath());
  QFile module(tempDir.absolutePath() + QDir::separator() + "qutesheet.py");
  module.open(QFile::WriteOnly | QIODevice::Text);
  QFile file(":/python/qutesheet.py");
  file.open(QIODevice::ReadOnly);
  QTextStream moduleStream(&module);
  moduleStream << file.readAll();
  file.close();
  module.close();
  qDebug() << module.fileName();

  QFile script(tempDir.absolutePath() + QDir::separator() + name.mid(name.lastIndexOf("/") + 1));
  script.open(QFile::WriteOnly | QIODevice::Text);
  QFile file2(name);
  file2.open(QIODevice::ReadOnly);
  QTextStream scriptStream(&script);
  scriptStream << file2.readAll();
  file2.close();
  script.close();

  QFile dataFile(tempDir.absolutePath() + QDir::separator() + "qutesheet_data.py");
  dataFile.open(QFile::WriteOnly | QIODevice::Text);
  QTextStream dataStream(&dataFile);
  dataStream << generateDataText();
  dataFile.close();

  QFile outFile(tempDir.absolutePath() + QDir::separator() + outFileName);

  QProcess p;
  p.start("python " + name.mid(name.lastIndexOf("/") + 1));

  if (!p.waitForFinished (30000)) {
    qDebug() << "EventSheet::runScript Script took too long!! Current max is 30 secs.";
  }
  QByteArray sout = p.readAllStandardOutput();
  QByteArray serr = p.readAllStandardError();
  qDebug() << "---------------\n" << serr;
  QDir::setCurrent(oldDir.absolutePath());
  if (p.exitCode() != 0) {
    QMessageBox::critical(this, name.mid(name.lastIndexOf("/") + 1) ,
                         QString(serr),
                         QMessageBox::Ok);
  }
  else {
    qDebug() << sout;
//    QMessageBox::information(this, name.mid(name.lastIndexOf("/") + 1) ,
//                         QString(stdout),
//                         QMessageBox::Ok);
    outFile.open(QIODevice::ReadWrite);
    QString text = outFile.readAll();
    QStringList lines = text.split("\n");
    if (lines.size() > 0 && lines[0].startsWith("__@ ")) {
      QStringList position = lines[0].split(" ");
      position.pop_front();
      lines.pop_front();
      QString pasteText = lines.join("\n");
      pasteText.chop(1);
      setFromText(pasteText, position[0].toInt(), position[1].toInt(),
                  position[2].toInt(), position[3].toInt(), true);
      markHistory();
    }
    else {
      qDebug() << "EventSheet::runScript invalid out file format";
    }
  }
  module.remove();
  script.remove();
  dataFile.remove();
  outFile.remove();
  tempDir.rmpath(tempDir.absolutePath());
}

void EventSheet::insertColumnHere()
{
  // TODO implement
}

void EventSheet::insertRowHere()
{
  // TODO implement
}

void EventSheet::appendColumn()
{
  this->insertColumn(this->columnCount());
  columnNames << QString("p%1").arg(this->columnCount() - 1);
  this->setHorizontalHeaderLabels(columnNames);
  this->setColumnWidth(this->columnCount() - 1, 50);
}

void EventSheet::appendRow()
{
//  qDebug() << "EventSheet::appendRow()";
  this->insertRow(this->rowCount());

}

void EventSheet::deleteColumn()
{
  // TODO: remove multiple columns
  this->removeColumn(this->columnCount() - 1);
  columnNames.takeLast();
}

void EventSheet::deleteRows()
{
  // TODO: remove multiple rows
  QModelIndexList list = this->selectedIndexes();
  QList<int> selectedRows;
  for (int i = 0; i < list.size(); i++) {
    if (!selectedRows.contains(list[i].row()) ) {
      selectedRows.append(list[i].row());
    }
  }
  qSort(selectedRows);
  for (int i = selectedRows.size() - 1; i >=0; i--) {
    this->removeRow(selectedRows[i]);
  }
}

void EventSheet::contextMenuEvent (QContextMenuEvent * event)
{
//  qDebug() << "EventSheet::contextMenuEvent";

  QMenu menu;
  menu.addAction(sendEventsAct);
  menu.addAction(sendEventsOffsetAct);
  menu.addAction(loopEventsAct);
  menu.addAction(stopAllEventsAct);
  menu.addSeparator();
  menu.addAction(subtractAct);
  menu.addAction(addAct);
  menu.addAction(multiplyAct);
  menu.addAction(divideAct);
  menu.addAction(randomizeAct);
  menu.addAction(reverseAct);
  menu.addAction(shuffleAct);
//  menu.addAction(mirrorAct);
  menu.addAction(rotateAct);
  menu.addAction(fillAct);
  menu.addSeparator();
  QMenu *scriptMenu = menu.addMenu(tr("Python Scripts"));
  QMenu *converterMenu = scriptMenu->addMenu(tr("Conversions"));
  for (int i = 0; i < converterScripts.size(); i++) {
    QAction *a = converterMenu->addAction(converterScripts[i].mid(converterScripts[i].lastIndexOf("/") + 1),
                                       this, SLOT(runScript() ));
    a->setData(converterScripts[i]);
  }
  QMenu *testMenu = scriptMenu->addMenu(tr("Tests"));
  for (int i = 0; i < testScripts.size(); i++) {
    QAction *a = testMenu->addAction(testScripts[i].mid(testScripts[i].lastIndexOf("/") + 1),
                                       this, SLOT(runScript() ));
    a->setData(testScripts[i]);
  }
  for (int i = 0; i < builtinScripts.size(); i++) {
    QAction *a = scriptMenu->addAction(builtinScripts[i].mid(builtinScripts[i].lastIndexOf("/") + 1),
                                       this, SLOT(runScript() ));
    a->setData(builtinScripts[i]);
  }
  scriptMenu->addSeparator();
  addDirectoryToMenu(scriptMenu, scriptDir);
  menu.addSeparator();
//  menu.addAction(insertColumnHereAct);
//  menu.addAction(insertRowHereAct);
  menu.addAction(appendColumnAct);
  menu.addAction(appendRowAct);
  menu.addAction(deleteColumnAct);
  menu.addAction(deleteRowAct);
  menu.exec(event->globalPos());
}

void EventSheet::addDirectoryToMenu(QMenu *m, QString dir, int depth)
{
  if (depth > 4)
    return;
  QDir d(dir);
  QStringList filters;
  filters << "*.py";
  d.setNameFilters(filters);
  QStringList scripts = d.entryList(QDir::Files,QDir::Name);
  QStringList directories = d.entryList(QDir::AllDirs,QDir::Name);
  if (scripts.size() > 0 || directories.size() > 2) {
    QMenu *newMenu = m;
    if (dir != scriptDir)
      newMenu = m->addMenu(dir.mid(dir.lastIndexOf("/")+ 1));
    for (int i = 0; i < directories.size(); i++) {
      if (directories[i] != "." && directories[i] != "..")
        addDirectoryToMenu(newMenu, dir + "/" + directories[i], depth++);
    }
    for (int i = 0; i < scripts.size(); i++) {
       QAction *a = newMenu->addAction(scripts[i], this, SLOT(runScript() ));
       a->setData(dir + "/" + scripts[i]);
    }
  }
}

void EventSheet::keyPressEvent (QKeyEvent * event) {
  if (event->matches(QKeySequence::Delete)) {
    this->del();
  }
  else if (event->matches(QKeySequence::Cut)) {
    this->cut();
  }
  else if (event->matches(QKeySequence::Copy)) {
    this->copy();
  }
  else if (event->matches(QKeySequence::Paste)) {
    this->paste();
  }
  else if (event->matches(QKeySequence::Undo)
    || (event->key() == Qt::Key_Z && event->modifiers() == Qt::ControlModifier) ){
    this->undo();
  }
  else if (event->matches(QKeySequence::Redo)
    || (event->key() == Qt::Key_Z
        && event->modifiers() == (Qt::ControlModifier | Qt::ShiftModifier)) ) {
    this->redo();
  }
  else if (event->matches(QKeySequence::InsertLineSeparator)) {
    this->sendEvents();
  }
  else {
//    qDebug() << "EventSheet::keyPressEvent  " << event->key();
//    event->ignore();
    QTableWidget::keyPressEvent(event);  // Propagate any other events
  }
}

void EventSheet::add(double value)
{
  QModelIndexList list = this->selectedIndexes();
  for (int i = 0; i < list.size(); i++) {
    QTableWidgetItem * item = this->item(list[i].row(), list[i].column());
    if (item != 0 && item->data(Qt::DisplayRole).canConvert(QVariant::Double)) {
      bool ok = false;
      double n = item->data(Qt::DisplayRole).toDouble(&ok);
      if (ok) {
        item->setData(Qt::DisplayRole,
                      QVariant(n + value));
      }
    }
  }
  markHistory();
}

void EventSheet::multiply(double value)
{
  QModelIndexList list = this->selectedIndexes();
  for (int i = 0; i < list.size(); i++) {
    QTableWidgetItem * item = this->item(list[i].row(), list[i].column());
    if (item != 0 && item->data(Qt::DisplayRole).canConvert(QVariant::Double)) {
      bool ok = false;
      double n = item->data(Qt::DisplayRole).toDouble(&ok);
      if (ok) {
        item->setData(Qt::DisplayRole,
                      QVariant(n * value));
      }
    }
  }
  markHistory();
}

void EventSheet::divide(double value)
{
  QModelIndexList list = this->selectedIndexes();
  for (int i = 0; i < list.size(); i++) {
    QTableWidgetItem * item = this->item(list[i].row(), list[i].column());
    if (item != 0 && item->data(Qt::DisplayRole).canConvert(QVariant::Double)) {
      bool ok = false;
      double n = item->data(Qt::DisplayRole).toDouble(&ok);
      if (ok) {
        item->setData(Qt::DisplayRole,
                      QVariant(n / value));
      }
    }
  }
  markHistory();
}

void EventSheet::randomize(double min, double max, int mode)
{
  // Mode 0 =
  // Mode 1 = integers only
  QModelIndexList list = this->selectedIndexes();
  QTime midnight(0, 0, 0);
  qsrand(midnight.secsTo(QTime::currentTime()));
  for (int i = 0; i < list.size(); i++) {
    QTableWidgetItem * item = this->item(list[i].row(), list[i].column());
    if (item == 0) {
      item = new QTableWidgetItem();
      this->setItem(list[i].row(), list[i].column(), item);
    }
    double value = 0.0;
    if (mode == 0) {
      value = min + ((double) qrand() / (double) RAND_MAX) * (max - min);
    }
    else /*if (mode == 1)*/ {  // Integers only
      value = min + (qrand() % (int) (max - min + 1)); // Include max value as a possibility
    }
    item->setData(Qt::DisplayRole,
                  QVariant(value));
  }
  markHistory();
}

void EventSheet::shuffle(int iterations)
{
  QTime midnight(0, 0, 0);
  qsrand(midnight.secsTo(QTime::currentTime()));
  QModelIndexList list = this->selectedIndexes();
  if (list.size() < 3)
    return;
  for (int i = 0; i < iterations; i++) { // First traverse to copy values
    int num1 = qrand() % list.size();
    QTableWidgetItem * item1 = this->item(list[num1].row(), list[num1].column());
    int num2 = qrand() % list.size();
    while (num2 == num1) {
      num2 = qrand() % list.size();
    }
    QTableWidgetItem * item2 = this->item(list[num2].row(), list[num2].column());
    QVariant value1 = QVariant();
    if (item1 != 0) {
      value1 = item1->data(Qt::DisplayRole);
    }
    else {
      item1 = new QTableWidgetItem();
      this->setItem(list[i].row(), list[i].column(), item1);
    }
    if (item2 == 0) {
      item2 = new QTableWidgetItem();
      this->setItem(list[i].row(), list[i].column(), item2);
    }
    item1->setData(Qt::DisplayRole,item2->data(Qt::DisplayRole));
    item2->setData(Qt::DisplayRole,value1);
  }
  markHistory();
}

void EventSheet::rotate(int amount)
{
  QModelIndexList list = this->selectedIndexes();
  QList<QVariant> oldValues;
  for (int i = 0; i < list.size(); i++) { // First traverse to copy values
    QTableWidgetItem * item = this->item(list[i].row(), list[i].column());
    if (item != 0) {
      oldValues.append(item->data(Qt::DisplayRole));
    }
    else {
      oldValues.append(QVariant());
    }
  }
  for (int i = 0; i < list.size(); i++) { // Then put in rotated values
    QTableWidgetItem * item = this->item(list[i].row(), list[i].column());
    if (item == 0) {
      item = new QTableWidgetItem();
      this->setItem(list[i].row(), list[i].column(), item);
    }
    int index = (i - amount + list.size())%list.size();
    item->setData(Qt::DisplayRole,oldValues[index]);
  }
  markHistory();
}

void EventSheet::fill(double start, double end, double slope)
{
  QModelIndexList list = this->selectedIndexes();
  double inc = (end - start) / (list.size() - 1.0);
  double value = start;
//  double listSize = (double) list.size();
  for (int i = 0; i < list.size(); i++) {
    QTableWidgetItem * item = this->item(list[i].row(), list[i].column());
    if (item == 0) {
      item = new QTableWidgetItem();
      this->setItem(list[i].row(), list[i].column(), item);
    }
    item->setData(Qt::DisplayRole,
                  QVariant(value));
    if (slope == 1.0) {
      value += inc;
    }
    else if (slope < 1.0 && slope >= 0.0) {
      value = start;
      value += (end-start) * (exp(((i + 1.0) / (list.size() - 1.0)) * log(slope))-1.0) / (slope-1.0);
    }
    else if (slope > 1.0) {
      value = start;
      value += (end-start) * (exp(((i + 1.0) / (list.size() - 1.0)) * log(slope))-1.0) / (slope-1.0);
    }
  }
  markHistory();
}

void EventSheet::createActions()
{
  // For some reason, the shortcuts set here have no effect and need to be
  // decoded in keyPressEvent... (at least for linux)
//  cutAct = new QAction(/*QIcon(":/a.png"),*/ tr("Cut"), this);
////  loopEventsAct->setIconText(tr("Loop Events"));
//  cutAct->setShortcut(QKeySequence(QKeySequence::Cut));
//  connect(cutAct, SIGNAL(triggered()), this, SLOT(cut()));
//
//  copyAct = new QAction(/*QIcon(":/a.png"),*/ tr("Copy"), this);
////  loopEventsAct->setIconText(tr("Loop Events"));
//  copyAct->setShortcut(QKeySequence(QKeySequence::Copy));
//  connect(copyAct, SIGNAL(triggered()), this, SLOT(copy()));
//
//  pasteAct = new QAction(/*QIcon(":/a.png"),*/ tr("Paste"), this);
////  loopEventsAct->setIconText(tr("Loop Events"));
//  pasteAct->setShortcut(QKeySequence(QKeySequence::Paste));
//  connect(pasteAct, SIGNAL(triggered()), this, SLOT(paste()));

  sendEventsAct = new QAction(/*QIcon(":/a.png"),*/ tr("&Send Events"), this);
  sendEventsAct->setStatusTip(tr("Send Events to Csound"));
  sendEventsAct->setIconText(tr("Send Events"));
  sendEventsAct->setShortcut(QKeySequence(QKeySequence::InsertLineSeparator));
  connect(sendEventsAct, SIGNAL(triggered()), this, SLOT(sendEvents()));

  sendEventsOffsetAct = new QAction(/*QIcon(":/a.png"),*/ tr("&Send Events without offset"), this);
  sendEventsOffsetAct->setStatusTip(tr("Send Events to Csound without offset"));
  sendEventsOffsetAct->setIconText(tr("Send Events no offset"));
  connect(sendEventsOffsetAct, SIGNAL(triggered()), this, SLOT(sendEventsOffset()));

  loopEventsAct = new QAction(/*QIcon(":/a.png"),*/ tr("&Loop Events"), this);
  loopEventsAct->setStatusTip(tr("Loop Events to Csound"));
  loopEventsAct->setIconText(tr("Loop Events"));
  connect(loopEventsAct, SIGNAL(triggered()), this, SLOT(loopEvents()));

  stopAllEventsAct = new QAction(/*QIcon(":/a.png"),*/ tr("&Stop Events"), this);
  stopAllEventsAct->setStatusTip(tr("Stop all running and pending events"));
  stopAllEventsAct->setIconText(tr("Stop Events"));
  connect(stopAllEventsAct, SIGNAL(triggered()), this, SLOT(stopAllEvents()));

  subtractAct = new QAction(/*QIcon(":/a.png"),*/ tr("&Subtract"), this);
  subtractAct->setStatusTip(tr("Subtract a value from the selected cells"));
  subtractAct->setIconText(tr("Subtract"));
  connect(subtractAct, SIGNAL(triggered()), this, SLOT(subtract()));

  addAct = new QAction(/*QIcon(":/a.png"),*/ tr("&Add"), this);
  addAct->setStatusTip(tr("Add a value to the selected cells"));
  addAct->setIconText(tr("Add"));
  connect(addAct, SIGNAL(triggered()), this, SLOT(add()));

  multiplyAct = new QAction(/*QIcon(":/a.png"),*/ tr("&Multiply"), this);
  multiplyAct->setStatusTip(tr("Multiply the selected cells by a value"));
  multiplyAct->setIconText(tr("Multiply"));
  connect(multiplyAct, SIGNAL(triggered()), this, SLOT(multiply()));

  divideAct = new QAction(/*QIcon(":/a.png"),*/ tr("&Divide"), this);
  divideAct->setStatusTip(tr("Divide the selected cells by a value"));
  divideAct->setIconText(tr("Divide"));
  connect(divideAct, SIGNAL(triggered()), this, SLOT(divide()));

  randomizeAct = new QAction(/*QIcon(":/a.png"),*/ tr("&Randomize"), this);
  randomizeAct->setStatusTip(tr("Randomize the selected cells"));
  randomizeAct->setIconText(tr("Randomize"));
  connect(randomizeAct, SIGNAL(triggered()), this, SLOT(randomize()));

  reverseAct = new QAction(/*QIcon(":/a.png"),*/ tr("&Reverse"), this);
  reverseAct->setStatusTip(tr("Reverse the selected cells by column"));
  reverseAct->setIconText(tr("Reverse"));
  connect(reverseAct, SIGNAL(triggered()), this, SLOT(reverse()));

  shuffleAct = new QAction(/*QIcon(":/a.png"),*/ tr("&Shuffle"), this);
  shuffleAct->setStatusTip(tr("Shuffle the selected cells"));
  shuffleAct->setIconText(tr("Shuffle"));
  connect(shuffleAct, SIGNAL(triggered()), this, SLOT(shuffle()));

//  mirrorAct = new QAction(/*QIcon(":/a.png"),*/ tr("&Mirror"), this);
//  mirrorAct->setStatusTip(tr("Mirror the selected cells"));
//  mirrorAct->setIconText(tr("Mirror"));
//  connect(mirrorAct, SIGNAL(triggered()), this, SLOT(mirror()));

  rotateAct = new QAction(/*QIcon(":/a.png"),*/ tr("&Rotate"), this);
  rotateAct->setStatusTip(tr("Rotate the selected cells"));
  rotateAct->setIconText(tr("Rotate"));
  connect(rotateAct, SIGNAL(triggered()), this, SLOT(rotate()));

  fillAct = new QAction(/*QIcon(":/a.png"),*/ tr("&Fill Cells"), this);
  fillAct->setStatusTip(tr("Fill selected cells"));
  fillAct->setIconText(tr("Fill"));
  connect(fillAct, SIGNAL(triggered()), this, SLOT(fill()));

  insertColumnHereAct = new QAction(/*QIcon(":/a.png"),*/ tr("&Insert Column"), this);
  insertColumnHereAct->setStatusTip(tr("Insert a column at the current position"));
  insertColumnHereAct->setIconText(tr("Insert Column"));
  connect(insertColumnHereAct, SIGNAL(triggered()), this, SLOT(insertColumnHere()));

  insertRowHereAct = new QAction(/*QIcon(":/a.png"),*/ tr("&Insert Row"), this);
  insertRowHereAct->setStatusTip(tr("Insert a row at the current position"));
  insertRowHereAct->setIconText(tr("Insert Row"));
  connect(insertRowHereAct, SIGNAL(triggered()), this, SLOT(insertRowHere()));


  appendColumnAct = new QAction(/*QIcon(":/a.png"),*/ tr("Append Column"), this);
  appendColumnAct->setStatusTip(tr("Append a column to the sheet"));
  appendColumnAct->setIconText(tr("Append Column"));
  connect(appendColumnAct, SIGNAL(triggered()), this, SLOT(appendColumn()));

  appendRowAct = new QAction(/*QIcon(":/a.png"),*/ tr("&Append Row"), this);
  appendRowAct->setStatusTip(tr("Append a row to the sheet"));
  appendRowAct->setIconText(tr("Append Row"));
  connect(appendRowAct, SIGNAL(triggered()), this, SLOT(appendRow()));

  deleteColumnAct = new QAction(/*QIcon(":/a.png"),*/ tr("Delete Last Column"), this);
  deleteColumnAct->setStatusTip(tr("Delete Last Column"));
  deleteColumnAct->setIconText(tr("Delete Last Column"));
  connect(deleteColumnAct, SIGNAL(triggered()), this, SLOT(deleteColumn()));

  deleteRowAct = new QAction(/*QIcon(":/a.png"),*/ tr("Delete Selected Rows"), this);
  deleteRowAct->setStatusTip(tr("Delete Rows"));
  deleteRowAct->setIconText(tr("Delete Rows"));
  connect(deleteRowAct, SIGNAL(triggered()), this, SLOT(deleteRows()));
}

QList<QPair<QString, QString> > EventSheet::parseLine(QString line)
{
  QList<QPair<QString, QString> > list;
  QPair<QString, QString> field;

  int count = 0;
  int pcount = 0;
  bool formula = false;
  bool string = false;
  bool isp = true; // Assume starting on a pfield, not white space
  QString pvalue = "";
  QString spacing = "";
  if (!line.isEmpty() && (line[0] == 'i' || line[0] == 'f') ) { // A space is not necessary between these and the first p-field
    field.first = QString(line[0]);
    field.second = QString();
    count++;
    isp = false; // consider p-field done with first character.
  }
  while (count < line.size()) {  // More characters left
    if (isp == true || formula || string) { // Processing p-field
      if (line[count] == '"') { // string takes precedence over formulas and comments
        string = !string;
      }
      else if (line[count] == '[') { //Start of formula
        formula = true;  // This should never happen as this character should always be after whitespace....
      }
      else if (line[count] == ']') { //End of formula
        formula = false;
      }
      else if (line[count] == ';') { // comment
        if (count > 0) {
        // First add current pfield, in case there is previous data
          field.first = pvalue;
          field.second = spacing;
          list.append(field);
          pcount++;
        }
        // Now add comment
        count++;
        QString comment = line.mid(count);
        QStringList parts = comment.split(";");
        for (int i = 0; i < parts.size(); i++) {
          if (i==0) {  // Only put the ; character visible on the first column
            field.first = ";" + parts[i];
          }
          else {
            field.first = parts[i];
          }
          field.second = "";
          list.append(field);
          pcount++;
        }
        isp = false;  // last p-field has been processed here
        break; // Nothing more todo for this line
      }  // End of comment processing
      // ----
      if (line[count].isSpace() && !formula && !string) { // White space so p-field has finished
        spacing = line[count];
        isp = false;
      }
      else { // A character or formula so continue p-field processing
        pvalue.append(line[count]);
        field.first = pvalue;
      }
    }
    else { // Processing white space
      if (line[count] == '"') { // string
        string = !string;
      }
      else if (line[count] == '[') { //Start of formula
        formula = true;
      }
      else if (line[count] == ';') { // comment
//        field.first = pvalue;
//        field.second = spacing;
//        list.append(field);  //Should only append when pcount is incremented
//        pcount++;
        count++;
        QString comment = line.mid(count);
        QStringList parts = comment.split(";");
        for (int i = 0; i < parts.size(); i++) {
          if (i==0) {  // Only put the ; character visible on the first column
            field.first = ";" + parts[i];
          }
          else {
            field.first = parts[i];
          }
          field.second = "";
          list.append(field);
          pcount++;
        }
        isp = false;  // last p-field has been processed here
        break; // Nothing more todo for this line
      }
      // ---
      if (!line[count].isSpace()) { // Not White space so new p-field has started
        field.second = spacing;
        list.append(field);  //Should only append when pcount is incremented
        pcount++;
        isp = true;
        pvalue = line[count];
        field.first = pvalue;
        spacing = "";
      }
      else { // Continue p-field processing
        spacing.append(line[count]);
      }
    }
    count++;
  }
  // Process final p-field
  if (isp == true) {
    field.first = pvalue;
    field.second = "";
    list.append(field);
  }
  return list;
}

void EventSheet::selectionChanged()
{
  QModelIndexList list = this->selectedIndexes();
  if (list.size() > 1) {
    this->setDragDropMode(QAbstractItemView::DragDrop); // Allow dragging items
  }
  else {
    this->setDragDropMode(QAbstractItemView::NoDragDrop); // Allow extending selection
  }
}

void EventSheet::cellDoubleClickedSlot(int row, int column)
{
  markHistory();
}

void EventSheet::cellChangedSlot(int row, int column)
{
  if (this->item(row, column) != 0 and this->item(row, column)->data(Qt::DisplayRole).toString() != 0) {
    while (column > 0) {
      column--;
      QTableWidgetItem * item = this->item(row, column);
//      qDebug() << "EventSheet::cellChangedSlot " << column;
      if (item == 0 or item->data(Qt::DisplayRole).toString() == "") {
        this->setItem(row, column, this->takeItem(row, column + 1));
      }
      else {
        break;
      }
    }
  }
  if (noHistoryChange == 0) {
    markHistory();
  }
  else {
    noHistoryChange = 0;
  }
  emit modified();
}
