#ifndef MARLINMACHINE_H
#define MARLINMACHINE_H

#include "tables/gcodetablemodel.h"
#include "tables/heightmaptablemodel.h"
#include "Machine.h"

class MarlinMachine : public Machine
{
    enum {UNKNOWN=0, IDLE, ALARM, RUN, HOME, HOLD0, HOLD1, QUEUE, CHECK, DOOR, JOG} Status;
public:
    MarlinMachine(frmMain* frm, Ui::frmMain* ui, CandleConnection& connection);
    void onReadyRead();
    void sendNextFileCommands();
    void cmdHome();
    void cmdZeroXY();
    void cmdZeroZ();
    void fileAbort();

private:
    void parseResponse(const QString& val);
    bool dataIsCmdResponse(QString data);

private:
    QString response; // Full response string
    int m_lastMarlinStatus;
    double m_leveling[3];
};

#endif // MARLINMACHINE_H
