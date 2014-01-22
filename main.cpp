#include <QtCore>
#include <iostream>

#include <pHash.h>

static const QString TMPNAME = "xraytmp";   // name of temporary dir (usually below /tmp)
static const int THRESHOLD = 5;             // find at least x dupe snapshots to consider video duped
static const int KEYFRAMES = 10;             // how many snapshots (keyframes) to compare


// create snapshots from video file
void
createSnaps( const QString& file, const QString& tmpPath )
{
    QStringList args;
    args << "-sstep" << "10"                            // only output keyframes in 10 second increments
         << "-nosound" << "-vo" << "jpeg"               // output as jpegs
         << "-ss" << "00:00:45"                         // start 45 seconds into the video
         << "-frames" << QString::number( KEYFRAMES )   // output x frames total
         << file;

    QProcess p;
    p.setWorkingDirectory( tmpPath );
    p.start( "mplayer", args );
    p.waitForFinished( 60000 );             // wait one minute at most
}


// create perceptual hash for snapshots
QHash<ulong64, int>
hashSnaps( const QString& path )
{
    QHash<ulong64, int> hashes;

    QDir dir( path );
    QFileInfoList list = dir.entryInfoList();
    for ( int i = 0; i < list.size(); ++i )
    {
        QFileInfo fileInfo = list.at( i );
        if ( !fileInfo.fileName().endsWith( ".jpg" ) )
            continue;

        ulong64 hash;
        int err = ph_dct_imagehash( fileInfo.absoluteFilePath().toLocal8Bit(), hash );
        if ( err )
            continue;

        hashes.insertMulti( hash, i );

        // delete snapshot
        QFile::remove( fileInfo.absoluteFilePath() );
    }

    return hashes;
}


// finds similar hashes by hamming distance
QList <QPair <int, QString> >
closestHashes( QHash<ulong64, QPair<int, QString> >& hashes, const ulong64& searchKey )
{
    QList <QPair <int, QString> > res;

    for ( int i = 0; i < hashes.keys().count(); i++ )
    {
        ulong64 key = hashes.keys().at( i );
        if ( ph_hamming_distance( searchKey, key ) < 4 )
        {
            QPair<int, QString> pair = hashes.values().at( i );
            res << pair;
        }
    }

    return res;
}


void
scanDir( QHash<ulong64, QPair<int, QString> >& hashes, const QString& path )
{
//    qDebug() << "Scanning dir:" << path;

    QDir dir( path );
    QFileInfoList list = dir.entryInfoList();
    for ( int i = 0; i < list.size(); ++i )
    {
        QFileInfo fileInfo = list.at( i );
        if ( fileInfo.isSymLink() )
            continue;
        if ( fileInfo.fileName().startsWith( "." ) )
            continue;

        if ( fileInfo.isDir() )
        {
            // scan recursively
            scanDir( hashes, fileInfo.absolutePath() + "/" + fileInfo.fileName() );
            continue;
        }

        if ( !fileInfo.fileName().endsWith(".wmv") && !fileInfo.fileName().endsWith(".avi") &&
             !fileInfo.fileName().endsWith(".mp4") && !fileInfo.fileName().endsWith(".mkv") &&
             !fileInfo.fileName().endsWith(".flv") && !fileInfo.fileName().endsWith(".mpg") &&
             !fileInfo.fileName().endsWith(".m4v") && !fileInfo.fileName().endsWith(".mov") &&
             !fileInfo.fileName().endsWith(".divx") && !fileInfo.fileName().endsWith(".mpeg") )
        {
            // only index videos
            continue;
        }

        std::cout << "Indexing \"" << qPrintable( fileInfo.absoluteFilePath() ) << "\" - size: " << fileInfo.size() << std::endl;

        // create snapshots
        const QString tmpPath = QDir::tempPath() + "/" + TMPNAME;
        createSnaps( fileInfo.absoluteFilePath(), tmpPath );
        // analyze and hash snapshots
        QHash<ulong64, int> hash = hashSnaps( tmpPath );

        QPair<int, QString> pair;       // snapshot #, filename
        QHash<QString, int> matches;    // filename, matches

        // go through all extracted snapshots and find dupes
        for ( int j = 0; j < hash.keys().count(); j++ )
        {
            ulong64 key = hash.keys().at( j );  // hash
            int value = hash.values().at( j );  // snapshot #

            {
                QList<QPair<int, QString> > res;    // snapshot #, filename

                // find closest snapshots by hamming distance
                res = closestHashes( hashes, key );
                for ( int x = 0; x < res.count(); x++ )
                {
                    pair = res.at( x );
                    if ( pair.second == fileInfo.absoluteFilePath() )
                    {
                        // ignore matches to self
                        continue;
                    }

                    // add/increment match count
                    if ( !matches.contains( pair.second ) )
                        matches[pair.second] = 1;
                    else
                        matches[pair.second] = matches[pair.second] + 1;
                }
            }

            // store hashes for future comparisons
            pair.first = value;
            pair.second = fileInfo.absoluteFilePath();
            hashes.insertMulti( key, pair );
        }

        for ( int y = 0; y < matches.keys().count(); y++ )
        {
            // threshold of matched snapshots to consider duped
            if ( matches.values().at( y ) >= THRESHOLD )
            {
                std::cout << "   --> Dupe of \"" << qPrintable( matches.keys().at( y ) ) << "\" - size: " << QFileInfo( matches.keys().at( y ) ).size()
                          << " (scores " << qMin( KEYFRAMES, matches.values().at( y ) ) << " out of " << KEYFRAMES << ")" << std::endl;
            }
        }
    }
}


int
main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    if ( app.arguments().count() < 2 )
    {
        std::cout << "Usage: xray /some/dir/full/of/videos" << std::endl;
        std::cout << "xray indexes video files by their perceptual hash and finds duplicates." << std::endl
                  << "This means files are not compared byte for byte, but by their visual content." << std::endl
                  << "It will/should find dupes of videos, even when they are encoded in different formats and resolutions." << std::endl;
        return 1;
    }

    QHash<ulong64, QPair<int, QString> > hashes;    // perceptual hash, pair<snapshot #, filename>

    QDir::temp().mkdir( TMPNAME );
    scanDir( hashes, app.arguments().at( 1 ) );
    QDir::temp().rmdir( TMPNAME );

    return 0;
}
