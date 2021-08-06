#ifndef MARLINMACHINE_H
#define MARLINMACHINE_H

#include "tables/gcodetablemodel.h"
#include "tables/heightmaptablemodel.h"
#include "Machine.h"

class MarlinMachine : public Machine
{
    // From Marlin code.
    enum M_StateEnum : int8_t {
      M_INIT = 0, //  0 machine is initializing
      M_RESET,    //  1 machine is ready for use
      M_ALARM,    //  2 machine is in alarm state (soft shut down)
      M_IDLE,     //  3 program stop or no more blocks (M0, M1, M60)
      M_END,      //  4 program end via M2, M30
      M_RUNNING,  //  5 motion is running
      M_HOLD,     //  6 motion is holding
      M_PROBE,    //  7 probe cycle active
      M_CYCLING,  //  8 machine is running (cycling)
      M_HOMING,   //  9 machine is homing
      M_JOGGING,  // 10 machine is jogging
      M_ERROR     // 11 machine is in hard alarm state (shut down)
    };

public:
    MarlinMachine(frmMain* frm, Ui::frmMain* ui, CandleConnection& connection);
    void onReadyRead();
    void sendNextFileCommands();
    void cmdHome();
    void cmdPause(bool checked);
    void cmdZeroXY();
    void cmdZeroZ();
    void cmdProbe(int gridPointsX, int gridPointsY, const QRectF& borderRect);
    void fileAbort();

private:
    void parseResponse(const QString& val);
    bool dataIsCmdResponse(QString data);

private:
    QString response; // Full response string
    int m_lastMarlinStatus;
};

#endif // MARLINMACHINE_H
