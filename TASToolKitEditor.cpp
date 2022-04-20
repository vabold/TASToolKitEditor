#include "TASToolKitEditor.h"

#include "InputFile.h"
#include "InputFileModel.h"

#include <QFileDialog>
#include <QMessageBox>
#include <QTextStream>

#include <iostream>

#define FRAMECOUNT_COLUMN_WIDTH 50
#define TABLE_COLUMN_WIDTH 30
#define TABLE_VIEW_WIDTH (FRAMECOUNT_COLUMN_WIDTH + (7 * TABLE_COLUMN_WIDTH))
#define TABLE_SIDE_PADDING 10
#define DEFAULT_WINDOW_WIDTH ((TABLE_VIEW_WIDTH) + (2 * TABLE_SIDE_PADDING))
#define DEFAULT_WINDOW_HEIGHT 500
#define DEFAULT_TABLE_COL_WIDTH 30

TASToolKitEditor::TASToolKitEditor(QWidget *parent)
    : QMainWindow(parent)
{
    setupUi();
    createInputFileInstances();
    connectActions();
}

void TASToolKitEditor::createInputFileInstances()
{
    InputFileMenus playerMenus = InputFileMenus(menuPlayer, actionUndoPlayer, actionRedoPlayer, actionClosePlayer, action0CenteredPlayer, action7CenteredPlayer);
    InputFileMenus ghostMenus = InputFileMenus(menuGhost, actionUndoGhost, actionRedoGhost, actionCloseGhost, action0CenteredGhost, action7CenteredGhost);
    playerFile = new InputFile(playerMenus, playerLabel, playerTableView);
    ghostFile = new InputFile(ghostMenus, ghostLabel, ghostTableView);
}

void TASToolKitEditor::connectActions()
{
    connect(actionOpenPlayer, &QAction::triggered, this, &TASToolKitEditor::onOpenPlayer);
    connect(actionOpenGhost, &QAction::triggered, this, &TASToolKitEditor::onOpenGhost);
    connect(actionClosePlayer, &QAction::triggered, this, &TASToolKitEditor::onClosePlayer);
    connect(actionCloseGhost, &QAction::triggered, this, &TASToolKitEditor::onCloseGhost);
    connect(actionUndoPlayer, &QAction::triggered, this, &TASToolKitEditor::onUndoPlayer);
    connect(actionUndoGhost, &QAction::triggered, this, &TASToolKitEditor::onUndoGhost);
    connect(actionRedoPlayer, &QAction::triggered, this, &TASToolKitEditor::onRedoPlayer);
    connect(actionRedoGhost, &QAction::triggered, this, &TASToolKitEditor::onRedoGhost);
    connect(actionScrollTogether, &QAction::toggled, this, &TASToolKitEditor::onToggleScrollTogether);
}

void TASToolKitEditor::onToggleScrollTogether(bool bTogether)
{
    m_bScrollTogether = bTogether;

    actionScrollTogether->setChecked(m_bScrollTogether);

    if (!m_bScrollTogether)
        return;

    // Jump ghost view to same row as player view]
    QTableView* pPlayerTable = playerFile->getTableView();
    QTableView* pGhostTable = ghostFile->getTableView();

    int playerTopRow = pPlayerTable->rowAt(0);

    QModelIndex index = pGhostTable->model()->index(playerTopRow, 0);
    pGhostTable->setCurrentIndex(index);
    pGhostTable->scrollTo(index);
}

void TASToolKitEditor::onUndoPlayer()
{
    onUndoRedo(playerFile, EOperationType::Undo);
}

void TASToolKitEditor::onUndoGhost()
{
    onUndoRedo(ghostFile, EOperationType::Undo);
}

void TASToolKitEditor::onRedoPlayer()
{
    onUndoRedo(playerFile, EOperationType::Redo);
}

void TASToolKitEditor::onRedoGhost()
{
    onUndoRedo(ghostFile, EOperationType::Redo);
}

void TASToolKitEditor::onUndoRedo(InputFile* pInputFile, EOperationType opType)
{
    bool bUndo = opType == EOperationType::Undo;

    TtkStack* undoStack = pInputFile->getUndoStack();
    TtkStack* redoStack = pInputFile->getRedoStack();

    // Refuse operation if the associated stack is empty
    if (bUndo && undoStack->count() == 0)
        return;
    if (!bUndo && redoStack->count() == 0)
        return;

    CellEditAction action = bUndo ? undoStack->pop() : redoStack->pop();
    action.flipValues();

    if (bUndo)
        redoStack->push(action);
    else
        undoStack->push(action);

    pInputFile->setCellValue(action.row(), action.col(), action.curVal());
    emit pInputFile->getTableView()->model()->layoutChanged();

    // Adjust menu items
    pInputFile->getMenus().redo->setEnabled(redoStack->count() > 0);
    pInputFile->getMenus().undo->setEnabled(undoStack->count() > 0);

    // Move tableview to the row that was just modified
    // Determine if the row is visible on-screen right now
    int rowUpper = pInputFile->getTableView()->rowAt(0);
    int rowLower = pInputFile->getTableView()->rowAt(pInputFile->getTableView()->height());

    if (action.row() < rowUpper || action.row() > rowLower)
        pInputFile->getTableView()->scrollTo(pInputFile->getTableView()->model()->index(action.row(), 0));
}

void TASToolKitEditor::onOpenPlayer()
{
    openFile(playerFile);
}

void TASToolKitEditor::onOpenGhost()
{
    openFile(ghostFile);
}

void TASToolKitEditor::onClosePlayer()
{
    closeFile(playerFile);
}

void TASToolKitEditor::onCloseGhost()
{
    closeFile(ghostFile);
}

void TASToolKitEditor::closeFile(InputFile* pInputFile)
{
    pInputFile->closeFile();
    m_filesLoaded--;
    adjustUiOnFileClose(pInputFile);
}

bool TASToolKitEditor::userClosedPreviousFile(InputFile* inputFile)
{
    // Have user confirm they want to close file
    QMessageBox::StandardButton reply;
    reply = QMessageBox::question(this, "Close Current File", "Are you sure you want to close the current file and open a new one?",
        QMessageBox::No | QMessageBox::Yes);

    if (reply != QMessageBox::Yes)
        return false;

    inputFile->closeFile();
    return true;
}

void TASToolKitEditor::openFile(InputFile* inputFile)
{
    QString filePath = QFileDialog::getOpenFileName(this, "Open File", "", "Input Files (*.csv)");

    if (inputFile->getPath() != "" && !userClosedPreviousFile(inputFile))
        return;

    if (filePath == playerFile->getPath() || filePath == ghostFile->getPath())
    {
        showError("Error Opening File", "This file is already open in the program!");
        return;
    }

    FileStatus status = inputFile->loadFile(filePath);

    if (status == FileStatus::WritePermission)
    {
        showError("Error Opening File", "This program does not have sufficient permissions to modify the file.\n\n" \
            "Try running this program in administrator mode and make sure the file is not open in another program.");
        return;
    }
    if (status == FileStatus::Parse)
    {
        showError("Error Parsing File", QString("There is an issue with the file on line %1.\n").arg(inputFile->getParseError()));
        return;
    }

    if (status != FileStatus::Success)
        return;

    m_filesLoaded++;

    adjustUiOnFileLoad(inputFile);
}

void TASToolKitEditor::adjustUiOnFileLoad(InputFile* pInputFile)
{
    adjustInputCenteringMenu(pInputFile);
    pInputFile->getMenus().root->menuAction()->setVisible(true);
    pInputFile->getMenus().close->setEnabled(true);
    pInputFile->getLabel()->setVisible(true);

    QTableView* pTable = pInputFile->getTableView();
    pTable->setModel(new InputFileModel(pInputFile));
    pTable->setVisible(true);

    /* This stuff really should be constant, but I can't do any of this until
    // the model is set, but I can't set the model until I instantiate the model
    // instance, but I can't instantiate the instance until I have the InputFile
    // instance, at which point I have to have the table instance already, which
    // means I can't yet give the table a model instance...
    // THIS IS SO CONFUSING!*/
    pTable->setColumnWidth(0, FRAMECOUNT_COLUMN_WIDTH);
    for (int i = 0; i < NUM_INPUT_COLUMNS - 1; i++)
        pTable->setColumnWidth(i + FRAMECOUNT_COLUMN, TABLE_COLUMN_WIDTH);

    //D-Pad column
    pTable->setColumnWidth(6, TABLE_COLUMN_WIDTH + 5);

    if (m_filesLoaded == 2)
    {
        actionSwapFiles->setEnabled(true);
        actionScrollTogether->setEnabled(true);
        resize(width() * 2, height());
    }
}

void TASToolKitEditor::adjustUiOnFileClose(InputFile* pInputFile)
{
    adjustMenuOnClose(pInputFile);
}

void TASToolKitEditor::adjustInputCenteringMenu(InputFile* inputFile)
{
    Centering fileCentering = inputFile->getCentering();

    inputFile->getMenus().center7->setChecked(fileCentering == Centering::Seven);
    inputFile->getMenus().center0->setChecked(fileCentering == Centering::Zero);
}

void TASToolKitEditor::adjustMenuOnClose(InputFile* inputFile)
{
    if (m_filesLoaded == 0)
    {
        inputFile->getMenus().center7->setChecked(false);
        inputFile->getMenus().center0->setChecked(false);
        actionSwapFiles->setEnabled(false);
    }
}

void TASToolKitEditor::setTableViewSettings(QTableView* pTable)
{
    pTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    pTable->horizontalHeader()->setMinimumSectionSize(0); // prevents minimum column size enforcement
    pTable->setVisible(false);
}

void TASToolKitEditor::showError(const QString& errTitle, const QString& errMsg)
{
    QMessageBox::warning(this, errTitle, errMsg, QMessageBox::Ok);
}

void TASToolKitEditor::addMenuItems()
{
    setMenuBar(menuBar = new QMenuBar(this));

    addFileMenuItems();
    addPlayerMenuItems();
    addGhostMenuItems();
}

void TASToolKitEditor::addFileMenuItems()
{
    menuFile = new QMenu(menuBar);
    actionOpenPlayer = new QAction(this);
    actionOpenGhost = new QAction(this);
    actionClosePlayer = new QAction(this);
    actionClosePlayer->setEnabled(false);
    actionCloseGhost = new QAction(this);
    actionCloseGhost->setEnabled(false);
    actionSwapFiles = new QAction(this);
    actionSwapFiles->setEnabled(false);
    actionScrollTogether = new QAction(this);
    actionScrollTogether->setEnabled(false);
    actionScrollTogether->setCheckable(true);
    actionScrollTogether->setChecked(false);
    menuFile->addAction(actionOpenPlayer);
    menuFile->addAction(actionOpenGhost);
    menuFile->addAction(actionClosePlayer);
    menuFile->addAction(actionCloseGhost);
    menuFile->addAction(actionSwapFiles);
    menuFile->addAction(actionScrollTogether);
    menuBar->addAction(menuFile->menuAction());
}

void TASToolKitEditor::addPlayerMenuItems()
{
    menuPlayer = new QMenu(menuBar);
    menuPlayer->menuAction()->setVisible(false);
    actionUndoPlayer = new QAction(this);
    actionUndoPlayer->setEnabled(false);
    actionRedoPlayer = new QAction(this);
    actionRedoPlayer->setEnabled(false);
    action0CenteredPlayer = new QAction(this);
    action0CenteredPlayer->setCheckable(true);
    action7CenteredPlayer = new QAction(this);
    action7CenteredPlayer->setCheckable(true);
    menuCenterPlayer = new QMenu(menuFile);
    menuCenterPlayer->addAction(action0CenteredPlayer);
    menuCenterPlayer->addAction(action7CenteredPlayer);
    menuPlayer->addAction(actionUndoPlayer);
    menuPlayer->addAction(actionRedoPlayer);
    menuPlayer->addAction(menuCenterPlayer->menuAction());
    menuBar->addAction(menuPlayer->menuAction());
}

void TASToolKitEditor::addGhostMenuItems()
{
    menuGhost = new QMenu(menuBar);
    menuGhost->menuAction()->setVisible(false);
    actionUndoGhost = new QAction(this);
    actionUndoGhost->setEnabled(false);
    actionRedoGhost = new QAction(this);
    actionRedoGhost->setEnabled(false);
    action0CenteredGhost = new QAction(this);
    action0CenteredGhost->setCheckable(true);
    action7CenteredGhost = new QAction(this);
    action7CenteredGhost->setCheckable(true);
    menuCenterGhost = new QMenu(menuFile);
    menuCenterGhost->addAction(action0CenteredGhost);
    menuCenterGhost->addAction(action7CenteredGhost);

    menuGhost->addAction(actionUndoGhost);
    menuGhost->addAction(actionRedoGhost);
    menuGhost->addAction(menuCenterGhost->menuAction());
    menuBar->addAction(menuGhost->menuAction());
}

void TASToolKitEditor::setupUi()
{
    resize(DEFAULT_WINDOW_WIDTH, DEFAULT_WINDOW_HEIGHT);
    addMenuItems();

    centralWidget = new QWidget(this);
    
    horizontalLayoutWidget = new QWidget(centralWidget);
    mainHorizLayout = new QHBoxLayout(horizontalLayoutWidget);
    mainHorizLayout->setSpacing(TABLE_SIDE_PADDING);
    mainHorizLayout->setContentsMargins(11, 11, 11, 11);
   
    centralWidget->setLayout(mainHorizLayout);

    playerVLayout = new QVBoxLayout();
    playerVLayout->setSpacing(6);
    playerLabel = new QLabel(horizontalLayoutWidget);
    playerLabel->setVisible(false);
    playerVLayout->addWidget(playerLabel);

    playerTableView = new QTableView(horizontalLayoutWidget);
    setTableViewSettings(playerTableView);

    playerVLayout->addWidget(playerTableView);

    mainHorizLayout->addLayout(playerVLayout);

    ghostVLayout = new QVBoxLayout();
    ghostVLayout->setSpacing(6);
    ghostLabel = new QLabel(horizontalLayoutWidget);
    ghostLabel->setVisible(false);
    ghostVLayout->addWidget(ghostLabel);

    ghostTableView = new QTableView(horizontalLayoutWidget);
    setTableViewSettings(ghostTableView);

    ghostVLayout->addWidget(ghostTableView);

    mainHorizLayout->addLayout(ghostVLayout);

    setCentralWidget(centralWidget);

    m_filesLoaded = 0;
    m_bScrollTogether = false;

    setTitles();
}

void TASToolKitEditor::setTitles()
{
    setTitleNames();
    setTitleShortcuts();
}

void TASToolKitEditor::setTitleNames()
{
    setWindowTitle("TTK Input Editor");
    actionUndoPlayer->setText("Undo");
    actionRedoPlayer->setText("Redo");
    actionUndoGhost->setText("Undo");
    actionRedoGhost->setText("Redo");
    actionOpenPlayer->setText("Open Player");
    actionOpenGhost->setText("Open Ghost");
    actionClosePlayer->setText("Close Player");
    actionCloseGhost->setText("Close Ghost");
    action0CenteredGhost->setText("0 Centered");
    action0CenteredPlayer->setText("0 Centered");
    action7CenteredGhost->setText("7 Centered");
    action7CenteredPlayer->setText("7 Centered");
    actionSwapFiles->setText("Swap Player and Ghost");
    actionScrollTogether->setText("Scroll Together");
    playerLabel->setText("Player");
    ghostLabel->setText("Ghost");
    menuFile->setTitle("File");
    menuCenterGhost->setTitle("Input Centering");
    menuCenterPlayer->setTitle("Input Centering");
    menuPlayer->setTitle("Player");
    menuGhost->setTitle("Ghost");
}

void TASToolKitEditor::setTitleShortcuts()
{
#if QT_CONFIG(shortcut)
    actionUndoPlayer->setShortcut(QString("Ctrl+Z"));
    actionRedoPlayer->setShortcut(QString("Ctrl+Y"));
    actionUndoGhost->setShortcut(QString("Ctrl+Shift+Z"));
    actionRedoGhost->setShortcut(QString("Ctrl+Shift+Y"));
    actionOpenPlayer->setShortcut(QString("Ctrl+O"));
    actionOpenGhost->setShortcut(QString("Ctrl+Shift+O"));
    actionClosePlayer->setShortcut(QString("Esc"));
    actionCloseGhost->setShortcut(QString("Shift+Esc"));
#endif
}