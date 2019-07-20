#include "Data/RX.h"


RX::RX(QString oStrFileName, QObject *parent):QObject(parent)
{
    oStrCSV = oStrFileName;

    this->importRX(oStrFileName);
}

/* Import csv file */
void RX::importRX(QString oStrFileName)
{
    QFileInfo oFileInfo(oStrFileName);

    QString oStrBaseName = oFileInfo.baseName();

    QStringList aoStrStationInfo = oStrBaseName.split('_', QString::SkipEmptyParts );

    //qDebugV0()<<aoStrStationInfo;

    /* LineId */
    QString oStrLineId = aoStrStationInfo.at(1);
    oStrLineId.remove(0,1);
    iLineId = oStrLineId.toInt();

    /* SiteId */
    QString oStrSiteId= aoStrStationInfo.at(2);
    oStrSiteId.remove(0,1);
    iSiteId = oStrSiteId.toInt();

    /* DevId */
    QString oStrDevId = aoStrStationInfo.at(3);
    oStrDevId.remove(0,1);
    iDevId = oStrDevId.toInt();

    /* DevCh */
    QString oStrDevCh= aoStrStationInfo.at(4);
    oStrDevCh.remove(0,1);
    iDevCh = oStrDevCh.toInt();

    /* Component identifier */
    oStrTag = aoStrStationInfo.at(5);

    /*  */
    QFile oFile(oStrFileName);
    QString oStrLineCSV;
    oStrLineCSV.clear();

    if(oFile.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        QTextStream oStream(&oFile);

        oStream.seek(0);

        QStringList aoStrLineCSV;

        QString oStrLineCSV;

        while(!oStream.atEnd())
        {
            oStrLineCSV.clear();
            oStrLineCSV = oStream.readLine();

            if(!oStrLineCSV.isEmpty())
            {
                aoStrLineCSV.clear();

                aoStrLineCSV = oStrLineCSV.split(',', QString::SkipEmptyParts);

                adF.append(aoStrLineCSV.first().toDouble());

                aoStrLineCSV.removeFirst();

                QVector<double>  adData;
                adData.clear();

                foreach(QString oStrData, aoStrLineCSV)
                {
                    adData.append(oStrData.toDouble());
                }

                aadScatter.append(adData);

                adE.append(getE(adData));

                adErr.append(getErr(adData));
            }
            else
            {
                break;
            }
        }
    }

    oFile.close();

    //qDebugV0()<<iDevId<<iDevCh<<iLineId<<iSiteId<<oStrTag;

    //qDebugV0()<<adF;
    //qDebugV0()<<aadScatter;
}

/************************************************************
 *
 */
void RX::renewScatter(int iIndex)
{
    /*  */
    qDebugV0()<<iIndex<<oStrCSV;

    QFile oFile(oStrCSV);
    QString oStrLineCSV;
    oStrLineCSV.clear();

    if(oFile.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        QTextStream oStream(&oFile);

        oStream.seek(0);

        QStringList aoStrLineCSV;

        QString oStrLineCSV;

        for(int i = 0; i < iIndex; i++)
        {
            oStream.readLine();
        }
        oStrLineCSV.clear();

        oStrLineCSV = oStream.readLine();

        if(!oStrLineCSV.isEmpty())
        {
            aoStrLineCSV.clear();

            aoStrLineCSV = oStrLineCSV.split(',', QString::SkipEmptyParts);

            //qDebugV0()<< aoStrLineCSV.first().toDouble();

            aoStrLineCSV.removeFirst();

            QVector<double> adData;
            adData.clear();

            foreach(QString oStrData, aoStrLineCSV)
            {
                adData.append(oStrData.toDouble());
            }

            aadScatter[iIndex] = adData;

            adE[iIndex] = getE(adData);

            adErr[iIndex] = getErr(adData);
        }
    }

    oFile.close();
}

double RX::getE(QVector<double> adData)
{
    double dAverage = 0;
    double dSum = 0;

    foreach(double dData, adData)
    {
        dSum += dData;
    }

    if(adData.count() != 0)
    {
        dAverage = dSum/adData.count();
    }

    return dAverage;
}

double RX::getErr(QVector<double> adData)
{
    double dError = 0;

    double dAvg = 0;
    dAvg = this->getE(adData);

    double dTemp = 0 ;

    for(qint32 i = 0 ; i < adData.count(); i++)
    {
        dTemp += qPow((adData.at(i) - dAvg)/dAvg, 2);
    }

    dError = qSqrt(dTemp/adData.count());

    return dError;
}

/***********************************************************
 * Update scatter, next update E, update Error
 */
void RX::updateScatter(int iIndex, QVector<double> adScatter)
{
    aadScatter[iIndex].clear();

    aadScatter[iIndex] = adScatter;

    adE[iIndex] = this->getE(adScatter);

    adErr[iIndex] = this->getErr(adScatter);
}
