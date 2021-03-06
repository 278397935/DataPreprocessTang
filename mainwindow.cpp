#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QPainter>
#include <QProxyStyle>

class CustomTabStyle : public QProxyStyle
{
public:
    QSize sizeFromContents(ContentsType type, const QStyleOption *option,
                           const QSize &size, const QWidget *widget) const
    {
        QSize s = QProxyStyle::sizeFromContents(type, option, size, widget);
        if (type == QStyle::CT_TabBarTab)
        {
            s.transpose();
            s.rwidth()  = 150; // 设置每个tabBar中item的大小
            s.rheight() = 50;
        }
        return s;
    }

    void drawControl(ControlElement element, const QStyleOption *option, QPainter *painter, const QWidget *widget) const
    {
        if (element == CE_TabBarTabLabel)
        {
            if (const QStyleOptionTab *tab = qstyleoption_cast<const QStyleOptionTab *>(option))
            {
                QRect allRect = tab->rect;

                if (tab->state & QStyle::State_Selected)
                {
                    painter->save();
                    //painter->setPen(Qt::darkBlue);
                    painter->setBrush(QBrush(Qt::blue));
                    painter->drawRect(allRect.adjusted(8, 8, -8, -8));
                    painter->restore();
                }

                QTextOption option;
                option.setAlignment(Qt::AlignVCenter | Qt::AlignHCenter);

                if (tab->state & QStyle::State_Selected)
                {
                    painter->setPen(0xf8fcff);
                }
                else
                {
                    painter->setPen(0x5d5d5d);
                }

                painter->drawText(allRect, tab->text, option);
                return;
            }
        }

        if (element == CE_TabBarTab)
        {
            QProxyStyle::drawControl(element, option, painter, widget);
        }
    }
};

/* 主界面的构造函数 */
MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    this->setWindowTitle("广域数据预处理工具 v2.2 (Design for Engineer Ma)");

    qRegisterMetaType<STATION_INFO>("STATION_INFO");
    qRegisterMetaType< QVector<qreal> >("QVector<qreal>");

    /* Draw marker line */
    ui->actionCutterH->setShortcut(QKeySequence(Qt::CTRL + Qt::Key_H));
    ui->actionCutterV->setShortcut(QKeySequence(Qt::CTRL + Qt::Key_V));

    /* 保存调整结果至Rx类，从csv文件中恢复过来。的快捷键 */
    ui->actionSave->setShortcut(QKeySequence(Qt::CTRL + Qt::Key_S));
    ui->actionRecovery->setShortcut(QKeySequence(Qt::CTRL + Qt::Key_R));

    /* 初始化一些变量，容器。 */
    gmapCurveData.clear();

    gmapCurveItem.clear();

    /* Init Global Var */
    gpoScatter = NULL;

    gpoErrorCurve = NULL;

    gpoSelectedCurve = NULL;
    giSelectedIndex = -1;

    gpoSelectedRX = NULL;

    poMarkerPicker = NULL;

    poPickerCurve = NULL;

    poPickerRho = NULL;

    poDb = NULL;

    /* Init poCurvePicker */
    poPickerCurve = new CanvasPicker( ui->plotCurve );
    connect(poPickerCurve, SIGNAL(SigSelected(QwtPlotCurve*,int)), this, SLOT(Selected(QwtPlotCurve*,int)));

    this->initPlotCurve();

    this->initPlotScatter();


    /* Init Marker line */
    sMkList.poTop    = NULL;
    sMkList.poBottom = NULL;
    sMkList.poLeft   = NULL;
    sMkList.poRight  = NULL;

    ui->actionImportRX->setEnabled(false);

    ui->actionExportRho->setEnabled(false);

    /* 初始状态, 置为:Disable */
    ui->actionImportTX->setEnabled(true);
    ui->actionImportRX->setEnabled(false);
    ui->actionClear->setEnabled(false);
    ui->actionCutterH->setEnabled(false);
    ui->actionCutterV->setEnabled(false);
    ui->actionSave->setEnabled(false);
    ui->actionRecovery->setEnabled(false);
    ui->actionStore->setEnabled(false);
    ui->actionCalRho->setEnabled(false);
    ui->actionExportRho->setEnabled(false);

    poDb = new MyDatabase();
    poDb->connect();

    connect(poDb, SIGNAL(SigModelTX(QSqlTableModel*)), this, SLOT(showTableTX(QSqlTableModel*)));
    connect(poDb, SIGNAL(SigModelRX(QSqlTableModel*)), this, SLOT(showTableRX(QSqlTableModel*)));
    connect(poDb, SIGNAL(SigModelXY(QSqlTableModel*)), this, SLOT(showTableXY(QSqlTableModel*)));
    connect(poDb, SIGNAL(SigModelRho(CustomTableModel*)), this, SLOT(showTableRho(CustomTableModel*)));

    ui->tabWidget->setTabText(0, "电流/I");
    ui->tabWidget->setTabText(1, "场值/mV");
    ui->tabWidget->setTabText(2, "坐标");
    ui->tabWidget->setTabText(3, "广域\u03c1表格");
    ui->tabWidget->setTabText(4, "广域\u03c1曲线");

    /* 根据内容，决定列宽 */
    ui->tableViewTX->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    ui->tableViewRX->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    ui->tableViewXY->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    ui->tableViewRho->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);

    /* 设置选中时为整行选中 */
    ui->tableViewTX->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->tableViewRX->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->tableViewXY->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->tableViewRho->setSelectionBehavior(QAbstractItemView::SelectRows);

    /* 设置表格的单元为只读属性，即不能编辑 */
    ui->tableViewTX->setEditTriggers(QAbstractItemView::NoEditTriggers);
    ui->tableViewRX->setEditTriggers(QAbstractItemView::NoEditTriggers);
    ui->tableViewXY->setEditTriggers(QAbstractItemView::NoEditTriggers);
    ui->tableViewRho->setEditTriggers(QAbstractItemView::NoEditTriggers);

    ui->tabWidget->setTabPosition(QTabWidget::East);
    ui->tabWidget->tabBar()->setStyle(new CustomTabStyle);

    connect(poDb, SIGNAL(SigMsg(QString)), this, SLOT(showMsg(QString)));

    poCalRho = new CalRhoThread(poDb);
    connect(poCalRho, SIGNAL(SigMsg(QString)), this, SLOT(showMsg(QString)));

    this->initPlotTx();
    this->initPlotRx();
    this->initPlotRho();

    connect(poCalRho, SIGNAL(SigRho(STATION, QVector<double>, QVector<double>)), this, SLOT(drawRho(STATION, QVector<double>, QVector<double>)));

    aoStrExisting.clear();

    bModifyField = false;
    bModifyRho   = false;

    ui->splitter->setStretchFactor(0, 3);
    ui->splitter->setStretchFactor(1, 1);
    ui->splitter_2->setStretchFactor(0, 5);
    ui->splitter_2->setStretchFactor(1, 1);
}

MainWindow::~MainWindow()
{
    delete ui;
}

/* 打开发射端电流文件 */
void MainWindow::on_actionImportTX_triggered()
{
    QString oStrFileName = QFileDialog::getOpenFileName(this,
                                                        "打开电流文件",
                                                        QString("%1").arg(this->LastDirRead()),
                                                        "电流文件(FFT_AVG_I_T*.csv)");

    if(oStrFileName.length() != 0)
    {
        poDb->importTX(oStrFileName);

        this->LastDirWrite( oStrFileName );

        ui->actionImportRX->setEnabled(true);
    }

    ui->plotTx->setFooter("电流文件："+oStrFileName);
}

/* 打开接收端场值文件 */
void MainWindow::on_actionImportRX_triggered()
{
    QStringList aoStrRxThisTime = QFileDialog::getOpenFileNames(this,
                                                                "打开电场文件",
                                                                QString("%1").arg(this->LastDirRead()),
                                                                "电场文件(FFT_SEC_V_T*.csv)");

    if(aoStrRxThisTime.isEmpty())
    {
        QMessageBox::warning(this, "警告","未选中有效文件，\n请重新选择！");
        return;
    }

    foreach(QString oStrRxThisTime, aoStrRxThisTime)
    {
        if( aoStrExisting.contains(oStrRxThisTime) )
        {
            //qDebugV0()<<"existing~~~"<<oStrRxThisTime;
        }
        else
        {
            //            qDebugV0()<<"Not existing~~~"<<oStrRxThisTime;

            RX *poRX = new RX(oStrRxThisTime);
            aoStrExisting.append(oStrRxThisTime);
            gapoRX.append(poRX);
        }
    }

    this->LastDirWrite( aoStrRxThisTime.last() );

    this->drawCurve();

    QFileInfo oFileInfo(aoStrRxThisTime.last());

    ui->plotRx->setTitle( "场值文件目录：" + oFileInfo.absolutePath() );

    /* 接收端数据来了,都做了电流归一化(画曲线的时候归一化)了,就不能再添加发射端的电流数据了 */
    if(ui->actionImportTX->isEnabled())
    {
        ui->actionImportTX->setEnabled(false);
    }

    /* 添加进了新的Rx数据,就可以开放清理数据了. */
    if(!ui->actionClear->isEnabled())
    {
        ui->actionClear->setEnabled(true);
    }

    ui->actionCalRho->setEnabled(true);
}

/* 导出RX平均值供马工使用 */
void MainWindow::on_actionExportRX_triggered()
{
    /* Import RX */
    poDb->importRX(gapoRX);

    QString oStrFileName = QFileDialog::getSaveFileName(this,
                                                        tr("保存当前接收数据"),
                                                        "",
                                                        tr("平均场值文件(*.csv)"));

    QSqlTableModel *poModel = new QSqlTableModel(poDb);

    poModel->setTable("rx");

    poModel->select();

    /* 在操作结果前先通过fetchmore()来获取所有的结果  more then 256 */
    while(poModel->canFetchMore())
    {
        poModel->fetchMore();
    }

    QStringList oStrList;//记录数据库中的一行报警数据
    oStrList.clear();

    QString oStrString;

    QFile oFile(oStrFileName);

    if (oFile.open(QIODevice::WriteOnly|QIODevice::Text|QIODevice::Truncate))
    {
        QString oStrColumnHead = QString("%1,%2,%3,%4,%5,%6,%7,%8,%9\n")
                .arg("线号")
                .arg("点号")
                .arg("设备号")
                .arg("通道号")
                .arg("分量标识")
                .arg("频率")
                .arg("电流")
                .arg("平均场值")
                .arg("相对均方误差");

        oFile.write(oStrColumnHead.toLocal8Bit());

        for (int i = 0; i < poModel->rowCount(); i++)
        {
            for(int j = 0; j < poModel->columnCount(); j++)
            {
                qDebugV0()<<poModel->rowCount()<<poModel->columnCount()<<i<<j;

                oStrList.insert(j,poModel->data(poModel->index(i,j)).toString());//把每一行的每一列数据读取到strList中
            }

            //给两个列数据之前加","号，一行数据末尾加回车
            oStrString = oStrList.join(",")+"\n";

            //记录一行数据后清空，再记下一行数据
            oStrList.clear();

            //使用方法：转换为Utf8格式后在windows下的excel打开是乱码,
            //可先用notepad++打开并转码为unicode，再次用excel打开即可。
            oFile.write(oStrString.toUtf8());
        }

        oFile.flush();
        oFile.close();
    }
}

/*  Close Application */
void MainWindow::on_actionClose_triggered()
{

    QMessageBox oMsgBoxClose(QMessageBox::Question, "退出", "确定退出？",
                             QMessageBox::Yes | QMessageBox::No, NULL);

    /* 1，先问退不退出 */
    if(oMsgBoxClose.exec() == QMessageBox::Yes)
    {
        /* 2，如果电场值有修改&&还没保存，此时可以询问，是否保存手动调整后的电场值 */
        if( bModifyField )
        {
            /* 判断有必要询问保存不保存中间结果,看store按钮是否可用. 都不可用了,还保存啥~?*/

            QMessageBox oMsgBoxStore(QMessageBox::Question, "保存？", "是否保存修改后的\n电位数据？",
                                     QMessageBox::Yes | QMessageBox::No, NULL);
            if(oMsgBoxStore.exec() == QMessageBox::Yes)
            {
                this->store();
            }
        }

        /* 3，如果广域视电阻率曲线有修改&&还没保存，此时可以询问，是否保存手动调整后的广域视电阻率 */
        if( bModifyRho )
        {
            QMessageBox oMsgBoxClose(QMessageBox::Question, "保存？", "是否保存圆滑后的\n电阻率数据？",
                                     QMessageBox::Yes | QMessageBox::No, NULL);

            if(oMsgBoxClose.exec() == QMessageBox::Yes)
            {
                /* 保存Rho曲线上的数据至数据库，以便导出数据库数据到csv文档。再调整广域视电阻率时，何时去点这个保存按钮？*/
                QMap<QwtPlotCurve*, STATION>::const_iterator it;
                for(it = gmapCurveStation.constBegin(); it!= gmapCurveStation.constEnd(); it++)
                {
                    QPolygonF aoPointF;
                    aoPointF.clear();

                    for(uint i = 0; i < it.key()->dataSize(); i++)
                    {
                        aoPointF.append(it.key()->sample(i));
                    }

                    poDb->modifyRho(it.value(), aoPointF);
                }
            }
        }

        /* 你选保存还是选不保存，自己定。最后还是要关闭程序 */
        this->close();
    }
}
/* 清空TX　RX */
void MainWindow::on_actionClear_triggered()
{
    QMessageBox oMsgBox(QMessageBox::Question, "清除", "确定清空数据?",
                        QMessageBox::Yes | QMessageBox::No, NULL);

    if(oMsgBox.exec() == QMessageBox::Yes)
    {
        /* 数据都清了,要重新来导入数据了. */
        if(!ui->actionImportTX->isEnabled())
        {
            ui->actionImportTX->setEnabled(true);
        }

        if( bModifyField )
        {
            QMessageBox oMsgBoxStore(QMessageBox::Question, "保存？", "是否保存修改后的\n电位数据？",
                                     QMessageBox::Yes | QMessageBox::No, NULL);
            if(oMsgBoxStore.exec() == QMessageBox::Yes)
            {
                this->store();
            }
        }

        ui->stackedWidget->setCurrentIndex(0);

        QVector<double> xData;
        xData.clear();
        QVector<double> yData;
        yData.clear();

        gpoScatter->setSamples(xData, yData);

        /* 散点图，褫干净 */
        ui->plotScatter->replot();

        /* 电压除以电流的图，褫干净 */
        foreach (QwtPlotCurve *poCurve, gmapCurveData.keys())
        {
            if(poCurve != NULL)
            {
                poCurve->detach();
                delete poCurve;
                poCurve == NULL;
            }
        }
        gmapCurveData.clear();

        foreach(RX *oRx, gapoRX)
        {
            if(oRx != NULL)
            {
                delete oRx;
                oRx = NULL;
            }
        }

        gapoRX.clear();

        if(gpoErrorCurve !=NULL)
        {
            gpoErrorCurve->detach();

            delete gpoErrorCurve;
            gpoErrorCurve = NULL;
        }

        /* 右手边，tableWidget 褫干净 */
        foreach (QTreeWidgetItem *poItem, gmapCurveItem.values())
        {
            if(poItem != NULL)
            {
                disconnect(ui->treeWidgetLegend, SIGNAL(itemChanged(QTreeWidgetItem*,int)), this, SLOT(shiftCurveSelect(QTreeWidgetItem*,int)));
                delete poItem;
                poItem == NULL;
            }
        }
        gmapCurveItem.clear();

        ui->treeWidgetLegend->clear();

        ui->plotCurve->setTitle("");

        aoStrExisting.clear();

        /*  */
        ui->actionClear->setEnabled(false);

        /* 发射端和接收端数据都清除了,关闭 剪裁 功能*/
        ui->actionCutterH->setEnabled(false);
        ui->actionCutterV->setEnabled(false);

        ui->plotCurve->setFooter("");

        gpoSelectedCurve = NULL;
        giSelectedIndex = -1;
        gpoErrorCurve = NULL;

        gpoSelectedRX = NULL;

        ui->plotCurve->detachItems();

        /* 返回去设置指针为空就完事了 */
        poPickerCurve->setNull();

        ui->plotCurve->replot();

        qDebugV0()<<"clear~~~";
    }
}

/* 显示提示信息，QMessage自动定时关闭。 */
void MainWindow::showMsg(QString oStrMsg)
{
    QMessageBox *poMsgBox = new QMessageBox(QMessageBox::Information,tr("提示(3秒自动关闭)"),oStrMsg);
    QTimer::singleShot(3000, poMsgBox, SLOT(accept())); //也可将accept改为close
    poMsgBox->exec();//box->show();都可以
}

void MainWindow::recoveryCurve(QwtPlotCurve *poCurve)
{
    QPolygonF aoPointF;
    aoPointF.clear();

    aoPointF = this->getR(gpoSelectedRX->mapAvg);

    poCurve->setSamples(aoPointF);

    ui->plotCurve->replot();
}

/* 针对电场信号做了手动调整后，保存中间结果，存到csv文件中 */
void MainWindow::store()
{
    foreach(RX *poRx, gapoRX)
    {
        QString oStrFileName = poRx->oStrCSV;
        oStrFileName.chop(4);

        oStrFileName.append("_filtered.csv");

        //        qDebugV0()<<oStrFileName;

        QFile file(oStrFileName);

        if(!file.open( QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text ) )
        {
            return;
        }

        QTextStream out(&file);

        foreach (double dF, poRx->mapScatterList.keys())
        {
            out<<dF<<",";

            foreach(double dScatter,  poRx->mapScatterList.value(dF))
            {
                out<<dScatter<<",";
            }

            out<<"\n";
        }
        file.close();
    }

    /* 中间结果保存完了之后,将store键置为Disable */
    ui->actionStore->setEnabled(false);

    /* 已经保存了，修改提示标识就应该置false，关闭程序和清除数据时就不必提示 */
    bModifyField = false;
}

/**********************************************************************
 * Draw Average curve
 *
 */
void MainWindow::drawCurve()
{
    foreach (QwtPlotCurve *poCurve, gmapCurveData.keys())
    {
        delete poCurve;
        poCurve == NULL;
    }

    gmapCurveData.clear();

    foreach (QTreeWidgetItem *poItem, gmapCurveItem.values())
    {
        if(poItem != NULL)
        {
            delete poItem;
            poItem == NULL;
        }
    }

    gmapCurveItem.clear();

    ui->plotCurve->detachItems(QwtPlotItem::Rtti_PlotCurve, true);

    ui->plotCurve->replot();

    disconnect(ui->treeWidgetLegend, SIGNAL(itemChanged(QTreeWidgetItem*,int)), this, SLOT(shiftCurveSelect(QTreeWidgetItem*,int)));
    ui->treeWidgetLegend->clear();

    QSet<double> setF;

    foreach(RX *poRX, gapoRX)
    {
        foreach(double dF, poRX->adF)
        {
            setF.insert(dF);
        }
    }

    QList<double> adF = setF.toList();

    qSort(adF);

    //qDebugV0()<<"F sequence:"<<adF;

    /* Fill ticks */
    QList<double> adTicks[QwtScaleDiv::NTickTypes];
    adTicks[QwtScaleDiv::MajorTick] = adF;
    QwtScaleDiv oScaleDiv( adTicks[QwtScaleDiv::MajorTick].last(),
            adTicks[QwtScaleDiv::MajorTick].first(),
            adTicks );

    ui->plotCurve->setAxisScaleDiv( QwtPlot::xBottom, oScaleDiv );

    foreach(RX* poRX, gapoRX)
    {
        /* Curve title, cut MCSD_ & suffix*/
        const QwtText oTxtTitle( QString(tr("L%1_S%2_D%3_CH%4_%5"))
                                 .arg(poRX->goStrLineId)
                                 .arg(poRX->goStrSiteId)
                                 .arg(poRX->giDevId)
                                 .arg(poRX->giDevCh)
                                 .arg(poRX->goStrCompTag) );

        /* Create a curve pointer */
        QwtPlotCurve *poCurve = NULL;
        poCurve = new QwtPlotCurve( oTxtTitle );

        poCurve->setPen( Qt::red, 2, Qt::SolidLine );
        poCurve->setRenderHint( QwtPlotItem::RenderAntialiased, true );

        QwtSymbol *poSymbol = new QwtSymbol( QwtSymbol::Ellipse,
                                             QBrush( Qt::yellow ),
                                             QPen( Qt::blue, 2 ),
                                             QSize( 8, 8 ) );
        poCurve->setSymbol( poSymbol );
        poCurve->setStyle(QwtPlotCurve::Lines);

        QPolygonF aoPointF;
        aoPointF.clear();
        aoPointF = this->getR( poRX->mapAvg);

        poCurve->setSamples( aoPointF );
        poCurve->setAxes(QwtPlot::xBottom, QwtPlot::yLeft);
        poCurve->attach(ui->plotCurve);
        poCurve->setVisible( true );

        gmapCurveData.insert(poCurve, poRX);

        QTreeWidgetItem *poItem = NULL;
        poItem = new QTreeWidgetItem(ui->treeWidgetLegend, QStringList({poCurve->title().text(),"",""}));

        poItem->setData(1, Qt::CheckStateRole, Qt::Checked);
        poItem->setData(2, Qt::CheckStateRole, Qt::Unchecked);

        gmapCurveItem.insert(poCurve, poItem);
    }

    connect(ui->treeWidgetLegend, SIGNAL(itemChanged(QTreeWidgetItem*,int)), this, SLOT(shiftCurveSelect(QTreeWidgetItem*,int)));


    gpoSelectedCurve = NULL;
    giSelectedIndex = -1;

    gpoSelectedRX = NULL;

    gpoErrorCurve = NULL;

    ui->plotCurve->replot();
}

/**************************************************************
 * Real time set scatter canvas scales & Set aside blank.
 *
 */
void MainWindow::resizeScaleScatter()
{
    /* Real time set  plot Scale and set aside blank. */
    double dXSacle = gpoScatter->maxXValue() - gpoScatter->minXValue();
    double dYSacle = gpoScatter->maxYValue() - gpoScatter->minYValue();

    ui->plotScatter->setAxisScale( QwtPlot::xBottom, gpoScatter->minXValue() - dXSacle*0.03, gpoScatter->maxXValue() + dXSacle*0.03 );
    ui->plotScatter->setAxisScale( QwtPlot::yLeft,   gpoScatter->minYValue() - dYSacle*0.1,  gpoScatter->maxYValue() + dYSacle*0.1 );

    if( sMkList.poTop != NULL && sMkList.poBottom != NULL )
    {
        this->on_actionCutterH_triggered();
    }
    else if( sMkList.poLeft != NULL && sMkList.poRight != NULL )
    {
        this->on_actionCutterV_triggered();
    }
}

/* 电压除以电流，得到电阻值。实际上就是用电流来归一化电压 */
QPolygonF MainWindow::getR(QMap<double, double> oMap)
{
    QPolygonF aoPointF;
    aoPointF.clear();

    QMap<double, double>::const_iterator it;

    for(it = oMap.constBegin(); it != oMap.constEnd(); ++it)
    {
        double dI = poDb->getI(it.key());

        QPointF oPointF;

        oPointF.setX(it.key());
        oPointF.setY(it.value()/dI);

        aoPointF.append(oPointF);
    }

    return aoPointF;
}

/******************************************************************************
 * Get all the points in the scatter diagram
 *
 */
QPolygonF MainWindow::currentScatterPoints()
{
    QPolygonF aoPointF;
    aoPointF.clear();

    qint32 iIndex = 0;

    /* Horizontal Cut */
    if( sMkList.poBottom != NULL && sMkList.poTop != NULL )
    {
        for( qint32 i = 0; i < (qint32)gpoScatter->dataSize(); i++ )
        {
            if( gpoScatter->data()->sample(i).y() >= sMkList.poBottom->yValue() &&  gpoScatter->data()->sample(i).y() <= sMkList.poTop->yValue() )
            {
                QPointF ptTmpPoint( iIndex, gpoScatter->data()->sample(i).y() );
                // QPointF ptTmpPoint( gpoScatter->data()->sample(i) );

                aoPointF.append( ptTmpPoint );

                iIndex++;
            }
        }
    }
    /* Vertical Cut */
    else if( sMkList.poLeft != NULL && sMkList.poRight != NULL )
    {
        for( qint32 i = 0; i < (qint32)gpoScatter->dataSize(); i++ )
        {
            if( gpoScatter->data()->sample(i).x() >= sMkList.poLeft->xValue() &&  gpoScatter->data()->sample(i).x() <= sMkList.poRight->xValue() )
            {
                QPointF ptTmpPoint( iIndex, gpoScatter->data()->sample(i).y() );
                //QPointF ptTmpPoint(  gpoScatter->data()->sample(i) );

                aoPointF.append( ptTmpPoint );

                iIndex++;
            }
        }
    }

    return aoPointF;
}

QPolygonF MainWindow::currentCurvePoints()
{
    QPolygonF aoPointF;
    aoPointF.clear();

    if(gpoSelectedCurve != NULL)
    {
        for(uint i = 0; i < gpoSelectedCurve->dataSize(); i++)
        {
            aoPointF.append(gpoSelectedCurve->sample(i));
        }
    }

    return aoPointF;
}

/**************************************************
 * Init Curve plot
 *
 */
void MainWindow::initPlotCurve()
{
    QFont oFont("Times New Roman", 12, QFont::Thin);

    ui->plotCurve->setFont(oFont);

    /* Nice blue */
    //    ui->plotCurve->setCanvasBackground(QColor(55, 100, 141));

    /* Set Log Scale */
    ui->plotCurve->setAxisScaleEngine( QwtPlot::xBottom, new QwtLogScaleEngine() );
    ui->plotCurve->setAxisScaleEngine( QwtPlot::yLeft,   new QwtLogScaleEngine() );
    ui->plotCurve->setAxisScaleEngine( QwtPlot::yRight,  new QwtLinearScaleEngine() );

    ui->plotCurve->enableAxis(QwtPlot::xBottom , true);
    ui->plotCurve->enableAxis(QwtPlot::yLeft   , true);
    ui->plotCurve->enableAxis(QwtPlot::yRight  , true);

    QwtScaleDraw *poScaleDraw  = new QwtScaleDraw();
    poScaleDraw->setLabelRotation( -26 );
    poScaleDraw->setLabelAlignment( Qt::AlignLeft | Qt::AlignVCenter );

    ui->plotCurve->setAxisScaleDraw(QwtPlot::xBottom, poScaleDraw);

    /* Set Axis title */
    //QwtText oTxtXAxisTitle( "频率(Hz)" );
    //QwtText oTxtYAxisTitle( "F/I" );
    QwtText oTxtErrAxisTitle(tr("RMS(%)"));
    //oTxtXAxisTitle.setFont( oFont );
    //oTxtYAxisTitle.setFont( oFont );
    oTxtErrAxisTitle.setFont( oFont );
    //ui->plotCurve->setAxisTitle(QwtPlot::xBottom, oTxtXAxisTitle);
    //ui->plotCurve->setAxisTitle(QwtPlot::yLeft,   oTxtYAxisTitle);
    ui->plotCurve->setAxisTitle(QwtPlot::yRight,  oTxtErrAxisTitle);

    /* Draw the canvas grid */
    QwtPlotGrid *poGrid = new QwtPlotGrid();
    //poGrid->enableX( true );
    poGrid->enableY( true );
    poGrid->setMajorPen( Qt::gray, 0.5, Qt::DotLine );
    poGrid->attach( ui->plotCurve );

    ui->plotCurve->setAutoDelete ( true );

    /* Remove the gap between the data axes */
    for ( int i = 0; i < ui->plotCurve->axisCnt; i++ )
    {
        QwtScaleWidget *poScaleWidget = ui->plotCurve->axisWidget( i);
        if (poScaleWidget)
        {
            poScaleWidget->setMargin( 0 );
        }

        QwtScaleDraw *poScaleDraw = ui->plotCurve->axisScaleDraw( i );
        if ( poScaleDraw )
        {
            poScaleDraw->enableComponent( QwtAbstractScaleDraw::Backbone, false );
        }
    }

    ui->plotCurve->plotLayout()->setAlignCanvasToScales( true );

    ui->plotCurve->setAutoReplot(true);

    ui->treeWidgetLegend->clear();
    ui->treeWidgetLegend->setHeaderLabels(QStringList()<<tr("曲线")<<tr("显示")<<tr("凸显"));

    ui->treeWidgetLegend->setColumnWidth(0, 120);
    ui->treeWidgetLegend->setColumnWidth(1, 40);
    ui->treeWidgetLegend->setColumnWidth(2, 40);
    //ui->treeWidgetLegend->header()->setSectionResizeMode(QHeaderView::ResizeToContents);

    connect(ui->treeWidgetLegend->header(), SIGNAL(sectionDoubleClicked(int)), this, SLOT(shiftAllCurve(int)));

    /* 放大镜 */
    QwtPlotMagnifier *poMagnifier = new QwtPlotMagnifier(ui->plotCurve->canvas());
    poMagnifier->setAxisEnabled(QwtPlot::xBottom, false);
    poMagnifier->setAxisEnabled(QwtPlot::yLeft, true);
    poMagnifier->setAxisEnabled(QwtPlot::yRight, true);

    /* 平移 */
    QwtPlotPanner *poPanner = new QwtPlotPanner(ui->plotCurve->canvas());
    poPanner->setMouseButton(Qt::LeftButton);

    ui->plotCurve->setAxisScale(QwtPlot::xBottom, 1/64, 8192);
    ui->plotCurve->setAxisScale(QwtPlot::yRight, 0, 100);

    ui->plotCurve->setAxisAutoScale(QwtPlot::yLeft, true);

    ui->plotCurve->replot();
}

/********************************************************************************
 * Init scatter plot
 *
 */
void MainWindow::initPlotScatter()
{
    ui->plotScatter->setFrameStyle(QFrame::NoFrame);
    ui->plotScatter->enableAxis(QwtPlot::xBottom, true);
    ui->plotScatter->enableAxis(QwtPlot::yLeft,   true);

    ui->plotScatter->enableAxis(QwtPlot::xTop,   false);
    ui->plotScatter->enableAxis(QwtPlot::yRight, false);

    ui->plotScatter->setAutoDelete ( true );

    /* Clean up the Copy items, Before creating new scatter, Even without. */
    if( gpoScatter != NULL )
    {
        delete gpoScatter;
        gpoScatter = NULL;
    }
    /* New a scatter */
    gpoScatter = new QwtPlotCurve;

    if( gpoScatter == NULL )
    {
        return;
    }

    /* New marker picker pointer */
    if(poMarkerPicker != NULL)
    {
        delete poMarkerPicker;
        poMarkerPicker = NULL;
    }

    for ( int i = 0; i < ui->plotScatter->axisCnt; i++ )
    {
        QwtScaleWidget *poScaleWidget = ui->plotScatter->axisWidget( i);
        if (poScaleWidget)
        {
            poScaleWidget->setMargin( 0 );
        }

        QwtScaleDraw *poScaleDraw = ui->plotScatter->axisScaleDraw( i );
        if ( poScaleDraw )
        {
            poScaleDraw->enableComponent( QwtAbstractScaleDraw::Backbone, false );
        }
    }

    poMarkerPicker = new MarkerPicker( ui->plotScatter, gpoScatter );
    connect( poMarkerPicker, SIGNAL(SigMarkerMoved()), this , SLOT(markerMoved()));

    gpoScatter->setStyle(QwtPlotCurve::NoCurve);

    QwtSymbol *poSymbol = new QwtSymbol( QwtSymbol::Ellipse,
                                         QBrush( Qt::blue ),
                                         QPen( Qt::blue, 1.0 ),
                                         QSize( 6, 6 ) );
    gpoScatter->setSymbol(poSymbol);

    gpoScatter->attach(ui->plotScatter);

    ui->plotScatter->replot();
}

void MainWindow::shiftAllCurve(int iLogicalIndex)
{
    qDebugV0()<<iLogicalIndex;

    if( iLogicalIndex == 0 )
    {
        return;
    }

    QTreeWidgetItemIterator it(ui->treeWidgetLegend);

    int iSum = 0;

    while (*it)
    {
        iSum += (*it)->data(iLogicalIndex, Qt::CheckStateRole).toUInt();

        ++it;
    }

    Qt::CheckState eCheckState = Qt::Unchecked;

    if( iSum == 0 )
    {
        eCheckState = Qt::Checked;
    }
    else if( iSum > 0 )
    {
        eCheckState = Qt::Unchecked;
    }

    QTreeWidgetItemIterator itWrite(ui->treeWidgetLegend);

    while (*itWrite)
    {
        //qDebugV0()<<"Name:"<<(*itWrite)->text(0);
        (*itWrite)->setData(iLogicalIndex, Qt::CheckStateRole, eCheckState );

        ++itWrite;
    }
}

/* 点击tavlewidget 的item */
void MainWindow::shiftCurveSelect(QTreeWidgetItem *poItem, int iCol)
{
    if(poItem == NULL || iCol == 0)
    {
        return;
    }
    else
    {
        QwtPlotCurve *poCurve = gmapCurveItem.key(poItem);

        //        qDebugV0()<<poCurve->title().text();

        switch (iCol)
        {
        case 1://display
            if(poItem->data(iCol, Qt::CheckStateRole).toUInt() == Qt::Checked)
            {
                poCurve->setVisible(true);
            }
            else if(poItem->data(iCol, Qt::CheckStateRole).toUInt() == Qt::Unchecked)
            {
                poCurve->setVisible(false);
            }

            break;
        case 2://highlights
            if(poItem->data(iCol, Qt::CheckStateRole).toUInt() == Qt::Checked)
            {
                poCurve->setPen( Qt::red, 4, Qt::SolidLine );
            }
            else if(poItem->data(iCol, Qt::CheckStateRole).toUInt() == Qt::Unchecked)
            {
                poCurve->setPen( Qt::black, 2, Qt::SolidLine );
            }
            break;
        default:
            break;
        }

        ui->plotCurve->replot();
    }
}

/* slot 函数。 接收picker对象的signal  SigSelected(poCurve, index) */
void MainWindow::Selected(QwtPlotCurve *poCurve, int iIndex)
{
    gpoSelectedCurve = poCurve;
    giSelectedIndex = iIndex;

    gpoSelectedRX = this->gmapCurveData.value(gpoSelectedCurve);

    this->restoreCurve();

    this->drawScatter();
    this->switchHighlightCurve();
    this->drawError();

    RX *poRxSelected = gmapCurveData.value(gpoSelectedCurve);


    QString oStrFooter(QString("%1_%2Hz_%3%")
                       .arg(poCurve->title().text())
                       .arg(poCurve->data()->sample(iIndex).x())
                       .arg(QString::number(poRxSelected->mapErr.value(gpoSelectedCurve->sample(giSelectedIndex).x()),'f',2)));
    QwtText oTxt;
    oTxt.setText(oStrFooter);
    QFont oFont("Times New Roman", 12, QFont::Bold);
    oTxt.setFont(oFont);
    oTxt.setColor(Qt::blue);

    ui->plotCurve->setFooter(oTxt);
}

/*  */
void MainWindow::SelectedRho(QwtPlotCurve *poCurve, int iIndex)
{
    if(poCurve != NULL && iIndex != -1)
    {
        gpoSelectedCurve = poCurve;
        giSelectedIndex = iIndex;

        /* 启用这个按钮的作用就只是为了保存调整后的Rho结果 */
        ui->actionStore->setEnabled(true);

        QString oStrFooter(QString("%1_%2Hz")
                           .arg(poCurve->title().text())
                           .arg(poCurve->data()->sample(iIndex).x()));
        QwtText oTxt;
        oTxt.setText(oStrFooter);
        QFont oFont("Times New Roman", 12, QFont::Bold);
        oTxt.setFont(oFont);
        oTxt.setColor(Qt::blue);

        ui->plotRho->setFooter(oTxt);
    }
}

void MainWindow::SelectedRho()
{
    ui->plotRho->setFooter("请选择曲线上的点！");
}

void MainWindow::rhoMoved()
{
    bModifyRho = true;
}

/* Scatter changed, then, curve's point need be change. */
void MainWindow::markerMoved()
{
    /* Scatter Cannot be null and void, Otherwise, return! */
    if( gpoScatter == NULL )
    {
        return;
    }

    /* All points on Scatter */
    QPolygonF apoPointScatter = currentScatterPoints();

    if( apoPointScatter.count() == 0)
    {
        return;
    }

    QVector<qreal> arE;
    arE.clear();

    for(qint32 i = 0; i < apoPointScatter.count(); i++)
    {
        arE.append(apoPointScatter.at(i).y());
    }

    double dE = gpoSelectedRX->getAvg(arE);

    double dI = poDb->getI(gpoSelectedCurve->data()->sample(giSelectedIndex).x());

    /* All points on selected curve */
    QPolygonF aoPointCurve = this->currentCurvePoints();

    /* Selected point's new value(just Y) */
    QPointF ptPoint( gpoSelectedCurve->data()->sample(giSelectedIndex).x(), dE/dI );

    /* Replace current selected point Y value. */
    aoPointCurve.replace( giSelectedIndex, ptPoint );

    /* Set new samples on Curve */
    gpoSelectedCurve->setSamples( aoPointCurve );

    /* All points on selected curve */
    QPolygonF aoPointFError;

    for(uint i = 0; i < gpoErrorCurve->dataSize(); i++)
    {
        aoPointFError.append(gpoErrorCurve->sample(i));
    }

    /* Selected point's new value(just Y) */
    QPointF oPointF( gpoErrorCurve->data()->sample(giSelectedIndex).x(), gpoSelectedRX->getErr(arE) );

    /* Replace current selected point Y value. */
    aoPointFError.replace( giSelectedIndex, oPointF );

    /* Set new samples on Curve */
    gpoErrorCurve->setSamples( aoPointFError );
    gpoErrorCurve->attach( ui->plotCurve );

    /* Update MSRE */
    QString oStrFooter(QString("%1_%2Hz_%3%")
                       .arg(gpoSelectedCurve->title().text())
                       .arg(gpoSelectedCurve->data()->sample(giSelectedIndex).x())
                       .arg(QString::number(gpoSelectedRX->getErr(arE),'f',2)));


    ui->plotCurve->setFooter(oStrFooter);

    /* 手动摘除散点图的点,需要开放save按键,以便用户用于保存散点图更新信息致Rx类中 */
    ui->actionSave->setEnabled(true);

    ui->plotCurve->replot();
}

/************************************************************************
 * Draw scatter points and Marker line
 * 两个地方调用此函数,1是选中点了 要画散点图,2是按了恢复至最初,这时要重新绘制散点图
 */
void MainWindow::drawScatter()
{
    /* Detach & Delete marker line pointer, plotCurve's curves,set NULL textLabel. */
    if( gpoSelectedCurve == NULL || giSelectedIndex == -1  )
    {
        clearMarker();
        return;
    }

    /* Selected some curve, restore that curve. */
    const QwtPlotItemList& itmList = ui->plotScatter->itemList();

    for( QwtPlotItemIterator it = itmList.begin(); it != itmList.end(); ++it )
    {
        if ( ( *it )->rtti() == QwtPlotItem::Rtti_PlotTextLabel )
        {
            QwtPlotTextLabel *poTextLabel = static_cast<QwtPlotTextLabel *>( *it );

            poTextLabel->detach();
            if(poTextLabel != NULL)
            {
                delete poTextLabel;
                poTextLabel = NULL;
            }
        }
    }

    if( !ui->plotScatter->autoDelete() )
    {
        qDebugV5()<<"Auto delete all items not work!";
    }

    gpoScatter->setTitle(QString("%1Hz(%2)")
                         .arg(gpoSelectedCurve->sample(giSelectedIndex).x())
                         .arg(gpoSelectedCurve->title().text()));


    RX *poRxSelected = gmapCurveData.value(gpoSelectedCurve);

    QVector<double> adScatter = poRxSelected->mapScatterList.value(gpoSelectedCurve->sample(giSelectedIndex).x());

    QVector<double> adX;
    adX.clear();

    for(int i = 0; i < adScatter.count(); i++)
    {
        adX.append(i);
    }

    gpoScatter->setSamples( adX, adScatter );

    QwtSymbol *poSymbol = new QwtSymbol( QwtSymbol::Ellipse,
                                         QBrush( Qt::blue ),
                                         QPen( Qt::blue, 1.0 ),
                                         QSize( 6, 6 ) );
    gpoScatter->setSymbol(poSymbol);


    /* Real time set plot canvas Scale & Set aside blank. */
    this->resizeScaleScatter();

    /* Read Excel file(copy), display MSRE on PlotCurve canvas(Top||Right) */

    //QString::number(gpoSelectedRX->getErr(arE),'f',2)
    /* Update MSRE */
    QString oStrFooter(QString("%1_%2Hz_%3%")
                       .arg(gpoSelectedCurve->title().text())
                       .arg(gpoSelectedCurve->data()->sample(giSelectedIndex).x())
                       .arg(QString::number(poRxSelected->mapErr.value(gpoSelectedCurve->sample(giSelectedIndex).x()),'f',2)));


    ui->plotCurve->setFooter(oStrFooter);

    ui->plotScatter->replot();

    /* 都已经挑数据了,还添加什么文件,先干啥去了~? */
    ui->actionImportRX->setEnabled(false);

    /*  */
    ui->actionRecovery->setEnabled(true);

    /* 发射端和接收端数据都来了,且双击选中了点,显示了 散点图,可以剪裁 */
    ui->actionCutterH->setEnabled(true);
    ui->actionCutterV->setEnabled(true);
}


/* 绘制RMS曲线 */
void MainWindow::drawError()
{
    if(gpoSelectedCurve ==NULL)
    {
        return;
    }

    if(gpoErrorCurve != NULL)
    {
        gpoErrorCurve->detach();

        delete gpoErrorCurve;

        gpoErrorCurve = NULL;
    }

    /* 初始化RMS曲线 */
    gpoErrorCurve = new QwtPlotCurve(tr("相对均方误差"));
    if( gpoErrorCurve == NULL )
    {
        return;
    }

    gpoErrorCurve->setLegendAttribute( QwtPlotCurve::LegendNoAttribute, true );

    QwtSymbol *PoSymbol = new QwtSymbol( QwtSymbol::Diamond,
                                         QBrush( Qt::red ),
                                         QPen( Qt::gray, 1 ),
                                         QSize( 4, 4 ) );

    gpoErrorCurve->setSymbol( PoSymbol );

    gpoErrorCurve->setAxes( QwtPlot::xBottom, QwtPlot::yRight );
    gpoErrorCurve->setPen( Qt::black, 0.5, Qt::DotLine );
    gpoErrorCurve->setStyle( QwtPlotCurve::Sticks );

    RX *poRX = gmapCurveData.value(gpoSelectedCurve);

    QPolygonF aoPointF;
    aoPointF.clear();
    QMap<double, double>::const_iterator it;
    for(it = poRX->mapErr.constBegin(); it!= poRX->mapErr.constEnd(); ++it)
    {
        aoPointF.append(QPointF(it.key(), it.value()));
    }

    gpoErrorCurve->setSamples( aoPointF );
    gpoErrorCurve->attach( ui->plotCurve );

    ui->plotCurve->setAxisScale(QwtPlot::yRight, 0, 1.1*gpoErrorCurve->maxYValue());

    ui->plotCurve->replot();
}

void MainWindow::switchHighlightCurve()
{
    foreach(QwtPlotCurve * poC, gmapCurveData.keys())
    {
        poC->setPen( Qt::black, 2, Qt::SolidLine );
    }
    if( gpoSelectedCurve != NULL )
    {
        gpoSelectedCurve->setPen( Qt::red, 4, Qt::SolidLine );

        // poDb->refineError(gmapCurveData.value(poCurve));
    }
}
/* 绘制发射端电流曲线 */
void MainWindow::drawTx(const QVector<double> adF, const QVector<double> adI)
{
    ui->plotTx->detachItems();
    ui->plotTx->replot();

    QSet<double> setF;

    for(int i = 0; i < adF.count(); i++)
    {
        setF.insert(adF.at(i));
    }

    QList<double> listF = setF.toList();

    qSort(listF);

    /* Fill ticks */
    QList<double> adTicks[QwtScaleDiv::NTickTypes];
    adTicks[QwtScaleDiv::MajorTick] = listF;
    QwtScaleDiv oScaleDiv( adTicks[QwtScaleDiv::MajorTick].last(),
            adTicks[QwtScaleDiv::MajorTick].first(),
            adTicks );

    ui->plotTx->setAxisScaleDiv( QwtPlot::xBottom, oScaleDiv );
    /* Create a curve pointer */
    QwtPlotCurve *poCurve = new QwtPlotCurve();

    poCurve->setPen( Qt::red, 2, Qt::SolidLine );
    poCurve->setRenderHint( QwtPlotItem::RenderAntialiased, true );

    QwtSymbol *poSymbol = new QwtSymbol( QwtSymbol::Ellipse,
                                         QBrush( Qt::yellow ),
                                         QPen( Qt::blue, 2 ),
                                         QSize( 8, 8 ) );
    poCurve->setSymbol( poSymbol );
    poCurve->setStyle(QwtPlotCurve::Lines);

    poCurve->setSamples( adF, adI );
    poCurve->setAxes(QwtPlot::xBottom, QwtPlot::yLeft);
    poCurve->attach(ui->plotTx);
    poCurve->setVisible( true );

    ui->plotTx->replot();
}

void MainWindow::initPlotRx()
{
    //    ui->plotRx->setCanvasBackground(QColor(29, 100, 141)); // nice blue

    //ui->plotCurve->setTitle(QwtText("Curve Plot"));
    QFont oFont("Times New Roman", 12, QFont::Thin);

    QwtText oTxtTitle( "接收端_场值曲线" );
    oTxtTitle.setFont( oFont );

    ui->plotRx->setTitle(oTxtTitle);

    ui->plotRx->setFont(oFont);

    /* Nice blue */
    //ui->plotCurve->setCanvasBackground(QColor(55, 100, 141));

    /* Set Log Scale */
    ui->plotRx->setAxisScaleEngine( QwtPlot::xBottom, new QwtLogScaleEngine() );
    ui->plotRx->setAxisScaleEngine( QwtPlot::yLeft,   new QwtLogScaleEngine() );

    ui->plotRx->enableAxis(QwtPlot::xBottom , true);
    ui->plotRx->enableAxis(QwtPlot::yLeft   , true);

    QwtScaleDraw *poScaleDraw  = new QwtScaleDraw();
    poScaleDraw->setLabelRotation( -26 );
    poScaleDraw->setLabelAlignment( Qt::AlignLeft | Qt::AlignVCenter );

    ui->plotRx->setAxisScaleDraw(QwtPlot::xBottom, poScaleDraw);

    /* Set Axis title */
    QwtText oTxtXAxisTitle( "F/Hz" );
    QwtText oTxtYAxisTitle( "Field" );
    oTxtXAxisTitle.setFont( oFont );
    oTxtYAxisTitle.setFont( oFont );

    ui->plotRx->setAxisTitle(QwtPlot::xBottom,  oTxtXAxisTitle);
    ui->plotRx->setAxisTitle(QwtPlot::yLeft,    oTxtYAxisTitle);

    /* Draw the canvas grid */
    QwtPlotGrid *poGrid = new QwtPlotGrid();
    poGrid->enableX( false );
    poGrid->enableY( true );
    poGrid->setMajorPen( Qt::gray, 0.5, Qt::DotLine );
    poGrid->attach( ui->plotRx );

    ui->plotRx->setAutoDelete ( true );

    /* Remove the gap between the data axes */
    for ( int i = 0; i < ui->plotRx->axisCnt; i++ )
    {
        QwtScaleWidget *poScaleWidget = ui->plotRx->axisWidget( i);
        if (poScaleWidget)
        {
            poScaleWidget->setMargin( 0 );
        }

        QwtScaleDraw *poScaleDraw = ui->plotRx->axisScaleDraw( i );
        if ( poScaleDraw )
        {
            poScaleDraw->enableComponent( QwtAbstractScaleDraw::Backbone, false );
        }
    }

    ui->plotRx->plotLayout()->setAlignCanvasToScales( true );

    ui->plotRx->setAutoReplot(true);
}

void MainWindow::drawRx()
{
    ui->plotRx->detachItems();
    ui->plotRx->replot();

    QSet<double> setF;

    foreach(RX *poRX, gapoRX)
    {
        for(int i = 0; i < poRX->adF.count(); i++)
        {
            setF.insert(poRX->adF.at(i));
        }
    }

    QList<double> adF = setF.toList();

    qSort(adF);

    qDebugV0()<<adF;

    /* Fill ticks */
    QList<double> adTicks[QwtScaleDiv::NTickTypes];
    adTicks[QwtScaleDiv::MajorTick] = adF;
    QwtScaleDiv oScaleDiv( adTicks[QwtScaleDiv::MajorTick].last(),
            adTicks[QwtScaleDiv::MajorTick].first(),
            adTicks );

    ui->plotRx->setAxisScaleDiv( QwtPlot::xBottom, oScaleDiv );

    foreach (RX* poRX, gapoRX)
    {
        /* Curve title, cut MCSD_ & suffix*/
        /* Curve title, cut MCSD_ & suffix*/
        const QwtText oTxtTitle( QString(tr("L%1_S%2_D%3_CH%4_%5"))
                                 .arg(poRX->goStrLineId)
                                 .arg(poRX->goStrSiteId)
                                 .arg(poRX->giDevId)
                                 .arg(poRX->giDevCh)
                                 .arg(poRX->goStrCompTag) );

        /* Create a curve pointer */
        QwtPlotCurve *poCurve = new QwtPlotCurve( oTxtTitle );

        poCurve->setPen( Qt::red, 2, Qt::SolidLine );
        poCurve->setRenderHint( QwtPlotItem::RenderAntialiased, true );

        QwtSymbol *poSymbol = new QwtSymbol( QwtSymbol::Ellipse,
                                             QBrush( Qt::yellow ),
                                             QPen( Qt::blue, 2 ),
                                             QSize( 8, 8 ) );
        poCurve->setSymbol( poSymbol );
        poCurve->setStyle(QwtPlotCurve::Lines);

        QPolygonF aoPointF;
        aoPointF.clear();
        aoPointF = this->getR( poRX->mapAvg);

        poCurve->setSamples( aoPointF );
        poCurve->setAxes(QwtPlot::xBottom, QwtPlot::yLeft);
        poCurve->attach(ui->plotRx);
        poCurve->setVisible( true );
    }

    QwtPlotMagnifier *poM = new QwtPlotMagnifier(ui->plotRx->canvas());
    poM->setAxisEnabled(QwtPlot::xBottom, false);
    poM->setAxisEnabled(QwtPlot::yLeft, true);
    poM->setAxisEnabled(QwtPlot::yRight, true);

    QwtPlotPanner *poP = new QwtPlotPanner(ui->plotRx->canvas());
    poP->setMouseButton(Qt::LeftButton);

    ui->plotRx->replot();
}

void MainWindow::initPlotRho()
{
    poPickerRho = new CanvasPickerRho( ui->plotRho );

    connect(poPickerRho, SIGNAL(SigSelectedRho(QwtPlotCurve*,int)), this, SLOT(SelectedRho(QwtPlotCurve*,int)));
    connect(poPickerRho, SIGNAL(SigSelectedRho()), this, SLOT(SelectedRho()));
    connect(poPickerRho, SIGNAL(SigMoved()), this, SLOT(rhoMoved()));
    //    ui->plotRho->setCanvasBackground(QColor(29, 100, 141)); // nice blue

    //ui->plotCurve->setTitle(QwtText("Curve Plot"));
    QFont oFont("Times New Roman", 12, QFont::Thin);

    QwtText oTxtTitle( "广域视电阻率曲线" );
    oTxtTitle.setFont( oFont );

    ui->plotRho->setTitle(oTxtTitle);

    ui->plotRho->setFont(oFont);

    /* Nice blue */
    //ui->plotCurve->setCanvasBackground(QColor(55, 100, 141));

    /* Set Log Scale */
    ui->plotRho->setAxisScaleEngine( QwtPlot::xBottom, new QwtLogScaleEngine() );
    ui->plotRho->setAxisScaleEngine( QwtPlot::yLeft,   new QwtLogScaleEngine() );

    ui->plotRho->enableAxis(QwtPlot::xBottom , true);
    ui->plotRho->enableAxis(QwtPlot::yLeft   , true);

    QwtScaleDraw *poScaleDraw  = new QwtScaleDraw();
    poScaleDraw->setLabelRotation( -26 );
    poScaleDraw->setLabelAlignment( Qt::AlignLeft | Qt::AlignVCenter );

    ui->plotRho->setAxisScaleDraw(QwtPlot::xBottom, poScaleDraw);

    /* Set Axis title */
    QwtText oTxtXAxisTitle( "F/Hz" );
    QwtText oTxtYAxisTitle( "Rho/\u03A9·m" );
    oTxtXAxisTitle.setFont( oFont );
    oTxtYAxisTitle.setFont( oFont );

    ui->plotRho->setAxisTitle(QwtPlot::xBottom,  oTxtXAxisTitle);
    ui->plotRho->setAxisTitle(QwtPlot::yLeft,    oTxtYAxisTitle);

    /* Draw the canvas grid */
    QwtPlotGrid *poGrid = new QwtPlotGrid();
    poGrid->enableX( false );
    poGrid->enableY( true );
    poGrid->setMajorPen( Qt::gray, 0.5, Qt::DotLine );
    poGrid->attach( ui->plotRho );

    ui->plotRho->setAutoDelete ( true );

    /* Remove the gap between the data axes */
    for ( int i = 0; i < ui->plotRho->axisCnt; i++ )
    {
        QwtScaleWidget *poScaleWidget = ui->plotRho->axisWidget( i);
        if (poScaleWidget)
        {
            poScaleWidget->setMargin( 0 );
        }

        QwtScaleDraw *poScaleDraw = ui->plotRho->axisScaleDraw( i );
        if ( poScaleDraw )
        {
            poScaleDraw->enableComponent( QwtAbstractScaleDraw::Backbone, false );
        }
    }

    ui->plotRho->plotLayout()->setAlignCanvasToScales( true );

    //ui->plotRho->setTitle(("-------"));

    ui->plotRho->setAutoReplot(true);
}

/*"Shift + Ctrl + R",恢复选中的Rho整条曲线 */
void MainWindow::keyPressEvent(QKeyEvent *event)
{
    if (event->modifiers() == (Qt::ShiftModifier | Qt::ControlModifier) &&
            event->key() == Qt::Key_R)
    {
        qDebugV0()<<"Shift + Ctrl + R";

        if(gpoSelectedCurve != NULL)
        {
            qDebugV0()<<gpoSelectedCurve->title().text();

            STATION oStation = gmapCurveStation.value(gpoSelectedCurve);

            QPolygonF aoPointF;
            aoPointF.clear();

            aoPointF = poDb->getRho(oStation);

            //qDebugV0()<<aoPointF;

            gpoSelectedCurve->setSamples(aoPointF);

            ui->plotRho->replot();
        }
    }
}

/*****************************************************************************
 * Restore Curve when release this point or curve.
 *
 */
void MainWindow::restoreCurve()
{
    RX *poRX = gmapCurveData.value(gpoSelectedCurve);

    if( gpoSelectedCurve != NULL)
    {
        QPolygonF aoPointF;
        aoPointF.clear();
        aoPointF = this->getR( poRX->mapAvg);

        gpoSelectedCurve->setSamples( aoPointF );

        ui->plotCurve->setTitle("");

        ui->plotCurve->replot();
    }

    if( gpoErrorCurve != NULL )
    {
        QPolygonF aoPointF;
        aoPointF.clear();
        QMap<double, double>::const_iterator it;
        for(it = poRX->mapErr.constBegin(); it!= poRX->mapErr.constEnd(); ++it)
        {
            aoPointF.append(QPointF(it.key(), it.value()));
        }

        gpoErrorCurve->setSamples( aoPointF );
        gpoErrorCurve->attach( ui->plotCurve );

        ui->plotScatter->replot();
    }
}

/*****************************************************************************
 * Insert 2 Horizontal Marker line
 *
 */
void MainWindow::on_actionCutterH_triggered()
{
    /* No Scatter, Don't draw the Marker line! */
    if( gpoScatter == NULL )
    {
        qDebugV5()<<"No scatter!";
        return;
    }

    /* Real time set  plot Scale and set aside blank. */
    double dYSacle = gpoScatter->maxYValue() - gpoScatter->minYValue();

    /* Detach & Delete marker line pointer */
    clearMarker();

    /* Top Marker line */
    sMkList.poTop = new QwtPlotMarker("Top");
    sMkList.poTop->setLineStyle( QwtPlotMarker::HLine );
    sMkList.poTop->setLabelAlignment(Qt::AlignLeft | Qt::AlignTop);
    sMkList.poTop->setYAxis( QwtPlot::yLeft );
    sMkList.poTop->setYValue( gpoScatter->maxYValue() + dYSacle*0.075 );
    sMkList.poTop->setLabel(QwtText(QString("Top: Y = %1").arg(QString::number(sMkList.poTop->yValue(), 'f', 0))));
    sMkList.poTop->setLinePen(Qt::black, 1.0, Qt::SolidLine);

    sMkList.poTop->attach( ui->plotScatter );

    /* Bottom Marker line */
    sMkList.poBottom = new QwtPlotMarker("Bottom");
    sMkList.poBottom->setLineStyle( QwtPlotMarker::HLine );
    sMkList.poBottom->setLabelAlignment(Qt::AlignLeft | Qt::AlignBottom);
    sMkList.poBottom->setYAxis( QwtPlot::yLeft );
    sMkList.poBottom->setYValue( gpoScatter->minYValue() - dYSacle*0.075 );
    sMkList.poBottom->setLabel(QwtText(QString("Bottom: Y = %1").arg(QString::number(sMkList.poBottom->yValue(), 'f', 0))));
    sMkList.poBottom->setLinePen(Qt::black, 1.0, Qt::SolidLine);

    sMkList.poBottom->attach( ui->plotScatter );

    QwtPlotCanvas *poCanvas = qobject_cast<QwtPlotCanvas *>( ui->plotScatter->canvas() );

    poCanvas->setCursor( Qt::SplitVCursor );

    /* 选中了,那就不能让用户作死地点了 */
    ui->actionCutterH->setEnabled(false);
    ui->actionCutterV->setEnabled(true);

    ui->plotScatter->replot();
}

/**********************************************************
 * Insert 2 Vertical Marker line
 *
 */
void MainWindow::on_actionCutterV_triggered()
{
    /* No Scatter, Don't draw the Marker line! */
    if( gpoScatter == NULL )
    {
        qDebugV5()<<"No scatter!";
        return;
    }

    /* X Axis scale */
    double dXSacle = gpoScatter->maxXValue() - gpoScatter->minXValue();

    /* Detach & Delete marker line pointer */
    clearMarker();

    /* Left Marker line */
    sMkList.poLeft = new QwtPlotMarker("Left");
    sMkList.poLeft->setLineStyle( QwtPlotMarker::VLine );
    sMkList.poLeft->setXAxis( QwtPlot::xBottom );
    sMkList.poLeft->setLabel(sMkList.poLeft->title());
    sMkList.poLeft->setLabelOrientation(Qt::Vertical);
    sMkList.poLeft->setLabelAlignment(Qt::AlignLeft | Qt::AlignTop);
    sMkList.poLeft->setXValue( gpoScatter->minXValue() - dXSacle*0.02 );
    sMkList.poLeft->setLabel(QwtText(QString("Left: X = %1").arg(QString::number(sMkList.poLeft->xValue(), 'f', 0))));
    sMkList.poLeft->setLinePen(Qt::black, 1.0, Qt::SolidLine);

    sMkList.poLeft->attach( ui->plotScatter );

    /*  Right Marker line */
    sMkList.poRight = new QwtPlotMarker("Right");
    sMkList.poRight->setLineStyle( QwtPlotMarker::VLine );
    sMkList.poRight->setXAxis( QwtPlot::xBottom );
    sMkList.poRight->setLabel(sMkList.poRight->title());
    sMkList.poRight->setLabelOrientation(Qt::Vertical);
    sMkList.poRight->setLabelAlignment(Qt::AlignRight | Qt::AlignTop);
    sMkList.poRight->setXValue( gpoScatter->maxXValue() + 0.02 );
    sMkList.poRight->setLabel(QwtText(QString("Right: X = %1").arg(QString::number(sMkList.poRight->xValue(), 'f', 0))));
    sMkList.poRight->setLinePen(Qt::black, 1.0, Qt::SolidLine);

    sMkList.poRight->attach( ui->plotScatter );

    QwtPlotCanvas *poCanvas = qobject_cast<QwtPlotCanvas *>( ui->plotScatter->canvas() );

    poCanvas->setCursor( Qt::SplitHCursor );

    /* 选中了,那就不能让用户作死地点了 */
    ui->actionCutterV->setEnabled(false);
    ui->actionCutterH->setEnabled(true);

    ui->plotScatter->replot();
}

/***************************************************************
 * Detach & Delete marker line pointer
 *
 */
void MainWindow::clearMarker()
{
    QwtPlotCanvas *poCanvas = qobject_cast<QwtPlotCanvas *>( ui->plotScatter->canvas() );

    poCanvas->setCursor( Qt::ArrowCursor );

    /* Detach & Delete marker line pointer */
    if(sMkList.poTop != NULL)
    {
        sMkList.poTop->detach();
        delete sMkList.poTop;
        sMkList.poTop = NULL;
    }

    if(sMkList.poBottom != NULL)
    {
        sMkList.poBottom->detach();
        delete sMkList.poBottom;
        sMkList.poBottom = NULL;
    }

    if(sMkList.poLeft != NULL)
    {
        sMkList.poLeft->detach();
        delete sMkList.poLeft;
        sMkList.poLeft = NULL;
    }

    if(sMkList.poRight != NULL)
    {
        sMkList.poRight->detach();
        delete sMkList.poRight;
        sMkList.poRight = NULL;
    }
}

/*******************************************************************
 * Read last Dir log file, get last Dir(Previous directory)
 */
QString MainWindow::LastDirRead()
{
    QString oStrLastDir;
    oStrLastDir.clear();

    //qDebugV0()<<"Read last time Dir.";

    QFile oFileLastDir(LASTDIR);

    if( oFileLastDir.open(QIODevice::ReadOnly | QIODevice::Text) )
    {
        QTextStream oTextStreamIn(&oFileLastDir);
        oStrLastDir = oTextStreamIn.readLine();

        if(oStrLastDir.isNull())
        {
            /* Default Dir */
            oStrLastDir = "D:/";
        }

        //qDebugV0()<<"Last Dir:"<<oStrLastDir;
    }
    else
    {
        qDebugV0()<<"Can't open the file! Return default Dir.";

        /* Default Dir */
        oStrLastDir = "D:/";
    }

    oFileLastDir.close();

    return oStrLastDir;
}

/***********************************************************************
 * Save the current project Dir as the most recently opened directory
 */
void MainWindow::LastDirWrite(QString oStrFileName)
{
    QFileInfo oFileInfoLastDir(oStrFileName);

    //qDebugV0()<<"Current project Dir:"<<oFileInfoLastDir.absoluteDir().absolutePath();

    QDir oDir = oFileInfoLastDir.absoluteDir();

    //qDebugV0()<<oDir;

    if( !oDir.cdUp() )
    {
        //qDebugV0()<<"After switching to the first level Dir:"<<oDir.absolutePath();
        qDebugV0()<<"The previous directory of the current directory does not exist!";
        return;
    }

    QFile oFileLastDir(LASTDIR);

    if( !oFileLastDir.open( QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text ) )
    {
        qDebugV0()<<"Open last dir file false!";
        return;
    }

    QTextStream oTextStreamOut(&oFileLastDir);

    QDir oDirRslt = oFileInfoLastDir.absoluteDir();

    //qDebugV0()<<oDirRslt.absolutePath();

    oTextStreamOut<<oDirRslt.absolutePath();

    oFileLastDir.close();
}

/* 发射电流曲线 */
void MainWindow::initPlotTx()
{
    //ui->plotCurve->setTitle(QwtText("Curve Plot"));
    QFont oFont("Times New Roman", 12, QFont::Thin);

    //    ui->plotTx->setCanvasBackground(QColor(29, 100, 141)); // nice blue

    ui->plotTx->setFont(oFont);

    QwtText oTxtTitle( "发射端_电流曲线" );
    oTxtTitle.setFont( oFont );

    ui->plotTx->setTitle(oTxtTitle);

    /* Nice blue */
    //ui->plotCurve->setCanvasBackground(QColor(55, 100, 141));

    /* Set Log Scale */
    ui->plotTx->setAxisScaleEngine( QwtPlot::xBottom, new QwtLogScaleEngine() );
    ui->plotTx->setAxisScaleEngine( QwtPlot::yLeft,   new QwtLogScaleEngine() );

    ui->plotTx->enableAxis(QwtPlot::xBottom , true);
    ui->plotTx->enableAxis(QwtPlot::yLeft   , true);

    QwtScaleDraw *poScaleDraw  = new QwtScaleDraw();
    poScaleDraw->setLabelRotation( -26 );
    poScaleDraw->setLabelAlignment( Qt::AlignLeft | Qt::AlignVCenter );

    ui->plotTx->setAxisScaleDraw(QwtPlot::xBottom, poScaleDraw);

    /* Set Axis title */
    QwtText oTxtXAxisTitle( "F/Hz" );
    QwtText oTxtYAxisTitle( "I/A" );
    oTxtXAxisTitle.setFont( oFont );
    oTxtYAxisTitle.setFont( oFont );

    ui->plotTx->setAxisTitle(QwtPlot::xBottom,  oTxtXAxisTitle);
    ui->plotTx->setAxisTitle(QwtPlot::yLeft,    oTxtYAxisTitle);

    /* Draw the canvas grid */
    QwtPlotGrid *poGrid = new QwtPlotGrid();
    poGrid->enableX( false );
    poGrid->enableY( true );
    poGrid->setMajorPen( Qt::gray, 0.5, Qt::DotLine );
    poGrid->attach( ui->plotTx );

    ui->plotTx->setAutoDelete ( true );

    /* Remove the gap between the data axes */
    for ( int i = 0; i < ui->plotTx->axisCnt; i++ )
    {
        QwtScaleWidget *poScaleWidget = ui->plotTx->axisWidget( i);
        if (poScaleWidget)
        {
            poScaleWidget->setMargin( 0 );
        }

        QwtScaleDraw *poScaleDraw = ui->plotTx->axisScaleDraw( i );
        if ( poScaleDraw )
        {
            poScaleDraw->enableComponent( QwtAbstractScaleDraw::Backbone, false );
        }
    }

    ui->plotTx->plotLayout()->setAlignCanvasToScales( true );

    ui->plotTx->setAutoReplot(true);
}

/* 导出广域视电阻率计算结果到csv文档。 */
void MainWindow::on_actionExportRho_triggered()
{
    /* 不管回写还是不回写，都要导出 */
    QString oStrDefault = QDateTime::currentDateTime().toString("yyyy年MM月dd日HH时mm分ss秒_广域视电阻率结果");

    QString oStrFileName = QFileDialog::getSaveFileName(this, tr("保存广域视电阻率计算结果"),
                                                        QString("%1/%2.csv").arg(this->LastDirRead()).arg(oStrDefault),
                                                        "(*.csv *.txt *.dat)");

    QFile oFile(oStrFileName);
    bool openOk = oFile.open( QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text );

    if(!openOk)
    {
        return;
    }

    QTextStream outStream(&oFile);

    QAbstractItemModel *poModel = ui->tableViewRho->model();

    /* 列头 */
    for(int i = 0; i < poModel->columnCount(); i++)
    {
        outStream<<poModel->headerData(i, Qt::Horizontal, Qt::DisplayRole).toString()<<",";
    }
    outStream<<endl;

    for(int i = 0; i < poModel->rowCount(); i++)
    {
        for(int j = 0 ; j < poModel->columnCount(); j++)
        {
            outStream<<poModel->data(poModel->index(i,j), Qt::DisplayRole).toString()<<",";
        }
        outStream<<endl;
    }

    outStream.flush();
    oFile.close();

    this->showMsg(QString("数据导出成功\n\n%1").arg(oStrFileName));
}

/***************************************************************
 * Read data from RX, update Scatter\Error\Curve
 *
 */
void MainWindow::on_actionRecovery_triggered()
{
    switch (ui->stackedWidget->currentIndex())
    {
    case 0://场值除以电流的曲线（场值调整plot）
    {
        if( gpoSelectedCurve == NULL || giSelectedIndex == -1)
        {
            this->showMsg("未选中曲线上的点！");
            return;
        }

        /* Read scatter from Sctatter table, then update curve && error Table */
        gpoSelectedRX->renewScatter(gpoSelectedCurve->sample(giSelectedIndex).x());

        /* Draw scatter(Point from scatter) */
        this->drawScatter();

        /* Get points from curve table, replace current curve's points, repplote. */
        this->recoveryCurve(gpoSelectedCurve);

        /* Draw error, get point from Error table */
        this->drawError();

        /* 按了恢复键,要等下一次保存之后 再开启. */
        ui->actionRecovery->setEnabled(false);
    }
        break;
        /* 广域视电阻率的曲线(视电阻率手动任意拖动 plot)，点击恢复键，则恢复选中点（单个点） */
    case 1:
    {
        STATION oStation = gmapCurveStation.value(gpoSelectedCurve);

        double dF = gpoSelectedCurve->sample(giSelectedIndex).x();

        double dRho = poDb->getRho(oStation, dF);

        QPolygonF aoPointF = this->currentCurvePoints();

        /* Rho的中间结果以QwtPlotCurve为载体存放 */
        aoPointF.replace(giSelectedIndex, QPointF(dF, dRho));

        gpoSelectedCurve->setSamples(aoPointF);

        ui->plotRho->replot();
    }
        break;
    default:
        break;
    }
}

/* 做了裁剪之后， 点击保存， 保存的是散点（detail）， 接着更新Curve */
void MainWindow::on_actionSave_triggered()
{
    QPolygonF aoPointF = this->currentScatterPoints();

    if( aoPointF.count() == 0)
    {
        return;
    }

    QVector<double> adY;
    adY.clear();

    for(qint32 i = 0; i < aoPointF.count(); i++)
    {
        adY.append(aoPointF.at(i).y());
    }

    RX *poRX = gmapCurveData.value(gpoSelectedCurve);

    /* 当前认可修改,将修改结果写入到Rx类中 */
    poRX->updateScatter(gpoSelectedCurve->sample( giSelectedIndex).x(), adY );

    /* 修改,确认 存进了Rx类里面了,需要恢复,打开恢复按钮. */
    ui->actionRecovery->setEnabled(true);

    /* 此时有可能会使用到保存频率域数据到新的csv文档中. 手动操作了,删除了散点图中的点,有必要开启store按钮 */
    if( !ui->actionStore->isEnabled())
    {
        ui->actionStore->setEnabled(true);
    }

    /* 频率域数据修改且认可了,那么就更新散点图 */
    gpoScatter->setSamples( aoPointF );

    this->resizeScaleScatter();

    ui->plotScatter->replot();

    /* 保存完了, 置为disable状态 */
    ui->actionSave->setEnabled(false);

    bModifyField = true;
}

/* 做了裁剪之后， 点击保存， 保存的是散点（detail）， 接着更新Curve
 * 1，存储手动修改后的电场数据到csv文件
 * 2，存储手动调整后的广域视电阻率数据到数据库文件 */
void MainWindow::on_actionStore_triggered()
{
    switch (ui->stackedWidget->currentIndex())
    {
    case 0://场值调整后，保存结果至csv _filtered.csv
        this->store();

        break;

    case 1://广域视电阻率调整后，保存结果至数据库，以备export
    {
        QMap<QwtPlotCurve*, STATION>::const_iterator it;
        for(it = gmapCurveStation.constBegin(); it!= gmapCurveStation.constEnd(); it++)
        {
            QPolygonF aoPointF;
            aoPointF.clear();

            for(uint i = 0; i < it.key()->dataSize(); i++)
            {
                // qDebugV0()<<it.key()->sample(i);
                aoPointF.append(it.key()->sample(i));
            }

            poDb->modifyRho(it.value(), aoPointF);

            /* 中间结果保存完了之后,将store键置为Disable */
            ui->actionStore->setEnabled(false);
            /* 保存了之后就置false */
            bModifyRho = false;
        }
    }
        break;
    default:
        break;
    }
}

/* 计算广域视电阻率 */
void MainWindow::on_actionCalRho_triggered()
{
    /* Import RX */
    poDb->importRX(gapoRX);

    QString oStrFileName = QFileDialog::getOpenFileName(this,
                                                        "打开坐标文件",
                                                        QString("%1").arg(this->LastDirRead()),
                                                        "坐标文件(*.dat *.txt *.csv)");

    if(oStrFileName.length() != 0)
    {
        if( !poDb->importXY(oStrFileName) )
        {
            return;
        }
    }

    if(! poCalRho->getAB() )
    {
        return;
    }

    QList<STATION> aoStation = poDb->getStation("RX");

    gmapCurveStation.clear();

    ui->plotRho->detachItems();

    gpoSelectedCurve = NULL;

    poPickerRho->setNULL();

    poDb->cleanRho();

    /* 每条曲线一个thread，每个thread计算完毕了，会发射一个信号，main线程会draw Rho曲线 */
    foreach(STATION oStation, aoStation)
    {
        poCalRho->CalRho(oStation);
    }

    //ui->actionClear->setEnabled(false);
    ui->actionCutterH->setEnabled(false);
    ui->actionCutterV->setEnabled(false);
    ui->actionExportRho->setEnabled(true);
    ui->actionImportRX->setEnabled(false);
    ui->actionImportTX->setEnabled(false);

    ui->actionRecovery->setEnabled(true);

    ui->actionSave->setEnabled(false);
    ui->actionStore->setEnabled(false);

    ui->actionCalRho->setEnabled(true);

    ui->stackedWidget->setCurrentIndex(1);
}

void MainWindow::showTableTX(QSqlTableModel *poModel)
{    
    ui->tableViewTX->setModel(poModel);

    ui->tableViewTX->update();

    ui->tableViewTX->repaint();

    ui->tabWidget->setCurrentIndex(0);

    QVector<double> adF, adI;

    for(int i = 0; i < poModel->rowCount(); i++)
    {
        adF.append( poModel->data(poModel->index(i,0), Qt::DisplayRole).toDouble() );
        adI.append( poModel->data(poModel->index(i,1), Qt::DisplayRole).toDouble() );
    }

    this->drawTx(adF, adI);
}

void MainWindow::showTableRX(QSqlTableModel *poModel)
{
    ui->tableViewRX->setModel(poModel);

    ui->tableViewRX->update();

    ui->tableViewRX->repaint();

    this->drawRx();

    ui->tabWidget->setCurrentIndex(1);
}

void MainWindow::showTableXY(QSqlTableModel *poModel)
{
    ui->tableViewXY->setModel(poModel);

    ui->tableViewXY->update();

    ui->tableViewXY->repaint();

    ui->tabWidget->setCurrentIndex(2);
}

void MainWindow::showTableRho(CustomTableModel *poModel)
{
    qDebug()<<poModel->rowCount();

    ui->tableViewRho->setModel(poModel);

    /* 这里要update，因为导出表格时的依据就是前表格的model */
    ui->tableViewRho->update();

    //    if(ui->tabWidget->currentIndex() !=4)
    //        ui->tabWidget->setCurrentIndex(3);
    ui->tabWidget->setCurrentIndex(3);
}

/* 画广域视电阻率曲线图，是按了计算Rho按钮，然后计算，计算完了发信号过来， */
void MainWindow::drawRho(STATION oStation, QVector<double> adF, QVector<double> adRho)
{
    QSet<double> setF;

    foreach(double dF, adF)
    {
        setF.insert(dF);
    }

    QList<double> listF = setF.toList();

    qSort(listF);

    /* Fill ticks */
    QList<double> adTicks[QwtScaleDiv::NTickTypes];
    adTicks[QwtScaleDiv::MajorTick] = listF;
    QwtScaleDiv oScaleDiv( adTicks[QwtScaleDiv::MajorTick].last(), adTicks[QwtScaleDiv::MajorTick].first(), adTicks );

    //QwtScaleDiv (double lowerBound, double upperBound, QList< double >[NTickTypes])
    //    qDebugV0()<<ui->plotRho->axisScaleDiv(QwtPlot::xBottom).lowerBound()
    //             <<adTicks[QwtScaleDiv::MajorTick].last()
    //            <<ui->plotRho->axisScaleDiv(QwtPlot::xBottom).upperBound()
    //           <<adTicks[QwtScaleDiv::MajorTick].first();

    ui->plotRho->setAxisScaleDiv( QwtPlot::xBottom, oScaleDiv );

    /* Curve title, cut MCSD_ & suffix*/
    const QwtText oTxtTitle( QString("L%1-%2_D%3-%4_%5")
                             .arg(oStation.oStrLineId)
                             .arg(oStation.oStrSiteId)
                             .arg(oStation.iDevId)
                             .arg(oStation.iDevCh)
                             .arg(oStation.oStrTag) );

    /* Create a curve pointer */
    QwtPlotCurve *poCurve = new QwtPlotCurve( oTxtTitle );

    gmapCurveStation.insert(poCurve, oStation);

    poCurve->setPen( Qt::red, 2, Qt::SolidLine );
    poCurve->setRenderHint( QwtPlotItem::RenderAntialiased, true );

    QwtSymbol *poSymbol = new QwtSymbol( QwtSymbol::Ellipse,
                                         QBrush( Qt::yellow ),
                                         QPen( Qt::blue, 2 ),
                                         QSize( 8, 8 ) );
    poCurve->setSymbol( poSymbol );
    poCurve->setStyle(QwtPlotCurve::Lines);

    poCurve->setSamples( adF, adRho );
    poCurve->setAxes(QwtPlot::xBottom, QwtPlot::yLeft);
    poCurve->attach(ui->plotRho);
    poCurve->setVisible( true );

    ui->plotRho->setFooter("请选中线上的点！");

    ui->plotRho->replot();
}

void MainWindow::on_actionReadme_triggered()
{
    QString qexeFullPath = QCoreApplication::applicationDirPath();

    qexeFullPath.append("//readme.pdf");

    QDesktopServices::openUrl(QUrl::fromLocalFile(qexeFullPath));
}

void MainWindow::on_actionImportRho_triggered()
{
    QStringList aoStrRhoFileName = QFileDialog::getOpenFileNames(this,
                                                                 "打开视电阻率文件",
                                                                 QString("%1").arg(this->LastDirRead()),
                                                                 "视电阻率文件(*.csv)");

    if(aoStrRhoFileName.isEmpty())
    {
        QMessageBox::warning(this, "警告","未选中有效文件，\n请重新选择！");
        return;
    }

    poDb->cleanRho();

    foreach(QString oStrRho, aoStrRhoFileName)
    {
        QFile oFile(oStrRho);
        if(!oFile.open(QIODevice::ReadOnly))
        {
            return;
        }

        QList<RhoResult> aoRho;
        aoRho.clear();
        QTextStream oStream(&oFile);

        /* 首行是列头 */
        oStream.readLine();

        while( !oStream.atEnd() )
        {
            QString oStrLine = oStream.readLine();
            QStringList aoStrLine = oStrLine.split(",", QString::SkipEmptyParts);

            RhoResult oRho;

            STATION oStation;
            oStation.oStrLineId = aoStrLine.at(0);
            oStation.oStrSiteId = aoStrLine.at(1);
            oStation.iDevId = aoStrLine.at(2).toInt();
            oStation.iDevCh = aoStrLine.at(3).toInt();
            oStation.oStrTag = aoStrLine.at(4);
            oRho.oStation = oStation;

            oRho.dF = aoStrLine.at(5).toDouble();
            oRho.dI = aoStrLine.at(6).toDouble();

            oRho.dField = aoStrLine.at(7).toDouble();
            QString oStrEr = aoStrLine.at(8);

            /* 剃掉% */
            oStrEr.chop(1);

            oRho.dErr = oStrEr.toDouble();

            oRho.dRho = aoStrLine.at(9).toDouble();

            Position oAB;
            oAB.dMX = aoStrLine.at(10).toDouble();
            oAB.dMY = aoStrLine.at(11).toDouble();
            oAB.dMZ = aoStrLine.at(12).toDouble();
            oAB.dNX = aoStrLine.at(13).toDouble();
            oAB.dNY = aoStrLine.at(14).toDouble();
            oAB.dNZ = aoStrLine.at(15).toDouble();
            oRho.oAB = oAB;

            Position oMN;
            oMN.dMX = aoStrLine.at(16).toDouble();
            oMN.dMY = aoStrLine.at(17).toDouble();
            oMN.dMZ = aoStrLine.at(18).toDouble();
            oMN.dNX = aoStrLine.at(19).toDouble();
            oMN.dNY = aoStrLine.at(20).toDouble();
            oMN.dNZ = aoStrLine.at(21).toDouble();
            oRho.oMN = oMN;

            aoRho.append( oRho );
        }

        oFile.close();

        poDb->importRho(aoRho);
    }

    /* draw Rho curve */
    QList<STATION> aoStation = poDb->getStation("Rho");

    foreach(STATION oStation, aoStation)
    {
        QPolygonF aoPoint = poDb->getRho(oStation);

        QVector<double> adF;
        QVector<double> adRho;
        adF.clear();
        adRho.clear();

        foreach(QPointF oPointF, aoPoint)
        {
            adF.append( oPointF.x() );
            adRho.append( oPointF.y() );
        }
        this->drawRho(oStation, adF, adRho);
    }
    /* Manual adjustment of apparent resistivity curve */
    ui->actionCutterH->setEnabled(false);
    ui->actionCutterV->setEnabled(false);
    ui->actionExportRho->setEnabled(true);
    ui->actionImportRX->setEnabled(false);
    ui->actionImportTX->setEnabled(false);

    ui->actionRecovery->setEnabled(true);

    ui->actionSave->setEnabled(false);
    ui->actionStore->setEnabled(false);

    ui->actionCalRho->setEnabled(true);

    ui->stackedWidget->setCurrentIndex(1);
}
