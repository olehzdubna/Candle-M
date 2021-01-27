#ifndef MARLINMACHINE_H
#define MARLINMACHINE_H

#include "tables/gcodetablemodel.h"
#include "tables/heightmaptablemodel.h"
#include "Machine.h"

class MarlinMachine : public Machine
{
public:
    MarlinMachine(frmMain* frm, Ui::frmMain* ui, CandleConnection& connection);
    void onReadyRead();
    void sendNextFileCommands();
private:
    bool dataIsEnd(QString data);
};

#endif // MARLINMACHINE_H
