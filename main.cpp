#include <QtCore>
#include <iostream>

#include <pHash.h>

QHash<ulong64, int>
hashDir( const QString& path )
{
    QHash<ulong64, int> hashes;
    QDir dir( path );
    QFileInfoList list = dir.entryInfoList();
    for ( int i = 0; i < list.size(); ++i )
    {
        QFileInfo fileInfo = list.at( i );
        if ( !fileInfo.fileName().endsWith(".jpg") && !fileInfo.fileName().endsWith(".png") )
            continue;

//        qDebug() << QString( "%1 %2" ).arg( fileInfo.size(), 10 ).arg( fileInfo.fileName() );

        ulong64 hash;
        int err = ph_dct_imagehash(fileInfo.absoluteFilePath().toLocal8Bit(), hash);
        if ( err )
            continue;

        hashes.insertMulti( hash, i );
        QFile::remove( fileInfo.absoluteFilePath() );
    }

    return hashes;
}


void
createSnaps( const QString& path )
{
    QDir::temp().mkdir( "xrayfoo" );
    QStringList args;
    args << "-sstep" << "1" << "-nosound" << "-vo" << "jpeg" << "-ss" << "00:00:30" << "-frames" << "10" << path;

    QString newpath = QDir::tempPath() + "/xrayfoo";
    QProcess p;
    p.setWorkingDirectory( newpath );
    p.start( "mplayer", args );
    p.waitForFinished();
}


bool
keyLessThan( const QString& a, const QString& b )
{
    return QString::localeAwareCompare( a, b ) < 0;
}


int
main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    if ( app.arguments().count() < 2 )
    {
        std::cout << "First param needs to be a directory that you want to be scanned for duplicate videos." << std::endl;
        return 1;
    }

    QHash<ulong64, QPair<int, QString> > hashes;

    QDir dir( app.arguments().at( 1 ) );
    QFileInfoList list = dir.entryInfoList();
    for ( int i = 0; i < list.size(); ++i )
    {
        QFileInfo fileInfo = list.at( i );
        if ( !fileInfo.fileName().endsWith(".wmv") && !fileInfo.fileName().endsWith(".avi") &&
             !fileInfo.fileName().endsWith(".mp4") && !fileInfo.fileName().endsWith(".mkv") &&
             !fileInfo.fileName().endsWith(".flv") && !fileInfo.fileName().endsWith(".mpg") )
            continue;

        qDebug() << QString( "%1 %2" ).arg( fileInfo.size(), 10 ).arg( fileInfo.fileName() );

        createSnaps( fileInfo.absoluteFilePath() );
        QHash<ulong64, int> hash = hashDir( QDir::tempPath() + "/xrayfoo" );

        QPair<int, QString> pair;
        for ( int j = 0; j < hash.keys().count(); j++ )
        {
            ulong64 key = hash.keys().at( j );
            int value = hash.values().at( j );

            if ( hashes.contains( key ) )
            {
                QList<QPair<int, QString> > res;
                res = hashes.values( key );

                for ( int x = 0; x < res.count(); x++ )
                {
                    pair = res.at( x );
                    if ( pair.second == fileInfo.absoluteFilePath() )
                        continue;

                    qDebug() << "FOUND DUPE:" << value << pair.first << pair.second;
                }
            }

            pair.first = value;
            pair.second = fileInfo.absoluteFilePath();
            hashes.insertMulti( key, pair );
        }
    }

/*    for ( int i = 0; i < hashes.keys().count(); i++ )
    {
        QHash<ulong64, QString> h1 = hashes.values().at( i );
        qDebug() << "Comparing" << i << "-" << hashes.keys().at( i );
        QHash<QString, bool> found;
        for ( int x = 0; x < h1.keys().count(); x++ )
        {
//            qDebug() << "Comparing" << x << "-" << h1.keys().at( x ) << h1.values().at( x );
            found[h1.values().at( x )] = false;

            for ( int j = i + 1; j < hashes.keys().count(); j++ )
            {
                QHash<ulong64, QString> h2 = hashes.values().at( j );
//                qDebug() << "    with" << j << "-" << hashes.keys().at( j );
                for ( int y = 0; y < h2.keys().count(); y++ )
                {
                    int distance = ph_hamming_distance( h1.keys().at( x ), h2.keys().at( y ) );
                    if ( distance < 6 )
                    {
//                        qDebug() << "    Exact copy of" << y << "-" << h2.keys().at( y ) << h2.values().at( y );
                        found[h1.values().at( x )] = true;
                        qDebug() << "DUPE DETECTED!";
                        qDebug() << "    with" << j << "-" << hashes.keys().at( j );
                        break;
                    }
                }
            }
        }

        int c = 0;
        QList<QString>fs = found.keys();
        qSort( fs.begin(), fs.end(), keyLessThan );
        foreach ( const QString& k, fs )
        {
            if ( found.value( k ) )
                c++;

//            qDebug() << "Found" << k << found.value( k );
        }
    }*/

    return 0;
}
