
#include <QDebug>
#include <QMessageBox>
#include <QTextBlock>
#include <QScrollBar>

#include "GrblMachine.h"
#include "ui_frmmain.h"
#include "frmmain.h"

GrblMachine::GrblMachine(frmMain* frm, Ui::frmMain* ui, CandleConnection& connection)
 : Machine(frm, ui, connection)
{
    m_status << "Unknown"
             << "Idle"
             << "Alarm"
             << "Run"
             << "Home"
             << "Hold:0"
             << "Hold:1"
             << "Queue"
             << "Check"
             << "Door"                     // TODO: Update "Door" state
             << "Jog";
    m_statusCaptions << tr("Unknown")
                     << tr("Idle")
                     << tr("Alarm")
                     << tr("Run")
                     << tr("Home")
                     << tr("Hold")
                     << tr("Hold")
                     << tr("Queue")
                     << tr("Check")
                     << tr("Door")
                     << tr("Jog");
    m_statusBackColors << "red"
                      << "palette(button)"
                      << "red"
                      << "lime"
                      << "lime"
                      << "yellow"
                      << "yellow"
                      << "yellow"
                      << "palette(button)"
                      << "red"
                      << "lime";
   m_statusForeColors << "white"
                      << "palette(text)"
                      << "white"
                      << "black"
                      << "black"
                      << "black"
                      << "black"
                      << "black"
                      << "palette(text)"
                      << "white"
                      << "black";
}

bool GrblMachine::dataIsReset(QString data) {
    return QRegExp("^GRBL|GCARVIN\\s\\d\\.\\d.").indexIn(data.toUpper()) != -1;
}

void GrblMachine::sendCommand(const QString &command, int tableIndex, bool showInConsole = false) {
   if (!m_resetCompleted)
      return;

   Machine::sendCommand(command, tableIndex, showInConsole);
}

void GrblMachine::onReadyRead()
{
    while (m_connection.canReadLine()) {
        QString data = m_connection.readLine().trimmed();

        // Filter prereset responses
        if (m_reseting) {
            qDebug() << "reseting filter:" << data;
            if (!dataIsReset(data)) continue;
            else {
                m_reseting = false;
                m_frm->timerStateQuery().setInterval(m_frm->settings()->queryStateTime());
            }
        }

        // Status response
        if (data[0] == '<') {
            int status = UNKNOWN;

            m_statusReceived = true;

            // Update machine coordinates
            static QRegExp mpx("MPos:([^,]*),([^,]*),([^,^>^|]*)");
            if (mpx.indexIn(data) != -1) {
                m_ui->txtMPosX->setText(mpx.cap(1));
                m_ui->txtMPosY->setText(mpx.cap(2));
                m_ui->txtMPosZ->setText(mpx.cap(3));
            }

            // Status
            static QRegExp stx("<([^,^>^|]*)");
            if (stx.indexIn(data) != -1) {
                status = m_status.indexOf(stx.cap(1));

                // Undetermined status
                if (status == -1) status = 0;

                // Update status
                if (status != m_lastGrblStatus) {
                    m_ui->txtStatus->setText(m_statusCaptions[status]);
                    m_ui->txtStatus->setStyleSheet(QString("background-color: %1; color: %2;")
                                                 .arg(m_statusBackColors[status]).arg(m_statusForeColors[status]));
                }

                // Update controls
                m_ui->cmdRestoreOrigin->setEnabled(status == IDLE);
                m_ui->cmdSafePosition->setEnabled(status == IDLE);
                m_ui->cmdZeroXY->setEnabled(status == IDLE);
                m_ui->cmdZeroZ->setEnabled(status == IDLE);
                m_ui->chkTestMode->setEnabled(status != RUN && !m_processingFile);
                m_ui->chkTestMode->setChecked(status == CHECK);
                m_ui->cmdFilePause->setChecked(status == HOLD0 || status == HOLD1 || status == QUEUE);
                m_ui->cmdSpindle->setEnabled(!m_processingFile || status == HOLD0);
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

                // Test for job complete
                if (m_processingFile && m_transferCompleted &&
                        ((status == IDLE && m_lastGrblStatus == RUN) || status == CHECK)) {
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

                // Store status
                if (status != m_lastGrblStatus) m_lastGrblStatus = status;

                // Abort
                static double x = sNan;
                static double y = sNan;
                static double z = sNan;

                if (m_aborting) {
                    switch (status) {
                    case IDLE: // Idle
                        if (!m_processingFile && m_resetCompleted) {
                            m_aborting = false;
                            restoreOffsets();
                            restoreParserState();
                            return;
                        }
                        break;
                    case HOLD0: // Hold
                    case HOLD1:
                    case QUEUE:
                        if (!m_reseting && compareCoordinates(x, y, z)) {
                            x = sNan;
                            y = sNan;
                            z = sNan;
                            machineReset();
                        } else {
                            x = m_ui->txtMPosX->text().toDouble();
                            y = m_ui->txtMPosY->text().toDouble();
                            z = m_ui->txtMPosZ->text().toDouble();
                        }
                        break;
                    }
                }
            }

            // Store work offset
            static QVector3D workOffset;
            static QRegExp wpx("WCO:([^,]*),([^,]*),([^,^>^|]*)");

            if (wpx.indexIn(data) != -1)
            {
                workOffset = QVector3D(wpx.cap(1).toDouble(), wpx.cap(2).toDouble(), wpx.cap(3).toDouble());
            }

            // Update work coordinates
            int prec = m_frm->settings()->units() == 0 ? 3 : 4;
            m_ui->txtWPosX->setText(QString::number(m_ui->txtMPosX->text().toDouble() - workOffset.x(), 'f', prec));
            m_ui->txtWPosY->setText(QString::number(m_ui->txtMPosY->text().toDouble() - workOffset.y(), 'f', prec));
            m_ui->txtWPosZ->setText(QString::number(m_ui->txtMPosZ->text().toDouble() - workOffset.z(), 'f', prec));

            // Update tool position
            QVector3D toolPosition;
            if (!(status == CHECK && m_fileProcessedCommandIndex < m_frm->currentModel()->rowCount() - 1)) {
                toolPosition = QVector3D(toMetric(m_ui->txtWPosX->text().toDouble()),
                                         toMetric(m_ui->txtWPosY->text().toDouble()),
                                         toMetric(m_ui->txtWPosZ->text().toDouble()));
                m_frm->toolDrawer().setToolPosition(m_frm->codeDrawer()->getIgnoreZ() ? QVector3D(toolPosition.x(), toolPosition.y(), 0) : toolPosition);
            }


            // toolpath shadowing
            if (m_processingFile && status != CHECK) {
                GcodeViewParse *parser = m_frm->currentDrawer()->viewParser();

                bool toolOntoolpath = false;

                QList<int> drawnLines;
                QList<LineSegment*> list = parser->getLineSegmentList();

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
                    if (!drawnLines.isEmpty()) m_frm->currentDrawer()->update(drawnLines);
                } else if (m_lastDrawnLineIndex < list.count()) {
                    qDebug() << "tool missed:" << list.at(m_lastDrawnLineIndex)->getLineNumber()
                             << m_frm->currentModel()->data(m_frm->currentModel()->index(m_fileProcessedCommandIndex, 4)).toInt()
                             << m_fileProcessedCommandIndex;
                }
            }

            // Get overridings
            static QRegExp ov("Ov:([^,]*),([^,]*),([^,^>^|]*)");
            if (ov.indexIn(data) != -1)
            {
                m_frm->updateOverride(m_ui->slbFeedOverride, ov.cap(1).toInt(), 0x91);
                m_frm->updateOverride(m_ui->slbSpindleOverride, ov.cap(3).toInt(), 0x9a);

                int rapid = ov.cap(2).toInt();
                m_ui->slbRapidOverride->setCurrentValue(rapid);

                int target = m_ui->slbRapidOverride->isChecked() ? m_ui->slbRapidOverride->value() : 100;

                if (rapid != target) switch (target) {
                case 25:
                    m_connection.write(QByteArray(1, char(0x97)));
                    break;
                case 50:
                    m_connection.write(QByteArray(1, char(0x96)));
                    break;
                case 100:
                    m_connection.write(QByteArray(1, char(0x95)));
                    break;
                }

                // Update pins state
                QString pinState;
                static QRegExp pn("Pn:([^|^>]*)");
                if (pn.indexIn(data) != -1) {
                    pinState.append(QString(tr("PS: %1")).arg(pn.cap(1)));
                }

                // Process spindle state
                static QRegExp as("A:([^,^>^|]+)");
                if (as.indexIn(data) != -1) {
                    QString state = as.cap(1);
                    m_spindleCW = state.contains("S");
                    if (state.contains("S") || state.contains("C")) {
                        m_frm->timerToolAnimation().start(25, this);
                        m_ui->cmdSpindle->setChecked(true);
                    } else {
                        m_frm->timerToolAnimation().stop();
                        m_ui->cmdSpindle->setChecked(false);
                    }

                    if (!pinState.isEmpty()) pinState.append(" / ");
                    pinState.append(QString(tr("AS: %1")).arg(as.cap(1)));
                } else {
                    m_frm->timerToolAnimation().stop();
                    m_ui->cmdSpindle->setChecked(false);
                }
                m_ui->glwVisualizer->setPinState(pinState);
            }

            // Get feed/spindle values
            static QRegExp fs("FS:([^,]*),([^,^|^>]*)");
            if (fs.indexIn(data) != -1) {
                m_ui->glwVisualizer->setSpeedState((QString(tr("F/S: %1 / %2")).arg(fs.cap(1)).arg(fs.cap(2))));
            }

        } else if (data.length() > 0) {

            // Processed commands
            if (m_commands.length() > 0
                && !dataIsFloating(data)
                && !(m_commands[0].command != "[CTRL+X]" && dataIsReset(data))) {

                static QString response; // Full response string

                if ((m_commands[0].command != "[CTRL+X]" && dataIsEnd(data))
                    || (m_commands[0].command == "[CTRL+X]" && dataIsReset(data))) {

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

                    // Jog
                    if (ca.command.toUpper().contains("$J=") && ca.tableIndex == -2) {
                        jogStep();
                    }

                    // Process parser status
                    if (ca.command.toUpper() == "$G" && ca.tableIndex == -3) {
                        // Update status in visualizer window
                        m_ui->glwVisualizer->setParserStatus(response.left(response.indexOf("; ")));

                        // Store parser status
                        if (m_processingFile) storeParserState();

                        // Spindle speed
                        QRegExp rx(".*S([\\d\\.]+)");
                        if (rx.indexIn(response) != -1) {
                            double speed = toMetric(rx.cap(1).toDouble()); //RPM in imperial?
                            m_ui->slbSpindle->setCurrentValue(speed);
                        }

                        m_updateParserStatus = true;
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

                    // Reset complete
                    if (ca.command == "[CTRL+X]") {
                        m_resetCompleted = true;
                        m_updateParserStatus = true;
                    }

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

                    // Toolpath shadowing on check mode
                    if (m_statusCaptions.indexOf(m_ui->txtStatus->text()) == CHECK) {
                        GcodeViewParse *parser = m_frm->currentDrawer()->viewParser();
                        QList<LineSegment*> list = parser->getLineSegmentList();

                        if (!m_transferCompleted && m_fileProcessedCommandIndex < m_frm->currentModel()->rowCount() - 1) {
                            int i;
                            QList<int> drawnLines;

                            for (i = m_lastDrawnLineIndex; i < list.count()
                                 && list.at(i)->getLineNumber()
                                 <= (m_frm->currentModel()->data(m_frm->currentModel()->index(m_fileProcessedCommandIndex, 4)).toInt()); i++) {
                                drawnLines << i;
                            }

                            if (!drawnLines.isEmpty() && (i < list.count())) {
                                m_lastDrawnLineIndex = i;
                                QVector3D vec = list.at(i)->getEnd();
                                m_frm->toolDrawer().setToolPosition(vec);
                            }

                            foreach (int i, drawnLines) {
                                list.at(i)->setDrawn(true);
                            }
                            if (!drawnLines.isEmpty()) m_frm->currentDrawer()->update(drawnLines);
                        } else {
                            foreach (LineSegment* s, list) {
                                if (!qIsNaN(s->getEnd().length())) {
                                    m_frm->toolDrawer().setToolPosition(s->getEnd());
                                    break;
                                }
                            }
                        }
                    }

                    response.clear();
                } else {
                    response.append(data + "; ");
                }

            } else {
                // Unprocessed responses
                qDebug() << "floating response:" << data;

                // Handle hardware reset
                if (dataIsReset(data)) {
                    qDebug() << "hardware reset";

                    m_processingFile = false;
                    m_transferCompleted = true;
                    m_fileCommandIndex = 0;

                    m_reseting = false;
                    m_homing = false;
                    m_lastGrblStatus = -1;

                    m_updateParserStatus = true;
                    m_statusReceived = true;

                    m_commands.clear();
                    m_queue.clear();

                    m_frm->updateControlsState();
                }
                m_ui->txtConsole->appendPlainText(data);
            }
        } else {
            // Blank response
//            m_ui->txtConsole->appendPlainText(data);
        }
    }
}

void GrblMachine::jogStep()
{
    if (m_jogVector.length() == 0) return;

    if (m_ui->cboJogStep->currentText().toDouble() == 0) {
        const double acc = m_frm->settings()->acceleration();              // Acceleration mm/sec^2
        int speed = m_ui->cboJogFeed->currentText().toInt();          // Speed mm/min
        double v = (double)speed / 60;                              // Rapid speed mm/sec
        int N = 15;                                                 // Planner blocks
        double dt = qMax(0.01, sqrt(v) / (2 * acc * (N - 1)));      // Single jog command time
        double s = v * dt;                                          // Jog distance

        QVector3D vec = m_jogVector.normalized() * s;

    //    qDebug() << "jog" << speed << v << acc << dt <<s;

        sendCommand(QString("$J=G21G91X%1Y%2Z%3F%4")
                    .arg(vec.x(), 0, 'g', 4)
                    .arg(vec.y(), 0, 'g', 4)
                    .arg(vec.z(), 0, 'g', 4)
                    .arg(speed), -2, m_frm->settings()->showUICommands());
    } else {
        int speed = m_ui->cboJogFeed->currentText().toInt();          // Speed mm/min
        QVector3D vec = m_jogVector * m_ui->cboJogStep->currentText().toDouble();

        sendCommand(QString("$J=G21G91X%1Y%2Z%3F%4")
                    .arg(vec.x(), 0, 'g', 4)
                    .arg(vec.y(), 0, 'g', 4)
                    .arg(vec.z(), 0, 'g', 4)
                    .arg(speed), -3, m_frm->settings()->showUICommands());
    }
}

void GrblMachine::cmdStop()
{
    m_queue.clear();
    m_connection.write(QByteArray(1, char(0x85)));
}

void GrblMachine::onTimerConnection()
{
    if (!m_connection.isOpen()) {
//        openPort();
    } else if (!m_homing && !m_reseting && !m_ui->cmdFilePause->isChecked() && m_queue.length() == 0) {
        if (m_updateSpindleSpeed) {
            m_updateSpindleSpeed = false;
            sendCommand(QString("S%1").arg(m_ui->slbSpindle->value()), -2, m_frm->settings()->showUICommands());
        }
        if (m_updateParserStatus) {
            m_updateParserStatus = false;
            sendCommand("$G", -3, false);
        }
    }
}

void GrblMachine::onTimerStateQuery()
{
    if (m_connection.isOpen() && m_resetCompleted && m_statusReceived) {
        m_connection.write(QByteArray(1, '?'));
        m_statusReceived = false;
    }

    m_ui->glwVisualizer->setBufferState(QString(tr("Buffer: %1 / %2 / %3")).arg(bufferLength()).arg(m_commands.length()).arg(m_queue.length()));
}

void GrblMachine::cmdHome()
{
    m_homing = true;
    m_updateSpindleSpeed = true;
    sendCommand("$H", -1, m_frm->settings()->showUICommands());
}

void GrblMachine::cmdZeroXY()
{
    m_settingZeroXY = true;
    sendCommand("G92X0Y0", -1, m_frm->settings()->showUICommands());
    sendCommand("$#", -2, m_frm->settings()->showUICommands());
}

void GrblMachine::cmdZeroZ()
{
    m_settingZeroZ = true;
    sendCommand("G92Z0", -1, m_frm->settings()->showUICommands());
    sendCommand("$#", -2, m_frm->settings()->showUICommands());
}

void GrblMachine::machineReset()
{
    qDebug() << "grbl reset";

    m_connection.write(QByteArray(1, (char)24));
//    m_serialPort.flush();

    m_processingFile = false;
    m_transferCompleted = true;
    m_fileCommandIndex = 0;

    m_reseting = true;
    m_homing = false;
    m_resetCompleted = false;
    m_updateSpindleSpeed = true;
    m_lastGrblStatus = -1;
    m_statusReceived = true;

    // Drop all remaining commands in buffer
    m_commands.clear();
    m_queue.clear();

    // Prepare reset response catch
    CommandAttributes ca;
    ca.command = "[CTRL+X]";
    if (m_frm->settings()->showUICommands()) m_ui->txtConsole->appendPlainText(ca.command);
    ca.consoleIndex = m_frm->settings()->showUICommands() ? m_ui->txtConsole->blockCount() - 1 : -1;
    ca.tableIndex = -1;
    ca.length = ca.command.length() + 1;
    m_commands.append(ca);

    m_frm->updateControlsState();
}

void GrblMachine::cmdUnlock()
{
    m_updateSpindleSpeed = true;
    sendCommand("$X", -1, m_frm->settings()->showUICommands());
}

void GrblMachine::cmdSafePosition()
{
    QStringList list = m_frm->settings()->safePositionCommand().split(";");

    foreach (QString cmd, list) {
        sendCommand(cmd.trimmed(), -1, m_frm->settings()->showUICommands());
    }
}

void GrblMachine::cmdSpindle(bool checked)
{
    if (m_ui->cmdFilePause->isChecked()) {
        m_connection.write(QByteArray(1, char(0x9e)));
    } else {
        sendCommand(checked ? QString("M3 S%1").arg(m_ui->slbSpindle->value()) : "M5", -1, m_frm->settings()->showUICommands());
    }
}

void GrblMachine::testMode(bool checked)
{
    if (checked) {
        storeOffsets();
        storeParserState();
        sendCommand("$C", -1, m_frm->settings()->showUICommands());
    } else {
        m_aborting = true;
        machineReset();
    };
}

// Store/restore coordinate system
void GrblMachine::storeCoordinateSystem(bool checked) {
    if (checked) {
        sendCommand("$G", -2, m_frm->settings()->showUICommands());
    } else {
        if (m_absoluteCoordinates)
            sendCommand("G90", -1, m_frm->settings()->showUICommands());
    }
}
void GrblMachine::storeParserState()
{
    m_storedParserStatus = m_ui->glwVisualizer->parserStatus().remove(
                QRegExp("GC:|\\[|\\]|G[01234]\\s|M[0345]+\\s|\\sF[\\d\\.]+|\\sS[\\d\\.]+"));
}

void GrblMachine::restoreParserState()
{
    if (!m_storedParserStatus.isEmpty()) sendCommand(m_storedParserStatus, -1, m_frm->settings()->showUICommands());
}

void GrblMachine::storeOffsets()
{
//    sendCommand("$#", -2, m_frm->settings()->showUICommands());
}

void GrblMachine::restoreOffsets()
{
    // Still have pre-reset working position
    sendCommand(QString("G21G53G90X%1Y%2Z%3").arg(toMetric(m_ui->txtMPosX->text().toDouble()))
                                       .arg(toMetric(m_ui->txtMPosY->text().toDouble()))
                                       .arg(toMetric(m_ui->txtMPosZ->text().toDouble())), -1, m_frm->settings()->showUICommands());
    sendCommand(QString("G21G92X%1Y%2Z%3").arg(toMetric(m_ui->txtWPosX->text().toDouble()))
                                       .arg(toMetric(m_ui->txtWPosY->text().toDouble()))
                                       .arg(toMetric(m_ui->txtWPosZ->text().toDouble())), -1, m_frm->settings()->showUICommands());
}

void GrblMachine::sendNextFileCommands() {
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

void GrblMachine::restoreOrigin()
{
    // Restore offset
    sendCommand(QString("G21"), -1, m_frm->settings()->showUICommands());
    sendCommand(QString("G53G90G0X%1Y%2Z%3").arg(toMetric(m_ui->txtMPosX->text().toDouble()))
                                            .arg(toMetric(m_ui->txtMPosY->text().toDouble()))
                                            .arg(toMetric(m_ui->txtMPosZ->text().toDouble())), -1, m_frm->settings()->showUICommands());
    sendCommand(QString("G92X%1Y%2Z%3").arg(toMetric(m_ui->txtMPosX->text().toDouble()) - m_storedX)
                                        .arg(toMetric(m_ui->txtMPosY->text().toDouble()) - m_storedY)
                                        .arg(toMetric(m_ui->txtMPosZ->text().toDouble()) - m_storedZ), -1, m_frm->settings()->showUICommands());

    // Move tool
    if (m_frm->settings()->moveOnRestore()) {
        switch (m_frm->settings()->restoreMode()) {
            case 0:
                sendCommand("G0X0Y0", -1, m_frm->settings()->showUICommands());
                break;
            case 1:
                sendCommand("G0X0Y0Z0", -1, m_frm->settings()->showUICommands());
                break;
        }
    }
}

bool GrblMachine::dataIsFloating(QString data) {
    QStringList ends;

    ends << "Reset to continue";
    ends << "'$H'|'$X' to unlock";
    ends << "ALARM: Soft limit";
    ends << "ALARM: Hard limit";
    ends << "Check Door";

    foreach (QString str, ends) {
        if (data.contains(str)) return true;
    }

    return false;
}

bool GrblMachine::dataIsEnd(QString data) {
    QStringList ends;

    ends << "ok";
    ends << "error";
//    ends << "Reset to continue";
//    ends << "'$' for help";
//    ends << "'$H'|'$X' to unlock";
//    ends << "Caution: Unlocked";
//    ends << "Enabled";
//    ends << "Disabled";
//    ends << "Check Door";
//    ends << "Pgm End";

    foreach (QString str, ends) {
        if (data.contains(str)) return true;
    }

    return false;
}

void GrblMachine::fileAbort()
{
    m_aborting = true;
    if (!m_ui->chkTestMode->isChecked()) {
        m_connection.write(QString("!"));
    } else {
        machineReset();
    }
}

void GrblMachine::cmdPause(bool checked)
{
    m_connection.write(QString(checked ? "!" : "~"));
}

void GrblMachine::cmdProbe(int gridPointsX, int gridPointsY, const QRectF& borderRect)
{
    double gridStepX = gridPointsX > 1 ? borderRect.width() / (gridPointsX - 1) : 0;
    double gridStepY = gridPointsY > 1 ? borderRect.height() / (gridPointsY - 1) : 0;

    m_frm->probeModel().setData(m_frm->probeModel().index(m_frm->probeModel().rowCount() - 1, 1), QString("G21G90F%1G0Z%2").
                         arg(m_frm->settings()->heightmapProbingFeed()).arg(m_ui->txtHeightMapGridZTop->value()));
    m_frm->probeModel().setData(m_frm->probeModel().index(m_frm->probeModel().rowCount() - 1, 1), QString("G0X0Y0"));
//                         .arg(ui->txtHeightMapGridZTop->value()));
    m_frm->probeModel().setData(m_frm->probeModel().index(m_frm->probeModel().rowCount() - 1, 1), QString("G38.2Z%1")
                         .arg(m_ui->txtHeightMapGridZBottom->value()));
    m_frm->probeModel().setData(m_frm->probeModel().index(m_frm->probeModel().rowCount() - 1, 1), QString("G0Z%1")
                         .arg(m_ui->txtHeightMapGridZTop->value()));
    double x, y;

    for (int i = 0; i < gridPointsY; i++) {
        y = borderRect.top() + gridStepY * i;
        for (int j = 0; j < gridPointsX; j++) {
            x = borderRect.left() + gridStepX * (i % 2 ? gridPointsX - 1 - j : j);
            m_frm->probeModel().setData(m_frm->probeModel().index(m_frm->probeModel().rowCount() - 1, 1), QString("G0X%1Y%2")
                                 .arg(x, 0, 'f', 3).arg(y, 0, 'f', 3));
            m_frm->probeModel().setData(m_frm->probeModel().index(m_frm->probeModel().rowCount() - 1, 1), QString("G38.2Z%1")
                                 .arg(m_ui->txtHeightMapGridZBottom->value()));
            m_frm->probeModel().setData(m_frm->probeModel().index(m_frm->probeModel().rowCount() - 1, 1), QString("G0Z%1")
                                 .arg(m_ui->txtHeightMapGridZTop->value()));
        }
    }
}

