/*  PocketPilot Intro
 *
 *  Part of PocketPilot, which is based on Pokemon Automation
 *  (https://github.com/PokemonAutomation/).
 *
 */

#include "Common/Compiler.h"
#include <memory>
#include <string>
#include <QApplication>
#include <QByteArray>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QMediaMetaData>
#include <QMediaPlayer>
#include <QSaveFile>
#include <QStandardPaths>
#include <QTimer>
#include <QUrl>
#include <QVideoFrame>
#include <QVideoSink>
#include "CommonFramework/Logging/Logger.h"
#include "PocketPilot_Intro.h"

namespace PokemonAutomation{
namespace PocketPilot{


const char* const NO_INTRO_OPTION = "--no-intro";
const char* const INTRO_TEST_OPTION = "--intro-test";


namespace{

//  The film in the program's resources (see pocketpilot.qrc).
const char* const INTRO_RESOURCE = ":/pocketpilot/intro.mp4";

//  The player wants a file. The film is written once to the temporary
//  folder (named after its size and checksum, so that a newer film never
//  meets an older file) and played from there.
QString extract_intro(Logger& logger){
    QFile source(INTRO_RESOURCE);
    if (!source.open(QIODevice::ReadOnly)){
        logger.log("Intro: the film is not in this build.", COLOR_RED);
        return QString();
    }
    const QByteArray data = source.readAll();
    const QString folder = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    const QString path = folder + "/PocketPilot-intro-" +
        QString::number(data.size()) + "-" + QString::number(qChecksum(data), 16) + ".mp4";

    const QFileInfo info(path);
    if (info.exists() && info.size() == data.size()){
        return path;
    }
    QSaveFile out(path);
    if (!out.open(QIODevice::WriteOnly) || out.write(data) != data.size() || !out.commit()){
        logger.log("Intro: cannot write the film to " + path.toStdString() + ".", COLOR_RED);
        return QString();
    }
    return path;
}


}



int run_intro_test(QApplication&){
    Logger& logger = global_logger_tagged();
    const QString path = extract_intro(logger);
    if (path.isEmpty()){
        return 1;
    }

    //  No window and no sound output: nothing on a build machine to show
    //  it on or to hear it with. The decoder works all the same.
    QMediaPlayer player;
    QVideoSink sink;
    player.setVideoSink(&sink);

    int frames = 0;
    QSize size;
    QObject::connect(&sink, &QVideoSink::videoFrameChanged, &player, [&](const QVideoFrame& frame){
        if (frame.isValid()){
            frames++;
            size = frame.size();
        }
    });

    QEventLoop loop;
    bool finished = false;
    QString error;
    QObject::connect(&player, &QMediaPlayer::mediaStatusChanged, &loop, [&](QMediaPlayer::MediaStatus status){
        if (status == QMediaPlayer::EndOfMedia){
            finished = true;
            loop.quit();
        }else if (status == QMediaPlayer::InvalidMedia){
            loop.quit();
        }
    });
    QObject::connect(&player, &QMediaPlayer::errorOccurred, &loop, [&](QMediaPlayer::Error, const QString& message){
        error = message;
        loop.quit();
    });
    QTimer::singleShot(30000, &loop, [&]{ loop.quit(); });

    player.setSource(QUrl::fromLocalFile(path));
    player.play();
    loop.exec();

    const int audio_tracks = (int)player.audioTracks().size();
    logger.log(
        "Intro test: " + std::to_string(frames) + " pictures of " +
        std::to_string(size.width()) + "x" + std::to_string(size.height()) +
        ", " + std::to_string(audio_tracks) + " sound track(s), length " +
        std::to_string(player.duration()) + " ms, " +
        (finished ? "played to the end" : "did not play to the end") +
        (error.isEmpty() ? "" : ", error: " + error.toStdString()) + "."
    );

    //  The film has 200 pictures; a few may be dropped on a slow machine.
    const bool ok =
        finished &&
        frames >= 150 &&
        size.width() == 1920 && size.height() == 1080 &&
        audio_tracks >= 1;
    logger.log(ok ? "Intro test: passed." : "Intro test: FAILED.", ok ? Color() : COLOR_RED);
    return ok ? 0 : 1;
}



}
}
