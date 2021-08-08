
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
{
    m_status << "Init"
             << "Reset"
             << "Alarm"
             << "Idle"
             << "End"
             << "Running"
             << "Hold"
             << "Probe"
             << "Cycling"
             << "Homing"
             << "Jogging"
             << "Error";

    m_statusCaptions << tr("Init")
                     << tr("Reset")
                     << tr("Alarm")
                     << tr("Idle")
                     << tr("End")
                     << tr("Running")
                     << tr("Hold")
                     << tr("Probe")
                     << tr("Cycling")
                     << tr("Homing")
                     << tr("Jogging")
                     << tr("Error");

    m_statusBackColors << "palette(button)"
                      << "palette(button)"
                      << "red"
                      << "palette(button)"
                      << "yellow"
                      << "lime"
                      << "yellow"
                      << "yellow"
                      << "yellow"
                      << "lime"
                      << "lime"
                      << "red";
   m_statusForeColors << "palette(text)"
                      << "palette(text)"
                      << "white"
                      << "palette(text)"
                      << "black"
                      << "black"
                      << "black"
                      << "black"
                      << "black"
                      << "black"
                      << "white";
}

void MarlinMachine::endOfRunProc()
{
    // Test for job complete
    if (m_processingFile && m_transferCompleted && m_endOfRun ) {

        qDebug() << "job completed:" << m_fileCommandIndex << m_frm->currentModel()->rowCount() - 1;

        // Shadow last segment
        GcodeViewParse *parser = m_frm->currentDrawer()->viewParser();
        QList<LineSegment*> list = parser->getLineSegmentList();
        if (m_lastDrawnLineIndex < list.count()) {
            list[m_lastDrawnLineIndex]->setDrawn(true);
            m_frm->currentDrawer()->update(QList<int>() << m_lastDrawnLineIndex);
        }

        // Update state
        m_processingFile = false;
        m_fileProcessedCommandIndex = 0;
        m_lastDrawnLineIndex = 0;
        m_storedParserStatus.clear();

        m_frm->updateControlsState();

        qApp->beep();

        m_frm->timerStateQuery().stop();
        m_frm->timerConnection().stop();

        QMessageBox::information((QWidget*)m_frm, qApp->applicationDisplayName(), tr("Job done.\nTime elapsed: %1")
                                 .arg(m_ui->glwVisualizer->spendTime().toString("hh:mm:ss")));

        m_frm->timerStateQuery().setInterval(m_frm->settings()->queryStateTime());
        m_frm->timerConnection().start();
        m_frm->timerStateQuery().start();
    }
}

void MarlinMachine::parseResponse(const QString& data) {

    // Update machine coordinates
    static QRegExp mpx("^X:([^\\s]*)\\sY:([^\\s]*)\\sZ:([^\\s]*)");
    static QRegExp stx("^S_XYZ:([\\d]*)");
    static QRegExp tmcs("^X:([^\\t.+]*)\\tY:([^\\t.+]*)\\tZ:([^\\t.+]*)");
    static QRegExp echx0("^echo:([^:]*)");
    static QRegExp echx1("^echo:([^:]*):([^:]*)");

    int status = M_INIT;

    qDebug() << "+++ parseResponse: " << data;

    // Status
    if (stx.indexIn(data) != -1) {

        m_statusReceived = true;

        qDebug() << "+++ S_XYZ: " << stx.cap(1);

        status = stx.cap(1).toInt();
        if(status < 0 || status > m_status.size())
                status = 0;

        // Update status
        if (status != m_lastMarlinStatus) {
            m_ui->txtStatus->setText(m_statusCaptions[status]);
            m_ui->txtStatus->setStyleSheet(QString("background-color: %1; color: %2;")
                                         .arg(m_statusBackColors[status]).arg(m_statusForeColors[status]));
        }

        // Update controls
        m_ui->cmdRestoreOrigin->setEnabled(status == M_IDLE);
        m_ui->cmdSafePosition->setEnabled(status == M_IDLE);
        m_ui->cmdZeroXY->setEnabled(status == M_IDLE);
        m_ui->cmdZeroZ->setEnabled(status == M_IDLE);
        m_ui->chkTestMode->setEnabled(status != M_RUNNING && !m_processingFile);
        m_ui->cmdFilePause->setChecked(status == M_HOLD);
        m_ui->cmdSpindle->setEnabled(!m_processingFile || status == M_HOLD);
#ifdef WINDOWS
        if (QSysInfo::windowsVersion() >= QSysInfo::WV_WINDOWS7) {
            if (m_taskBarProgress) m_taskBarProgress->setPaused(status == HOLD0 || status == HOLD1 || status == QUEUE);
        }
#endif

        // Update "elapsed time" timer
        if (m_processingFile) {
            QTime time(0, 0, 0);
            int elapsed = m_frm->startTime().elapsed();
            m_ui->glwVisualizer->setSpendTime(time.addMSecs(elapsed));
        }

        qDebug() << "+++ STATS, m_processingFile: " << m_processingFile << ", m_transferCompleted:" << m_transferCompleted;

        m_endOfRun = status == M_IDLE && (m_lastMarlinStatus == M_RUNNING || m_lastMarlinStatus == M_PROBE);

        m_lastMarlinStatus = status;
    }
    else
    // Leveling status
    // Process probing on heightmap mode only from table commands
    if(!m_commands.isEmpty()
        && m_commands.first().command.contains("G29")
        && m_frm->heightMapMode()
        && m_commands.first().tableIndex > -1) {

        static QRegExp levels("^Bed\\sX:\\s([^\\s]*)\\sY:\\s([^\\s]*)\\sZ:\\s([^\\s]*)");

        if (levels.indexIn(data) != -1) {
            qDebug() << "+++ Bed X:Y:Z: " << levels.cap(1) << ", " << levels.cap(2) << ", " << levels.cap(3);
            //            double x = levels.cap(1).toDouble();
            //            double y = levels.cap(2).toDouble();

            // Get probe Z coordinate
            double z = levels.cap(3).toDouble();

            // Calculate table indexes
            int row = trunc(m_probeIndex / m_frm->heightMapModel().columnCount());
            int column = m_probeIndex - row * m_frm->heightMapModel().columnCount();
            if (row % 2) column = m_frm->heightMapModel().columnCount() - 1 - column;

            // Store Z in table
            m_frm->heightMapModel().setData(m_frm->heightMapModel().index(row, column), z, Qt::UserRole);
            m_ui->tblHeightMap->update(m_frm->heightMapModel().index(m_frm->heightMapModel().rowCount() - 1 - row, column));
            m_frm->updateHeightMapInterpolationDrawer();

            m_probeIndex++;
        }
    } else
    if (mpx.indexIn(data) != -1) {
        qDebug() << "+++ X:Y:Z: " << mpx.cap(1) << ", " << mpx.cap(2) << ", " << mpx.cap(3);
        m_ui->txtMPosX->setText(mpx.cap(1));
        m_ui->txtMPosY->setText(mpx.cap(2));
        m_ui->txtMPosZ->setText(mpx.cap(3));
    } else
    // TMC driver status
    if (tmcs.indexIn(data) != -1) {
        qDebug() << "+++ TMC X:Y:Z: " << tmcs.cap(1) << ", " << tmcs.cap(2) << ", " << tmcs.cap(3);
    } else
    if (echx0.indexIn(data) != -1) {
        qDebug() << "+++ ECHO: " << echx0.cap(1) << ", " << echx0.captureCount();
    } else
    if (echx1.indexIn(data) != -1) {
        qDebug() << "+++ ECHO: " << echx1.cap(1) << ", " << echx1.cap(2) << ", " << echx1.captureCount();
    } else {
        response.append(data + ";");
    }
}

void MarlinMachine::onReadyRead(){
    while (m_connection.canReadLine()) {
        QString rcvData = m_connection.readLine().trimmed();

        qDebug() << "+++ DATA:" << rcvData;

        if(rcvData.length() > 0) {

            // Processed commands
            if (m_commands.length() > 0) {

                if (dataIsCmdResponse(rcvData)) {
                    response.append(rcvData); // Add "ok" to the response

                    // Take command from buffer
                    CommandAttributes ca = m_commands.takeFirst();
                    QTextBlock tb = m_ui->txtConsole->document()->findBlockByNumber(ca.consoleIndex);
                    QTextCursor tc(tb);

                    qDebug() << "+++ COMMAND:" << ca.command << ", ca.tableIndex:"<< ca.tableIndex;

                    // Restore absolute/relative coordinate system after jog
                    if (ca.command.toUpper() == "M115" && ca.tableIndex == -2) {
                        qDebug() << "+++ M115:" << response;
                    }

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
                    if ((ca.command.toUpper() == "G28") && m_homing)
                        m_homing = false;

                    // Clear command buffer on "M2" & "M30" command (old firmwares)
                    if (ca.command.contains("M400") && response.contains("ok") && !response.contains("[Pgm End]")) {
                        m_commands.clear();
                        m_queue.clear();
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

                        GcodeViewParse *parser = m_frm->currentDrawer()->viewParser();
                        QList<LineSegment*> list = parser->getLineSegmentList();

                        // Store work offset
                        static QVector3D workOffset;

                        //m_lastDrawnLineIndex = m_frm->currentModel()->data(m_frm->currentModel()->index(m_fileProcessedCommandIndex, 4)).toInt();
                        m_lastDrawnLineIndex = m_fileProcessedCommandIndex;

                        if (m_lastDrawnLineIndex < list.size()) {

                            auto vec = list.at(m_lastDrawnLineIndex)->getStart();

                            m_ui->txtMPosX->setText(QString::number(vec.x(), 'f', 3));
                            m_ui->txtMPosY->setText(QString::number(vec.y(), 'f', 3));
                            m_ui->txtMPosZ->setText(QString::number(vec.z(), 'f', 3));

                            workOffset = QVector3D(0.0, 0.0, 0.0);

                            // Update work coordinates
                            int prec = m_frm->settings()->units() == 0 ? 3 : 4;
                            m_ui->txtWPosX->setText(QString::number(m_ui->txtMPosX->text().toDouble() - workOffset.x(), 'f', prec));
                            m_ui->txtWPosY->setText(QString::number(m_ui->txtMPosY->text().toDouble() - workOffset.y(), 'f', prec));
                            m_ui->txtWPosZ->setText(QString::number(m_ui->txtMPosZ->text().toDouble() - workOffset.z(), 'f', prec));

                        }

                        // Update tool position
                        QVector3D toolPosition;
                        if (m_lastDrawnLineIndex < m_frm->currentModel()->rowCount() - 1) {
                            toolPosition = QVector3D(toMetric(m_ui->txtWPosX->text().toDouble()),
                                                     toMetric(m_ui->txtWPosY->text().toDouble()),
                                                     toMetric(m_ui->txtWPosZ->text().toDouble()));
                            m_frm->toolDrawer().setToolPosition(m_frm->codeDrawer()->getIgnoreZ() ? QVector3D(toolPosition.x(), toolPosition.y(), 0) : toolPosition);
                        }

                        // toolpath shadowing
                            bool toolOntoolpath = false;

                            QList<int> drawnLines;

                            for (int i = m_lastDrawnLineIndex; i < list.count()
                                 && list.at(i)->getLineNumber()
                                 <= (m_frm->currentModel()->data(m_frm->currentModel()->index(m_fileProcessedCommandIndex, 4)).toInt() + 1); i++) {
                                if (list.at(i)->contains(toolPosition)) {
                                    toolOntoolpath = true;
                                    m_lastDrawnLineIndex = i;
                                    break;
                                }
                                drawnLines << i;
                            }

                            if (toolOntoolpath) {
                                foreach (int i, drawnLines) {
                                    list.at(i)->setDrawn(true);
                                }
                                if (!drawnLines.isEmpty())
                                    m_frm->currentDrawer()->update(drawnLines);
                            } else
                                if (m_lastDrawnLineIndex < list.count()) {
                                    qDebug() << "tool missed:" << list.at(m_lastDrawnLineIndex)->getLineNumber()
                                             << m_frm->currentModel()->data(m_frm->currentModel()->index(m_fileProcessedCommandIndex, 4)).toInt()
                                             << m_fileProcessedCommandIndex;
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

                                m_connection.write(QString("P000"));
                                m_frm->senderErrorBox()->checkBox()->setChecked(false);
                                qApp->beep();
                                int result = m_frm->senderErrorBox()->exec();

                                holding = false;
                                errors.clear();
                                if (m_frm->senderErrorBox()->checkBox()->isChecked()) m_frm->settings()->setIgnoreErrors(true);
                                if (result == QMessageBox::Ignore)
                                    m_connection.write(QString("R000"));
                                else
                                    fileAbort();
                            }
                        }

                        qDebug() << "+++ EOF: m_fileProcessedCommandIndex " << m_fileProcessedCommandIndex << ", m_frm->currentModel()->rowCount() " << m_frm->currentModel()->rowCount();

                        // Check transfer complete (last row always blank, last command row = rowcount - 2)
                        if (m_fileProcessedCommandIndex == m_frm->currentModel()->rowCount() - 2
                            || ca.command.contains(QRegExp("M400"))) {
                                m_transferCompleted = true;
                                endOfRunProc();
                        }
                        // Send next program commands
                        else
                            if(!m_fileEndSent
                               && (m_fileCommandIndex < m_frm->currentModel()->rowCount())
                               && !holding) {
                                sendNextFileCommands();
                            }
                    }

                    // Scroll to first line on "M30" command
                    if (ca.command.contains("M30"))
                        m_ui->tblProgram->setCurrentIndex(m_frm->currentModel()->index(0, 1));

                    response.clear();
                } else {
                    parseResponse(rcvData);
                    qDebug() << "+++ RESPONSE:" << response;
                }
            } else {
                // Unprocessed responses
                qDebug() << "floating response:" << rcvData;

                m_ui->txtConsole->appendPlainText(rcvData);
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
           && m_commands.last().command.contains("M400"))) {
        m_frm->currentModel()->setData(m_frm->currentModel()->index(m_fileCommandIndex, 2), GCodeItem::Sent);
        sendCommand(command, m_fileCommandIndex, m_frm->settings()->showProgramCommands());
        m_fileCommandIndex++;
        command = feedOverride(m_frm->currentModel()->data(m_frm->currentModel()->index(m_fileCommandIndex, 1)).toString());
    }
}

bool MarlinMachine::dataIsCmdResponse(QString data) {
        QStringList ends;

        ends << "ok";

        foreach (QString str, ends) {
            if (data.contains(str)) return true;
        }

        return false;
}

void MarlinMachine::cmdHome()
{
    m_homing = true;
    m_updateSpindleSpeed = true;
    sendCommand("G28", -1, m_frm->settings()->showUICommands());
}

void MarlinMachine::cmdZeroXY()
{
    m_settingZeroXY = true;
    sendCommand("G28 X Y", -1, m_frm->settings()->showUICommands());
}

void MarlinMachine::cmdZeroZ()
{
    m_settingZeroZ = true;
    sendCommand("G28 Z", -1, m_frm->settings()->showUICommands());
}

void MarlinMachine::fileAbort()
{
    m_aborting = true;
    if (!m_ui->chkTestMode->isChecked()) {
        m_connection.write(QString("M112"));
    } else {
        machineReset();
    }
}

void MarlinMachine::cmdPause(bool checked)
{
    m_connection.write(QString(checked ? "P000" : "R000"));
}

void MarlinMachine::cmdProbe(int gridPointsX, int gridPointsY, const QRectF &borderRect)
{
    m_frm->probeModel().setData(m_frm->probeModel().index(m_frm->probeModel().rowCount() - 1, 1), QString("G21G90F%1G0Z%2").
                         arg(m_frm->settings()->heightmapProbingFeed()).arg(m_ui->txtHeightMapGridZTop->value()));
    m_frm->probeModel().setData(m_frm->probeModel().index(m_frm->probeModel().rowCount() - 1, 1), QString("G29 X%1 Y%2 L%3 R%4 F%5 B%6 V3")
                                .arg(gridPointsX)
                                .arg(gridPointsY)
                                .arg(borderRect.left(), 0, 'f', 3)
                                .arg(borderRect.right(), 0, 'f', 3)
                                .arg(borderRect.bottom(), 0, 'f', 3)
                                .arg(borderRect.top(), 0, 'f', 3));

    m_probeIndex = 0;
    m_endOfRun = false;
}

