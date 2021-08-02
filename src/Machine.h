#ifndef MACHINE_H
#define MACHINE_H

#include <QObject>
#include <QString>
#include <QVector3D>

#include "CandleConnection.h"

struct CommandAttributes {
    int length;
    int consoleIndex;
    int tableIndex;
    QString command;
};

struct CommandQueue {
    QString command;
    int tableIndex;
    bool showInConsole;
};

namespace Ui {
class frmMain;
}

class frmMain;

class Machine : public QObject
{
public:
    Machine(frmMain *frm, Ui::frmMain *m_ui, CandleConnection& connection);
    virtual ~Machine(){}

    virtual void onTimerConnection();
    virtual void onTimerStateQuery();

    void init();
    void fileCmdIndex(int cmdIndex);
    void resetFileProgress(int cmdIndex = 0);
    void resetFile(int cmdIndex = 0);
    void startFile();
    bool processingFile()
    {return m_processingFile;}
    void clear();
    bool spindleCW()
    {return m_spindleCW;}
    double& storedX()
    {return m_storedX;}
    double& storedY()
    {return m_storedY;}
    double& storedZ()
    {return m_storedZ;}
    bool& updateSpindleSpeed()
    {return m_updateSpindleSpeed;}
    int& lastDrawnLineIndex()
    {return m_lastDrawnLineIndex;}
    QVector3D& jogVector()
    {return m_jogVector;}

    virtual void sendNextFileCommands();
    virtual void sendCommand(QString command, int tableIndex, bool showInConsole=false);
    virtual void onReadyRead();
    virtual void onCommError(int error);
    virtual void storeParserState();
    virtual void restoreParserState();
    virtual void storeOffsets();
    virtual void restoreOffsets();
    virtual void storeCoordinateSystem(bool checked);
    virtual void jogStep();
    virtual void cmdStop();
    virtual void cmdCommandSend();
    virtual void cmdHome();
    virtual void cmdZeroXY();
    virtual void cmdZeroZ();
    virtual void restoreOrigin();
    virtual void machineReset();
    virtual void cmdUnlock();
    virtual void cmdSafePosition();
    virtual void cmdSpindle(bool checked);
    virtual void fileAbort();
    virtual void testMode(bool checked);

protected:

    double toMetric(double value);
    bool compareCoordinates(double x, double y, double z);
    int bufferLength();
    QString feedOverride(QString command);

protected:
    const int BUFFERLENGTH = 127;

    frmMain *m_frm;
    Ui::frmMain *m_ui;
    CandleConnection& m_connection;

    QStringList m_status;
    QStringList m_statusCaptions;
    QStringList m_statusBackColors;
    QStringList m_statusForeColors;

    QList<CommandAttributes> m_commands;
    QList<CommandQueue> m_queue;

    // Flags
    bool m_settingZeroXY = false;
    bool m_settingZeroZ = false;
    bool m_homing = false;
    bool m_updateSpindleSpeed = false;
    bool m_updateParserStatus = false;
    bool m_updateFeed = false;

    bool m_processingFile = false;
    bool m_transferCompleted = false;
    bool m_fileEndSent = false;

    bool m_statusReceived = false;

    // Indices
    int m_fileCommandIndex;
    int m_fileProcessedCommandIndex;
    int m_probeIndex;

    // Current values
    int m_lastDrawnLineIndex;
    double m_originalFeed;

    // Spindle
    bool m_spindleCW = true;
    bool m_spindleCommandSpeed = false;

    // Stored origin
    double m_storedX = 0;
    double m_storedY = 0;
    double m_storedZ = 0;
    QString m_storedParserStatus;

    bool m_absoluteCoordinates;

    // Jog
    QVector3D m_jogVector;
    bool m_aborting = false;
};

#endif // MACHINE_H
