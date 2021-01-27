// This file is a part of "Candle" application.
// Copyright 2015-2016 Hayrullin Denis Ravilevich

#ifndef FRMMAIN_H
#define FRMMAIN_H

#include <QMainWindow>
#include <QtSerialPort/QSerialPort>
#include <QSettings>
#include <QTimer>
#include <QBasicTimer>
#include <QStringList>
#include <QList>
#include <QTime>
#include <QMenu>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QProgressDialog>
#include <exception>

#include "parser/gcodeviewparse.h"

#include "drawers/origindrawer.h"
#include "drawers/gcodedrawer.h"
#include "drawers/tooldrawer.h"
#include "drawers/heightmapborderdrawer.h"
#include "drawers/heightmapgriddrawer.h"
#include "drawers/heightmapinterpolationdrawer.h"
#include "drawers/shaderdrawable.h"
#include "drawers/selectiondrawer.h"

#include "tables/gcodetablemodel.h"
#include "tables/heightmaptablemodel.h"

#include "utils/interpolation.h"

#include "widgets/styledtoolbutton.h"
#include "widgets/sliderbox.h"

#include "frmsettings.h"
#include "frmabout.h"

#include "CandleConnection.h"
#include "Machine.h"

#ifdef WINDOWS
    #include <QtWinExtras/QtWinExtras>
    #include "shobjidl.h"
#endif

namespace Ui {
class frmMain;
}

class CancelException : public std::exception {
public:
#ifdef Q_OS_MAC
#undef _GLIBCXX_USE_NOEXCEPT
#define _GLIBCXX_USE_NOEXCEPT _NOEXCEPT
#endif

    const char* what() const noexcept override
    {
        return "Operation was cancelled by user";
    }
};

class frmMain : public QMainWindow
{
    Q_OBJECT

public:
    explicit frmMain(QWidget *parent = 0);
    ~frmMain();

    double toolZPosition();
    void updateControlsState();
    void updateOverride(SliderBox *slider, int value, char command);
    void updateHeightMapInterpolationDrawer(bool reset = false);

    frmSettings* settings()
    {return m_settings;}
    QTimer& timerStateQuery()
    {return m_timerStateQuery;}
    QTimer& timerConnection()
    {return m_timerConnection;}
    QTime& startTime()
    {return m_startTime;}
    GcodeDrawer* currentDrawer()
    {return m_currentDrawer;}
    ToolDrawer& toolDrawer()
    {return m_toolDrawer;}
    GcodeDrawer* codeDrawer()
    {return m_codeDrawer;}
    GCodeTableModel* currentModel()
    {return m_currentModel;}
    const GCodeTableModel* currentModel() const
    {return m_currentModel;}
    QBasicTimer& timerToolAnimation()
    {return m_timerToolAnimation;}
    QMessageBox* senderErrorBox()
    {return m_senderErrorBox;}
    HeightMapTableModel& heightMapModel()
    {return m_heightMapModel;}
    bool& heightMapMode()
    {return m_heightMapMode;}

private slots:
    void placeVisualizerButtons();

    void onCommReadyRead();
    void onCommError(int);
    void onTimerConnection();
    void onTimerStateQuery();
    void onVisualizatorRotationChanged();
    void onScroolBarAction(int action);
    void onJogTimer();
    void onTableInsertLine();
    void onTableDeleteLines();
    void onActRecentFileTriggered();
    void onCboCommandReturnPressed();
    void onTableCurrentChanged(QModelIndex idx1, QModelIndex idx2);
    void onConsoleResized(QSize size);
    void onPanelsSizeChanged(QSize size);
    void onCmdUserClicked(bool checked);
    void onOverridingToggled(bool checked);
    void onActSendFromLineTriggered();

    void on_actFileExit_triggered();
    void on_cmdFileOpen_clicked();
    void on_cmdFit_clicked();
    void on_cmdFileSend_clicked();
    void onTableCellChanged(QModelIndex i1, QModelIndex i2);
    void on_actServiceSettings_triggered();
    void on_actFileOpen_triggered();
    void on_cmdCommandSend_clicked();
    void on_cmdHome_clicked();
    void on_cmdTouch_clicked();
    void on_cmdZeroXY_clicked();
    void on_cmdZeroZ_clicked();
    void on_cmdRestoreOrigin_clicked();
    void on_cmdReset_clicked();
    void on_cmdUnlock_clicked();
    void on_cmdSafePosition_clicked();
    void on_cmdSpindle_toggled(bool checked);
    void on_chkTestMode_clicked(bool checked);
    void on_cmdFilePause_clicked(bool checked);
    void on_cmdFileReset_clicked();
    void on_actFileNew_triggered();
    void on_cmdClearConsole_clicked();
    void on_actFileSaveAs_triggered();
    void on_actFileSave_triggered();
    void on_actFileSaveTransformedAs_triggered();
    void on_cmdTop_clicked();
    void on_cmdFront_clicked();
    void on_cmdLeft_clicked();
    void on_cmdIsometric_clicked();
    void on_actAbout_triggered();
    void on_grpOverriding_toggled(bool checked);
    void on_grpSpindle_toggled(bool checked);
    void on_grpJog_toggled(bool checked);
    void on_grpUserCommands_toggled(bool checked);
    void on_chkKeyboardControl_toggled(bool checked);
    void on_tblProgram_customContextMenuRequested(const QPoint &pos);
    void on_splitter_splitterMoved(int pos, int index);
    void on_actRecentClear_triggered();
    void on_grpHeightMap_toggled(bool arg1);
    void on_chkHeightMapBorderShow_toggled(bool checked);
    void on_txtHeightMapBorderX_valueChanged(double arg1);
    void on_txtHeightMapBorderWidth_valueChanged(double arg1);
    void on_txtHeightMapBorderY_valueChanged(double arg1);
    void on_txtHeightMapBorderHeight_valueChanged(double arg1);
    void on_chkHeightMapGridShow_toggled(bool checked);
    void on_txtHeightMapGridX_valueChanged(double arg1);
    void on_txtHeightMapGridY_valueChanged(double arg1);
    void on_txtHeightMapGridZBottom_valueChanged(double arg1);
    void on_txtHeightMapGridZTop_valueChanged(double arg1);
    void on_cmdHeightMapMode_toggled(bool checked);
    void on_chkHeightMapInterpolationShow_toggled(bool checked);
    void on_cmdHeightMapLoad_clicked();
    void on_txtHeightMapInterpolationStepX_valueChanged(double arg1);
    void on_txtHeightMapInterpolationStepY_valueChanged(double arg1);
    void on_chkHeightMapUse_clicked(bool checked);
    void on_cmdHeightMapCreate_clicked();
    void on_cmdHeightMapBorderAuto_clicked();
    void on_cmdFileAbort_clicked();
    void on_cmdSpindle_clicked(bool checked);   

    void on_cmdYPlus_pressed();

    void on_cmdYPlus_released();

    void on_cmdYMinus_pressed();

    void on_cmdYMinus_released();

    void on_cmdXPlus_pressed();

    void on_cmdXPlus_released();

    void on_cmdXMinus_pressed();

    void on_cmdXMinus_released();

    void on_cmdZPlus_pressed();

    void on_cmdZPlus_released();

    void on_cmdZMinus_pressed();

    void on_cmdZMinus_released();

    void on_cmdStop_clicked();

    void on_pbConnect_clicked();

protected:
    void showEvent(QShowEvent *se);
    void hideEvent(QHideEvent *he);
    void resizeEvent(QResizeEvent *re);
    void timerEvent(QTimerEvent *);
    void closeEvent(QCloseEvent *ce);
    void dragEnterEvent(QDragEnterEvent *dee);
    void dropEvent(QDropEvent *de);

private:

    Ui::frmMain *ui;
    Machine* m_machine;
    GcodeViewParse m_viewParser;
    GcodeViewParse m_probeParser;

    OriginDrawer *m_originDrawer;

    GcodeDrawer *m_codeDrawer;
    GcodeDrawer *m_probeDrawer;
    GcodeDrawer *m_currentDrawer;

    ToolDrawer m_toolDrawer;
    HeightMapBorderDrawer m_heightMapBorderDrawer;
    HeightMapGridDrawer m_heightMapGridDrawer;
    HeightMapInterpolationDrawer m_heightMapInterpolationDrawer;

    SelectionDrawer m_selectionDrawer;

    GCodeTableModel m_programModel;
    GCodeTableModel m_probeModel;
    GCodeTableModel m_programHeightmapModel;

    HeightMapTableModel m_heightMapModel;

    bool m_programLoading;
    bool m_settingsLoading;

    CandleConnection m_connection;

    frmSettings *m_settings;
    frmAbout m_frmAbout;

    QString m_settingsFileName;
    QString m_programFileName;
    QString m_heightMapFileName;
    QString m_lastFolder;

    bool m_fileChanged = false;
    bool m_heightMapChanged = false;

    QTimer m_timerConnection;
    QTimer m_timerStateQuery;
    QBasicTimer m_timerToolAnimation;

#ifdef WINDOWS
    QWinTaskbarButton *m_taskBarButton;
    QWinTaskbarProgress *m_taskBarProgress;
#endif

    QMenu *m_tableMenu;
    QTime m_startTime;

    QMessageBox* m_senderErrorBox;

    // Console window
    int m_storedConsoleMinimumHeight;
    int m_storedConsoleHeight;
    int m_consolePureHeight;


    bool m_heightMapMode;
    bool m_cellChanged;


    // Keyboard
    bool m_keyPressed = false;
    bool m_jogBlock = false;
    bool m_storedKeyboardControl;

    QStringList m_recentFiles;
    QStringList m_recentHeightmaps;

    void loadFile(QString fileName);
    void loadFile(QList<QString> data);
    void clearTable();
    void preloadSettings();
    void loadSettings();
    void saveSettings();
    bool saveChanges(bool heightMapMode);
    void openPort();
    void applySettings();
    void updateParser();
    bool dataIsFloating(QString data);
    bool dataIsEnd(QString data);
    bool dataIsReset(QString data);

    QTime updateProgramEstimatedTime(QList<LineSegment *> lines);
    bool saveProgramToFile(QString fileName, GCodeTableModel *model);
    QString feedOverride(QString command);

    bool eventFilter(QObject *obj, QEvent *event);
    bool keyIsMovement(int key);
    void resizeCheckBoxes();
    void updateLayouts();
    void updateRecentFilesMenu();
    void addRecentFile(QString fileName);
    void addRecentHeightmap(QString fileName);

    QRectF borderRectFromTextboxes();
    QRectF borderRectFromExtremes();
    void updateHeightMapBorderDrawer();
    bool updateHeightMapGrid();
    void loadHeightMap(QString fileName);
    bool saveHeightMap(QString fileName);

    GCodeTableModel *m_currentModel;
    QList<LineSegment *> subdivideSegment(LineSegment *segment);
    void resizeTableHeightMapSections();
    void updateHeightMapGrid(double arg1);
    void resetHeightmap();
    bool isGCodeFile(QString fileName);
    bool isHeightmapFile(QString fileName);
    bool compareCoordinates(double x, double y, double z);
    int getConsoleMinHeight();
    void updateJogTitle();
};

#endif // FRMMAIN_H
