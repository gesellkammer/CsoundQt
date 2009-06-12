/*
    Copyright (C) 2008, 2009 Andres Cabrera
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

#include "qutecsound.h"
#include "console.h"
#include "dockhelp.h"
#include "widgetpanel.h"
#include "opentryparser.h"
#include "options.h"
#include "highlighter.h"
#include "configdialog.h"
#include "configlists.h"
#include "documentpage.h"
#include "utilitiesdialog.h"
#include "findreplace.h"
#include "graphicwindow.h"
#include "keyboardshortcuts.h"

// Structs for csound graphs
#include <cwindow.h>
#include "curve.h"

#ifdef WIN32
static const QString SCRIPT_NAME = "qutecsound_run_script.bat";
#else
static const QString SCRIPT_NAME = "qutecsound_run_script.sh";
#endif

//csound performance thread function prototype
uintptr_t csThread(void *clientData);

//FIXME why does qutecsound not end when it receives a terminate signal?
qutecsound::qutecsound(QStringList fileNames)
{
  setWindowTitle("QuteCsound[*]");
  resize(660,350);
  setWindowIcon(QIcon(":/images/qtcs.png"));
  textEdit = NULL;
  QLocale::setDefault(QLocale::system());  //Does this take care of the decimal separator for different locales?
  // Initialize user data pointer passed to Csound
  ud = (CsoundUserData *)malloc(sizeof(CsoundUserData));
  ud->PERF_STATUS = 0;
  ud->qcs = this;
  pFields = (MYFLT *) calloc(EVENTS_MAX_PFIELDS, sizeof(MYFLT)); // Maximum number of p-fields for events

  curPage = -1;
  m_options = new Options();

  // Create GUI panels
  lineNumberLabel = new QLabel("Line 1"); // Line number display
  statusBar()->addPermanentWidget(lineNumberLabel); // This must be done before a file is loaded
  m_console = new DockConsole(this);
  m_console->setObjectName("m_console");
//   m_console->setAllowedAreas(Qt::RightDockWidgetArea | Qt::BottomDockWidgetArea);
  addDockWidget(Qt::BottomDockWidgetArea, m_console);
  helpPanel = new DockHelp(this);
  helpPanel->setAllowedAreas(Qt::RightDockWidgetArea | Qt::BottomDockWidgetArea | Qt::LeftDockWidgetArea);
  helpPanel->setObjectName("helpPanel");
  helpPanel->show();
  connect(helpPanel, SIGNAL(openManualExample(QString)), this, SLOT(openManualExample(QString)));
  addDockWidget(Qt::RightDockWidgetArea, helpPanel);

  // WidgetPanel must be created before createAcctions since it contains the editAct action
  widgetPanel = new WidgetPanel(this);
  widgetPanel->hide();
  widgetPanel->setAllowedAreas(Qt::RightDockWidgetArea | Qt::BottomDockWidgetArea |Qt::LeftDockWidgetArea);
  widgetPanel->setObjectName("widgetPanel");
  addDockWidget(Qt::RightDockWidgetArea, widgetPanel);
  utilitiesDialog = new UtilitiesDialog(this, m_options/*, _configlists*/);
  connect(utilitiesDialog, SIGNAL(runUtility(QString)), this, SLOT(runUtility(QString)));
//   connect(widgetPanel,SIGNAL(topLevelChanged(bool)), this, SLOT(widgetDockStateChanged(bool)));
//   connect(widgetPanel,SIGNAL(dockLocationChanged(Qt::DockWidgetArea)),
//           this, SLOT(widgetDockLocationChanged(Qt::DockWidgetArea)));

  createActions(); // Must be before readSettings as this sets the default shortcuts
  readSettings();

  bool widgetsVisible = !widgetPanel->isHidden(); // Must be after readSettings() to save last state
  if (widgetsVisible)
    widgetPanel->hide();  // Hide until QuteCsound has finished loading

  createMenus();
  createToolBars();
  createStatusBar();

  documentTabs = new QTabWidget (this);
  connect(documentTabs, SIGNAL(currentChanged(int)), this, SLOT(changePage(int)));
  setCentralWidget(documentTabs);
  closeTabButton = new QToolButton(documentTabs);
  closeTabButton->setDefaultAction(closeTabAct);
  documentTabs->setCornerWidget(closeTabButton);
  modIcon.addFile(":/images/modIcon2.png", QSize(), QIcon::Normal);
  modIcon.addFile(":/images/modIcon.png", QSize(), QIcon::Disabled);

  fillFileMenu(); //Must be placed after readSettings to include recent Files
  if (m_options->opcodexmldir == "") {
    opcodeTree = new OpEntryParser(":/opcodes.xml");
  }
  else
    opcodeTree = new OpEntryParser(QString(m_options->opcodexmldir + "/opcodes.xml"));
  m_highlighter = new Highlighter();
  configureHighlighter();

  // Open files saved from last session
  if (!lastFiles.isEmpty()) {
    foreach (QString lastFile, lastFiles) {
      if (lastFile!="" and !lastFile.startsWith("untitled")) {
        loadFile(lastFile);
      }
    }
  }
  // Open files passed in the command line. Only valid for non OS X platforms
  foreach (QString fileName, fileNames) {
    if (fileName!="") {
      loadFile(fileName, true);
    }
  }
  if (widgetsVisible) { // Reshow widget panel if necessary
    widgetPanel->show();
  }
  showWidgetsAct->setChecked(widgetsVisible);  // Button will initialize to current state of panel
  showConsoleAct->setChecked(!m_console->isHidden());  // Button will initialize to current state of panel
  showHelpAct->setChecked(!helpPanel->isHidden());  // Button will initialize to current state of panel

  if (documentPages.size() == 0) { // No files yet open. Open default
    newFile();
  }

  changeFont();

  helpPanel->docDir = m_options->csdocdir;
  QString index = m_options->csdocdir + QString("/index.html");
  helpPanel->loadFile(index);


  applySettings();

  csound = NULL;
  int init = csoundInitialize(0,0,0);
  if (init<0) {
    qDebug("Error initializing Csound!");
    QMessageBox::warning(this, tr("QuteCsound"),
                         tr("Error initializing Csound!\nQutecsound will probably crash if you try to run Csound."));
  }
  else if (init>0) {
    qDebug("Csound already initialized.");
  }

#ifndef QUTECSOUND_DESTROY_CSOUND
  // Create only once
  csound=csoundCreate(0);
#endif

  queueTimer = new QTimer(this);
  queueTimer->setSingleShot(true);
  connect(queueTimer, SIGNAL(timeout()), this, SLOT(dispatchQueues()));
  dispatchQueues(); //start queue dispatcher
}

qutecsound::~qutecsound()
{
  qDebug() << "qutecsound::~qutecsound()";
  // This function is not called... see closeEvent()
}

void qutecsound::messageCallback_NoThread(CSOUND *csound,
                                          int /*attr*/,
                                          const char *fmt,
                                          va_list args)
{
  CsoundUserData *ud = (CsoundUserData *) csoundGetHostData(csound);
  DockConsole *console = ud->qcs->m_console;
  QString msg;
  msg = msg.vsprintf(fmt, args);
  console->appendMessage(msg);
  ud->qcs->widgetPanel->appendMessage(msg);
  console->update();
}

void qutecsound::messageCallback_Thread(CSOUND *csound,
                                          int /*attr*/,
                                          const char *fmt,
                                          va_list args)
{
  CsoundUserData *ud = (CsoundUserData *) csoundGetHostData(csound);
  QString msg;
  msg = msg.vsprintf(fmt, args);
//   csoundLockMutex(ud->qcs->perfMutex);
  ud->qcs->queueMessage(msg);
//   csoundUnlockMutex(ud->qcs->perfMutex);
}

void qutecsound::messageCallback_Devices(CSOUND *csound,
                                         int /*attr*/,
                                         const char *fmt,
                                         va_list args)
{
  CsoundUserData *ud = (CsoundUserData *) csoundGetHostData(csound);
  QStringList *messages = &ud->qcs->m_deviceMessages;
  QString msg;
  msg = msg.vsprintf(fmt, args);
  messages->append(msg);
}

void qutecsound::changeFont()
{
  for (int i = 0; i < documentPages.size(); i++) {
    documentPages[i]->document()->setDefaultFont(QFont(m_options->font, (int) m_options->fontPointSize));
  }
  m_console->setDefaultFont(QFont(m_options->consoleFont,
                            (int) m_options->consoleFontPointSize));
  m_console->setColors(m_options->consoleFontColor, m_options->consoleBgColor);
//   widgetPanel->setConsoleFont()
}

void qutecsound::changePage(int index)
{
  stop();
  if (textEdit != NULL) {
    textEdit->setMacWidgetsText(widgetPanel->widgetsText()); //Updated changes to widgets in file
  }
  textEdit = documentPages[index];
  textEdit->setTabStopWidth(m_options->tabWidth);
  textEdit->setLineWrapMode(m_options->wrapLines ? QTextEdit::WidgetWidth : QTextEdit::NoWrap);
  m_highlighter->setColorVariables(m_options->colorVariables);
  m_highlighter->setDocument(textEdit->document());
  curPage = index;
  setCurrentFile(documentPages[curPage]->fileName);
  connectActions();
  setWidgetPanelGeometry();
}

void qutecsound::updateWidgets()
{
  widgetPanel->loadWidgets(textEdit->getMacWidgetsText());
  widgetPanel->showTooltips(m_options->showTooltips);
}

void qutecsound::openExample()
{
  QObject *sender = QObject::sender();
  if (sender == 0)
    return;
  QAction *action = static_cast<QAction *>(sender);
  loadFile(action->data().toString());
//   saveAs();
}

void qutecsound::closeEvent(QCloseEvent *event)
{
  stop();
#ifndef QUTECSOUND_DESTROY_CSOUND
  csoundDestroy(csound);
#endif
  if (maybeSave()) {
    writeSettings();
    foreach (QString tempFile, tempScriptFiles) {
      QDir().remove(tempFile);
    }
    free(ud);
    delete closeTabButton;
    close();
    free(pFields);
    event->accept();
  } else {
    event->ignore();
  }
}

void qutecsound::newFile()
{
  if (m_options->defaultCsdActive && m_options->defaultCsd.endsWith(".csd")) {
    loadFile(m_options->defaultCsd);
  }
  else {
    loadFile(":/default.csd");
  }
  documentPages[curPage]->fileName = "";
  setWindowModified(false);
  documentTabs->setTabIcon(curPage, modIcon);
  documentTabs->setTabText(curPage, "default.csd");
//   documentPages[curPage]->setTabStopWidth(m_options->tabWidth);
  connectActions();
}

void qutecsound::open()
{
  QString fileName = "";
  bool widgetsVisible = widgetPanel->isVisible();
  if (widgetsVisible)
    widgetPanel->hide(); // Necessary for Mac, as widget Panel covers open dialog
  fileName = QFileDialog::getOpenFileName(this, tr("Open File"), lastUsedDir , tr("Csound Files (*.csd *.orc *.sco);;All Files (*)"));
  if (widgetsVisible)
    widgetPanel->show();
  int index = isOpen(fileName);
  if (index != -1) {
    documentTabs->setCurrentIndex(index);
    changePage(index);
      statusBar()->showMessage(tr("File already open"), 10000);
    return;
  }
  if (!fileName.isEmpty()) {
    loadCompanionFile(fileName);
    loadFile(fileName, true);
  }
}

void qutecsound::reload()
{
  if (documentPages[curPage]->document()->isModified()) {
    QString fileName = documentPages[curPage]->fileName;
    documentPages.remove(curPage);
    documentTabs->removeTab(curPage);
    loadFile(fileName);
  }
}

void qutecsound::openRecent0()
{
  if (maybeSave()) {
    QString fileName = recentFiles[0];
    if (!fileName.isEmpty()) {
      loadCompanionFile(fileName);
      loadFile(fileName);
    }
  }
}

void qutecsound::openRecent1()
{
  if (maybeSave()) {
    QString fileName = recentFiles[1];
    if (!fileName.isEmpty()) {
      loadCompanionFile(fileName);
      loadFile(fileName);
    }
  }
}

void qutecsound::openRecent2()
{
  if (maybeSave()) {
    QString fileName = recentFiles[2];
    if (!fileName.isEmpty()) {
      loadCompanionFile(fileName);
      loadFile(fileName);
    }
  }
}

void qutecsound::openRecent3()
{
  if (maybeSave()) {
    QString fileName = recentFiles[3];
    if (!fileName.isEmpty()) {
      loadCompanionFile(fileName);
      loadFile(fileName);
    }
  }
}

void qutecsound::openRecent4()
{
  if (maybeSave()) {
    QString fileName = recentFiles[4];
    if (!fileName.isEmpty()) {
      loadCompanionFile(fileName);
      loadFile(fileName);
    }
  }
}

void qutecsound::openRecent5()
{
  if (maybeSave()) {
    QString fileName = recentFiles[5];
    if (!fileName.isEmpty()) {
      loadCompanionFile(fileName);
      loadFile(fileName);
    }
  }
}

void qutecsound::createGraph()
{
  QString command = m_options->dot + " -V";
  int ret = system(command.toStdString().c_str());
  if (ret != 0) {
    QMessageBox::warning(this, tr("QuteCsound"),
                         tr("Dot executable not found.\n"
                            "Please install graphviz from\n"
                            "www.graphviz.org"));
    return;
  }
  QString dotText = documentPages[curPage]->getDotText();
//   qDebug() << dotText;
  QTemporaryFile file(QDir::tempPath() + "/" + "QuteCsound-GraphXXXXXX.dot");
  QTemporaryFile pngFile(QDir::tempPath() + "/" + "QuteCsound-GraphXXXXXX.png");
  if (!file.open() || !pngFile.open()) {
    QMessageBox::warning(this, tr("QuteCsound"),
                         tr("Cannot create temp dot/png file."));
    return;
  }
  QTextStream out(&file);
  out << dotText;
  file.close();
  file.open();
  command = "\"" + m_options->dot + "\" -Tpng -o " + pngFile.fileName() + " " + file.fileName();
//   qDebug() << command;
  system(command.toStdString().c_str());
  m_graphic = new GraphicWindow(this);
  m_graphic->show();
  m_graphic->openPng(pngFile.fileName());
  connect(m_graphic, SIGNAL(destroyed()), this, SLOT(closeGraph()));
}

void qutecsound::closeGraph()
{
  qDebug("qutecsound::closeGraph()");
}

bool qutecsound::save()
{
  if (documentPages[curPage]->fileName.isEmpty() or documentPages[curPage]->fileName.startsWith(":/examples/")) {
    return saveAs();
  }
  else if (documentPages[curPage]->readOnly){
    if (saveAs()) {
      documentPages[curPage]->readOnly = false;
      return true;
    }
    else {
      return false;
    }
  }
  else {
    return saveFile(documentPages[curPage]->fileName);
  }
}

void qutecsound::copy()
{
  if (documentPages[curPage]->hasFocus()) {
#ifdef QUTECSOUND_COPYPASTE
    m_clipboard = documentPages[curPage]->textCursor();
    m_clipboardText = m_clipboard.selectedText();
    QApplication::clipboard()->setText(m_clipboardText);
#else
    documentPages[curPage]->copy();
#endif
  }
  else if (helpPanel->hasFocus()) {
    helpPanel->copy();
  }
  else if (m_console->widgetHasFocus()) {
    m_console->copy();
  }
  else
    widgetPanel->copy();
}

void qutecsound::cut()
{
  if (documentPages[curPage]->hasFocus()) {
#ifdef QUTECSOUND_COPYPASTE
    qDebug() << "aweasf";
    m_clipboard = documentPages[curPage]->textCursor();
    m_clipboardText = m_clipboard.selectedText();
    QApplication::clipboard()->setText(m_clipboardText);
    documentPages[curPage]->insertPlainText("");
#else
    documentPages[curPage]->cut();
#endif
  }
  else
    widgetPanel->cut();
}

void qutecsound::paste()
{
  if (documentPages[curPage]->hasFocus()) {
#ifdef QUTECSOUND_COPYPASTE
    documentPages[curPage]->insertPlainText(m_clipboardText);
#else
    documentPages[curPage]->paste();
#endif
  }
  else
    widgetPanel->paste();
}

void qutecsound::undo()
{
  if (documentPages[curPage]->hasFocus()) {
    documentPages[curPage]->undo();
  }
  else
    widgetPanel->undo();
}

void qutecsound::redo()
{
  if (documentPages[curPage]->hasFocus()) {
    documentPages[curPage]->redo();
  }
  else
    widgetPanel->redo();
}

void qutecsound::controlD()
{
  if (documentPages[curPage]->hasFocus()) {
    documentPages[curPage]->comment();
  }
  else
    widgetPanel->duplicate();
}

void qutecsound::del()
{
  //FIXME finish this...
  if (documentPages[curPage]->hasFocus()) {
//     documentPages[curPage]->comment();
  }
  else
    widgetPanel->deleteSelected();
}

bool qutecsound::saveAs()
{
  bool widgetsVisible = widgetPanel->isVisible();
  if (widgetsVisible)
    widgetPanel->hide(); // Necessary for Mac, as widget Panel covers open dialog
  QString fileName = QFileDialog::getSaveFileName(this, tr("Save File As"), lastUsedDir , tr("Csound Files (*.csd *.orc *.sco)"));
  if (widgetsVisible)
    widgetPanel->show(); // Necessary for Mac, as widget Panel covers open dialog
  if (fileName.isEmpty())
    return false;
  if (isOpen(fileName) != -1) {
    QMessageBox::critical(this, tr("QuteCsound"),
                tr("The file is already open in another tab.\nFile not saved!"),
                  QMessageBox::Ok | QMessageBox::Default);
    return false;
  }
  if (!fileName.endsWith(".csd") && !fileName.endsWith(".orc") && !fileName.endsWith(".sco"))
    fileName += ".csd";

  return saveFile(fileName);
}

bool qutecsound::closeTab()
{
//   qDebug("qutecsound::closeTab() curPage = %i documentPages.size()=%i", curPage, documentPages.size());
  if (documentPages[curPage]->document()->isModified()) {
    int ret = QMessageBox::warning(this, tr("QuteCsound"),
                                   tr("File has been modified.\nDo you want to save it?"),
                                      QMessageBox::Yes | QMessageBox::Default,
                                      QMessageBox::No,
                                      QMessageBox::Cancel);
    if (ret == QMessageBox::Cancel)
      return false;
    else if (ret == QMessageBox::Yes) {
      if (!save())
        return false;
    }
  }
  if (documentPages.size() <= 1) {
    if (QMessageBox::warning(this, tr("QuteCsound"),
        tr("Do you want to exit QuteCsound?"),
           QMessageBox::Yes | QMessageBox::Default,
           QMessageBox::No) == QMessageBox::Yes)
    {
      close();
      return false;
    }
    else {
      newFile();
      curPage = 0;
    }
  }
  documentPages.remove(curPage);
  documentTabs->removeTab(curPage);
//   if (curPage > 0) {
//     curPage--;
//   }
  documentTabs->setCurrentIndex(curPage);
  textEdit = documentPages[curPage];
//   textEdit->setTabStopWidth(m_options->tabWidth);
//   textEdit->setLineWrapMode(m_options->wrapLines ? QTextEdit::WidgetWidth : QTextEdit::NoWrap);
  setCurrentFile(documentPages[curPage]->fileName);
  m_highlighter->setColorVariables(m_options->colorVariables);
  m_highlighter->setDocument(documentPages[curPage]->document());
  connectActions();
  return true;
}

void qutecsound::print()
{
  QPrinter printer;
  QPrintDialog *dialog = new QPrintDialog(&printer, this);
  dialog->setWindowTitle(tr("Print Document"));
//   if (editor->textCursor().hasSelection())
//     dialog->addEnabledOption(QAbstractPrintDialog::PrintSelection);
  if (dialog->exec() != QDialog::Accepted)
    return;
  documentPages[curPage]->print(&printer);
}

void qutecsound::findReplace()
{
  FindReplace *dialog = new FindReplace(this, documentPages[curPage]);
  dialog->show();
}

void qutecsound::join()
{
  QDialog dialog(this);
  dialog.resize(700, 350);
  dialog.setModal(true);
  QPushButton *okButton = new QPushButton(tr("Ok"));
  QPushButton *cancelButton = new QPushButton(tr("Cancel"));

  connect(okButton, SIGNAL(released()), &dialog, SLOT(accept()));
  connect(cancelButton, SIGNAL(released()), &dialog, SLOT(reject()));

  QGridLayout *layout = new QGridLayout(&dialog);
  QListWidget *list1 = new QListWidget(&dialog);
  QListWidget *list2 = new QListWidget(&dialog);
  layout->addWidget(list1, 0, 0);
  layout->addWidget(list2, 0, 1);
  layout->addWidget(okButton, 1,0);
  layout->addWidget(cancelButton, 1,1);
//   layout->resize(400, 200);

  for (int i = 0; i < documentPages.size(); i++) {
    QString name = documentPages[i]->fileName;
    if (documentPages[i]->fileName.endsWith(".orc"))
      list1->addItem(documentPages[i]->fileName);
    else if (documentPages[i]->fileName.endsWith(".sco"))
      list2->addItem(documentPages[i]->fileName);
  }
  QList<QListWidgetItem *> itemList = list1->findItems(documentPages[curPage]->fileName,
      Qt::MatchExactly);
  if (itemList.size() > 0)
    list1->setCurrentItem(itemList[0]);
  QString name = documentPages[curPage]->fileName;
  QList<QListWidgetItem *> itemList2 = list2->findItems(name.replace(".orc", ".sco"),
      Qt::MatchExactly);
  if (itemList2.size() > 0)
    list2->setCurrentItem(itemList2[0]);
  if (itemList.size() == 0 or itemList.size() == 0) {
    QMessageBox::warning(this, tr("Join"),
                        tr("Please open the orc and sco files in QuteCsound first!"));
    return;
  }
  if (dialog.exec() == QDialog::Accepted) {
    QString orcText = "";
    QString scoText = "";
    for (int i = 0; i < documentPages.size(); i++) {
      QString name = documentPages[i]->fileName;
      if (name == list1->currentItem()->text())
        orcText = documentPages[i]->getFullText();
      else if (name == list2->currentItem()->text())
        scoText = documentPages[i]->getFullText();
    }
    QString text = "<CsoundSynthesizer>\n<CsOptions>\n</CsOptions>\n<CsInstruments>\n";
    text += orcText;
    text += "</CsInstruments>\n<CsScore>\n";
    text += scoText;
    text += "</CsScore>\n</CsoundSynthesizer>\n";
    newFile();
    documentPages[curPage]->setTextString(text, m_options->showWidgetsOnRun);
  }
//   else {
//     qDebug("qutecsound::join() : No Action");
//   }
}

void qutecsound::getToIn()
{
  documentPages[curPage]->getToIn();
}

void qutecsound::inToGet()
{
  documentPages[curPage]->inToGet();
}

void qutecsound::putCsladspaText()
{
  QString text = "<csLADSPA>\nName=";
  text += documentPages[curPage]->fileName.mid(documentPages[curPage]->fileName.lastIndexOf("/") + 1) + "\n";
  text += "Maker=QuteCsound\n";
  text += "UniqueID=69873\n";
  text += "Copyright=none\n";
  text += widgetPanel->getCsladspaLines();
  text += "</csLADSPA>";
  documentPages[curPage]->updateCsladspaText(text);
}

void qutecsound::exportCabbage()
{
  //TODO finish this
}

void qutecsound::runCsound(bool realtime)
{
  if (ud->PERF_STATUS == -1) { //Currently stopping, do nothing
    return;
  }
  else if (ud->PERF_STATUS == 1) { //If running, stop
    stop();
    return;
  }
  bool useAPI = true;
  if (QObject::sender() == renderAct) {
    useAPI = m_options->useAPI;
  }
  else if (QObject::sender() == runTermAct) {
    useAPI = false;
  }
  if (documentPages[curPage]->fileName.isEmpty()) {
    QMessageBox::warning(this, tr("QuteCsound"),
                         tr("This file has not been been saved\nPlease select name and location."));
    if (!saveAs()) {
      runAct->setChecked(false);
      return;
    }
  }
  else if (documentPages[curPage]->document()->isModified()) {
    if (m_options->saveChanges)
      if (!save()) {
        runAct->setChecked(false);
        return;
      }
  }
  //Go to directory of current file
  if (documentPages[curPage]->fileName.contains('/')) {
    m_options->csdPath =
        documentPages[curPage]->fileName.left(documentPages[curPage]->fileName.lastIndexOf('/'));
    QString command = "cd \"" + m_options->csdPath +"\"";
    QProcess::execute(command);
  }
  else {
    m_options->csdPath = "";
  }
  QString fileName, fileName2;
  fileName = documentPages[curPage]->fileName;
  if (!fileName.endsWith(".csd")) {
    if (documentPages[curPage]->askForFile)
      getCompanionFileName();
    if (fileName.endsWith(".sco")) {
      //Must switch filename order when open file is a sco file
      fileName2 = fileName;
      fileName = documentPages[curPage]->companionFile;
    }
    else
      fileName2 = documentPages[curPage]->companionFile;
  }

  widgetPanel->flush();
  widgetPanel->clearGraphs();
//   outValueQueue.clear();
  inValueQueue.clear();
  outStringQueue.clear();
  messageQueue.clear();

  if (useAPI) {
#ifdef MACOSX
//Remember menu bar to set it after FLTK grabs it
    menuBarHandle = GetMenuBar();
#endif
    m_console->clear();
    widgetPanel->clearGraphs();
    audioOutputBuffer.allZero();
    QTemporaryFile tempFile;
    if (fileName.startsWith(":/examples/")) {
      QString tmpFileName = QDir::tempPath();
      if (!tmpFileName.endsWith("/") and !tmpFileName.endsWith("\\")) {
        tmpFileName += QDir::separator();
      }
      tmpFileName += QString("QuteCsoundExample-XXXXXXXX.csd");
      tempFile.setFileTemplate(tmpFileName);
      if (!tempFile.open()) {
        QMessageBox::critical(this,
                              tr("QuteCsound"),
                                tr("Error creating temporary file."),
                                    QMessageBox::Ok);
        runAct->setChecked(false);
        return;
      }
      QString csdText = textEdit->document()->toPlainText();
      fileName = tempFile.fileName();
      tempFile.write(csdText.toAscii());
      tempFile.flush();
    }
    QTemporaryFile csdFile;
    QString tmpFileName = QDir::tempPath();
    if (!tmpFileName.endsWith("/") and !tmpFileName.endsWith("\\")) {
      tmpFileName += QDir::separator();
    }
    tmpFileName += QString("csound-tmpXXXXXXXX.csd");
    csdFile.setFileTemplate(tmpFileName);
    if (!csdFile.open()) {
      QMessageBox::critical(this,
                            tr("QuteCsound"),
                            tr("Error creating temporary file."),
                            QMessageBox::Ok);
      runAct->setChecked(false);
      return;
    }
    if (documentPages[curPage]->fileName.endsWith(".csd")) {
      QString csdText = textEdit->document()->toPlainText();
      QString fileName = csdFile.fileName();
      csdFile.write(csdText.toAscii());
      csdFile.flush();
    }
    char **argv;
    argv = (char **) calloc(33, sizeof(char*));
    // TODO use: PUBLIC int csoundSetGlobalEnv(const char *name, const char *value);
    int argc = m_options->generateCmdLine(argv, realtime, fileName, fileName2);
#ifdef QUTECSOUND_DESTROY_CSOUND
    csound=csoundCreate(0);
#endif

    // Callbacks must be set before compile, otherwise some information is missed
    if (m_options->thread) {
      csoundSetMessageCallback(csound, &qutecsound::messageCallback_Thread);
#ifdef QUTE_USE_CSOUNDPERFORMANCETHREAD
//       qDebug("Create CsoundPerformanceThread");
      perfThread = new CsoundPerformanceThread(csound);
      perfThread->SetProcessCallback(qutecsound::csThread, (void*)ud);
#endif
    }
    else {
      csoundSetMessageCallback(csound, &qutecsound::messageCallback_NoThread);
    }

    csoundReset(csound); // This is what causes the clipboard problems in Windows... why????
    csoundSetHostData(csound, (void *) ud);
    csoundPreCompile(csound);  //Need to run PreCompile to create the FLTK_Flags global variable

    int variable = csoundCreateGlobalVariable(csound, "FLTK_Flags", sizeof(int));
    if (m_options->enableFLTK or !useAPI) {
      // disable FLTK graphs, but allow FLTK widgets
      *((int*) csoundQueryGlobalVariable(csound, "FLTK_Flags")) = 4;
    }
    else {
//       qDebug("play() FLTK Disabled");
      *((int*) csoundQueryGlobalVariable(csound, "FLTK_Flags")) = 3;
    }
    qDebug("Command Line args:");
    for (int index=0; index< argc; index++) {
      qDebug() << argv[index];
    }

    csoundSetIsGraphable(csound, true);
    csoundSetMakeGraphCallback(csound, &qutecsound::makeGraphCallback);
    csoundSetDrawGraphCallback(csound, &qutecsound::drawGraphCallback);
    csoundSetKillGraphCallback(csound, &qutecsound::killGraphCallback);
    csoundSetExitGraphCallback(csound, &qutecsound::exitGraphCallback);

    ud->result=csoundCompile(csound,argc,argv);
    ud->csound = csound;
    ud->zerodBFS = csoundGet0dBFS(csound);
    ud->sampleRate = csoundGetSr(csound);
    ud->numChnls = csoundGetNchnls(csound);

    qDebug("Csound compiled %i", ud->result);

    if (ud->result!=CSOUND_SUCCESS or variable != CSOUND_SUCCESS) {
      qDebug("Csound compile failed!");
      stop();
      free(argv);
      markErrorLine();
      return;
    }
    if (m_options->enableWidgets and m_options->showWidgetsOnRun) {
      showWidgetsAct->setChecked(true);
      widgetPanel->setVisible(true);
    }
    ud->PERF_STATUS = 1;
    if (m_options->invalueEnabled and m_options->enableWidgets) {
      csoundSetInputValueCallback(csound, &qutecsound::inputValueCallback);
      csoundSetOutputValueCallback(csound, &qutecsound::outputValueCallback);
    }
    else {
      csoundSetInputValueCallback(csound, NULL);
      csoundSetOutputValueCallback(csound, NULL);
    }

    //TODO is something here necessary to work with doubles?
//     PUBLIC int csoundGetSampleFormat(CSOUND *);
//     PUBLIC int csoundGetSampleSize(CSOUND *);
    unsigned int numWidgets = widgetPanel->widgetCount();
    ud->qcs->channelNames.resize(numWidgets*2);
    ud->qcs->values.resize(numWidgets*2);
    ud->qcs->stringValues.resize(numWidgets*2);
    if(m_options->thread) {
#ifdef QUTE_USE_CSOUNDPERFORMANCETHREAD
      perfThread->Play();
#else
      ThreadID = csoundCreateThread(qutecsound::csThread, (void*)ud);
#endif
    }
    else {
//       int numChnls = csoundGetNchnls(ud->csound);
      while(ud->PERF_STATUS == 1 && csoundPerformKsmps(csound)==0) {
        ud->outputBufferSize = csoundGetKsmps(ud->csound);
        ud->outputBuffer = csoundGetSpout(ud->csound);
        for (int i = 0; i < ud->outputBufferSize*ud->numChnls; i++) {
          ud->qcs->audioOutputBuffer.put(ud->outputBuffer[i]);
        }
        qApp->processEvents();
        if (ud->qcs->m_options->enableWidgets) {
          widgetPanel->getValues(&channelNames, &values, &stringValues);
//           if (ud->qcs->m_options->chngetEnabled) {
//             writeWidgetValues(ud);
//             readWidgetValues(ud);
//           }
          processEventQueue(ud);
        }
      }
      ud->PERF_STATUS = 0; // Confirm that Csound has stopped
      stop();  // To flush pending queues
#ifdef MACOSX
// Put menu bar back
      SetMenuBar(menuBarHandle);
#endif
    }
    free(argv);
  }
  else {  // Run in external shell
    QFile tempFile("QuteCsoundExample.csd");
    if (fileName.startsWith(":/examples/")) {
      QString tmpFileName = QDir::tempPath();
      if (!tmpFileName.endsWith("/") and !tmpFileName.endsWith("\\")) {
        tmpFileName += QDir::separator();
      }
//       tmpFileName += QString("QuteCsoundExample.csd");
//       tempFile.setFileTemplate(tmpFileName);
      if (!tempFile.open(QIODevice::ReadWrite | QIODevice::Text)) {
        QMessageBox::critical(this,
                              tr("QuteCsound"),
                                 tr("Error creating temporary file."),
                                    QMessageBox::Ok);
        runAct->setChecked(false);
        return;
      }
      QString csdText = textEdit->document()->toPlainText();
      fileName = tempFile.fileName();
      tempFile.write(csdText.toAscii());
      tempFile.flush();
      if (!tempScriptFiles.contains(fileName))
        tempScriptFiles << fileName;
    }
    QString script = generateScript(realtime, fileName);
    QString scriptFileName = QDir::tempPath();
	if (!scriptFileName.endsWith("/"))
	  scriptFileName += "/";
	scriptFileName += SCRIPT_NAME;
    QFile file(scriptFileName);
    if (!file.open(QIODevice::ReadWrite | QIODevice::Text)) {
      qDebug() << "qutecsound::runCsound() : Error creating temp file";
      return;
    }
    QTextStream out(&file);
    out << script;
//     file.flush();
    file.close();
    file.setPermissions (QFile::ExeOwner| QFile::WriteOwner| QFile::ReadOwner);

    QString options;
#ifdef LINUX
    options = "-e " + scriptFileName;
#endif
#ifdef SOLARIS
    options = "-e " + scriptFileName;
#endif
#ifdef MACOSX
    options = scriptFileName;
#endif
#ifdef WIN32
    options = scriptFileName;
    qDebug() << "m_options->terminal == " << m_options->terminal;
#endif
    execute(m_options->terminal, options);
    runAct->setChecked(false);
    recAct->setChecked(false);
    if (!tempScriptFiles.contains(scriptFileName))
      tempScriptFiles << scriptFileName;
  }
  if (QObject::sender() == renderAct) {
    if (QDir::isRelativePath(m_options->fileOutputFilename)) {
      currentAudioFile = m_options->csdPath + "/" + m_options->fileOutputFilename;
    }
    else {
      currentAudioFile = m_options->fileOutputFilename;
    }
  }
}

void qutecsound::stop()
{
  // Must guarantee that csound has stopped when it returns
  qDebug("qutecsound::stop()");
  if (ud->PERF_STATUS == 1) {
    stopCsound();
  }
  foreach (QString msg, messageQueue) { //Flush pending messages
    m_console->appendMessage(msg);
    widgetPanel->appendMessage(msg);
  }
  if (!m_options->thread) {
    while (ud->PERF_STATUS == -1) {
      ;
      //TODO: wait here?
    }
  }
  runAct->setChecked(false);
  recAct->setChecked(false);
  if (m_options->enableWidgets and m_options->showWidgetsOnRun) {
    //widgetPanel->setVisible(false);
  }
}

void qutecsound::stopCsound()
{
  qDebug("qutecsound::stopCsound()");
  ud->PERF_STATUS = -1;
  if (m_options->thread) {
#ifdef QUTE_USE_CSOUNDPERFORMANCETHREAD
//       perfThread->ScoreEvent(0, 'e', 0, 0);
    //csoundLockMutex(perfMutex);
    perfThread->Stop();
    //csoundUnlockMutex(perfMutex);
    perfThread->Join();
    delete perfThread;
//       perfThread = 0;
//     }
#else
//     csoundScoreEvent(csound,'e' , 0, 0);
    csoundStop(csound);
    csoundJoinThread(ThreadID);

#endif
    csoundCleanup(csound);
    ud->PERF_STATUS = 0;
  }
  else {
    csoundStop(csound);
    csoundCleanup(csound);
    ud->PERF_STATUS = 0;
  }
#ifdef MACOSX
// Put menu bar back
  SetMenuBar(menuBarHandle);
#endif
#ifdef QUTECSOUND_DESTROY_CSOUND
  csoundDestroy(csound);
#endif
}

void qutecsound::record()
{
  if (documentPages[curPage]->fileName.startsWith(":/")) {
    QMessageBox::critical(this,
                          tr("QuteCsound"),
                             tr("You must save the examples to use Record."),
                                QMessageBox::Ok);
    recAct->setChecked(false);
    return;
  }
  if (!runAct->isChecked()) {
    runAct->setChecked(true);
    runCsound(true);
  }
  if (recAct->isChecked()) {
#ifdef USE_LIBSNDFILE
    int format=SF_FORMAT_WAV;
    switch (m_options->sampleFormat) {
      case 0:
        format |= SF_FORMAT_PCM_16;
        break;
      case 1:
        format |= SF_FORMAT_PCM_24;
        break;
      case 2:
        format |= SF_FORMAT_FLOAT;
        break;
    }
 //  const int format=SF_FORMAT_WAV | SF_FORMAT_FLOAT;
    const int channels=ud->numChnls;
    const int sampleRate=ud->sampleRate;
    int number = 0;
    QString fileName = documentPages[curPage]->fileName + "-000.wav";
    while (QFile::exists(fileName)) {
      number++;
      fileName = documentPages[curPage]->fileName + "-";
      if (number < 10)
        fileName += "0";
      if (number < 100)
        fileName += "0";
      fileName += QString::number(number) + ".wav";
    }
    currentAudioFile = fileName;
    qDebug("start recording: %s", fileName.toStdString().c_str());
    outfile = new SndfileHandle(fileName.toStdString().c_str(), SFM_WRITE, format, channels, sampleRate);
    samplesWritten = 0;

    QTimer::singleShot(20, this, SLOT(recordBuffer()));
#else
  QMessageBox::warning(this, tr("QuteCsound"),
                       tr("This version of QuteCsound has been compiled\nwithout Record support!"),
                       QMessageBox::Ok);
#endif
  }
}

void qutecsound::recordBuffer()
{
  int bufferSize = 4096;
  MYFLT sample[bufferSize];
#ifdef USE_LIBSNDFILE
  if (recAct->isChecked()) {
    if (audioOutputBuffer.copyAvailableBuffer(sample, bufferSize)) {
      int samps = outfile->write(sample, bufferSize);
      samplesWritten += samps;
    }
    else {
//       qDebug("qutecsound::recordBuffer() : Empty Buffer!");
    }
    QTimer::singleShot(20, this, SLOT(recordBuffer()));
  }
  else { //Stop recording

    delete outfile;
    qDebug("closed file: %s\nWritten %li samples", currentAudioFile.toStdString().c_str(), samplesWritten);
  }
#endif
}


// void qutecsound::selectMidiOutDevice(QPoint pos)
// {
//   QList<QPair<QString, QString> > devs = ConfigDialog::getMidiInputDevices();
//   QMenu menu;
//
//   for (int i = 0; i < devs.size(); i++) {
//     QAction *action = menu.addAction(devs[i].first/*, this, SLOT()*/);
//     action->setData(devs[i].second);
//   }
//   menu.exec();
// }
//
// void qutecsound::selectMidiInDevice(QPoint pos)
// {
// }
//
// void qutecsound::selectAudioOutDevice(QPoint pos)
// {
// }
//
// void qutecsound::selectAudioInDevice(QPoint pos)
// {
// }

void qutecsound::render()
{
  if (m_options->fileAskFilename) {
    QFileDialog dialog(this,tr("Output Filename"),lastFileDir);
    dialog.setAcceptMode(QFileDialog::AcceptSave);
    dialog.setConfirmOverwrite(false);
    QString filter = QString(_configlists.fileTypeLongNames[m_options->fileFileType] + " Files ("
        + _configlists.fileTypeExtensions[m_options->fileFileType] + ")");
    dialog.setFilter(filter);
    if (dialog.exec() == QDialog::Accepted) {
      QString extension = _configlists.fileTypeExtensions[m_options->fileFileType];
      // Remove the '*' from the extension
      extension.remove(0,1);
      m_options->fileOutputFilename = dialog.selectedFiles()[0];
      if (!m_options->fileOutputFilename.endsWith(extension))
        m_options->fileOutputFilename += extension;
      if (QFile::exists(m_options->fileOutputFilename)) {
        int ret = QMessageBox::warning(this, tr("QuteCsound"),
                tr("The file %1 \nalready exists.\n"
                  "Do you want to overwrite it?").arg(m_options->fileOutputFilename),
                QMessageBox::Save | QMessageBox::Cancel,
                QMessageBox::Save);
        if (ret == QMessageBox::Cancel)
          return;
      }
      lastFileDir = dialog.directory().path();
    }
    else {
      return;
    }
  }
#ifdef Q_OS_WIN32
  m_options->fileOutputFilename.replace('\\', '/');
#endif
  currentAudioFile = m_options->fileOutputFilename;
  runCsound(false);
}

void qutecsound::openExternalEditor()
{
  QString options = currentAudioFile;
  QString optionsText = documentPages[curPage]->getOptionsText();
  if (options == "") {
    if (!optionsText.contains("-o")) {
      options = "test.wav";
    }
    else {
      //TODO this is not very robust...
      optionsText = optionsText.mid(optionsText.indexOf("-o") + 2);
      optionsText = optionsText.left(optionsText.indexOf(" -")).trimmed();
      if (!optionsText.startsWith("dac"))
        options = optionsText;
    }
  }
  options = "\"" + options + "\"";
  QString waveeditor = "\"" + m_options->waveeditor + "\"";
  execute(m_options->waveeditor, options);
}

void qutecsound::openExternalPlayer()
{
  QString options = currentAudioFile;
  QString optionsText = documentPages[curPage]->getOptionsText();
  if (options == "") {
    if (!optionsText.contains("-o")) {
      options = "test.wav";
    }
    else {
      //TODO this is not very robust...
      optionsText = optionsText.mid(optionsText.indexOf("-o") + 2);
      optionsText = optionsText.left(optionsText.indexOf(" -")).trimmed();
      if (!optionsText.startsWith("dac"))
        options = optionsText;
    }
  }
  options = "\"" + currentAudioFile + "\"";
  QString waveplayer = "\"" + m_options->waveplayer + "\"";
  execute(waveplayer, options);
}

void qutecsound::setHelpEntry()
{
  QTextCursor cursor = textEdit->textCursor();
  cursor.select(QTextCursor::WordUnderCursor);
  if (m_options->csdocdir != "") {
    QString file =  m_options->csdocdir + "/" + cursor.selectedText() + ".html";
    helpPanel->docDir = m_options->csdocdir;
    helpPanel->loadFile(file);
    helpPanel->show();
  }
  else {
    QMessageBox::critical(this,
                          tr("Error"),
                          tr("HTML Documentation directory not set!\n"
                             "Please go to Edit->Options->Environment and select directory\n"));
  }
}

void qutecsound::openManualExample(QString fileName)
{
  loadFile(fileName);
}

void qutecsound::openExternalBrowser()
{
  QTextCursor cursor = textEdit->textCursor();
  cursor.select(QTextCursor::WordUnderCursor);
  if (m_options->csdocdir != "") {
    QString file =  m_options->csdocdir + "/" + cursor.selectedText() + ".html";
    execute(m_options->browser, file);
  }
  else {
    QMessageBox::critical(this,
                          tr("Error"),
                          tr("HTML Documentation directory not set!\n"
                             "Please go to Edit->Options->Environment and select directory\n"));
  }
}

void qutecsound::openShortcutDialog()
{
  KeyboardShortcuts dialog(this, m_keyActions);
  connect(&dialog, SIGNAL(restoreDefaultShortcuts()), this, SLOT(setDefaultKeyboardShortcuts()));
  dialog.exec();
}

void qutecsound::utilitiesDialogOpen()
{
  qDebug("qutecsound::utilitiesDialog()");
}

void qutecsound::about()
{
  QString text = tr("by: Andres Cabrera\nReleased under the LGPLv2 or GPLv3\nVersion %1\n").arg(QUTECSOUND_VERSION);
  text += tr("French translation:\nFrancois Pinot\n");
  //Why is the ç character not displayed correctly...?
//   text += tr("French translation:\nFrançois Pinot\n");
  text += tr("German translation:\nJoachim Heintz\n");
  text += tr("Portuguese translation:\nVictor Lazzarini\n");
  text += QString("qutecsound.sourceforge.net");
  QMessageBox::about(this, tr("About QuteCsound"), text);
}

void qutecsound::documentWasModified()
{
  setWindowModified(textEdit->document()->isModified());
  if (textEdit->document()->isModified())
    documentTabs->setTabIcon(curPage, modIcon);
}

void qutecsound::syntaxCheck()
{
  QTextCursor cursor = textEdit->textCursor();
  cursor.select(QTextCursor::LineUnderCursor);
  QStringList words = cursor.selectedText().split(QRegExp("\\b"));
  showLineNumber(textEdit->currentLine());
  foreach( QString word, words) {
       // We need to remove all not possibly opcode
    word.remove(QRegExp("[^\\d\\w]"));
    if(word=="")
      continue;
    QString syntax = opcodeTree->getSyntax(word);
    if(syntax!="") {
      statusBar()->showMessage(syntax, 20000);
      return;
    }
  }
}

void qutecsound::autoComplete()
{
  QTextCursor cursor = textEdit->textCursor();
  cursor.select(QTextCursor::WordUnderCursor);
  QString opcodeName = cursor.selectedText();
  if (opcodeName=="")
    return;
  textEdit->setTextCursor(cursor);
  textEdit->cut();
  QString syntax = opcodeTree->getSyntax(opcodeName);
  textEdit->insertPlainText (syntax);
}

void qutecsound::configure()
{
  ConfigDialog *dialog = new ConfigDialog(this, m_options/*, _configlists*/);
  connect(dialog, SIGNAL(finished(int)), this, SLOT(applySettings(int)));
  dialog->show();
}

void qutecsound::applySettings(int /*result*/)
{
  m_highlighter->setDocument(textEdit->document());
  m_highlighter->setColorVariables(m_options->colorVariables);
  documentPages[curPage]->setTabStopWidth(m_options->tabWidth);
  documentPages[curPage]->setLineWrapMode(m_options->wrapLines ? QTextEdit::WidgetWidth : QTextEdit::NoWrap);
  widgetPanel->setEnabled(m_options->enableWidgets);
  Qt::ToolButtonStyle toolButtonStyle = (m_options->iconText?
      Qt::ToolButtonTextUnderIcon: Qt::ToolButtonIconOnly);
  fileToolBar->setToolButtonStyle(toolButtonStyle);
  editToolBar->setToolButtonStyle(toolButtonStyle);
  controlToolBar->setToolButtonStyle(toolButtonStyle);
  configureToolBar->setToolButtonStyle(toolButtonStyle);
  widgetPanel->showTooltips(m_options->showTooltips);

  QString currentOptions = (m_options->useAPI ? tr("API") : tr("Console")) + " ";
  if (m_options->useAPI) {
    currentOptions +=  (m_options->thread ? tr("Thread") : tr("NoThread")) + " ";
  }
  currentOptions +=  (m_options->saveWidgets ? tr("SaveWidgets") : tr("DontSaveWidgets")) + " ";
  QString playOptions = " (Audio:" + _configlists.rtAudioNames[m_options->rtAudioModule] + " ";
  playOptions += "MIDI:" +  _configlists.rtMidiNames[m_options->rtMidiModule] + ")";
  playOptions += " (" + (m_options->rtUseOptions? tr("UseQuteCsoundOptions"): tr("DiscardQuteCsoundOptions"));
  playOptions += " " + (m_options->rtOverrideOptions? tr("OverrideCsOptions"): tr("")) + ") ";
  playOptions += currentOptions;
  QString renderOptions = " (" + (m_options->fileUseOptions? tr("UseQuteCsoundOptions"): tr("DiscardQuteCsoundOptions")) + " ";
  renderOptions +=  "" + (m_options->fileOverrideOptions? tr("OverrideCsOptions"): tr("")) + ") ";
  renderOptions += currentOptions;
  runAct->setStatusTip(tr("Play") + playOptions);
  renderAct->setStatusTip(tr("Render to file") + renderOptions);
}

void qutecsound::checkSelection()
{
  //TODO add highlighting of words
}

void qutecsound::runUtility(QString flags)
{
  //TODO Run utilities from API using soundRunUtility(CSOUND *, const char *name, int argc, char **argv)
  qDebug("qutecsound::runUtility");
  if (m_options->useAPI) {
#ifdef MACOSX
//Remember menu bar to set it after FLTK grabs it
    menuBarHandle = GetMenuBar();
#endif
    m_console->clear();
    static char *argv[33];
    QString name = "";
    QString fileFlags = flags.mid(flags.indexOf("\""));
    flags.remove(fileFlags);
    QStringList indFlags= flags.split(" ",QString::SkipEmptyParts);
    QStringList files = fileFlags.split("\"", QString::SkipEmptyParts);
    if (indFlags.size() < 2) {
      qDebug("qutecsound::runUtility: Error: empty flags");
      return;
    }
    if (indFlags[0] == "-U") {
      indFlags.removeAt(0);
      name = indFlags[0];
      indFlags.removeAt(0);
    }
    else {
      qDebug("qutecsound::runUtility: Error: unexpected flag!");
      return;
    }
    int index = 0;
    foreach (QString flag, indFlags) {
      argv[index] = (char *) calloc(flag.size()+1, sizeof(char));
      strcpy(argv[index],flag.toStdString().c_str());
      index++;
//       qDebug("%s",flag.toStdString().c_str());
    }
    argv[index] = (char *) calloc(files[0].size()+1, sizeof(char));
    strcpy(argv[index++],files[0].toStdString().c_str());
    argv[index] = (char *) calloc(files[2].size()+1, sizeof(char));
    strcpy(argv[index++],files[2].toStdString().c_str());
    int argc = index;
    CSOUND *csoundU;
    csoundU=csoundCreate(0);
    csoundReset(csoundU);
    csoundSetHostData(csoundU, (void *) ud);
    csoundSetMessageCallback(csoundU, &qutecsound::messageCallback_NoThread);
    csoundPreCompile(csoundU);
    // Utilities always run in the same thread as QuteCsound
    csoundRunUtility(csoundU, name.toStdString().c_str(), argc, argv);
    csoundCleanup(csoundU);
    csoundDestroy(csoundU);
//     free(argv);
#ifdef MACOSX
// Put menu bar back
    SetMenuBar(menuBarHandle);
#endif
  }
  else {
    QString script;
#ifdef WIN32
    script = "";
    if (m_options->opcodedirActive)
      script += "set OPCODEDIR=" + m_options->opcodedir + "\n";
    if (m_options->sadirActive)
      script += "set SADIR=" + m_options->sadir + "\n";
    if (m_options->ssdirActive)
      script += "set SSDIR=" + m_options->ssdir + "\n";
    if (m_options->sfdirActive)
      script += "set SFDIR=" + m_options->sfdir + "\n";
    if (m_options->ssdirActive)
      script += "set INCDIR=" + m_options->incdir + "\n";

    script += "cd " + QFileInfo(documentPages[curPage]->fileName).absoluteFilePath() + "\n";
    script += "csound " + flags + "\n";
#else
    script = "#!/bin/sh\n";
    if (m_options->opcodedirActive)
      script += "export OPCODEDIR=" + m_options->opcodedir + "\n";
    if (m_options->sadirActive)
      script += "export SADIR=" + m_options->sadir + "\n";
    if (m_options->ssdirActive)
      script += "export SSDIR=" + m_options->ssdir + "\n";
    if (m_options->sfdirActive)
      script += "export SFDIR=" + m_options->sfdir + "\n";
    if (m_options->ssdirActive)
      script += "export INCDIR=" + m_options->incdir + "\n";

    script += "cd " + QFileInfo(documentPages[curPage]->fileName).absoluteFilePath() + "\n";
#ifdef MACOSX
    script += "/usr/local/bin/csound " + flags + "\n";
#else
    script += "csound " + flags + "\n";
#endif
    script += "echo \"\nPress return to continue\"\n";
    script += "dummy_var=\"\"\n";
    script += "read dummy_var\n";
    script += "rm $0\n";
#endif
    QFile file(SCRIPT_NAME);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
      return;

    QTextStream out(&file);
    out << script;
    file.flush();
    file.close();
    file.setPermissions (QFile::ExeOwner| QFile::WriteOwner| QFile::ReadOwner);

    QString options;
#ifdef LINUX
    options = "-e " + SCRIPT_NAME;
#endif
#ifdef SOLARIS
    options = "-e " + SCRIPT_NAME;
#endif
#ifdef MACOSX
    options = SCRIPT_NAME;
#endif
#ifdef WIN32
    options = SCRIPT_NAME;
#endif
    execute(m_options->terminal, options);
  }
}

void qutecsound::dispatchQueues()
{
//   qDebug("qutecsound::dispatchQueues()");
  widgetPanel->processNewValues();
  if (ud->PERF_STATUS == 1) {
    while (true) {
      messageMutex.lock();
      if (messageQueue.isEmpty()) {
        messageMutex.unlock();
        break;
      }
      QString msg = messageQueue.takeFirst();
      messageMutex.unlock();
      m_console->appendMessage(msg);
      widgetPanel->appendMessage(msg);
  //     qApp->processEvents();
    }
    m_console->refresh();
  //  widgetPanel->refresh(); // Doesn't work... ??
  //   QList<QString> channels = outValueQueue.keys();
  //   foreach (QString channel, channels) {
  //     widgetPanel->setValue(channel, outValueQueue[channel]);
  //   }
  //   outValueQueue.clear();

    stringValueMutex.lock();
    QStringList channels = outStringQueue.keys();
    for  (int i = 0; i < channels.size(); i++) { //TODO is there an iterator in Qt which can do this better?
      widgetPanel->setValue(channels[i], outStringQueue[channels[i]]);
    }
    outStringQueue.clear();
    stringValueMutex.unlock();
    processEventQueue(ud);
  //   csoundUnlockMutex(perfMutex);
    while (!newCurveBuffer.isEmpty()) {
      Curve * curve = newCurveBuffer.pop();
  // //     qDebug("qutecsound::dispatchQueues() %i-%s", index, curve->get_caption().toStdString().c_str());
        widgetPanel->newCurve(curve);
    }
    if (curveBuffer.size() > 32) {
      qDebug("qutecsound::dispatchQueues() WARNING: curve update buffer too large!");
      curveBuffer.resize(32);
    }
    foreach (WINDAT * windat, curveBuffer){
      Curve *curve = widgetPanel->getCurveById(windat->windid);
      if (curve != 0) {
  //       qDebug("qutecsound::dispatchQueues() %s -- %s",windat->caption, curve->get_caption().toStdString().c_str());
        curve->set_data(windat->fdata);
        curve->set_size(windat->npts);      // number of points
        curve->set_caption(QString(windat->caption)); // title of curve
    //     curve->set_polarity(windat->polarity); // polarity
        curve->set_max(windat->max);        // curve max
        curve->set_min(windat->min);        // curve min
        curve->set_absmax(windat->absmax);     // abs max of above
    //     curve->set_y_scale(windat->y_scale);    // Y axis scaling factor
        widgetPanel->setCurveData(curve);
      }
      curveBuffer.remove(curveBuffer.indexOf(windat));
    }
  }
  queueTimer->start(QCS_QUEUETIMER_TIME); //will launch this function again later
}

// void qutecsound::widgetDockStateChanged(bool topLevel)
// {
// //   qDebug("qutecsound::widgetDockStateChanged()");
//   if (documentPages.size() < 1)
//     return; //necessary check, since widget panel is created early by consructor
//   qApp->processEvents();
//   if (topLevel) {
// //     widgetPanel->setGeometry(documentPages[curPage]->getWidgetPanelGeometry());
//     QRect geometry = documentPages[curPage]->getWidgetPanelGeometry();
//     widgetPanel->move(geometry.x(), geometry.y());
//     widgetPanel->widget()->resize(geometry.width(), geometry.height());
//     qDebug(" %i %i %i %i",geometry.x(), geometry.y(), geometry.width(), geometry.height());
//   }
// }
// 
// void qutecsound::widgetDockLocationChanged(Qt::DockWidgetArea area)
// {
//   qDebug("qutecsound::widgetDockLocationChanged() %i", area);
// }

void qutecsound::showLineNumber(int lineNumber)
{
  lineNumberLabel->setText(tr("Line %1").arg(lineNumber));
}

void qutecsound::setDefaultKeyboardShortcuts()
{
  newAct->setShortcut(tr("Ctrl+N"));
  openAct->setShortcut(tr("Ctrl+O"));
  reloadAct->setShortcut(tr(""));
  saveAct->setShortcut(tr("Ctrl+S"));
  saveAsAct->setShortcut(tr("Shift+Ctrl+S"));
  closeTabAct->setShortcut(tr("Ctrl+W"));

  printAct->setShortcut(tr("Ctrl+P"));
  exitAct->setShortcut(tr("Ctrl+Q"));

  undoAct->setShortcut(tr("Ctrl+Z"));
  redoAct->setShortcut(tr("Shift+Ctrl+Z"));

  cutAct->setShortcut(tr("Ctrl+X"));
  copyAct->setShortcut(tr("Ctrl+C"));
  pasteAct->setShortcut(tr("Ctrl+V"));
  joinAct->setShortcut(tr(""));
  inToGetAct->setShortcut(tr(""));
  getToInAct->setShortcut(tr(""));
  csladspaAct->setShortcut(tr(""));
  findAct->setShortcut(tr("Ctrl+F"));
  autoCompleteAct->setShortcut(tr("Alt+C"));
  configureAct->setShortcut(tr(""));
  editAct->setShortcut(tr(""));
  runAct->setShortcut(tr("CTRL+R"));
  runTermAct->setShortcut(tr(""));
  stopAct->setShortcut(tr("Alt+S"));
  recAct->setShortcut(tr("Ctrl+Space"));
  renderAct->setShortcut(tr("Alt+F"));
  externalPlayerAct->setShortcut(tr(""));
  externalEditorAct->setShortcut(tr(""));
  showWidgetsAct->setShortcut(tr("Alt+1"));
  showHelpAct->setShortcut(tr("Alt+2"));
  showGenAct->setShortcut(tr(""));
  showOverviewAct->setShortcut(tr(""));
  showConsoleAct->setShortcut(tr("Alt+3"));
  showUtilitiesAct->setShortcut(tr("Alt+4"));
  createGraphAct->setShortcut(tr("Alt+5"));
  setHelpEntryAct->setShortcut(tr("Shift+F1"));
  browseBackAct->setShortcut(QKeySequence(Qt::CTRL + Qt::Key_Left));
  browseForwardAct->setShortcut(QKeySequence(Qt::CTRL + Qt::Key_Right));
  externalBrowserAct->setShortcut(tr("Shift+Alt+F1"));
  commentAct->setShortcut(tr("Ctrl+D"));
  uncommentAct->setShortcut(tr("Shift+Ctrl+D"));
  indentAct->setShortcut(tr("Ctrl+I"));
  unindentAct->setShortcut(tr("Shift+Ctrl+I"));
}

void qutecsound::createActions()
{
  // Actions that are not connected here depend on the active document, so they are
  // connected with connectActions() and are changed when the document changes.
  newAct = new QAction(QIcon(":/images/gtk-new.png"), tr("&New"), this);
  newAct->setStatusTip(tr("Create a new file"));
  newAct->setIconText(tr("New"));
  connect(newAct, SIGNAL(triggered()), this, SLOT(newFile()));

  openAct = new QAction(QIcon(":/images/gnome-folder.png"), tr("&Open..."), this);
  openAct->setStatusTip(tr("Open an existing file"));
  openAct->setIconText(tr("Open"));
  connect(openAct, SIGNAL(triggered()), this, SLOT(open()));

  reloadAct = new QAction(QIcon(":/images/gtk-reload.png"), tr("Reload"), this);
  reloadAct->setStatusTip(tr("Reload file from disk, discarding changes"));
//   reloadAct->setIconText(tr("Reload"));
  connect(reloadAct, SIGNAL(triggered()), this, SLOT(reload()));

  saveAct = new QAction(QIcon(":/images/gnome-dev-floppy.png"), tr("&Save"), this);
  saveAct->setStatusTip(tr("Save the document to disk"));
  saveAct->setIconText(tr("Save"));
  connect(saveAct, SIGNAL(triggered()), this, SLOT(save()));

  saveAsAct = new QAction(tr("Save &As..."), this);
  saveAsAct->setStatusTip(tr("Save the document under a new name"));
  saveAsAct->setIconText(tr("Save as"));
  connect(saveAsAct, SIGNAL(triggered()), this, SLOT(saveAs()));

  closeTabAct = new QAction(tr("Close current tab"), this);
  closeTabAct->setStatusTip(tr("Close current tab"));
//   closeTabAct->setIconText(tr("Close"));
  closeTabAct->setIcon(QIcon(":/images/cross.png"));
  connect(closeTabAct, SIGNAL(triggered()), this, SLOT(closeTab()));

  printAct = new QAction(tr("Print"), this);
  printAct->setStatusTip(tr("Print current document"));
//   printAct->setIconText(tr("Print"));
//   closeTabAct->setIcon(QIcon(":/images/cross.png"));
  connect(printAct, SIGNAL(triggered()), this, SLOT(print()));

  exitAct = new QAction(tr("E&xit"), this);
  exitAct->setStatusTip(tr("Exit the application"));
//   exitAct->setIconText(tr("Exit"));
  connect(exitAct, SIGNAL(triggered()), this, SLOT(close()));

  createGraphAct = new QAction(tr("View Code &Graph"), this);
  createGraphAct->setStatusTip(tr("View Code Graph"));
//   createGraphAct->setIconText("Exit");
  connect(createGraphAct, SIGNAL(triggered()), this, SLOT(createGraph()));

//   for (int i = 0; i < recentFiles.size(); i++) {
//     openRecentAct[i] = new QAction(tr("Recent 0"), this);
//     connect(openRecentAct[i], SIGNAL(triggered()), this, SLOT(openRecent()));
//   }

  undoAct = new QAction(QIcon(":/images/gtk-undo.png"), tr("Undo"), this);
  undoAct->setStatusTip(tr("Undo last action"));
  exitAct->setIconText(tr("Undo"));
  connect(undoAct, SIGNAL(triggered()), this, SLOT(undo()));

  redoAct = new QAction(QIcon(":/images/gtk-redo.png"), tr("Redo"), this);
  redoAct->setStatusTip(tr("Redo last action"));
  redoAct->setIconText(tr("Redo"));
  connect(redoAct, SIGNAL(triggered()), this, SLOT(redo()));

  cutAct = new QAction(QIcon(":/images/gtk-cut.png"), tr("Cu&t"), this);
  cutAct->setStatusTip(tr("Cut the current selection's contents to the "
      "clipboard"));
  cutAct->setIconText(tr("Cut"));
  connect(cutAct, SIGNAL(triggered()), this, SLOT(cut()));

  copyAct = new QAction(QIcon(":/images/gtk-copy.png"), tr("&Copy"), this);
  copyAct->setStatusTip(tr("Copy the current selection's contents to the "
      "clipboard"));
  copyAct->setIconText(tr("Copy"));
  connect(copyAct, SIGNAL(triggered()), this, SLOT(copy()));

  pasteAct = new QAction(QIcon(":/images/gtk-paste.png"), tr("&Paste"), this);
  pasteAct->setStatusTip(tr("Paste the clipboard's contents into the current "
      "selection"));
  pasteAct->setIconText(tr("Paste"));
  connect(pasteAct, SIGNAL(triggered()), this, SLOT(paste()));

  joinAct = new QAction(/*QIcon(":/images/gtk-paste.png"),*/ tr("&Join orc/sco"), this);
  joinAct->setStatusTip(tr("Join orc/sco files in a single csd file"));
//   joinAct->setIconText(tr("Join"));
  connect(joinAct, SIGNAL(triggered()), this, SLOT(join()));

  inToGetAct = new QAction(/*QIcon(":/images/gtk-paste.png"),*/ tr("Invalue->Chnget"), this);
  inToGetAct->setStatusTip(tr("Convert invalue/outvalue to chnget/chnset"));
  connect(inToGetAct, SIGNAL(triggered()), this, SLOT(inToGet()));

  getToInAct = new QAction(/*QIcon(":/images/gtk-paste.png"),*/ tr("Chnget->Invalue"), this);
  getToInAct->setStatusTip(tr("Convert chnget/chnset to invalue/outvalue"));
  connect(getToInAct, SIGNAL(triggered()), this, SLOT(getToIn()));

  csladspaAct = new QAction(/*QIcon(":/images/gtk-paste.png"),*/ tr("Insert/Update CsLADSPA text"), this);
  csladspaAct->setStatusTip(tr("Insert/Update CsLADSPA section to csd file"));
  connect(csladspaAct, SIGNAL(triggered()), this, SLOT(putCsladspaText()));

  findAct = new QAction(/*QIcon(":/images/gtk-paste.png"),*/ tr("&Find and Replace"), this);
  findAct->setStatusTip(tr("Find and replace strings in file"));
//   findAct->setIconText(tr("Find"));
  connect(findAct, SIGNAL(triggered()), this, SLOT(findReplace()));

  autoCompleteAct = new QAction(tr("AutoComplete"), this);
  autoCompleteAct->setStatusTip(tr("Autocomplete according to Status bar display"));
//   autoCompleteAct->setIconText(tr("AutoComplete"));
  connect(autoCompleteAct, SIGNAL(triggered()), this, SLOT(autoComplete()));

  configureAct = new QAction(QIcon(":/images/control-center2.png"), tr("Configuration"), this);
  configureAct->setStatusTip(tr("Open configuration dialog"));
  configureAct->setIconText(tr("Configure"));
  connect(configureAct, SIGNAL(triggered()), this, SLOT(configure()));

//   editAct = new QAction(/*QIcon(":/images/gtk-media-play-ltr.png"),*/ tr("Widget Edit Mode"), this);
//   editAct->setStatusTip(tr("Activate Edit Mode for Widget Panel"));
// //   editAct->setIconText("Play");
//   editAct->setCheckable(true);
//   connect(editAct, SIGNAL(triggered(bool)), this, SLOT(edit(bool)));
  editAct = static_cast<WidgetPanel *>(widgetPanel)->editAct;

  runAct = new QAction(QIcon(":/images/gtk-media-play-ltr.png"), tr("Run"), this);
  runAct->setStatusTip(tr("Run"));
  runAct->setIconText(tr("Run"));
  runAct->setCheckable(true);
  connect(runAct, SIGNAL(triggered()), this, SLOT(runCsound()));

  runTermAct = new QAction(QIcon(":/images/gtk-media-play-ltr2.png"), tr("Run in Terminal"), this);
  runTermAct->setStatusTip(tr("Run in external shell"));
  runTermAct->setIconText(tr("Run in Term"));
  connect(runTermAct, SIGNAL(triggered()), this, SLOT(runCsound()));

  stopAct = new QAction(QIcon(":/images/gtk-media-stop.png"), tr("Stop"), this);
  stopAct->setStatusTip(tr("Stop"));
  stopAct->setIconText(tr("Stop"));
  connect(stopAct, SIGNAL(triggered()), this, SLOT(stop()));

  recAct = new QAction(QIcon(":/images/gtk-media-record.png"), tr("Record"), this);
  recAct->setStatusTip(tr("Record"));
  recAct->setIconText(tr("Record"));
  recAct->setCheckable(true);
  connect(recAct, SIGNAL(triggered()), this, SLOT(record()));

  renderAct = new QAction(QIcon(":/images/render.png"), tr("Render to file"), this);
  renderAct->setStatusTip(tr("Render to file"));
  renderAct->setIconText(tr("Render"));
  connect(renderAct, SIGNAL(triggered()), this, SLOT(render()));

  externalPlayerAct = new QAction(QIcon(":/images/playfile.png"), tr("Play Audiofile"), this);
  externalPlayerAct->setStatusTip(tr("Play rendered audiofile in External Editor"));
  externalPlayerAct->setIconText(tr("Ext. Player"));
  connect(externalPlayerAct, SIGNAL(triggered()), this, SLOT(openExternalPlayer()));

  externalEditorAct = new QAction(QIcon(":/images/editfile.png"), tr("Edit Audiofile"), this);
  externalEditorAct->setStatusTip(tr("Edit rendered audiofile in External Editor"));
  externalEditorAct->setIconText(tr("Ext. Editor"));
  connect(externalEditorAct, SIGNAL(triggered()), this, SLOT(openExternalEditor()));

  showWidgetsAct = new QAction(QIcon(":/images/gnome-mime-application-x-diagram.png"), tr("Widgets"), this);
  showWidgetsAct->setCheckable(true);
  //showWidgetsAct->setChecked(true);
  showWidgetsAct->setStatusTip(tr("Show Realtime Widgets"));
  showWidgetsAct->setIconText(tr("Widgets"));
  connect(showWidgetsAct, SIGNAL(triggered(bool)), widgetPanel, SLOT(setVisible(bool)));
  connect(widgetPanel, SIGNAL(Close(bool)), showWidgetsAct, SLOT(setChecked(bool)));

  showHelpAct = new QAction(QIcon(":/images/gtk-info.png"), tr("Help Panel"), this);
  showHelpAct->setCheckable(true);
  showHelpAct->setChecked(true);
  showHelpAct->setStatusTip(tr("Show the Csound Manual Panel"));
  showHelpAct->setIconText(tr("Manual"));
  connect(showHelpAct, SIGNAL(toggled(bool)), helpPanel, SLOT(setVisible(bool)));
  connect(helpPanel, SIGNAL(Close(bool)), showHelpAct, SLOT(setChecked(bool)));

  showGenAct = new QAction(/*QIcon(":/images/gtk-info.png"), */tr("GEN Routines"), this);
  showGenAct->setStatusTip(tr("Show the GEN Routines Manual page"));
  connect(showGenAct, SIGNAL(triggered()), helpPanel, SLOT(showGen()));

  showOverviewAct = new QAction(/*QIcon(":/images/gtk-info.png"), */tr("Opcode Overview"), this);
  showOverviewAct->setStatusTip(tr("Show opcode overview"));
  connect(showOverviewAct, SIGNAL(triggered()), helpPanel, SLOT(showOverview()));

  showConsoleAct = new QAction(QIcon(":/images/gksu-root-terminal.png"), tr("Output Console"), this);
  showConsoleAct->setCheckable(true);
  showConsoleAct->setChecked(true);
  showConsoleAct->setStatusTip(tr("Show Csound's message console"));
  showConsoleAct->setIconText(tr("Console"));
  connect(showConsoleAct, SIGNAL(toggled(bool)), m_console, SLOT(setVisible(bool)));
  connect(m_console, SIGNAL(Close(bool)), showConsoleAct, SLOT(setChecked(bool)));

  setHelpEntryAct = new QAction(QIcon(":/images/gtk-info.png"), tr("Show Opcode Entry"), this);
  setHelpEntryAct->setStatusTip(tr("Show Opcode Entry in help panel"));
  setHelpEntryAct->setIconText(tr("Manual for opcode"));
  connect(setHelpEntryAct, SIGNAL(triggered()), this, SLOT(setHelpEntry()));

  browseBackAct = new QAction(tr("Help Back"), this);
  browseBackAct->setStatusTip(tr("Go back in help page"));
  connect(browseBackAct, SIGNAL(triggered()), helpPanel, SLOT(browseBack()));

  browseForwardAct = new QAction(tr("Help Forward"), this);
  browseForwardAct->setStatusTip(tr("Go forward in help page"));
  connect(browseForwardAct, SIGNAL(triggered()), helpPanel, SLOT(browseForward()));

  externalBrowserAct = new QAction(/*QIcon(":/images/gtk-info.png"), */ tr("Show Opcode Entry in External Browser"), this);
  externalBrowserAct->setStatusTip(tr("Show Opcode Entry in external browser"));
  connect(externalBrowserAct, SIGNAL(triggered()), this, SLOT(openExternalBrowser()));

  showUtilitiesAct = new QAction(QIcon(":/images/gnome-devel.png"), tr("Utilities"), this);
  showUtilitiesAct->setCheckable(true);
  showUtilitiesAct->setChecked(false);
  showUtilitiesAct->setStatusTip(tr("Show the Csound Utilities dialog"));
  showUtilitiesAct->setIconText(tr("Utilities"));
  connect(showUtilitiesAct, SIGNAL(triggered(bool)), utilitiesDialog, SLOT(setVisible(bool)));
  connect(utilitiesDialog, SIGNAL(Close(bool)), showUtilitiesAct, SLOT(setChecked(bool)));

  setShortcutsAct = new QAction(tr("Set Keyboard Shortcuts"), this);
  setShortcutsAct->setStatusTip(tr("Set Keyboard Shortcuts"));
  setShortcutsAct->setIconText(tr("Set Shortcuts"));
  connect(setShortcutsAct, SIGNAL(triggered()), this, SLOT(openShortcutDialog()));

  commentAct = new QAction(tr("Comment"), this);
  commentAct->setStatusTip(tr("Comment selection"));
  commentAct->setIconText(tr("Comment"));
  connect(commentAct, SIGNAL(triggered()), this, SLOT(controlD()));

  uncommentAct = new QAction(tr("Uncomment"), this);
  uncommentAct->setStatusTip(tr("Uncomment selection"));
//   uncommentAct->setIconText(tr("Uncomment"));
//   connect(uncommentAct, SIGNAL(triggered()), this, SLOT(uncomment()));

  indentAct = new QAction(tr("Indent"), this);
  indentAct->setStatusTip(tr("Indent selection"));
//   indentAct->setIconText(tr("Indent"));
//   connect(indentAct, SIGNAL(triggered()), this, SLOT(indent()));

  unindentAct = new QAction(tr("Unindent"), this);
  unindentAct->setStatusTip(tr("Unindent selection"));
//   unindentAct->setIconText(tr("Unindent"));
//   connect(unindentAct, SIGNAL(triggered()), this, SLOT(unindent()));

  aboutAct = new QAction(tr("&About QuteCsound"), this);
  aboutAct->setStatusTip(tr("Show the application's About box"));
//   aboutAct->setIconText(tr("About"));
  connect(aboutAct, SIGNAL(triggered()), this, SLOT(about()));

  aboutQtAct = new QAction(tr("About &Qt"), this);
  aboutQtAct->setStatusTip(tr("Show the Qt library's About box"));
//   aboutQtAct->setIconText(tr("About Qt"));
  connect(aboutQtAct, SIGNAL(triggered()), qApp, SLOT(aboutQt()));

  //TODO Put this back when documentpage has focus
//   cutAct->setEnabled(false);
//   copyAct->setEnabled(false);
  setKeyboardShortcuts();
  setDefaultKeyboardShortcuts();
}

void qutecsound::setKeyboardShortcuts()
{
  // Do not change the order of these actions because the settings
  // read shortcuts for a number. Only add at the end.
  m_keyActions.append(newAct);
  m_keyActions.append(openAct);
  m_keyActions.append(reloadAct);
  m_keyActions.append(saveAct);
  m_keyActions.append(saveAsAct);
  m_keyActions.append(closeTabAct);
  m_keyActions.append(printAct);
  m_keyActions.append(exitAct);
  m_keyActions.append(createGraphAct);
  m_keyActions.append(undoAct);
  m_keyActions.append(redoAct);
  m_keyActions.append(cutAct);
  m_keyActions.append(copyAct);
  m_keyActions.append(pasteAct);
  m_keyActions.append(joinAct);
  m_keyActions.append(inToGetAct);
  m_keyActions.append(getToInAct);
  m_keyActions.append(csladspaAct);
  m_keyActions.append(findAct);
  m_keyActions.append(autoCompleteAct);
  m_keyActions.append(configureAct);
  m_keyActions.append(editAct);
  m_keyActions.append(runAct);
  m_keyActions.append(runTermAct);
  m_keyActions.append(stopAct);
  m_keyActions.append(recAct);
  m_keyActions.append(renderAct);
  m_keyActions.append(commentAct);
  m_keyActions.append(uncommentAct);
  m_keyActions.append(indentAct);
  m_keyActions.append(unindentAct);
  m_keyActions.append(externalPlayerAct);
  m_keyActions.append(externalEditorAct);
  m_keyActions.append(showWidgetsAct);
  m_keyActions.append(showHelpAct);
  m_keyActions.append(showGenAct);
  m_keyActions.append(showOverviewAct);
  m_keyActions.append(showConsoleAct);
  m_keyActions.append(setHelpEntryAct);
  m_keyActions.append(browseBackAct);
  m_keyActions.append(browseForwardAct);
  m_keyActions.append(externalBrowserAct);
}

void qutecsound::connectActions()
{
  disconnect(undoAct, 0, 0, 0);
  connect(undoAct, SIGNAL(triggered()), textEdit, SLOT(undo()));
  disconnect(redoAct, 0, 0, 0);
  connect(redoAct, SIGNAL(triggered()), textEdit, SLOT(redo()));
  disconnect(cutAct, 0, 0, 0);
  connect(cutAct, SIGNAL(triggered()), this, SLOT(cut()));
  disconnect(copyAct, 0, 0, 0);
  connect(copyAct, SIGNAL(triggered()), this, SLOT(copy()));
  disconnect(pasteAct, 0, 0, 0);
  connect(pasteAct, SIGNAL(triggered()), this, SLOT(paste()));
  
//   disconnect(commentAct, 0, 0, 0);
  disconnect(uncommentAct, 0, 0, 0);
  disconnect(indentAct, 0, 0, 0);
  disconnect(unindentAct, 0, 0, 0);
//   connect(commentAct, SIGNAL(triggered()), textEdit, SLOT(comment()));
  connect(uncommentAct, SIGNAL(triggered()), textEdit, SLOT(uncomment()));
  connect(indentAct, SIGNAL(triggered()), textEdit, SLOT(indent()));
  connect(unindentAct, SIGNAL(triggered()), textEdit, SLOT(unindent()));

  disconnect(textEdit, SIGNAL(copyAvailable(bool)), 0, 0);
  disconnect(textEdit, SIGNAL(copyAvailable(bool)), 0, 0);
  //TODO put these back but only when document has focus
//   connect(textEdit, SIGNAL(copyAvailable(bool)),
//           cutAct, SLOT(setEnabled(bool)));
//   connect(textEdit, SIGNAL(copyAvailable(bool)),
//           copyAct, SLOT(setEnabled(bool)));

  disconnect(textEdit, SIGNAL(textChanged()), 0, 0);
  disconnect(textEdit, SIGNAL(cursorPositionChanged()), 0, 0);
  connect(textEdit, SIGNAL(textChanged()),
          this, SLOT(documentWasModified()));
  connect(textEdit, SIGNAL(textChanged()),
          this, SLOT(syntaxCheck()));
  connect(textEdit, SIGNAL(cursorPositionChanged()),
          this, SLOT(syntaxCheck()));
  connect(textEdit, SIGNAL(selectionChanged()),
          this, SLOT(checkSelection()));

  disconnect(widgetPanel, SIGNAL(widgetsChanged(QString)),0,0);
//   connect(widgetPanel, SIGNAL(widgetsChanged(QString)),
//           textEdit, SLOT(setMacWidgetsText(QString)) );
  disconnect(widgetPanel, SIGNAL(moved(QPoint)),0,0);
  connect(widgetPanel, SIGNAL(moved(QPoint)),
          textEdit, SLOT(setWidgetPanelPosition(QPoint)) );
  disconnect(widgetPanel, SIGNAL(resized(QSize)),0,0);
  connect(widgetPanel, SIGNAL(resized(QSize)),
          textEdit, SLOT(setWidgetPanelSize(QSize)) );
  disconnect(textEdit, SIGNAL(currentLineChanged(int)), 0, 0);
  connect(textEdit, SIGNAL(currentLineChanged(int)), this, SLOT(showLineNumber(int)));
}

void qutecsound::createMenus()
{
  fileMenu = menuBar()->addMenu(tr("File"));

  editMenu = menuBar()->addMenu(tr("Edit"));
  editMenu->addAction(undoAct);
  editMenu->addAction(redoAct);
  editMenu->addSeparator();
  editMenu->addAction(cutAct);
  editMenu->addAction(copyAct);
  editMenu->addAction(pasteAct);
  editMenu->addSeparator();
  editMenu->addAction(findAct);
  editMenu->addAction(autoCompleteAct);
  editMenu->addSeparator();
  editMenu->addAction(commentAct);
  editMenu->addAction(uncommentAct);
  editMenu->addAction(indentAct);
  editMenu->addAction(unindentAct);
  editMenu->addSeparator();
  editMenu->addAction(joinAct);
  editMenu->addAction(inToGetAct);
  editMenu->addAction(getToInAct);
  editMenu->addAction(csladspaAct);
  editMenu->addSeparator();
  editMenu->addAction(editAct);
  editMenu->addSeparator();
  editMenu->addAction(configureAct);
  editMenu->addAction(setShortcutsAct);

  controlMenu = menuBar()->addMenu(tr("Control"));
  controlMenu->addAction(runAct);
  controlMenu->addAction(runTermAct);
  controlMenu->addAction(stopAct);
  controlMenu->addAction(runAct);
  controlMenu->addAction(renderAct);
  controlMenu->addAction(recAct);
  controlMenu->addAction(externalEditorAct);
  controlMenu->addAction(externalPlayerAct);

  viewMenu = menuBar()->addMenu(tr("View"));
  viewMenu->addAction(showWidgetsAct);
  viewMenu->addAction(showHelpAct);
  viewMenu->addAction(showConsoleAct);
  viewMenu->addAction(showUtilitiesAct);
  viewMenu->addAction(createGraphAct);

  QStringList exampleFiles;
  QStringList widgetFiles;
  QStringList tutFiles;
  QStringList synthFiles;
  QStringList utilitiesFiles;
  QList<QStringList> subMenus;
  QStringList subMenuNames;

  tutFiles.append(":/examples/Toot1.csd");
  tutFiles.append(":/examples/Toot2.csd");
  tutFiles.append(":/examples/Toot3.csd");
  tutFiles.append(":/examples/Toot4.csd");
  tutFiles.append(":/examples/Toot5.csd");
  tutFiles.append(":/examples/Widgets_1.csd");
  tutFiles.append(":/examples/Widgets_2.csd");

  subMenus << tutFiles;
  subMenuNames << "Tutorials";

  widgetFiles.append(":/examples/Widget_Panel.csd");
  widgetFiles.append(":/examples/Label_Widget.csd");
  widgetFiles.append(":/examples/Display_Widget.csd");
  widgetFiles.append(":/examples/Slider_Widget.csd");
  widgetFiles.append(":/examples/Scrollnumber_Widget.csd");
  widgetFiles.append(":/examples/Graph_Widget.csd");
  widgetFiles.append(":/examples/Button_Widget.csd");
  widgetFiles.append(":/examples/Checkbox_Widget.csd");
  widgetFiles.append(":/examples/Menu_Widget.csd");
  widgetFiles.append(":/examples/Controller_Widget.csd");
  widgetFiles.append(":/examples/Scope_Widget.csd");

  subMenus << widgetFiles;
  subMenuNames << "Widgets";

  synthFiles.append(":/examples/Additive_Synth.csd");
  synthFiles.append(":/examples/Simple_Subtractive.csd");
  synthFiles.append(":/examples/Simple_FM_Synth.csd");
  synthFiles.append(":/examples/Phase_Mod_Synth.csd");
  synthFiles.append(":/examples/Formant_Synth.csd");

  subMenus << synthFiles;
  subMenuNames << tr("Synths");

  exampleFiles.append(":/examples/SF_Play_from_buffer.csd");
  exampleFiles.append(":/examples/SF_Play_from_buffer_2.csd");
  exampleFiles.append(":/examples/SF_Play_from_HD.csd");
  exampleFiles.append(":/examples/SF_Play_from_HD_2.csd");
  exampleFiles.append(":/examples/8_Chn_Player.csd");
  exampleFiles.append(":/examples/SF_Record.csd");
  exampleFiles.append(":/examples/Simple_Convolution.csd");
  exampleFiles.append(":/examples/Oscillator_Aliasing.csd");
  exampleFiles.append(":/examples/Circle.csd");
  exampleFiles.append(":/examples/Pvstencil.csd");
  exampleFiles.append(":/examples/Lineedit_Widget.csd");
  exampleFiles.append(":/examples/Rms.csd");
  exampleFiles.append(":/examples/Reinit_Example.csd");
  exampleFiles.append(":/examples/No_Reinit.csd");
  exampleFiles.append(":/examples/String_Channels.csd");
  exampleFiles.append(":/examples/Reserved_Channels.csd");
  exampleFiles.append(":/examples/Noise_Reduction.csd");

  subMenus << exampleFiles;
  subMenuNames << "Examples";

  utilitiesFiles.append(":/examples/IO_Test.csd");
  utilitiesFiles.append(":/examples/Audio_Input_Test.csd");
  utilitiesFiles.append(":/examples/Audio_Output_Test.csd");
  utilitiesFiles.append(":/examples/Audio_Thru_Test.csd");
  utilitiesFiles.append(":/examples/MIDI_IO_Test.csd");

  subMenus << utilitiesFiles;
  subMenuNames << tr("Utilities");

  QMenu *examplesMenu = menuBar()->addMenu(tr("Examples"));
//   QAction *newAction = examplesMenu->addAction("About the examples...");
//   connect(newAction,SIGNAL(triggered()), this, SLOT(aboutExamples()));
  QAction *newAction;
  QMenu *submenu;
  for (int i = 0; i < subMenus.size(); i++) {
    submenu = examplesMenu->addMenu(subMenuNames[i]);
    foreach (QString fileName, subMenus[i]) {
      QString name = fileName.mid(fileName.lastIndexOf("/") + 1).replace("_", " ").remove(".csd");
      newAction = submenu->addAction(name);
      newAction->setData(fileName);
      connect(newAction,SIGNAL(triggered()), this, SLOT(openExample()));
    }
  }

  menuBar()->addSeparator();

  helpMenu = menuBar()->addMenu(tr("Help"));
  helpMenu->addAction(setHelpEntryAct);
  helpMenu->addAction(externalBrowserAct);
  helpMenu->addSeparator();
  helpMenu->addAction(showOverviewAct);
  helpMenu->addAction(showGenAct);
  helpMenu->addSeparator();
  helpMenu->addAction(browseBackAct);
  helpMenu->addAction(browseForwardAct);
  helpMenu->addSeparator();
  helpMenu->addAction(aboutAct);
  helpMenu->addAction(aboutQtAct);

}

void qutecsound::fillFileMenu()
{
  fileMenu->clear();
  fileMenu->addAction(newAct);
  fileMenu->addAction(openAct);
  fileMenu->addAction(saveAct);
  fileMenu->addAction(saveAsAct);
  fileMenu->addAction(reloadAct);
//   fileMenu->addAction(cabbageAct);
  fileMenu->addAction(closeTabAct);
  fileMenu->addAction(printAct);
  fileMenu->addSeparator();
  fileMenu->addAction(exitAct);
  fileMenu->addSeparator();
  recentMenu = fileMenu->addMenu(tr("Recent files"));
  for (int i = 0; i< recentFiles.size(); i++) {
    if (recentFiles[i] != "") {
      openRecentAct[i]->setText(recentFiles[i]);
      recentMenu->addAction(openRecentAct[i]);
    }
  }
}

void qutecsound::createToolBars()
{
  fileToolBar = addToolBar(tr("File"));
  fileToolBar->setObjectName("fileToolBar");
  fileToolBar->addAction(newAct);
  fileToolBar->addAction(openAct);
  fileToolBar->addAction(saveAct);

  editToolBar = addToolBar(tr("Edit"));
  editToolBar->setObjectName("editToolBar");
  editToolBar->addAction(undoAct);
  editToolBar->addAction(redoAct);
  editToolBar->addAction(cutAct);
  editToolBar->addAction(copyAct);
  editToolBar->addAction(pasteAct);

  controlToolBar = addToolBar(tr("Control"));
  controlToolBar->setObjectName("controlToolBar");
  controlToolBar->addAction(runAct);
  controlToolBar->addAction(stopAct);
  controlToolBar->addAction(runTermAct);
  controlToolBar->addAction(recAct);
  controlToolBar->addAction(renderAct);
  controlToolBar->addAction(externalEditorAct);
  controlToolBar->addAction(externalPlayerAct);

  configureToolBar = addToolBar(tr("Configure"));
  configureToolBar->setObjectName("configureToolBar");
  configureToolBar->addAction(configureAct);
  configureToolBar->addAction(showWidgetsAct);
  configureToolBar->addAction(showHelpAct);
  configureToolBar->addAction(showConsoleAct);
  configureToolBar->addAction(showUtilitiesAct);

  Qt::ToolButtonStyle toolButtonStyle = (m_options->iconText?
      Qt::ToolButtonTextUnderIcon: Qt::ToolButtonIconOnly);
  fileToolBar->setToolButtonStyle(toolButtonStyle);
  editToolBar->setToolButtonStyle(toolButtonStyle);
  controlToolBar->setToolButtonStyle(toolButtonStyle);
  configureToolBar->setToolButtonStyle(toolButtonStyle);
}

void qutecsound::createStatusBar()
{
  statusBar()->showMessage(tr("Ready"));
}

void qutecsound::readSettings()
{
  QSettings settings("csound", "qutecsound");
  int settingsVersion = settings.value("settingsVersion", 0).toInt();
  settings.beginGroup("GUI");
  QPoint pos = settings.value("pos", QPoint(200, 200)).toPoint();
  QSize size = settings.value("size", QSize(600, 500)).toSize();
  resize(size);
  move(pos);
  restoreState(settings.value("dockstate").toByteArray());
  lastUsedDir = settings.value("lastuseddir", "").toString();
  lastFileDir = settings.value("lastfiledir", "").toString();
  m_options->language = _configlists.languageCodes.indexOf(settings.value("language", QLocale::system().name()).toString());
  if (m_options->language < 0)
    m_options->language = 0;
  recentFiles.clear();
  QAction *newAct;
  recentFiles.append(settings.value("recentFiles0", "").toString());
  newAct = new QAction(this);
  openRecentAct.append(newAct);
  connect(newAct, SIGNAL(triggered()), this, SLOT(openRecent0()));
  recentFiles.append(settings.value("recentFiles1", "").toString());
  newAct = new QAction(this);
  openRecentAct.append(newAct);
  connect(newAct, SIGNAL(triggered()), this, SLOT(openRecent1()));
  recentFiles.append(settings.value("recentFiles2", "").toString());
  newAct = new QAction(this);
  openRecentAct.append(newAct);
  connect(newAct, SIGNAL(triggered()), this, SLOT(openRecent2()));
  recentFiles.append(settings.value("recentFiles3", "").toString());
  newAct = new QAction(this);
  openRecentAct.append(newAct);
  connect(newAct, SIGNAL(triggered()), this, SLOT(openRecent3()));
  recentFiles.append(settings.value("recentFiles4", "").toString());
  newAct = new QAction(this);
  openRecentAct.append(newAct);
  connect(newAct, SIGNAL(triggered()), this, SLOT(openRecent4()));
  recentFiles.append(settings.value("recentFiles5", "").toString());
  newAct = new QAction(this);
  openRecentAct.append(newAct);
  connect(newAct, SIGNAL(triggered()), this, SLOT(openRecent5()));
  settings.beginGroup("Shortcuts");
  for (int i = 0; i < m_keyActions.size();i++) {
    QString shortcut = settings.value(QString::number(i), "").toString();
    if (shortcut != "")
      m_keyActions[i]->setShortcut(shortcut);
  }
  settings.endGroup();
  settings.endGroup();
  settings.beginGroup("Options");
  settings.beginGroup("Editor");
  m_options->font = settings.value("font", "Courier").toString();
  m_options->fontPointSize = settings.value("fontsize", 12).toDouble();
  m_options->consoleFont = settings.value("consolefont", "Courier").toString();
  m_options->consoleFontPointSize = settings.value("consolefontsize", 10).toDouble();

  m_options->consoleFontColor = settings.value("consoleFontColor", QVariant(QColor(Qt::black))).value<QColor>();
  m_options->consoleBgColor = settings.value("consoleBgColor", QVariant(QColor(Qt::white))).value<QColor>();
  m_options->tabWidth = settings.value("tabWidth", 40).toInt();
  m_options->colorVariables = settings.value("colorvariables", true).toBool();
  m_options->autoPlay = settings.value("autoplay", false).toBool();
  m_options->saveChanges = settings.value("savechanges", true).toBool();
  m_options->rememberFile = settings.value("rememberfile", true).toBool();
  m_options->saveWidgets = settings.value("savewidgets", true).toBool();
  m_options->iconText = settings.value("iconText", true).toBool();
  m_options->wrapLines = settings.value("wrapLines", true).toBool();
  m_options->invalueEnabled = settings.value("invalueEnabled", true).toBool();
//   m_options->chngetEnabled = settings.value("chngetEnabled", false).toBool();
  m_options->showWidgetsOnRun = settings.value("showWidgetsOnRun", true).toBool();
  m_options->showTooltips = settings.value("showTooltips", true).toBool();
  m_options->enableFLTK = settings.value("enableFLTK", false).toBool();
  lastFiles = settings.value("lastfiles", "").toStringList();
  settings.endGroup();
  settings.beginGroup("Run");
  m_options->useAPI = settings.value("useAPI", true).toBool();
  m_options->thread = settings.value("thread", true).toBool();
  m_options->bufferSize = settings.value("bufferSize", 1024).toInt();
  m_options->bufferSizeActive = settings.value("bufferSizeActive", false).toBool();
  m_options->HwBufferSize = settings.value("HwBufferSize", 1024).toInt();
  m_options->HwBufferSizeActive = settings.value("HwBufferSizeActive", false).toBool();
  m_options->dither = settings.value("dither", false).toBool();
  m_options->additionalFlags = settings.value("additionalFlags", "").toString();
  if (settingsVersion < 1)
    m_options->additionalFlags.remove("-d");  // remove old -d preference, as it is fixed now.
  m_options->additionalFlagsActive = settings.value("additionalFlagsActive", false).toBool();
  m_options->fileUseOptions = settings.value("fileUseOptions", true).toBool();
  m_options->fileOverrideOptions = settings.value("fileOverrideOptions", false).toBool();
  m_options->fileAskFilename = settings.value("fileAskFilename", false).toBool();
  m_options->filePlayFinished = settings.value("filePlayFinished", false).toBool();
  m_options->fileFileType = settings.value("fileFileType", 0).toInt();
  m_options->fileSampleFormat = settings.value("fileSampleFormat", 1).toInt();
  m_options->fileInputFilenameActive = settings.value("fileInputFilenameActive", false).toBool();
  m_options->fileInputFilename = settings.value("fileInputFilename", "").toString();
  m_options->fileOutputFilenameActive = settings.value("fileOutputFilenameActive", false).toBool();
  m_options->fileOutputFilename = settings.value("fileOutputFilename", "").toString();
  m_options->rtUseOptions = settings.value("rtUseOptions", true).toBool();
  m_options->rtOverrideOptions = settings.value("rtOverrideOptions", false).toBool();
  m_options->enableWidgets = settings.value("enableWidgets", true).toBool();
  m_options->rtAudioModule = settings.value("rtAudioModule", 0).toInt();
  m_options->rtInputDevice = settings.value("rtInputDevice", "adc").toString();
  m_options->rtOutputDevice = settings.value("rtOutputDevice", "dac").toString();
  m_options->rtJackName = settings.value("rtJackName", "").toString();
  m_options->rtMidiModule = settings.value("rtMidiModule", 0).toInt();
  m_options->rtMidiInputDevice = settings.value("rtMidiInputDevice", "0").toString();
  m_options->rtMidiOutputDevice = settings.value("rtMidiOutputDevice", "").toString();
  m_options->sampleFormat = settings.value("sampleFormat", 0).toInt();
  settings.endGroup();
  settings.beginGroup("Environment");
  m_options->csdocdir = settings.value("csdocdir", DEFAULT_HTML_DIR).toString();
  m_options->opcodedir = settings.value("opcodedir","").toString();
  m_options->opcodedirActive = settings.value("opcodedirActive","").toBool();
  m_options->sadir = settings.value("sadir","").toString();
  m_options->sadirActive = settings.value("sadirActive","").toBool();
  m_options->ssdir = settings.value("ssdir","").toString();
  m_options->ssdirActive = settings.value("ssdirActive","").toBool();
  m_options->sfdir = settings.value("sfdir","").toString();
  m_options->sfdirActive = settings.value("sfdirActive","").toBool();
  m_options->incdir = settings.value("incdir","").toString();
  m_options->incdirActive = settings.value("incdirActive","").toBool();
  m_options->defaultCsd = settings.value("defaultCsd","").toString();
  m_options->defaultCsdActive = settings.value("defaultCsdActive","").toBool();
  m_options->opcodexmldir = settings.value("opcodexmldir", "").toString();
  m_options->opcodexmldirActive = settings.value("opcodexmldirActive","").toBool();
  settings.endGroup();
  settings.beginGroup("External");
  m_options->terminal = settings.value("terminal", DEFAULT_TERM_EXECUTABLE).toString();
  m_options->browser = settings.value("browser", DEFAULT_BROWSER_EXECUTABLE).toString();
  m_options->dot = settings.value("dot", DEFAULT_DOT_EXECUTABLE).toString();
  m_options->waveeditor = settings.value("waveeditor",
                                         DEFAULT_WAVEEDITOR_EXECUTABLE
                                        ).toString();
  m_options->waveplayer = settings.value("waveplayer",
                                         DEFAULT_WAVEPLAYER_EXECUTABLE
                                        ).toString();
  settings.endGroup();
  settings.endGroup();
}

void qutecsound::writeSettings()
{
  QSettings settings("csound", "qutecsound");
  settings.setValue("settingsVersion", 1);
  settings.beginGroup("GUI");
  settings.setValue("pos", pos());
  settings.setValue("size", size());
  settings.setValue("dockstate", saveState());
  settings.setValue("lastuseddir", lastUsedDir);
  settings.setValue("lastfiledir", lastFileDir);
  settings.setValue("language", _configlists.languageCodes[m_options->language]);
  for (int i = 0; i < recentFiles.size();i++) {
    QString key = "recentFiles" + QString::number(i);
    settings.setValue(key, recentFiles[i]);
  }
  settings.beginGroup("Shortcuts");
  for (int i = 0; i < m_keyActions.size();i++) {
//     QString key = m_keyActions[i]->text();
    QString key = QString::number(i);
    settings.setValue(key, m_keyActions[i]->shortcut().toString());
  }
  settings.endGroup();
  settings.endGroup();
  settings.beginGroup("Options");
  settings.beginGroup("Editor");
  settings.setValue("font", m_options->font );
  settings.setValue("fontsize", m_options->fontPointSize);
  settings.setValue("consolefont", m_options->consoleFont );
  settings.setValue("consolefontsize", m_options->consoleFontPointSize);
  settings.setValue("consoleFontColor", QVariant(m_options->consoleFontColor));
  settings.setValue("consoleBgColor", QVariant(m_options->consoleBgColor));
  settings.setValue("tabWidth", m_options->tabWidth );
  settings.setValue("colorvariables", m_options->colorVariables);
  settings.setValue("autoplay", m_options->autoPlay);
  settings.setValue("savechanges", m_options->saveChanges);
  settings.setValue("rememberfile", m_options->rememberFile);
  settings.setValue("savewidgets", m_options->saveWidgets);
  settings.setValue("iconText", m_options->iconText);
  settings.setValue("wrapLines", m_options->wrapLines);
  settings.setValue("enableWidgets", m_options->enableWidgets);
  settings.setValue("invalueEnabled", m_options->invalueEnabled);
//   settings.setValue("chngetEnabled", m_options->chngetEnabled);
  settings.setValue("showWidgetsOnRun", m_options->showWidgetsOnRun);
  settings.setValue("showTooltips", m_options->showTooltips);
  settings.setValue("enableFLTK", m_options->enableFLTK);
  QStringList files;
  if (m_options->rememberFile) {
    for (int i = 0; i < documentPages.size(); i++ ) {
          files.append(documentPages[i]->fileName);
    }
  }
  settings.setValue("lastfiles", files);
  settings.endGroup();
  settings.beginGroup("Run");
  settings.setValue("useAPI", m_options->useAPI);
  settings.setValue("thread", m_options->thread);
  settings.setValue("bufferSize", m_options->bufferSize);
  settings.setValue("bufferSizeActive", m_options->bufferSizeActive);
  settings.setValue("HwBufferSize",m_options->HwBufferSize);
  settings.setValue("HwBufferSizeActive", m_options->HwBufferSizeActive);
  settings.setValue("dither", m_options->dither);
  settings.setValue("additionalFlags", m_options->additionalFlags);
  settings.setValue("additionalFlagsActive", m_options->additionalFlagsActive);
  settings.setValue("fileUseOptions", m_options->fileUseOptions);
  settings.setValue("fileOverrideOptions", m_options->fileOverrideOptions);
  settings.setValue("fileAskFilename", m_options->fileAskFilename);
  settings.setValue("filePlayFinished", m_options->filePlayFinished);
  settings.setValue("fileFileType", m_options->fileFileType);
  settings.setValue("fileSampleFormat", m_options->fileSampleFormat);
  settings.setValue("fileInputFilenameActive", m_options->fileInputFilenameActive);
  settings.setValue("fileInputFilename", m_options->fileInputFilename);
  settings.setValue("fileOutputFilenameActive", m_options->fileOutputFilenameActive);
  settings.setValue("fileOutputFilename", m_options->fileOutputFilename);
  settings.setValue("rtUseOptions", m_options->rtUseOptions);
  settings.setValue("rtOverrideOptions", m_options->rtOverrideOptions);
  settings.setValue("rtAudioModule", m_options->rtAudioModule);
  settings.setValue("rtInputDevice", m_options->rtInputDevice);
  settings.setValue("rtOutputDevice", m_options->rtOutputDevice);
  settings.setValue("rtJackName", m_options->rtJackName);
  settings.setValue("rtMidiModule", m_options->rtMidiModule);
  settings.setValue("rtMidiInputDevice", m_options->rtMidiInputDevice);
  settings.setValue("rtMidiOutputDevice", m_options->rtMidiOutputDevice);
  settings.setValue("sampleFormat", m_options->sampleFormat);
  settings.endGroup();
  settings.beginGroup("Environment");
  settings.setValue("csdocdir", m_options->csdocdir);
  settings.setValue("opcodedir",m_options->opcodedir);
  settings.setValue("opcodedirActive",m_options->opcodedirActive);
  settings.setValue("sadir",m_options->sadir);
  settings.setValue("sadirActive",m_options->sadirActive);
  settings.setValue("ssdir",m_options->ssdir);
  settings.setValue("ssdirActive",m_options->ssdirActive);
  settings.setValue("sfdir",m_options->sfdir);
  settings.setValue("sfdirActive",m_options->sfdirActive);
  settings.setValue("incdir",m_options->incdir);
  settings.setValue("incdirActive",m_options->incdirActive);
  settings.setValue("defaultCsd",m_options->defaultCsd);
  settings.setValue("defaultCsdActive",m_options->defaultCsdActive);
  settings.setValue("opcodexmldir", m_options->opcodexmldir);
  settings.setValue("opcodexmldirActive",m_options->opcodexmldirActive);
  settings.endGroup();
  settings.beginGroup("External");
  settings.setValue("terminal", m_options->terminal);
  settings.setValue("browser", m_options->browser);
  settings.setValue("dot", m_options->dot);
  settings.setValue("waveeditor", m_options->waveeditor);
  settings.setValue("waveplayer", m_options->waveplayer);
  settings.endGroup();
  settings.endGroup();
}

int qutecsound::execute(QString executable, QString options)
{
//   qDebug() << "qutecsound::execute";
  QStringList optionlist;
  optionlist = options.split(QRegExp("\\s+"));

  // cd to current directory on all platforms
  QString cdLine = "cd \"" + documentPages[curPage]->getFilePath() + "\"";
  QProcess::execute(cdLine);

#ifdef MACOSX
  QString commandLine = "open -a \"" + executable + "\" " + options;
#endif
#ifdef LINUX
  QString commandLine = executable + " " + options;
#endif
#ifdef SOLARIS
  QString commandLine = executable + " " + options;
#endif
#ifdef WIN32
  QString commandLine = executable + (executable.startsWith("cmd")? " /k ": " ") + options;
#endif
  QProcess::startDetached(commandLine);
  return 1;
}

void qutecsound::configureHighlighter()
{
  m_highlighter->setOpcodeNameList(opcodeTree->opcodeNameList());
}

bool qutecsound::maybeSave()
{
  for (int i = 0; i< documentPages.size(); i++) {
    if (documentPages[i]->document()->isModified()) {
      documentTabs->setCurrentIndex(i);
      changePage(i);
      QString message = tr("The document ")
          + (documentPages[i]->fileName != "" ? documentPages[i]->fileName: "untitled.csd")
          + tr("\nhas been modified.\nDo you want to save the changes before closing?");
      int ret = QMessageBox::warning(this, tr("QuteCsound"),
                                     message,
                                     QMessageBox::Yes | QMessageBox::Default,
                                     QMessageBox::No,
                                     QMessageBox::Cancel | QMessageBox::Escape);
      if (ret == QMessageBox::Yes) {
        if (!save())
          return false;
//         closeTab();
      }
      else if (ret == QMessageBox::Cancel) {
        return false;
      }
    }
  }
  return true;
}

QString qutecsound::fixLineEndings(const QString &text)
{
  qDebug("n = %i  r = %i", text.count("\n"), text.count("\r"));
  return text;
}

bool qutecsound::loadFile(QString fileName, bool runNow)
{
  QFile file(fileName);
  if (!file.open(QFile::ReadOnly | QFile::Text)) {
    QMessageBox::warning(this, tr("QuteCsound"),
                         tr("Cannot read file %1:\n%2.")
                             .arg(fileName)
                             .arg(file.errorString()));
    return false;
  }
  int index = isOpen(fileName);
  if (index != -1) {
    documentTabs->setCurrentIndex(index);
    changePage(index);
    statusBar()->showMessage(tr("File already open"), 10000);
    return false;
  }
  QApplication::setOverrideCursor(Qt::WaitCursor);
  DocumentPage *newPage = new DocumentPage(this, opcodeTree);
  documentPages.append(newPage);
  documentTabs->addTab(newPage,"");
  curPage = documentPages.size() - 1;
  documentTabs->setCurrentIndex(curPage);
  textEdit = newPage;
  textEdit->setTabStopWidth(m_options->tabWidth);
  textEdit->setLineWrapMode(m_options->wrapLines ? QTextEdit::WidgetWidth : QTextEdit::NoWrap);
  connectActions();
// #ifdef QUTECSOUND_COPYPASTE
  connect(textEdit, SIGNAL(doCut()), this, SLOT(cut()));
  connect(textEdit, SIGNAL(doCopy()), this, SLOT(copy()));
  connect(textEdit, SIGNAL(doPaste()), this, SLOT(paste()));
// #endif

  if (fileName.startsWith(m_options->csdocdir))
    documentPages[curPage]->readOnly = true;
  QString text;
  while (!file.atEnd()) {
    QByteArray line = file.readLine();
    line.replace("\r\n", "\n");
    line.replace("\r", "\n");  //Change Mac returns to line endings
    QTextDecoder decoder(QTextCodec::codecForLocale());
    text = text + decoder.toUnicode(line);
    if (!line.endsWith("\n"))
      text += "\n";
  }
  //textEdit->setPlainText(fixLineEndings(in.readAll()));
//   textEdit->setPlainText(text);
  textEdit->setTextString(text, m_options->saveWidgets);
//   textEdit->setTabStopWidth(m_options->tabWidth);
  m_highlighter->setColorVariables(m_options->colorVariables);
  m_highlighter->setDocument(textEdit->document());
  QApplication::restoreOverrideCursor();

  textEdit->document()->setModified(false);
  if (fileName == ":/default.csd")
    fileName = QString("");
  documentPages[curPage]->fileName = fileName;
  setCurrentFile(fileName);
  setWindowModified(false);
  documentTabs->setTabIcon(curPage, modIcon);
  lastUsedDir = fileName;
  lastUsedDir.resize(fileName.lastIndexOf(QRegExp("[/]")));
  if (recentFiles.count(fileName) == 0 and fileName!="") {
    recentFiles.prepend(fileName);
    recentFiles.removeLast();
    fillFileMenu();
  }
  changeFont();
  statusBar()->showMessage(tr("File loaded"), 2000);
  setWidgetPanelGeometry();
  if (runNow && m_options->autoPlay) {
    runAct->setChecked(true);
    runCsound();
  }
  return true;
}

void qutecsound::loadCompanionFile(const QString &fileName)
{
  QString companionFileName = fileName;
  if (fileName.endsWith(".orc")) {
    companionFileName.replace(".orc", ".sco");
  }
  else if (fileName.endsWith(".sco")) {
    companionFileName.replace(".sco", ".orc");
  }
  else
    return;
  if (QFile::exists(companionFileName))
    loadFile(companionFileName);
}

bool qutecsound::saveFile(const QString &fileName)
{
  qDebug("qutecsound::saveFile");
  QString text;
  // Update widget panel text on save.
  perfMutex.lock();
  documentPages[curPage]->setMacWidgetsText(widgetPanel->widgetsText());
  perfMutex.unlock();
  documentTabs->setTabIcon(curPage, QIcon());
  QApplication::setOverrideCursor(Qt::WaitCursor);
  if (m_options->saveWidgets)
    text = documentPages[curPage]->getFullText();
  else
    text = documentPages[curPage]->toPlainText();
  QApplication::restoreOverrideCursor();

  if (fileName != documentPages[curPage]->fileName) {
    documentPages[curPage]->fileName = fileName;
    setCurrentFile(fileName);
  }
  textEdit->document()->setModified(false);
  setWindowModified(false);
  lastUsedDir = fileName;
  lastUsedDir.resize(fileName.lastIndexOf(QRegExp("[/\\]")));
  if (recentFiles.count(fileName) == 0) {
    recentFiles.prepend(fileName);
    recentFiles.removeLast();
    fillFileMenu();
  }
  QFile file(fileName);
  if (!file.open(QFile::WriteOnly | QFile::Text)) {
    QMessageBox::warning(this, tr("Application"),
                         tr("Cannot write file %1:\n%2.")
                             .arg(fileName)
                             .arg(file.errorString()));
    return false;
  }

  QTextStream out(&file);
  out << text;
  statusBar()->showMessage(tr("File saved"), 2000);
  return true;
}

void qutecsound::setCurrentFile(const QString &fileName)
{
  QString shownName;
  if (fileName.isEmpty())
    shownName = "untitled.csd";
  else
    shownName = strippedName(fileName);

  setWindowTitle(tr("%1[*] - %2").arg(shownName).arg(tr("QuteCsound")));
  documentTabs->setTabText(curPage, shownName);
  updateWidgets();
}

QString qutecsound::strippedName(const QString &fullFileName)
{
  return QFileInfo(fullFileName).fileName();
}

QString qutecsound::generateScript(bool realtime, QString tempFileName)
{
#ifndef WIN32
  QString script = "#!/bin/sh\n";
#else
  QString script = "";
#endif

  QString cmdLine = "";
  if (m_options->opcodedirActive)
    script += "export OPCODEDIR=" + m_options->opcodedir + "\n";
  if (m_options->sadirActive)
    script += "export SADIR=" + m_options->sadir + "\n";
  if (m_options->ssdirActive)
    script += "export SSDIR=" + m_options->ssdir + "\n";
  if (m_options->sfdirActive)
    script += "export SFDIR=" + m_options->sfdir + "\n";
  if (m_options->ssdirActive)
    script += "export INCDIR=" + m_options->incdir + "\n";

#ifndef WIN32
  script += "cd " + QFileInfo(documentPages[curPage]->fileName).absoluteFilePath() + "\n";
#else // WIN32 defined
  QString script_cd = "@pushd " + QFileInfo(documentPages[curPage]->fileName).absolutePath() + "\n";
  script_cd.replace("/", "\\");
  script += script_cd;
#endif

#ifdef MACOSX
  cmdLine = "/usr/local/bin/csound ";
#else
  cmdLine = "csound ";
#endif

  if (tempFileName == ""){
    if (documentPages[curPage]->companionFile != "") {
      if (documentPages[curPage]->fileName.endsWith(".orc"))
        cmdLine += "\""  + documentPages[curPage]->fileName
            + "\" \""+ documentPages[curPage]->companionFile + "\" ";
      else
        cmdLine += "\""  + documentPages[curPage]->companionFile
            + "\" \""+ documentPages[curPage]->fileName + "\" ";
    }
    else if (documentPages[curPage]->fileName.endsWith(".csd")) {
      cmdLine += "\""  + documentPages[curPage]->fileName + "\" ";
    }
  }
  else {
    cmdLine += "\""  + tempFileName + "\" ";
  }

  cmdLine += m_options->generateCmdLineFlags(realtime);
  script += "echo \"" + cmdLine + "\"\n";
  script += cmdLine + "\n";

#ifndef WIN32
  script += "echo \"\nPress return to continue\"\n";
  script += "dummy_var=\"\"\n";
  script += "read dummy_var\n";
  script += "rm $0\n";
#else // WIN32 defined.
  script += "@echo.\n";
  script += "@pause\n";
  script += "@exit\n";
#endif
  return script;
}

void qutecsound::getCompanionFileName()
{
  QString fileName = "";
  QDialog dialog(this);
  dialog.resize(400, 200);
  dialog.setModal(true);
  QPushButton *button = new QPushButton(tr("Ok"));

  connect(button, SIGNAL(released()), &dialog, SLOT(accept()));

  QSplitter *splitter = new QSplitter(&dialog);
  QListWidget *list = new QListWidget(&dialog);
  QCheckBox *checkbox = new QCheckBox(tr("Do not ask again"), &dialog);
  splitter->addWidget(list);
  splitter->addWidget(checkbox);
  splitter->addWidget(button);
  splitter->resize(400, 200);
  splitter->setOrientation(Qt::Vertical);
  QString extensionComplement = "";
  if (documentPages[curPage]->fileName.endsWith(".orc"))
    extensionComplement = ".sco";
  else if (documentPages[curPage]->fileName.endsWith(".sco"))
    extensionComplement = ".orc";

  for (int i = 0; i < documentPages.size(); i++) {
    QString name = documentPages[i]->fileName;
    if (documentPages[i]->fileName.endsWith(extensionComplement))
      list->addItem(documentPages[i]->fileName);
  }
  QList<QListWidgetItem *> itemList = list->findItems(documentPages[curPage]->companionFile,
      Qt::MatchExactly);
  if (itemList.size() > 0)
    list->setCurrentItem(itemList[0]);
  dialog.exec();
  QListWidgetItem *item = list->currentItem();
  QString itemText = item->text();
  if (checkbox->isChecked())
    documentPages[curPage]->askForFile = false;
  documentPages[curPage]->companionFile = itemText;
  for (int i = 0; i < documentPages.size(); i++) {
    if (documentPages[i]->fileName == documentPages[curPage]->companionFile) {
      documentPages[i]->companionFile = documentPages[curPage]->fileName;
      documentPages[i]->askForFile = documentPages[curPage]->askForFile;
      break;
    }
  }
}
void qutecsound::setWidgetPanelGeometry()
{
  QRect geometry = documentPages[curPage]->getWidgetPanelGeometry();
  if (geometry.width() == 0)
    return;
  if (geometry.x() < 0) {
    geometry.setX(10);
	qDebug() << "qutecsound::setWidgetPanelGeometry() Warining: X is negative.";
  }
  if (geometry.y() < 0) {
    geometry.setY(10);
	qDebug() << "qutecsound::setWidgetPanelGeometry() Warining: Y is negative.";
  }
  widgetPanel->setGeometry(geometry);
}

int qutecsound::isOpen(QString fileName)
{
  bool open = false;
  int i = 0;
  for (i = 0; i < documentPages.size(); i++) {
      if (documentPages[i]->fileName == fileName) {
        open = true;
        break;
      }
  }
  return (open ? i: -1);
}

void qutecsound::markErrorLine()
{
//   qDebug("qutecsound::markErrorLine()");
  documentPages[curPage]->markErrorLines(m_console->errorLines);
}

void qutecsound::readWidgetValues(CsoundUserData *ud)
{
  MYFLT* pvalue;
  for (int i = 0; i < ud->qcs->channelNames.size(); i++) {
    if(csoundGetChannelPtr(ud->csound, &pvalue, ud->qcs->channelNames[i].toStdString().c_str(),
        CSOUND_INPUT_CHANNEL | CSOUND_CONTROL_CHANNEL) == 0) {
      *pvalue = (MYFLT) ud->qcs->values[i];
    }
    if(csoundGetChannelPtr(ud->csound, &pvalue, ud->qcs->channelNames[i].toStdString().c_str(),
       CSOUND_INPUT_CHANNEL | CSOUND_STRING_CHANNEL) == 0) {
      char *string = (char *) pvalue;
      strcpy(string, ud->qcs->stringValues[i].toStdString().c_str());
    }
  }
}

void qutecsound::writeWidgetValues(CsoundUserData *ud)
{
//   qDebug("qutecsound::writeWidgetValues");
  MYFLT* pvalue;
  for (int i = 0; i < ud->qcs->channelNames.size(); i++) {
    if (ud->qcs->channelNames[i] != "") {
      if(csoundGetChannelPtr(ud->csound, &pvalue, ud->qcs->channelNames[i].toStdString().c_str(),
         CSOUND_OUTPUT_CHANNEL | CSOUND_CONTROL_CHANNEL) == 0) {
           ud->qcs->widgetPanel->setValue(i,*pvalue);
      }
      else if(csoundGetChannelPtr(ud->csound, &pvalue, ud->qcs->channelNames[i].toStdString().c_str(),
        CSOUND_OUTPUT_CHANNEL | CSOUND_STRING_CHANNEL) == 0) {
        ud->qcs->widgetPanel->setValue(i,QString((char *)pvalue));
      }
    }
  }
}

void qutecsound::processEventQueue(CsoundUserData *ud)
{
  // This function should only be called when Csound is running
  while (ud->qcs->widgetPanel->eventQueueSize > 0) {
    ud->qcs->widgetPanel->eventQueueSize--;
    ud->qcs->widgetPanel->eventQueue[ud->qcs->widgetPanel->eventQueueSize];
    char type = ud->qcs->widgetPanel->eventQueue[ud->qcs->widgetPanel->eventQueueSize][0].unicode();
    QStringList eventElements =
        ud->qcs->widgetPanel->eventQueue[ud->qcs->widgetPanel->eventQueueSize].remove(0,1).split(" ",QString::SkipEmptyParts);
//     qDebug("type %c line: %s", type, ud->qcs->widgetPanel->eventQueue[ud->qcs->widgetPanel->eventQueueSize].toStdString().c_str());
    // eventElements.size() should never be larger than EVENTS_MAX_PFIELDS
    for (int j = 0; j < eventElements.size(); j++) {
      ud->qcs->pFields[j] = (MYFLT) eventElements[j].toDouble();
    }
    if (ud->qcs->m_options->thread) {
#ifdef QUTE_USE_CSOUNDPERFORMANCETHREAD
        //TODO this is not working!!!
//         ud->qcs->perfThread->ScoreEvent(0, type, eventElements.size(), pFields);
      ud->qcs->perfMutex.lock();
      csoundScoreEvent(ud->csound,type, ud->qcs->pFields, eventElements.size());
      ud->qcs->perfMutex.unlock();
#else
      csoundLockMutex(ud->qcs->perfMutex);
      csoundScoreEvent(ud->csound,type, ud->qcs->pFields, eventElements.size());
      csoundUnlockMutex(ud->qcs->perfMutex);
#endif
    }
    else {
      csoundScoreEvent(ud->csound,type ,ud->qcs->pFields, eventElements.size());
    }
  }
}

void qutecsound::queueOutValue(QString channelName, double value)
{
//   outValueQueue.insert();
 // csoundLockMutex(ud->qcs->perfMutex);
  widgetPanel->newValue(QPair<QString, double>(channelName, value));
//  csoundUnlockMutex(ud->qcs->perfMutex);
}

// void qutecsound::queueInValue(QString channelName, double value)
// {
//   inValueQueue.insert(channelName, value);
// }

void qutecsound::queueOutString(QString channelName, QString value)
{
//   qDebug() << "qutecsound::queueOutString";
  stringValueMutex.lock();
  if (outStringQueue.contains(channelName)) {
    outStringQueue[channelName] = value;
  }
  else {
    outStringQueue.insert(channelName, value);
  }
  stringValueMutex.unlock();
}

void qutecsound::queueMessage(QString message)
{
  messageMutex.lock();
  messageQueue << message;
  messageMutex.unlock();
}

#ifdef QUTE_USE_CSOUNDPERFORMANCETHREAD
void qutecsound::csThread(void *data)
#else
uintptr_t qutecsound::csThread(void *data)
#endif
{
  CsoundUserData* udata = (CsoundUserData*)data;
  if(!udata->result) {
    unsigned int numWidgets = udata->qcs->widgetPanel->widgetCount();
    udata->qcs->channelNames.resize(numWidgets*2);
    udata->qcs->values.resize(numWidgets*2);
    udata->qcs->stringValues.resize(numWidgets*2);
    if (udata->qcs->m_options->enableWidgets) {
      udata->qcs->widgetPanel->getValues(&udata->qcs->channelNames,
                                          &udata->qcs->values,
                                          &udata->qcs->stringValues);
    }
    int perform = csoundPerformKsmps(udata->csound);
    udata->outputBufferSize = csoundGetKsmps(udata->csound);
    udata->outputBuffer = csoundGetSpout(udata->csound);
//     int numChnls = csoundGetNchnls(udata->csound);
    while((perform == 0) and (udata->PERF_STATUS == 1)) {
      for (int i = 0; i < udata->outputBufferSize*udata->numChnls; i++) {
        udata->qcs->audioOutputBuffer.put(udata->outputBuffer[i]/ udata->zerodBFS);
      }
      if (udata->qcs->m_options->enableWidgets) {
        udata->qcs->widgetPanel->getValues(&udata->qcs->channelNames,
                                            &udata->qcs->values,
                                            &udata->qcs->stringValues);
      }
//         if (udata->qcs->m_options->chngetEnabled) {
//           writeWidgetValues(udata);
//           readWidgetValues(udata);
//         }
//         processEventQueue(udata);
      perform = csoundPerformKsmps(udata->csound);
    }
  }
//   udata->qcs->stop();
  udata->PERF_STATUS = 0;
#ifdef QUTE_USE_CSOUNDPERFORMANCETHREAD
#else
  return 1;
#endif
}

QStringList qutecsound::runCsoundInternally(QStringList flags)
{
qDebug("qutecsound::runCsoundInternally()");
  static char *argv[33];
  int index = 0;
  foreach (QString flag, flags) {
    argv[index] = (char *) calloc(flag.size()+1, sizeof(char));
    strcpy(argv[index],flag.toStdString().c_str());
    index++;
  }
  int argc = flags.size();
#ifdef MACOSX
//Remember menu bar to set it after FLTK grabs it
  menuBarHandle = GetMenuBar();
#endif
  CSOUND *csoundD;
  csoundD=csoundCreate(0);
  csoundReset(csoundD);
  csoundSetHostData(csoundD, (void *) ud);
  m_deviceMessages.clear();
  csoundSetMessageCallback(csoundD, &qutecsound::messageCallback_Devices);
  int result = csoundCompile(csoundD,argc,argv);
  if(!result){
    while(csoundPerformKsmps(csoundD)==0) {};
  }

  csoundCleanup(csoundD);
  csoundDestroy(csoundD);

#ifdef MACOSX
// Put menu bar back
  SetMenuBar(menuBarHandle);
#endif
  return m_deviceMessages;
}

void qutecsound::newCurve(Curve * curve)
{
  newCurveBuffer.append(curve);
}

void qutecsound::updateCurve(WINDAT *windat)
{
  WINDAT *windat_ = (WINDAT *) malloc(sizeof(WINDAT));
  *windat_ = *windat;
  curveBuffer.append(windat_);
}

int qutecsound::killCurves(CSOUND *csound)
{
  //FIXME: This is not safe, but the memory must be cleared...
//   for (int i = 0; i < curveBuffer.size(); i++) {
//     free(curveBuffer[i]);
//   }
  curveBuffer.clear();
//   widgetPanel->clearGraphs();
  return 0;
}

void qutecsound::outputValueCallback (CSOUND *csound,
                                     const char *channelName,
                                     MYFLT value)
{
  CsoundUserData *ud = (CsoundUserData *) csoundGetHostData(csound);
  if (ud->PERF_STATUS == 1) {
    QString name = QString(channelName);
    ud->qcs->perfMutex.lock();
    if (name.startsWith('$')) {
      QString channelName = name;
      channelName.chop(name.size() - (int) value + 1);
      QString sValue = name;
      sValue = sValue.right(name.size() - (int) value);
      channelName.remove(0,1);
      ud->qcs->queueOutString(channelName, sValue);
    }
    else {
      ud->qcs->queueOutValue(name, value);
    }
    ud->qcs->perfMutex.unlock();
  }
}

void qutecsound::inputValueCallback (CSOUND *csound,
                                     const char *channelName,
                                     MYFLT *value)
{
  // from qutecsound to Csound
  CsoundUserData *ud = (CsoundUserData *) csoundGetHostData(csound);
  if (ud->PERF_STATUS == 1) {
    QString name = QString(channelName);
    ud->qcs->perfMutex.lock();
    if (name.startsWith('$')) {
      int index = ud->qcs->channelNames.indexOf(name.mid(1));
      char *string = (char *) value;
      if (index>=0) {
        strcpy(string, ud->qcs->stringValues[index].toStdString().c_str());
      }
      else {
        string[0] = '\0'; //empty c string
      }
    }
    else {
      int index = ud->qcs->channelNames.indexOf(name);
      if (index>=0)
        *value = (MYFLT) ud->qcs->values[index];
      else {
        *value = 0;
      }
    }
    ud->qcs->perfMutex.unlock();
  }
}

void qutecsound::makeGraphCallback(CSOUND *csound, WINDAT *windat, const char *name)
{
//   qDebug("qutecsound::makeGraph()");
  CsoundUserData *ud = (CsoundUserData *) csoundGetHostData(csound);
  windat->caption[CAPSIZE - 1] = 0; // Just in case...
  Polarity polarity;
    // translate polarities and hope the definition in Csound doesn't change.
  switch (windat->polarity) {
    case NEGPOL:
      polarity = POLARITY_NEGPOL;
      break;
    case POSPOL:
      polarity = POLARITY_POSPOL;
      break;
    case BIPOL:
      polarity = POLARITY_BIPOL;
      break;
    default:
      polarity = POLARITY_NOPOL;
  }
  Curve *curve
      = new Curve(windat->fdata,
                  windat->npts,
                  windat->caption,
                  polarity,
                  windat->max,
                  windat->min,
                  windat->absmax,
                  windat->oabsmax,
                  windat->danflag);
  curve->set_id((uintptr_t) curve);
  ud->qcs->newCurve(curve);
  windat->windid = (uintptr_t) curve;
//   qDebug("qutecsound::makeGraphCallback %i", windat->windid);
}

void qutecsound::drawGraphCallback(CSOUND *csound, WINDAT *windat)
{
  CsoundUserData *udata = (CsoundUserData *) csoundGetHostData(csound);
//   qDebug("qutecsound::drawGraph()");
  udata->qcs->updateCurve(windat);
}

void qutecsound::killGraphCallback(CSOUND *csound, WINDAT *windat)
{
//   udata->qcs->killCurve(windat);
  qDebug("qutecsound::killGraph()");
}

int qutecsound::exitGraphCallback(CSOUND *csound)
{
  qDebug("qutecsound::exitGraph()");
  CsoundUserData *udata = (CsoundUserData *) csoundGetHostData(csound);
  return udata->qcs->killCurves(csound);
}

