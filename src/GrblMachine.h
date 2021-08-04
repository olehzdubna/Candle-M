#ifndef GRBLMACHINE_H
#define GRBLMACHINE_H

#include <QString>
#include <QVector3D>

#include "tables/gcodetablemodel.h"
#include "tables/heightmaptablemodel.h"

#include "Machine.h"

class GrblMachine : public Machine
{
    enum {UNKNOWN=0, IDLE, ALARM, RUN, HOME, HOLD0, HOLD1, QUEUE, CHECK, DOOR, JOG} Status;
public:
    GrblMachine(frmMain *frm, Ui::frmMain *m_ui, CandleConnection& connection);

    void sendCommand(const QString& command, int tableIndex, bool showInConsole);
    void onReadyRead();
    void jogStep();
    void onTimerConnection();
    void onTimerStateQuery();
    void cmdStop();
    void cmdHome();
    void cmdPause(bool checked);
    void cmdZeroXY();
    void cmdZeroZ();
    void cmdProbe(int gridPointsX, int gridPointsY, const QRectF& borderRect);
    void restoreOrigin();
    void machineReset();
    void storeCoordinateSystem(bool checked);
    void storeParserState();
    void restoreParserState();
    void storeOffsets();
    void restoreOffsets();
    void sendNextFileCommands();
    bool dataIsReset(QString data);
    bool dataIsFloating(QString data);
    bool dataIsEnd(QString data);
    void cmdUnlock();
    void cmdSafePosition();
    void cmdSpindle(bool checked);
    void fileAbort();
    void testMode(bool checked);

private:
    int m_lastGrblStatus;
    bool m_reseting = false;
    bool m_resetCompleted = true;
};

#endif // GRBLMACHINE_H
