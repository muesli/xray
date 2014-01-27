#include <QtCore>
#include <iostream>

#include <pHash.h>

static QString TMPPATH;                 // name of temporary dir (usually below /tmp)
static const double THRESHOLD = 0.6;    // find at least x% dupe frames to consider video duped
static const int FRAMES = 5;            // how many frames to compare
static const int HAMMINGDIST = 16;      // hamming distance threshold for frame comparison


QString
timeToString( int seconds )
{
    int mins = seconds / 60 % 60;
    int secs = seconds % 60;

    if ( seconds < 0 )
    {
        mins = secs = 0;
    }

    return QString( "%1:%2" )
        .arg( mins < 10 ? "0" + QString::number( mins ) : QString::number( mins ) )
        .arg( secs < 10 ? "0" + QString::number( secs ) : QString::number( secs ) );
}


QVariantMap
videoMetaData( const QString& file )
{
    QStringList args;
    args
         << "-i" << file
         << "-show_format";

    QProcess p;
    p.start( "ffprobe", args );
    p.waitForFinished( 60000 );     // wait one minute at most

    QString output = p.readAll();
    QStringList lines = output.split( "\n" );
    QVariantMap res;

    foreach ( const QString& l, lines )
    {
        const QString value = l.split("=").last();

        if ( l.startsWith( "duration" ) )
        {
            res[ "duration" ] = value.toDouble();
        }
        if ( l.startsWith( "bit_rate" ) )
        {
            res[ "bitrate" ] = value.toInt();
        }
    }

    return res;
}


// create snapshots from video file
void
createSnaps( const QString& file, const QString& tmpPath )
{
    // get meta data
    QVariantMap metadata = videoMetaData( file );
    std::cout << "   --> Duration: " << metadata[ "duration" ].toInt() << " - bitrate: " << metadata[ "bitrate" ].toInt() << std::endl;

    int start = metadata[ "duration" ].toInt() / 10;
    int step = metadata[ "duration" ].toInt() / ( FRAMES * 1.5 );

    for ( int i = 0; i < FRAMES; i++ )
    {
        QStringList args;
        args
             << "-ss" << timeToString( i * step + start )
             << "-i" << file
             << "-y"
             << "-f" << "image2"
             << "-vcodec" << "mjpeg"
             << "-vframes" << "1"
             << "-vf" << "scale=128:-1"
             << QString( "%1.jpg" ).arg( i );

        QProcess p;
        p.setWorkingDirectory( tmpPath );
        p.start( "ffmpeg", args );
        p.waitForFinished( 60000 );     // wait one minute at most
    }
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
        if ( ph_hamming_distance( searchKey, key ) < HAMMINGDIST )
        {
            QPair<int, QString> pair = hashes.values().at( i );
            res << pair;
        }
    }

    return res;
}


QByteArray
sha1Sum( const QString& path )
{
    QCryptographicHash hash( QCryptographicHash::Sha1 );
    QFile file( path );

    if ( file.open( QIODevice::ReadOnly ) )
    {
        hash.addData( file.readAll() );
        return hash.result();
    }

    return QByteArray();
}


bool
hasValidExtension( const QFileInfo& file )
{
    static QStringList exts;
    if ( !exts.count() )
        exts << ".wmv" << ".avi" << ".mp4" << ".mkv" << ".flv" << ".mpg" << ".m4v" << ".mov" << ".divx" << ".mpeg";

    foreach ( const QString& ext, exts )
    {
        if ( file.fileName().endsWith( ext ) )
        {
            return true;
        }
    }

    return false;
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
        if ( !hasValidExtension( fileInfo ) )
        {
            // only index videos
            continue;
        }

        std::cout << "Indexing \"" << qPrintable( fileInfo.absoluteFilePath() ) << "\" - size: " << fileInfo.size() << std::endl;

        // create snapshots
        createSnaps( fileInfo.absoluteFilePath(), TMPPATH );
        // analyze and hash snapshots
        QHash<ulong64, int> hash = hashSnaps( TMPPATH );

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

                    break;
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
            if ( (double)matches.values().at( y ) / (double)hash.count() >= THRESHOLD )
            {
                QFileInfo dupeFile( matches.keys().at( y ) );
                QByteArray sha1;
                if ( matches.values().at( y ) == hash.count()
                    && fileInfo.size() == dupeFile.size()
                    && ( sha1 = sha1Sum( fileInfo.absoluteFilePath() ) ) == sha1Sum( dupeFile.absoluteFilePath() ) )
                {
                    std::cout << "   --> Exact copy of \"" << qPrintable( matches.keys().at( y ) ) << "\" - sha1: " << qPrintable( sha1.toHex() ) << std::endl;
                }
                else
                {
                    std::cout << "   --> Perceptual dupe of \"" << qPrintable( matches.keys().at( y ) ) << "\" - size: " << QFileInfo( matches.keys().at( y ) ).size()
                              << " (scores " << qMin( FRAMES, matches.values().at( y ) ) << " out of " << qMin( FRAMES, hash.count() ) << ")" << std::endl;
                }
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

    QTemporaryDir tmpDir;
    TMPPATH = tmpDir.path();
    scanDir( hashes, app.arguments().at( 1 ) );

    return 0;
}
