
#include "ui_frmmain.h"
#include "frmmain.h"
#include "Machine.h"

Machine::Machine(frmMain *frm, Ui::frmMain *ui, CandleConnection& connection)
    : m_frm(frm)
    , m_ui(ui)
    , m_connection(connection)
{}

void Machine::init(){
    m_lastDrawnLineIndex = 0;
    m_fileProcessedCommandIndex = 0;
    m_transferCompleted = true;
}

double Machine::toMetric(double value)
{
    return m_frm->settings()->units() == 0 ? value : value * 25.4;
}

bool Machine::compareCoordinates(double x, double y, double z)
{
    return m_ui->txtMPosX->text().toDouble() == x &&
            m_ui->txtMPosY->text().toDouble() == y &&
            m_ui->txtMPosZ->text().toDouble() == z;
}


void Machine::sendCommand(const QString& cmd, int tableIndex, bool showInConsole)
{
    if (!m_connection.isOpen()) return;

    QString command = cmd.toUpper();

    qDebug() << "+++ command:" << command;

    // Commands queue
    if ((bufferLength() + command.length() + 1) > BUFFERLENGTH) {
        qDebug() << "+++ queue:" << command;

        CommandQueue cq;

        cq.command = command;
        cq.tableIndex = tableIndex;
        cq.showInConsole = showInConsole;

        m_queue.append(cq);
        return;
    }

    CommandAttributes ca;

//    if (!(command == "$G" && tableIndex < -1) && !(command == "$#" && tableIndex < -1)
//            && (!m_transferringFile || (m_transferringFile && m_showAllCommands) || tableIndex < 0)) {
    if (showInConsole) {
        m_ui->txtConsole->appendPlainText(command);
        ca.consoleIndex = m_ui->txtConsole->blockCount() - 1;
    } else {
        ca.consoleIndex = -1;
    }

    ca.command = command;
    ca.length = command.length() + 1;
    ca.tableIndex = tableIndex;

    m_commands.append(ca);

    // Processing spindle speed only from g-code program
    QRegExp s("[Ss]0*(\\d+)");
    if (s.indexIn(command) != -1 && ca.tableIndex > -2) {
        int speed = s.cap(1).toInt();
        if (m_ui->slbSpindle->value() != speed) {
            m_ui->slbSpindle->setValue(speed);
        }
    }

    // Set M2 & M30 commands sent flag
    if (command.contains(QRegExp("M0*2|M30"))) {
        m_fileEndSent = true;
    }

    m_connection.write((command + "\r").toLatin1());
}

int Machine::bufferLength()
{
    int length = 0;

    foreach (CommandAttributes ca, m_commands) {
        length += ca.length;
    }

    return length;
}

QString Machine::feedOverride(QString command)
{
    // Feed override if not in heightmap probing mode
//    if (!ui->cmdHeightMapMode->isChecked()) command = GcodePreprocessorUtils::overrideSpeed(command, ui->chkFeedOverride->isChecked() ?
//        ui->txtFeed->value() : 100, &m_originalFeed);

    return command;
}



void Machine::onCommError(int error)
{
    static int previousError;

    if (error != QSerialPort::NoError && error != previousError) {
        previousError = error;
        m_ui->txtConsole->appendPlainText(tr("Connection error ") + QString::number(error) + ": " + m_connection.errorString());
        if (m_connection.isOpen()) {
            m_connection.close();
            m_frm->updateControlsState();
        }
    }
}

void Machine::onReadyRead(){}

void Machine::sendNextFileCommands(){}
void Machine::onTimerConnection(){}
void Machine::onTimerStateQuery(){}

void Machine::clear() {
  if (m_queue.length() > 0) {
      m_commands.clear();
      m_queue.clear();
  }
}

void Machine::fileCmdIndex(int cmdIndex) {
    m_fileCommandIndex = cmdIndex;
    m_fileProcessedCommandIndex = cmdIndex;
}

void Machine::startFile() {
    m_transferCompleted = false;
    m_processingFile = true;
    m_fileEndSent = false;
}

// Reset file progress
void Machine::resetFileProgress(int cmdIndex) {
    fileCmdIndex(cmdIndex);
    m_lastDrawnLineIndex = 0;
}

// Reset file
void Machine::resetFile(int cmdIndex) {
    resetFileProgress(cmdIndex);
    m_probeIndex = -1;
}

void Machine::storeParserState(){}
void Machine::restoreParserState(){}
void Machine::storeOffsets(){}
void Machine::restoreOffsets(){}

void Machine::jogStep(){}
void Machine::cmdStop(){}
void Machine::cmdCommandSend()
{
    QString command = m_ui->cboCommand->currentText();
    if (command.isEmpty())
        return;

    m_ui->cboCommand->storeText();
    m_ui->cboCommand->setCurrentText("");
    sendCommand(command, -1);
}
void Machine::cmdHome(){}
void Machine::cmdZeroXY(){}
void Machine::cmdZeroZ(){}
void Machine::restoreOrigin(){}
void Machine::machineReset(){}
void Machine::cmdUnlock() {}
void Machine::cmdSafePosition() {}
void Machine::cmdSpindle(bool checked) {Q_UNUSED(checked);}
void Machine::fileAbort(){}
void Machine::testMode(bool checked) {Q_UNUSED(checked);}
void Machine::storeCoordinateSystem(bool checked){Q_UNUSED(checked);}

