﻿#include "saisieQT_window.h"
#include "ui_saisieQT_window.h"


SaisieQtWindow::SaisieQtWindow(int mode, QWidget *parent) :
        QMainWindow(parent),
        _ui(new Ui::SaisieQtWindow),
        _Engine(new cEngine),
        _layout_GLwidgets(new QGridLayout),
        _zoomLayout(new QGridLayout),
        _params(new cParameters),
        _appMode(mode),
        _bSaved(false)
{
    _ui->setupUi(this);

    _params->read();

    _Engine->setParams(_params);

    initData();

    init(_params, _appMode > MASK3D);

    setUI();

    connectActions();

    createRecentFileMenu();

    applyParams();

    if (_appMode != MASK3D)
    {
        setImagePosition(QPointF(-1.f,-1.f));
        setImageName("");
    }

    tableView_PG()->setContextMenuPolicy(Qt::CustomContextMenu);
    tableView_Images()->setContextMenuPolicy(Qt::CustomContextMenu);
    tableView_Objects()->setContextMenuPolicy(Qt::CustomContextMenu);

    tableView_PG()->setMouseTracking(true);
    tableView_Objects()->setMouseTracking(true);
}

SaisieQtWindow::~SaisieQtWindow()
{
    delete _ui;
    delete _Engine;
    if (_appMode <= MASK3D) delete _RFMenu;
    delete _layout_GLwidgets;
    delete _zoomLayout;
    delete _signalMapper;
    delete _params;
}

void SaisieQtWindow::connectActions()
{
    _ProgressDialog = new QProgressDialog("Loading files", "Stop",0,100,this, Qt::ToolTip);

    connect(&_FutureWatcher, SIGNAL(finished()),_ProgressDialog, SLOT(cancel()));

    for (int aK = 0; aK < nbWidgets();++aK)
    {
        connect(getWidget(aK),	SIGNAL(filesDropped(const QStringList&, bool)), this,	SLOT(addFiles(const QStringList&, bool)));
        connect(getWidget(aK),	SIGNAL(overWidget(void*)), this,SLOT(changeCurrentWidget(void*)));
        connect(this, SIGNAL(selectPoint(QString)),getWidget(aK),SLOT(selectPoint(QString)));
    }

    //File menu
    if (_appMode <= MASK3D) connect(_ui->actionClose_all, SIGNAL(triggered()), this, SLOT(closeAll()));

    for (int i = 0; i < MaxRecentFiles; ++i)
    {
        _recentFileActs[i] = new QAction(this);
        _recentFileActs[i]->setVisible(false);
        connect(_recentFileActs[i], SIGNAL(triggered()), this, SLOT(openRecentFile()));
    }

    //Zoom menu
    _signalMapper = new QSignalMapper (this);

    connect(_ui->action4_1_400,		    SIGNAL(triggered()),   _signalMapper, SLOT(map()));
    connect(_ui->action2_1_200,		    SIGNAL(triggered()),   _signalMapper, SLOT(map()));
    connect(_ui->action1_1_100,		    SIGNAL(triggered()),   _signalMapper, SLOT(map()));
    connect(_ui->action1_2_50,		    SIGNAL(triggered()),   _signalMapper, SLOT(map()));
    connect(_ui->action1_4_25,		    SIGNAL(triggered()),   _signalMapper, SLOT(map()));

    _signalMapper->setMapping (_ui->action4_1_400, 400);
    _signalMapper->setMapping (_ui->action2_1_200, 200);
    _signalMapper->setMapping (_ui->action1_1_100, 100);
    _signalMapper->setMapping (_ui->action1_2_50, 50);
    _signalMapper->setMapping (_ui->action1_4_25, 25);

    connect (_signalMapper, SIGNAL(mapped(int)), this, SLOT(zoomFactor(int)));

}

void SaisieQtWindow::createRecentFileMenu()
{
    if (_appMode <= MASK3D)
    {
        _RFMenu = new QMenu(tr("&Recent files"), this);

        _ui->menuFile->insertMenu(_ui->actionSettings, _RFMenu);

        for (int i = 0; i < MaxRecentFiles; ++i)
            _RFMenu->addAction(_recentFileActs[i]);

        updateRecentFileActions();
    }
}

void SaisieQtWindow::setPostFix(QString str)
{
    _params->setPostFix(str);

    _Engine->setPostFix();
}

QString SaisieQtWindow::getPostFix()
{
    return _params->getPostFix();
}

void SaisieQtWindow::progression()
{
    if(_incre)
        _ProgressDialog->setValue(*_incre);
}

void SaisieQtWindow::runProgressDialog(QFuture<void> future)
{
    _FutureWatcher.setFuture(future);
    _ProgressDialog->setWindowModality(Qt::WindowModal);
    int ax = pos().x() + (size().width()  - _ProgressDialog->size().width())/2;
    int ay = pos().y() + (size().height() - _ProgressDialog->size().height())/2;
    _ProgressDialog->move(ax, ay);
    _ProgressDialog->exec();

    future.waitForFinished();
}

void SaisieQtWindow::loadPly(const QStringList& filenames)
{
    QTimer *timer_test = new QTimer(this);
    _incre = new int(0);
    connect(timer_test, SIGNAL(timeout()), this, SLOT(progression()));
    timer_test->start(10);

    runProgressDialog(QtConcurrent::run(_Engine, &cEngine::loadClouds,filenames,_incre));

    timer_test->stop();
    disconnect(timer_test, SIGNAL(timeout()), this, SLOT(progression()));
    delete _incre;
    delete timer_test;
}

void SaisieQtWindow::addFiles(const QStringList& filenames, bool setGLData)
{
    if (filenames.size())
    {
        for (int i=0; i< filenames.size();++i)
        {
            if(!QFile(filenames[i]).exists())
            {
                QMessageBox::critical(this, "Error", "File does not exist (or bad argument)");
                return;
            }
        }

        _Engine->setFilenamesAndDir(filenames);

        QString suffix = QFileInfo(filenames[0]).suffix();

        if (suffix == "ply")
        {
            loadPly(filenames);

            _appMode = MASK3D;
        }
        else if (suffix == "xml")
        {
            runProgressDialog(QtConcurrent::run(_Engine, &cEngine::loadCameras, filenames));

            _ui->actionShow_cams->setChecked(true);

            _appMode = MASK3D;
        }
        else // LOAD IMAGE
        {
            if (_appMode <= MASK3D)
            {
                closeAll();
                initData(); //TODO: ne pas détruire les polygones dans le closeAll
            }

            if ((filenames.size() == 1) && (_appMode == MASK3D)) _appMode = MASK2D;

            _Engine->loadImages(filenames);
        }

        _Engine->allocAndSetGLData(_appMode, *_params);

        if (setGLData)
        {
            for (int aK = 0; aK < nbWidgets();++aK)
            {
                getWidget(aK)->setGLData(_Engine->getGLData(aK), _ui->actionShow_messages->isChecked());
                getWidget(aK)->setParams(_params);

                if (aK < filenames.size()) getWidget(aK)->getHistoryManager()->setFilename(_Engine->getFilenamesIn()[aK]);
            }
        }
        else
            emit imagesAdded(-4, false);

        for (int aK=0; aK < filenames.size();++aK) setCurrentFile(filenames[aK]);

        updateUI();
    }
}

void SaisieQtWindow::on_actionFullScreen_toggled(bool state)
{
    _params->setFullScreen(state);

    return state ? showFullScreen() : showNormal();
}

void SaisieQtWindow::on_actionShow_ball_toggled(bool state)
{
    if (_appMode == MASK3D)
    {
        currentWidget()->setOption(cGLData::OpShow_Ball,state);

        if (state && _ui->actionShow_axis->isChecked())
        {
            currentWidget()->setOption(cGLData::OpShow_BBox,!state);
            _ui->actionShow_axis->setChecked(!state);
        }
    }
}

void SaisieQtWindow::on_actionShow_bbox_toggled(bool state)
{
    if (_appMode == MASK3D)
        currentWidget()->setOption(cGLData::OpShow_BBox,state);
}

void SaisieQtWindow::on_actionShow_grid_toggled(bool state)
{
    if (_appMode == MASK3D)
        currentWidget()->setOption(cGLData::OpShow_Grid,state);
}

void SaisieQtWindow::on_actionShow_axis_toggled(bool state)
{
    if (_appMode == MASK3D)
    {
        currentWidget()->setOption(cGLData::OpShow_Axis,state);

        if (state && _ui->actionShow_ball->isChecked())
        {
            currentWidget()->setOption(cGLData::OpShow_Ball,!state);
            _ui->actionShow_ball->setChecked(!state);
        }
    }
}

void SaisieQtWindow::on_actionShow_cams_toggled(bool state)
{
    if (_appMode == MASK3D)
        currentWidget()->setOption(cGLData::OpShow_Cams,state);
}

void SaisieQtWindow::on_actionShow_messages_toggled(bool state)
{
    currentWidget()->setOption(cGLData::OpShow_Mess,state);

    labelShowMode(state);
}

void SaisieQtWindow::on_actionShow_names_toggled(bool show)
{
    for (int aK = 0; aK < nbWidgets();++aK)
    {
        if (getWidget(aK)->hasDataLoaded())
        {
            getWidget(aK)->getGLData()->currentPolygon()->showNames(show);
            getWidget(aK)->update();
        }
    }
}

void SaisieQtWindow::on_actionShow_refuted_toggled(bool show)
{
    for (int aK = 0; aK < nbWidgets();++aK)
    {
        if (getWidget(aK)->hasDataLoaded())
        {
            getWidget(aK)->getGLData()->currentPolygon()->showRefuted(show);
            getWidget(aK)->update();
        }
    }

    emit showRefuted( show );
}

void SaisieQtWindow::on_actionToggleMode_toggled(bool mode)
{
    if (_appMode == MASK3D)
        currentWidget()->setInteractionMode(mode ? SELECTION : TRANSFORM_CAMERA,_ui->actionShow_messages->isChecked());
}

void SaisieQtWindow::on_actionHelpShortcuts_triggered()
{
    QString title = QApplication::applicationName() + tr(" shortcuts");
    cHelpDlg *helpDialog = new cHelpDlg(title, this);

    const QPoint global = qApp->desktop()->availableGeometry().center();
    helpDialog->move(global.x() - helpDialog->width() / 2, global.y() - helpDialog->height() / 2);

    helpDialog->show();

    QStringList shortcuts;
    QStringList actions;

    if (_appMode == MASK3D)
    {
        shortcuts.push_back("Ctrl+P");
        actions.push_back(tr("open .ply files"));
        shortcuts.push_back("Ctrl+C");
        actions.push_back(tr("open .xml camera files"));
    }
    if (_appMode <= MASK3D)
    {
        shortcuts.push_back("Ctrl+O");
        actions.push_back(tr("open image file"));
        if (_appMode == MASK3D)
        {
            shortcuts.push_back("Ctrl+E");
        actions.push_back(tr("save .xml selection infos"));
        }
        shortcuts.push_back("Ctrl+S");
        actions.push_back(tr("save mask file"));
        shortcuts.push_back("Ctrl+Maj+S");
        actions.push_back(tr("save mask file as"));
        shortcuts.push_back("Ctrl+X");
        actions.push_back(tr("close files"));
    }
    shortcuts.push_back("Ctrl+T");
    actions.push_back(tr("settings"));
    shortcuts.push_back("Ctrl+Q");
    actions.push_back(tr("quit"));

    shortcuts.push_back("F2");
    actions.push_back(tr("full screen"));
    if (_appMode == MASK3D)
    {
        shortcuts.push_back("F3");
        actions.push_back(tr("show axis"));
        shortcuts.push_back("F4");
        actions.push_back(tr("show ball"));
        shortcuts.push_back("F5");
        actions.push_back(tr("show bounding box"));
        shortcuts.push_back("F6");
        actions.push_back(tr("show grid"));
        shortcuts.push_back("F7");
        actions.push_back(tr("show cameras"));
    }
    shortcuts.push_back("F8");
    actions.push_back(tr("show messages"));
    if (_appMode > MASK3D)
    {
         shortcuts.push_back("Ctrl+N");
         actions.push_back(tr("show names"));
         shortcuts.push_back("Ctrl+R");
         actions.push_back(tr("show refuted"));
    }

    if (_appMode == MASK3D)
    {
        shortcuts.push_back(tr("Key +/-"));
        actions.push_back(tr("increase/decrease point size"));
    }
    else
    {
        shortcuts.push_back(tr("Key +/-"));
        actions.push_back(tr("zoom +/-"));
        shortcuts.push_back(tr("Key 9"));
        actions.push_back(tr("zoom fit"));
        shortcuts.push_back("Key 4");
         actions.push_back(tr("zoom 400%"));
        shortcuts.push_back(tr("Key 2"));
        actions.push_back(tr("zoom 200%"));
        shortcuts.push_back(tr("Key 1"));
        actions.push_back(tr("zoom 100%"));
        shortcuts.push_back("Ctrl+2");
        actions.push_back(tr("zoom 50%"));
        shortcuts.push_back("Ctrl+4");
        actions.push_back(tr("zoom 25%"));
    }

    shortcuts.push_back("Shift+R");
    actions.push_back(tr("reset view"));

    if (_appMode <= MASK3D)
    {
        if (_appMode == MASK3D)
        {
            shortcuts.push_back("F9");
            actions.push_back(tr("move mode / selection mode (only 3D)"));
        }
        shortcuts.push_back(tr("Left click"));
        actions.push_back(tr("add a vertex to polyline"));
        shortcuts.push_back(tr("Right click"));
        actions.push_back(tr("close polyline or delete nearest vertex"));
        shortcuts.push_back(tr("Echap"));
        actions.push_back(tr("delete polyline"));

#ifdef ELISE_Darwin
    #if ELISE_QT_VERSION >= 5
            shortcuts.push_back("Ctrl+U");
            actions.push_back(tr("select inside polyline"));
            shortcuts.push_back("Ctrl+Y");
            actions.push_back(tr("remove inside polyline"));
    #else
            shortcuts.push_back(tr("Space bar"));
            actions.push_back(tr("select inside polyline"));
            shortcuts.push_back(tr("Del"));
            actions.push_back(tr("remove inside polyline"));
    #endif
#else
        shortcuts.push_back(tr("Space bar"));
        actions.push_back(tr("select inside polyline"));
        shortcuts.push_back(tr("Del"));
        actions.push_back(tr("remove inside polyline"));
#endif

        shortcuts.push_back(tr("Shift+drag"));
        actions.push_back(tr("insert vertex in polyline"));
        shortcuts.push_back(tr("Ctrl+right click"));
        actions.push_back(tr("remove last vertex"));
        shortcuts.push_back(tr("Drag & drop"));
        actions.push_back(tr("move selected polyline vertex"));
        shortcuts.push_back(tr("Arrow keys"));
        actions.push_back(tr("move selected vertex"));
        shortcuts.push_back(tr("Alt+arrow keys"));
        actions.push_back(tr("move selected vertex faster"));
        shortcuts.push_back("Ctrl+A");
        actions.push_back(tr("select all"));
        shortcuts.push_back("Ctrl+D");
        actions.push_back(tr("select none"));
        shortcuts.push_back("Ctrl+R");
        actions.push_back(tr("reset"));
        shortcuts.push_back("Ctrl+I");
        actions.push_back(tr("invert selection"));
    }
    else
    {
        shortcuts.push_back(tr("Left click"));
        actions.push_back(tr("add point"));
        shortcuts.push_back(tr("Right click"));
        actions.push_back(tr("show state menu or window menu"));
        shortcuts.push_back(tr("Drag & drop"));
        actions.push_back(tr("move selected point"));
    }
    shortcuts.push_back("Ctrl+Z");
    actions.push_back(tr("undo last action"));
    shortcuts.push_back("Ctrl+Shift+Z");
    actions.push_back(tr("redo last action"));

    helpDialog->populateTableView(shortcuts, actions);
}

void SaisieQtWindow::on_actionAbout_triggered()
{
    QFont font("Courier New", 9, QFont::Normal);

    QMessageBox *msgBox = new QMessageBox(this);
    msgBox->setText(QString(getBanniereMM3D().c_str()));
    msgBox->setWindowTitle(QApplication::applicationName());
    msgBox->setFont(font);

    //trick to enlarge QMessageBox...
    QSpacerItem* horizontalSpacer = new QSpacerItem(600, 0, QSizePolicy::Minimum, QSizePolicy::Expanding);
    QGridLayout* layout = (QGridLayout*)msgBox->layout();
    layout->addItem(horizontalSpacer, layout->rowCount(), 0, 1, layout->columnCount());

    msgBox->setWindowModality(Qt::NonModal);
    msgBox->show();
}

void SaisieQtWindow::resizeEvent(QResizeEvent *)
{
    _params->setSzFen(size());
}

void SaisieQtWindow::moveEvent(QMoveEvent *)
{
    _params->setPosition(pos());
}

void SaisieQtWindow::on_actionAdd_triggered()
{
    currentWidget()->Select(ADD);
}

void SaisieQtWindow::on_actionSelect_none_triggered()
{
    currentWidget()->Select(NONE);
}

void SaisieQtWindow::on_actionInvertSelected_triggered()
{
    currentWidget()->Select(INVERT);
}

void SaisieQtWindow::on_actionSelectAll_triggered()
{
    currentWidget()->Select(ALL);
}

void SaisieQtWindow::on_actionReset_triggered()
{
    if (_appMode != MASK3D)
    {
        closeAll();
        initData();

        addFiles(_Engine->getFilenamesIn());
    }
    else
    {
        currentWidget()->Select(ALL);
    }
}

void SaisieQtWindow::on_actionRemove_triggered()
{
    if (_appMode > MASK3D)
        currentWidget()->polygon()->removeSelectedPoint();  //TODO: actuellement on ne garde pas le point selectionné (ajouter une action)
    else
        currentWidget()->Select(SUB);
}

void SaisieQtWindow::on_actionSetViewTop_triggered()
{
    if (_appMode == MASK3D)
        currentWidget()->setView(TOP_VIEW);
}

void SaisieQtWindow::on_actionSetViewBottom_triggered()
{
    if (_appMode == MASK3D)
        currentWidget()->setView(BOTTOM_VIEW);
}

void SaisieQtWindow::on_actionSetViewFront_triggered()
{
    if (_appMode == MASK3D)
        currentWidget()->setView(FRONT_VIEW);
}

void SaisieQtWindow::on_actionSetViewBack_triggered()
{
    if (_appMode == MASK3D)
        currentWidget()->setView(BACK_VIEW);
}

void SaisieQtWindow::on_actionSetViewLeft_triggered()
{
    if (_appMode == MASK3D)
        currentWidget()->setView(LEFT_VIEW);
}

void SaisieQtWindow::on_actionSetViewRight_triggered()
{
    if (_appMode == MASK3D)
        currentWidget()->setView(RIGHT_VIEW);
}

void SaisieQtWindow::on_actionReset_view_triggered()
{
    currentWidget()->resetView(true,true,true,true);
}

void SaisieQtWindow::on_actionZoom_Plus_triggered()
{
    currentWidget()->setZoom(currentWidget()->getZoom()*1.5f);
}

void SaisieQtWindow::on_actionZoom_Moins_triggered()
{
    currentWidget()->setZoom(currentWidget()->getZoom()/1.5f);
}

void SaisieQtWindow::on_actionZoom_fit_triggered()
{
    currentWidget()->zoomFit();
}

void SaisieQtWindow::zoomFactor(int aFactor)
{
    currentWidget()->zoomFactor(aFactor);
}

void SaisieQtWindow::on_actionLoad_plys_triggered()
{
    addFiles(QFileDialog::getOpenFileNames(this, tr("Open Cloud Files"),QString(), tr("Files (*.ply)")));
}

void SaisieQtWindow::on_actionLoad_camera_triggered()
{
    addFiles(QFileDialog::getOpenFileNames(this, tr("Open Camera Files"),QString(), tr("Files (*.xml)")));
}

void SaisieQtWindow::on_actionLoad_image_triggered()
{
    QString img_filename = QFileDialog::getOpenFileName(this, tr("Open Image File"),QString(), tr("File (*.*)"));

    if (!img_filename.isEmpty())
    {
        //TODO: factoriser
        QStringList & filenames = _Engine->getFilenamesIn();
        filenames.clear();
        filenames.push_back(img_filename);

        setCurrentFile(img_filename);

        addFiles(filenames);
    }
}

void SaisieQtWindow::on_actionSave_masks_triggered()
{
    _Engine->saveMask(currentWidgetIdx(), currentWidget()->isFirstAction());
    _bSaved = true;
}

void SaisieQtWindow::on_actionSave_as_triggered()
{
    QString fname = QFileDialog::getSaveFileName(NULL, tr("Save mask Files"),QString(), tr("Files (*.*)"));

    if (!fname.isEmpty())
    {
        _Engine->setFilenameOut(fname);

        _Engine->saveMask(currentWidgetIdx(), currentWidget()->isFirstAction());
    }
}

void SaisieQtWindow::on_actionSave_selection_triggered()
{
    currentWidget()->getHistoryManager()->save();
}

void SaisieQtWindow::on_actionSettings_triggered()
{
    cSettingsDlg uiSettings(this, _params);
    connect(&uiSettings, SIGNAL(nbFenChanged(bool)), this, SLOT(redraw(bool)));

    connect(&uiSettings, SIGNAL(prefixTextEdit(QString)), this, SLOT(setAutoName(QString)));

    for (int aK = 0; aK < nbWidgets();++aK)
    {
        connect(&uiSettings, SIGNAL(lineThicknessChanged(float)), getWidget(aK), SLOT(lineThicknessChanged(float)));
        connect(&uiSettings, SIGNAL(pointDiameterChanged(float)), getWidget(aK), SLOT(pointDiameterChanged(float)));
        connect(&uiSettings, SIGNAL(gammaChanged(float)),         getWidget(aK), SLOT(gammaChanged(float)));
        connect(&uiSettings, SIGNAL(selectionRadiusChanged(int)), getWidget(aK), SLOT(selectionRadiusChanged(int)));
        connect(&uiSettings, SIGNAL(shiftStepChanged(float)),     getWidget(aK), SLOT(shiftStepChanged(float)));
    }

    if (zoomWidget() != NULL)
    {
        connect(&uiSettings, SIGNAL(zoomWindowChanged(float)), zoomWidget(), SLOT(setZoom(float)));
        //connect(zoomWidget(), SIGNAL(zoomChanged(float)), this, SLOT(setZoom(float)));
    }

    const QPoint global = qApp->desktop()->availableGeometry().center();
    uiSettings.move(global.x() - uiSettings.width() / 2, global.y() - uiSettings.height() / 2);

    //uiSettings.setFixedSize(uiSettings.size());
    uiSettings.exec();

    /*#if defined(Q_OS_SYMBIAN)
        uiSettings.showMaximized();
    #else
        uiSettings.show();
    #endif*/

    disconnect(&uiSettings, 0, 0, 0);
}

void SaisieQtWindow::closeAll()
{
    emit sCloseAll();

    _Engine->unloadAll();

    for (int aK=0; aK < nbWidgets(); ++aK)
        getWidget(aK)->reset();

    if (zoomWidget() != NULL)
    {
        zoomWidget()->reset();
        zoomWidget()->setOption(cGLData::OpShow_Mess,false);
    }

    setImageName("");
}

void SaisieQtWindow::closeCurrentWidget()
{
    _Engine->unloadAll();
    //_Engine->unload(currentWidgetIdx());

    currentWidget()->reset();
}

void SaisieQtWindow::openRecentFile()
{
    // A TESTER en multi images

#if WINVER == 0x0601
    QAction *action = dynamic_cast<QAction *>(sender());
#else
    QAction *action = qobject_cast<QAction *>(sender());
#endif

    if (action)
    {
        _Engine->setFilenamesAndDir(QStringList(action->data().toString()));

        addFiles(_Engine->getFilenamesIn());
    }
}

void SaisieQtWindow::setCurrentFile(const QString &fileName)
{
    // Rafraichit le menu des fichiers récents
    _curFile = fileName;
    setWindowFilePath(_curFile);

    QSettings settings;
    QStringList files = settings.value("recentFileList").toStringList();
    QString fname = QDir::toNativeSeparators(fileName);
    files.removeAll(fname);
    files.prepend(fname);
    while (files.size() > MaxRecentFiles)
        files.removeLast();

    settings.setValue("recentFileList", files);

    foreach (QWidget *widget, QApplication::topLevelWidgets())
    {
        #if WINVER == 0x0601
            SaisieQtWindow *mainWin = dynamic_cast<SaisieQtWindow *>(widget);
        #else
            SaisieQtWindow *mainWin = qobject_cast<SaisieQtWindow *>(widget);
        #endif
        if (mainWin)
            mainWin->updateRecentFileActions();
    }

}

void SaisieQtWindow::updateRecentFileActions()
{
    QSettings settings;
    QStringList files = settings.value("recentFileList").toStringList();

    int numRecentFiles = qMin(files.size(), (int)MaxRecentFiles);

    for (int i = 0; i < numRecentFiles; ++i) {
        QString text = tr("&%1 - %2").arg(i + 1).arg(strippedName(files[i]));
        _recentFileActs[i]->setText(text);
        _recentFileActs[i]->setData(files[i]);
        _recentFileActs[i]->setVisible(true);
    }
    for (int j = numRecentFiles; j < MaxRecentFiles; ++j)
        _recentFileActs[j]->setVisible(false);

    //m_RFMenu->setVisible(numRecentFiles > 0);
}

QString SaisieQtWindow::strippedName(const QString &fullFileName)
{
    return QFileInfo(fullFileName).fileName();
}

void hideAction(QAction* action, bool show)
{
    action->setVisible(show);
    action->setEnabled(show);
}

void SaisieQtWindow::setLayout(uint sy)
{
    _layout_GLwidgets->setContentsMargins(sy,sy,sy,sy);
    _layout_GLwidgets->setHorizontalSpacing(sy);
    _layout_GLwidgets->setVerticalSpacing(sy);
    _ui->QFrame_OpenglLayout->setLayout(_layout_GLwidgets);

    int cpt=0;
    for (int aK = 0; aK < _params->getNbFen().x();++aK)
        for (int bK = 0; bK < _params->getNbFen().y();++bK, cpt++)
            _layout_GLwidgets->addWidget(getWidget(cpt), bK, aK);
}

void SaisieQtWindow::updateUI()
{
    labelShowMode(true);

    bool isMode3D = _appMode == MASK3D;

    hideAction(_ui->actionLoad_plys,  isMode3D);
    hideAction(_ui->actionLoad_camera,isMode3D);
    hideAction(_ui->actionShow_cams,  isMode3D);
    hideAction(_ui->actionShow_axis,  isMode3D);
    hideAction(_ui->actionShow_ball,  isMode3D);
    hideAction(_ui->actionShow_bbox,  isMode3D);
    hideAction(_ui->actionShow_grid,  isMode3D);
    hideAction(_ui->actionShow_cams,  isMode3D);
    hideAction(_ui->actionToggleMode, isMode3D);

    bool isModeMask = _appMode <= MASK3D;
    hideAction(_ui->actionShow_names, !isModeMask);
    hideAction(_ui->actionShow_refuted, !isModeMask);

    //disable some actions
    hideAction(_ui->actionAdd, isModeMask);
    hideAction(_ui->actionSelect_none, isModeMask);
    hideAction(_ui->actionInvertSelected, isModeMask);
    hideAction(_ui->actionSelectAll, isModeMask);
    hideAction(_ui->actionReset, isModeMask);

    hideAction(_ui->actionRemove, isModeMask);

    _ui->menuStandard_views->menuAction()->setVisible(isMode3D);
}

void SaisieQtWindow::setUI()
{
    setLayout(0);

#ifdef ELISE_Darwin
#if(ELISE_QT_VERSION >= 5) //TODO: verifier avec QT5 - mettre a jour l'aide
    _ui->actionRemove->setShortcut(QKeySequence(Qt::ControlModifier + Qt::Key_Y));
    _ui->actionAdd->setShortcut(QKeySequence(Qt::ControlModifier + Qt::Key_U));
#endif
#endif

    updateUI();

    if (_appMode > MASK3D)
    {
        if (_appMode == POINT2D_INIT)          setWindowTitle("Micmac - SaisieAppuisInitQT");
        else if (_appMode == POINT2D_PREDIC)   setWindowTitle("Micmac - SaisieAppuisPredicQT");
        else if (_appMode == BASC)             setWindowTitle("Micmac - SaisieBascQT");

        hideAction(_ui->actionLoad_image, false);
        hideAction(_ui->actionSave_masks, false);
        hideAction(_ui->actionSave_as, false);
        hideAction(_ui->actionSave_selection, false);
        hideAction(_ui->actionClose_all, false);

        //zoom Window
        _zoomLayout->addWidget(zoomWidget());
        _zoomLayout->setContentsMargins(2,2,2,2);
        _ui->QFrame_zoom->setLayout(_zoomLayout);
        _ui->QFrame_zoom->setContentsMargins(0,0,0,0);

         QGridLayout*            _tdLayout = new QGridLayout;

         _tdLayout->addWidget(threeDWidget());
         _tdLayout->setContentsMargins(2,2,2,2);
        _ui->frame_preview3D->setLayout(_tdLayout);
        _ui->frame_preview3D->setContentsMargins(0,0,0,0);
        _ui->menuSelection->setTitle(tr("H&istory"));

        tableView_PG()->installEventFilter(this);
        tableView_Objects()->installEventFilter(this);

        _ui->splitter_Tools->setContentsMargins(2,0,0,0);
    }
    else
    {
        _ui->splitter_Tools->hide();
    }

    /*if (_appMode != BASC)*/ _ui->tableView_Objects->hide();
}

bool SaisieQtWindow::eventFilter( QObject* object, QEvent* event )
{
    if( object == tableView_PG() )
    {
        QAbstractItemView* table    = (QAbstractItemView*)object;

        QItemSelectionModel* sModel = table->selectionModel();

        if(sModel)
        {
            QString pointName = sModel->currentIndex().data(Qt::DisplayRole).toString();

            if(event->type() == QEvent::KeyRelease )
            {
                QKeyEvent *keyEvent = static_cast<QKeyEvent *>(event);

                if (keyEvent->key() == Qt::Key_Delete)

                    emit removePoint(pointName); // we send point name, because point has not necessarily a widget index (point non saisi)

                else if (keyEvent->key() == Qt::Key_Up || keyEvent->key() == Qt::Key_Down)
                {
                    emit selectPoint(pointName);
                }
            }
        }
    }

    return false;
}

QTableView *SaisieQtWindow::tableView_PG(){return _ui->tableView_PG;}

QTableView *SaisieQtWindow::tableView_Images(){return _ui->tableView_Images;}

QTableView *SaisieQtWindow::tableView_Objects(){return _ui->tableView_Objects;}

void SaisieQtWindow::resizeTables()
{
    tableView_PG()->resizeColumnsToContents();
    tableView_PG()->resizeRowsToContents();
    tableView_PG()->horizontalHeader()->setStretchLastSection(true);

    tableView_Images()->resizeColumnsToContents();
    tableView_Images()->resizeRowsToContents();
    tableView_Images()->horizontalHeader()->setStretchLastSection(true);

    tableView_Objects()->resizeColumnsToContents();
    tableView_Objects()->resizeRowsToContents();
    tableView_Objects()->horizontalHeader()->setStretchLastSection(true);
}

void SaisieQtWindow::setModel(QAbstractItemModel *model_Pg, QAbstractItemModel *model_Images, QAbstractItemModel *model_Objects)
{
    tableView_PG()->setModel(model_Pg);
    tableView_Images()->setModel(model_Images);
    tableView_Objects()->setModel(model_Objects);
}

void SaisieQtWindow::SelectPointAllWGL(QString pointName)
{
    emit selectPoint(pointName);
}

void SaisieQtWindow::SetDataToGLWidget(int idGLW, cGLData *glData)
{
    if (glData)
    {
        GLWidget * glW = getWidget(idGLW);
        glW->setGLData(glData, glData->stateOption(cGLData::OpShow_Mess));
        glW->setParams(getParams());
    }
}

void SaisieQtWindow::loadPlyIn3DPrev(const QStringList &filenames, cData *dataCache)
{
    if (filenames.size())
    {
        for (int i=0; i< filenames.size();++i)
        {
            if(!QFile(filenames[i]).exists())
            {
                QMessageBox::critical(this, "Error", "File does not exist (or bad argument)");
                return;
            }
        }

        QString suffix = QFileInfo(filenames[0]).suffix();

        if (suffix == "ply")
        {
            loadPly(filenames);
            dataCache->addCloud(getEngine()->getData()->getCloud(0));
            threeDWidget()->getGLData()->clearClouds();
            dataCache->computeBBox();
            threeDWidget()->getGLData()->setData(dataCache,false);
            threeDWidget()->resetView(false,false,false,true);
            option3DPreview();
        }
    }
}

void SaisieQtWindow::setCurrentPolygonIndex(int idx)
{
    for (int aK = 0; aK < getEngine()->nbGLData(); ++aK)
    {
        _Engine->getGLData(aK)->setCurrentPolygonIndex(idx);
    }
}

void SaisieQtWindow::normalizeCurrentPolygon(bool nrm)
{
    for (int aK = 0; aK < getEngine()->nbGLData(); ++aK)
    {
        _Engine->getGLData(aK)->normalizeCurrentPolygon(nrm);
    }
}

void SaisieQtWindow::initData()
{
    if (_appMode == BOX2D)
    {
        _Engine->addObject(new cRectangle());
    }
    /*else if (_appMode == BASC)
    {
        _Engine->addObject(new cPolygon(2)); //line
        _Engine->addObject(new cPolygon(1)); //origin
        _Engine->addObject(new cPolygon(2)); //scale
    }*/
    else
    {
        _Engine->addObject(new cPolygon());
    }
}

void  SaisieQtWindow::setGamma(float aGamma)
{
    _params->setGamma(aGamma);
}

void SaisieQtWindow::closeEvent(QCloseEvent *event)
{
    if ((!_bSaved) && (_appMode == MASK3D || _appMode == MASK2D))
    {
        QMessageBox::StandardButton reply = QMessageBox::question(this, tr("Warning"), tr("Quit without saving?"),QMessageBox::Yes|QMessageBox::No);
        if (reply == QMessageBox::No)
        {
            event->ignore();
            return;
        }
    }

    emit sgnClose();

    if (zoomWidget())
        _params->setZoomWindowValue(zoomWidget()->getZoom());

    _params->write();

    event->accept();
}

void SaisieQtWindow::redraw(bool nbWidgetsChanged)
{
    if (size() != _params->getSzFen())
    {
        if (_appMode > MASK3D)
            resize(_params->getSzFen().width() + _ui->QFrame_zoom->width(), _params->getSzFen().height());
        else
            resize(_params->getSzFen());
    }

    if (nbWidgetsChanged)
    {
        delete _layout_GLwidgets;
        _layout_GLwidgets = new QGridLayout;

        int newWidgetNb = _params->getNbFen().x()*_params->getNbFen().y();
        int col =  _layout_GLwidgets->columnCount();
        int row =  _layout_GLwidgets->rowCount();

        if (col < _params->getNbFen().x() || row < _params->getNbFen().y())
        {
            widgetSetResize(newWidgetNb);

            int cpt = 0;
            for (; cpt < nbWidgets();++cpt)
                _layout_GLwidgets->removeWidget(getWidget(cpt));

            cpt = 0;
            for (int aK =0; aK < _params->getNbFen().x();++aK)
                for (int bK =0; bK < _params->getNbFen().y();++bK)
                {
                    _layout_GLwidgets->addWidget(getWidget(cpt), bK, aK);

                    if (cpt < _Engine->getData()->getNbImages())
                        getWidget(cpt)->setGLData(_Engine->getGLData(cpt),_ui->actionShow_messages);

                    cpt++;
                }
            _ui->QFrame_OpenglLayout->setLayout(_layout_GLwidgets);
        }
        else
        {
            //TODO
        }
    }
}

void SaisieQtWindow::setAutoName(QString val)
{
    emit setName(val);
}

void SaisieQtWindow::setImagePosition(QPointF pt)
{
    QString text(tr("Image position : "));

    if (pt.x() >= 0.f && pt.y() >= 0.f)
    {
        GLWidget* glW = currentWidget();
        if(glW)
            if ( glW->hasDataLoaded() && !glW->getGLData()->is3D() && (glW->isPtInsideIm(pt)))
            {
                int imHeight = glW->getGLData()->glImage()._m_image->height();
                text =  QString(text + QString::number(pt.x(),'f',1) + ", " + QString::number(imHeight - pt.y(),'f',1)+" px");
            }
    }

    _ui->label_ImagePosition_1->setText(text);
    _ui->label_ImagePosition_2->setText(text);
}

void SaisieQtWindow::setImageName(QString name)
{
    _ui->label_ImageName->setText(QString(tr("Image name : ") + name));
}

void SaisieQtWindow::setZoom(float val)
{
    _params->setZoomWindowValue(val);
}

void SaisieQtWindow::changeCurrentWidget(void *cuWid)
{
    GLWidget* glW = (GLWidget*) cuWid;

    setCurrentWidget(glW);

    if (_appMode != MASK3D)
    {
        connect(glW, SIGNAL(newImagePosition(QPointF)), this, SLOT(setImagePosition(QPointF)));

        connect(glW, SIGNAL(gammaChangedSgnl(float)), this, SLOT(setGamma(float)));

        if (zoomWidget())
        {
            zoomWidget()->setGLData(glW->getGLData(),false,true,false);
            zoomWidget()->setZoom(_params->getZoomWindowValue());
            zoomWidget()->setOption(cGLData::OpShow_Mess,false);

            connect(glW, SIGNAL(newImagePosition(QPointF)), zoomWidget(), SLOT(centerViewportOnImagePosition(QPointF)));
        }
    }

    if (_appMode > MASK3D)
    {
        if ( glW->hasDataLoaded() && !glW->getGLData()->isImgEmpty() )
            setImageName(glW->getGLData()->glImage().cObjectGL::name());
    }
}

void SaisieQtWindow::undo(bool undo)
{
    if (_appMode <= MASK3D)
    {
        if (currentWidget()->getHistoryManager()->size())
        {
            if (_appMode != MASK3D)
            {
                int idx = currentWidgetIdx();

                _Engine->reloadImage(_appMode, idx);

                currentWidget()->setGLData(_Engine->getGLData(idx),_ui->actionShow_messages);
            }

            undo ? currentWidget()->getHistoryManager()->undo() : currentWidget()->getHistoryManager()->redo();
            currentWidget()->applyInfos();
            _bSaved = false;
        }
    }
    else
    {
        emit undoSgnl(undo);
    }
}
int SaisieQtWindow::appMode() const
{
    return _appMode;
}

void SaisieQtWindow::setAppMode(int appMode)
{
    _appMode = appMode;
}

void SaisieQtWindow::applyParams()
{
    move(_params->getPosition());

    QSize szFen = _params->getSzFen();

    if (_params->getFullScreen())
    {
        showFullScreen();

        QRect screen = QApplication::desktop()->screenGeometry ( -1 );

        _params->setSzFen(screen.size());
        _params->setPosition(QPoint(0,0));

        _params->write();

        _ui->actionFullScreen->setChecked(true);
    }
    else if (_appMode > MASK3D)
        resize(szFen.width() + _ui->QFrame_zoom->width(), szFen.height());
    else
        resize(szFen);
}

void SaisieQtWindow::labelShowMode(bool state)
{
    if ((!state) || (_appMode == MASK3D))
    {
        _ui->label_ImagePosition_1->hide();
        _ui->label_ImagePosition_2->hide();
        _ui->label_ImageName->hide();
    }
    else
    {
        if(_appMode <= MASK2D)
        {
            _ui->label_ImagePosition_1->hide();
            _ui->label_ImagePosition_2->show();
            _ui->label_ImageName->hide();
        }
        else if(_appMode > MASK3D)
        {
            _ui->label_ImagePosition_1->show();
            _ui->label_ImagePosition_2->hide();
            _ui->label_ImageName->show();
        }
    }
}
