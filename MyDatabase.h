#ifndef MYDATABASE_H
#define MYDATABASE_H

#include <QObject>

#include <QFileDialog>
#include <QMessageBox>

#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>

#include <QVector3D>

#include <QSqlTableModel>


#include "Data/RX.h"


#include "CustomTableModel.h"

typedef struct _STATION
{
    QString oStrLineId;
    QString oStrSiteId;

    int iDevId;
    int iDevCh;

    QString oStrTag;
}STATION;

/* Rho result struct */
typedef struct _RhoResult
{
    STATION oStation;

    double dF;
    double dI;
    double dField;
    double dErr;
    double dRho;

    Position oAB;
    Position oMN;
}RhoResult;


class MyDatabase : public QObject
{
    Q_OBJECT
public:
    explicit MyDatabase(QObject *parent = 0);

    void connect();

    /* 将发射端电流值写入到数据库中 */
    void importTX(QString oStrFileName);

    /* 将接收端场值写入到数据库中 */
    void importRX(QVector<RX *> apoRX); 

    /* 将AB和测点坐标写入到数据库中 */
    bool importXY(QString oStrFileName);

    /* 将计算得到的广域视电阻率与相关条件信息一并写入到数据库中 */
    void importRho( QList<RhoResult> aoRhoResult);

    void cleanRho();

    QVector<double> getF(STATION oStation);

    double getI(double dF);

    double getField(STATION oStation, double dF);

    double getErr(STATION oStation, double dF);

    Position getCoordinate(QString oStrLineId, QString oStrSiteId);

    QList<STATION> getStation(QString oStrTableName);

    /* 从数据库里面读取指定线&&指定点的广域视电阻率值 */
    double getRho(STATION oStation, double dF);

    /* 从数据库里面读取指定线的广域视电阻率值 */
    QPolygonF getRho(STATION oStation);

    /* 人为拖动Rho曲线上的点，将呈现的值写进数据库中。*/
    void modifyRho(STATION oStation, QPolygonF aoPointF);

    QSqlDatabase *poDb;

signals:    
    void SigMsg(QString);

    void SigModelTX(QSqlTableModel *);

    void SigModelRX(QSqlTableModel *);

    void SigModelXY(QSqlTableModel *);

    void SigModelRho(CustomTableModel *);

public slots:

};

#endif // MYDATABASE_H
