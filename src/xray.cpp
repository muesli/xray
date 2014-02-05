/* The MIT License (MIT)

Copyright (c) 2014 Christian Muehlhaeuser <muesli@gmail.com>

Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the "Software"), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
the Software, and to permit persons to whom the Software is furnished to do so,
subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

#include <QtCore>
#include <iostream>

#include <pHash.h>

static QString TMPPATH;                 // name of temporary dir (usually below /tmp)
static const double THRESHOLD = 0.6;    // find at least x% dupe frames to consider video duped
static int FRAMES = 5;                  // how many frames to compare
static int HAMMINGDIST = 16;            // hamming distance threshold for frame comparison


void
ffmpegMissing()
{
    std::cout << "Error: ffmpeg could not be found!" << std::endl;
    std::exit( 1 );
}


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
    if ( !p.waitForFinished( 60000 ) )  // wait one minute at most
    {
        ffmpegMissing();    // this aborts the app
    }

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
createSnaps( const QString& file, const QString& path )
{
    // get meta data
    QVariantMap metadata = videoMetaData( file );
    std::cout << "   --> Duration: " << metadata[ "duration" ].toInt() << " - bitrate: " << metadata[ "bitrate" ].toInt() << std::endl;

    // start 10% into the video
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
        p.setWorkingDirectory( path );
        p.start( "ffmpeg", args );
        if ( !p.waitForFinished( 60000 ) )  // wait one minute at most
        {
            ffmpegMissing();    // this aborts the app
        }
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


// find the files matching most of the p-hashes
QHash<QString, int>
fileMatches( const QString& file, const QHash<ulong64, int>& hash, QHash<ulong64, QPair<int, QString> >& hashes )
{
    QHash<QString, int> matches;    // filename, matches

    // go through all extracted snapshots and find dupes
    for ( int j = 0; j < hash.keys().count(); j++ )
    {
        ulong64 key = hash.keys().at( j );  // hash
        int value = hash.values().at( j );  // snapshot #

        QPair<int, QString> pair;           // snapshot #, filename
        QList<QPair<int, QString> > res;    // snapshot #, filename

        // find closest snapshots by hamming distance
        res = closestHashes( hashes, key );
        for ( int x = 0; x < res.count(); x++ )
        {
            pair = res.at( x );
            if ( pair.second == file )
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

        // store hashes for future comparisons
        pair.first = value;
        pair.second = file;
        hashes.insertMulti( key, pair );
    }

    return matches;
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
        exts << ".3gp" << ".aaf" << ".asf" << ".avi" << ".divx" << ".flv"
             << ".m1v" << ".m2v" << ".m4v" << ".mkv" << ".mov" << ".mp4" << ".mpg" << ".mpeg"
             << ".rm" << ".ts" << ".vob" << ".wmv";

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

        // find known files matching the extracted hashes
        QHash<QString, int> matches;    // filename, matches
        matches = fileMatches( fileInfo.absoluteFilePath(), hash, hashes );

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
    QCoreApplication app( argc, argv );
    QCoreApplication::setApplicationName( "xray" );
    QCoreApplication::setApplicationVersion( "0.0.1" );

    QCommandLineParser cliParser;
    cliParser.setApplicationDescription( "xray indexes video files by their perceptual hash and finds duplicates.\n"
                                         "This means files are not compared byte for byte, but by their visual content.\n"
                                         "It will/should find dupes of videos, even when they are encoded in different formats and resolutions." );
    cliParser.addHelpOption();
    cliParser.addVersionOption();
    cliParser.addPositionalArgument( "path", QCoreApplication::translate( "main", "Directory to analyze" ) );

    QCommandLineOption framesOption( QStringList() << "f" << "frames",
                                     QCoreApplication::translate( "main", "Number of <frames> to analyze per video" ),
                                     "frames", "5" );
    QCommandLineOption hammingOption( QStringList() << "d" << "hamming-distance",
                                      QCoreApplication::translate( "main", "Maximum <distance> between two frames" ),
                                      "distance", "16" );

    cliParser.addOption( framesOption );
    cliParser.addOption( hammingOption );
    cliParser.process( app );

    const QStringList args = cliParser.positionalArguments();
    if ( !args.count() )
    {
        cliParser.showHelp( 1 );
    }

    FRAMES = cliParser.value( framesOption ).toInt();
    HAMMINGDIST = cliParser.value( hammingOption ).toInt();

    QHash<ulong64, QPair<int, QString> > hashes;    // perceptual hash, pair<snapshot #, filename>
    QTemporaryDir tmpDir;
    TMPPATH = tmpDir.path();
    scanDir( hashes, args.first() );

    return 0;
}
