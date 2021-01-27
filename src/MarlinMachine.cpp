
#include <QDebug>
#include <QMessageBox>
#include <QTextBlock>
#include <QScrollBar>

#include "parser/gcodeviewparse.h"
#include "MarlinMachine.h"
#include "frmmain.h"
#include "ui_frmmain.h"

MarlinMachine::MarlinMachine(frmMain* frm, Ui::frmMain* ui, CandleConnection& connection)
    : Machine(frm, ui, connection)
{}

void MarlinMachine::onReadyRead(){

    qDebug() << "+++ MarlinMachine::onReadyRead, before while";

    while (m_connection.canReadLine()) {
        qDebug() << "+++ MarlinMachine::onReadyRead, before";

        QString data = m_connection.readLine().trimmed();

        qDebug() << "+++ MarlinMachine::onReadyRead:" << data;

        if(data.length() > 0) {

            // Processed commands
            if (m_commands.length() > 0) {

                static QString response; // Full response string

                if ((m_commands[0].command != "[CTRL+X]" && dataIsEnd(data))) {

                    response.append(data);

                    // Take command from buffer
                    CommandAttributes ca = m_commands.takeFirst();
                    QTextBlock tb = m_ui->txtConsole->document()->findBlockByNumber(ca.consoleIndex);
                    QTextCursor tc(tb);

                    // Restore absolute/relative coordinate system after jog
                    if (ca.command.toUpper() == "$G" && ca.tableIndex == -2) {
                        if (m_ui->chkKeyboardControl->isChecked())
                            m_absoluteCoordinates = response.contains("G90");
                        else
                            if (response.contains("G90"))
                                sendCommand("G90", -1, m_frm->settings()->showUICommands());
                    }

                    // Store origin
                    if (ca.command == "$#" && ca.tableIndex == -2) {
                        qDebug() << "Received offsets:" << response;
                        QRegExp rx(".*G92:([^,]*),([^,]*),([^\\]]*)");

                        if (rx.indexIn(response) != -1) {
                            if (m_settingZeroXY) {
                                m_settingZeroXY = false;
                                m_storedX = toMetric(rx.cap(1).toDouble());
                                m_storedY = toMetric(rx.cap(2).toDouble());
                            } else if (m_settingZeroZ) {
                                m_settingZeroZ = false;
                                m_storedZ = toMetric(rx.cap(3).toDouble());
                            }
                            m_ui->cmdRestoreOrigin->setToolTip(QString(tr("Restore origin:\n%1, %2, %3")).arg(m_storedX).arg(m_storedY).arg(m_storedZ));
                        }
                    }

                    // Homing response
                    if ((ca.command.toUpper() == "$H" || ca.command.toUpper() == "$T") && m_homing)
                        m_homing = false;

                    // Clear command buffer on "M2" & "M30" command (old firmwares)
                    if ((ca.command.contains("M2") || ca.command.contains("M30")) && response.contains("ok") && !response.contains("[Pgm End]")) {
                        m_commands.clear();
                        m_queue.clear();
                    }

                    // Process probing on heightmap mode only from table commands
                    if (ca.command.contains("G38.2") && m_frm->heightMapMode() && ca.tableIndex > -1) {
                        // Get probe Z coordinate
                        // "[PRB:0.000,0.000,0.000:0];ok"
                        QRegExp rx(".*PRB:([^,]*),([^,]*),([^]^:]*)");
                        double z = qQNaN();
                        if (rx.indexIn(response) != -1) {
                            qDebug() << "probing coordinates:" << rx.cap(1) << rx.cap(2) << rx.cap(3);
                            z = toMetric(rx.cap(3).toDouble());
                        }

                        static double firstZ;
                        if (m_probeIndex == -1) {
                            firstZ = z;
                            z = 0;
                        } else {
                            // Calculate delta Z
                            z -= firstZ;

                            // Calculate table indexes
                            int row = trunc(m_probeIndex / m_frm->heightMapModel().columnCount());
                            int column = m_probeIndex - row * m_frm->heightMapModel().columnCount();
                            if (row % 2) column = m_frm->heightMapModel().columnCount() - 1 - column;

                            // Store Z in table
                            m_frm->heightMapModel().setData(m_frm->heightMapModel().index(row, column), z, Qt::UserRole);
                            m_ui->tblHeightMap->update(m_frm->heightMapModel().index(m_frm->heightMapModel().rowCount() - 1 - row, column));
                            m_frm->updateHeightMapInterpolationDrawer();
                        }

                        m_probeIndex++;
                    }

                    // Change state query time on check mode on
                    if (ca.command.contains(QRegExp("$[cC]"))) {
                        m_frm->timerStateQuery().setInterval(response.contains("Enable") ? 1000 : m_frm->settings()->queryStateTime());
                    }

                    // Add response to console
                    if (tb.isValid() && tb.text() == ca.command) {

                        bool scrolledDown = m_ui->txtConsole->verticalScrollBar()->value() == m_ui->txtConsole->verticalScrollBar()->maximum();

                        // Update text block numbers
                        int blocksAdded = response.count("; ");

                        if (blocksAdded > 0) for (int i = 0; i < m_commands.count(); i++) {
                            if (m_commands[i].consoleIndex != -1) m_commands[i].consoleIndex += blocksAdded;
                        }

                        tc.beginEditBlock();
                        tc.movePosition(QTextCursor::EndOfBlock);

                        tc.insertText(" < " + QString(response).replace("; ", "\r\n"));
                        tc.endEditBlock();

                        if (scrolledDown) m_ui->txtConsole->verticalScrollBar()->setValue(m_ui->txtConsole->verticalScrollBar()->maximum());
                    }

                    // Check queue
                    if (m_queue.length() > 0) {
                        CommandQueue cq = m_queue.takeFirst();
                        while ((bufferLength() + cq.command.length() + 1) <= BUFFERLENGTH) {
                            sendCommand(cq.command, cq.tableIndex, cq.showInConsole);
                            if (m_queue.isEmpty())
                                break;
                            else
                                cq = m_queue.takeFirst();
                        }
                    }

                    // Add response to table, send next program commands
                    if (m_processingFile) {

                        // Only if command from table
                        if (ca.tableIndex > -1) {
                            m_frm->currentModel()->setData(m_frm->currentModel()->index(ca.tableIndex, 2), GCodeItem::Processed);
                            m_frm->currentModel()->setData(m_frm->currentModel()->index(ca.tableIndex, 3), response);

                            m_fileProcessedCommandIndex = ca.tableIndex;

                            if (m_ui->chkAutoScroll->isChecked() && ca.tableIndex != -1) {
                                m_ui->tblProgram->scrollTo(m_frm->currentModel()->index(ca.tableIndex + 1, 0));      // TODO: Update by timer
                                m_ui->tblProgram->setCurrentIndex(m_frm->currentModel()->index(ca.tableIndex, 1));
                            }
                        }

                        // Update taskbar progress
#ifdef WINDOWS
                        if (QSysInfo::windowsVersion() >= QSysInfo::WV_WINDOWS7) {
                            if (m_taskBarProgress) m_taskBarProgress->setValue(m_fileProcessedCommandIndex);
                        }
#endif
                        // Process error messages
                        static bool holding = false;
                        static QString errors;

                        if (ca.tableIndex > -1 && response.toUpper().contains("ERROR") && !m_frm->settings()->ignoreErrors()) {
                            errors.append(QString::number(ca.tableIndex + 1) + ": "
                                          + ca.command
                                          + " < " + response + "\n");

                            m_frm->senderErrorBox()->setText(tr("Error message(s) received:\n") + errors);

                            if (!holding) {
                                holding = true;         // Hold transmit while messagebox is visible
                                response.clear();

                                m_connection.write(QString("!"));
                                m_frm->senderErrorBox()->checkBox()->setChecked(false);
                                qApp->beep();
                                int result = m_frm->senderErrorBox()->exec();

                                holding = false;
                                errors.clear();
                                if (m_frm->senderErrorBox()->checkBox()->isChecked()) m_frm->settings()->setIgnoreErrors(true);
                                if (result == QMessageBox::Ignore)
                                    m_connection.write(QString("~"));
                                else
                                    fileAbort();
                            }
                        }

                        // Check transfer complete (last row always blank, last command row = rowcount - 2)
                        if (m_fileProcessedCommandIndex == m_frm->currentModel()->rowCount() - 2
                                || ca.command.contains(QRegExp("M0*2|M30"))) m_transferCompleted = true;
                        // Send next program commands
                        else
                            if(!m_fileEndSent
                               && (m_fileCommandIndex < m_frm->currentModel()->rowCount()) && !holding)
                                sendNextFileCommands();
                    }

                    // Scroll to first line on "M30" command
                    if (ca.command.contains("M30"))
                        m_ui->tblProgram->setCurrentIndex(m_frm->currentModel()->index(0, 1));

                    response.clear();
                } else {
                    response.append(data + "; ");
                }
            } else {
                // Unprocessed responses
                qDebug() << "floating response:" << data;

                m_ui->txtConsole->appendPlainText(data);
            }
        } else {
            // Blank response
//            m_ui->txtConsole->appendPlainText(data);
        }
    }
}

void MarlinMachine::sendNextFileCommands() {
    if (m_queue.length() > 0) return;

    QString command = feedOverride(m_frm->currentModel()->data(m_frm->currentModel()->index(m_fileCommandIndex, 1)).toString());

    while ((bufferLength() + command.length() + 1) <= BUFFERLENGTH
           && m_fileCommandIndex < m_frm->currentModel()->rowCount() - 1
           && !(!m_commands.isEmpty()
           && m_commands.last().command.contains(QRegExp("M0*2|M30")))) {
        m_frm->currentModel()->setData(m_frm->currentModel()->index(m_fileCommandIndex, 2), GCodeItem::Sent);
        sendCommand(command, m_fileCommandIndex, m_frm->settings()->showProgramCommands());
        m_fileCommandIndex++;
        command = feedOverride(m_frm->currentModel()->data(m_frm->currentModel()->index(m_fileCommandIndex, 1)).toString());
    }
}

bool MarlinMachine::dataIsEnd(QString data) {
        QStringList ends;

        ends << "ok";

        foreach (QString str, ends) {
            if (data.contains(str)) return true;
        }

        return false;
}
