#pragma once

#include <QStack>

#define NUM_INPUT_COLUMNS 6
#define FRAMECOUNT_COLUMN 1

enum class FileStatus
{
    Success = 0,
    WritePermission,
    Parse,
};

enum class Centering
{
    Unknown = 0,
    Seven,
    Zero,
};

class CellEditAction
{
public:
    CellEditAction();

    bool operator==(const CellEditAction& rhs);

private:
    int m_rowIdx;
    int m_colIdx;
    int m_prev;
    int m_cur;

    void flipValues();
};

typedef QVector<QVector<QString>> TtkFileData;

class QAction;
class QMenu;
class QTableView;


class InputFile
{
public:
    InputFile(QMenu* root, QAction* undo, QAction* redo, QAction* close, QTableView* tableView);

    QString getPath() { return m_filePath; }
    TtkFileData getData() { return m_fileData; }
    QString getCellValue(int rowIdx, int colIdx) { return m_fileData[rowIdx][colIdx]; }
    void setCellValue(int rowIdx, int colIdx, QString value) { m_fileData[rowIdx][colIdx] = value; }
    FileStatus loadFile(QString path);
    void closeFile();
    Centering getCentering() { return m_fileCentering; }
    void setTableView(QTableView* tableView) { pTableView = tableView; }
    QTableView* getTableView() { return pTableView; }
    QMenu* getRootMenu() { return pRootMenu; }
    QAction* getCloseMenu() { return pCloseMenu; }

private:

    QString m_filePath;
    TtkFileData m_fileData;
    Centering m_fileCentering;
    bool m_tableViewLoaded;
    QStack<CellEditAction> m_undoStack;
    QStack<CellEditAction> m_redoStack;
    QTableView* pTableView;
    QMenu* pRootMenu;
    QAction* pUndoMenu;
    QAction* pRedoMenu;
    QAction* pCloseMenu;
    int m_frameParseError;

    bool valuesFormattedProperly(const QStringList& data);
    bool valueRestrictionsAreMet(const QStringList& data);
    int getSmallestAcceptedValue(int index, int value);
    int getLargestAcceptedValue(int index, int value);
    bool ableToDiscernCentering(int value);
    void clearData();

    const QVector<int> BUTTON_COL_IDXS{ 0, 1, 2 };
    const int DPAD_COL_IDX = 5;
};

