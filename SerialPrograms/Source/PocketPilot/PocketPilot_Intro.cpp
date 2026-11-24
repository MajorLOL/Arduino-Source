/*  PocketPilot Intro
 *
 *  Part of PocketPilot, which is based on Pokemon Automation
 *  (https://github.com/PokemonAutomation/).
 *
 */

#include "Common/Compiler.h"
#include <functional>
#include <memory>
#include <string>
#include <QApplication>
#include <QAudioDevice>
#include <QAudioOutput>
#include <QByteArray>
#include <QCursor>
#include <QElapsedTimer>
#include <QEvent>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QMediaDevices>
#include <QMediaMetaData>
#include <QMediaPlayer>
#include <QObject>
#include <QPalette>
#include <QSaveFile>
#include <QScreen>
#include <QStandardPaths>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>
#include <QVideoFrame>
#include <QVideoSink>
#include <QVideoWidget>
#include <QWidget>
#include "CommonFramework/Logging/Logger.h"
#include "PocketPilot_Intro.h"

namespace PokemonAutomation{
namespace PocketPilot{


const char* const NO_INTRO_OPTION = "--no-intro";
const char* const INTRO_TEST_OPTION = "--intro-test";


namespace{

//  The film in the program's resources (see pocketpilot.qrc).
const char* const INTRO_RESOURCE = ":/pocketpilot/intro.mp4";

//  Longest the intro may take (the film is under 7 seconds) and the longest
//  wait for the first picture.
constexpr int INTRO_MAX_MS = 20000;
constexpr int INTRO_FIRST_PICTURE_MS = 6000;
//  A click or key in the first moments is not meant for the intro: the
//  program was just started, perhaps by a double click.
constexpr int INTRO_IGNORE_INPUT_MS = 500;
//  A beat on the last picture, the logo.
constexpr int INTRO_LINGER_MS = 400;



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


//  A click or a key, anywhere in the program, skips the intro.
class SkipFilter : public QObject{
public:
    explicit SkipFilter(std::function<void()> skip)
        : m_skip(std::move(skip))
    {
        m_since.start();
    }
    virtual bool eventFilter(QObject*, QEvent* event) override{
        if (event->type() != QEvent::KeyPress && event->type() != QEvent::MouseButtonPress){
            return false;
        }
        if (m_since.elapsed() >= INTRO_IGNORE_INPUT_MS){
            m_skip();
        }
        return true;
    }
private:
    std::function<void()> m_skip;
    QElapsedTimer m_since;
};


//  The black screen the film plays on.
class IntroWindow : public QWidget{
public:
    IntroWindow()
        : QWidget(nullptr, Qt::Window | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint)
    {
        setWindowTitle("PocketPilot Studio");
        setAutoFillBackground(true);
        QPalette black_palette = palette();
        black_palette.setColor(QPalette::Window, Qt::black);
        setPalette(black_palette);
        setStyleSheet("background: #000000; border: none;");
        setCursor(Qt::BlankCursor);

        m_video = new QVideoWidget(this);
        m_video->setAspectRatioMode(Qt::KeepAspectRatio);
        m_video->setAutoFillBackground(true);
        m_video->setPalette(black_palette);
        QVBoxLayout* column = new QVBoxLayout(this);
        column->setContentsMargins(0, 0, 0, 0);
        column->addWidget(m_video);
    }
    QVideoWidget* video() const{
        return m_video;
    }
private:
    QVideoWidget* m_video;
};


}



void play_intro(){
    Logger& logger = global_logger_tagged();
    try{
        const QString path = extract_intro(logger);
        if (path.isEmpty()){
            return;
        }

        //  The window first, so that it is destroyed last.
        IntroWindow window;
        QMediaPlayer player;
        //  The sound only where there is something to play it on: a machine
        //  without a sound device plays the film without sound. (Some Qt
        //  versions fail in the player's sound thread when there is no device.)
        std::unique_ptr<QAudioOutput> audio;
        if (!QMediaDevices::audioOutputs().isEmpty()){
            audio = std::make_unique<QAudioOutput>();
            audio->setVolume(0.8f);
            player.setAudioOutput(audio.get());
        }else{
            logger.log("Intro: no sound device, the film plays without sound.");
        }
        player.setVideoOutput(window.video());

        QEventLoop loop;
        bool picture = false;
        bool ended = false;
        std::string reason = "it was skipped";

        auto finish = [&](const std::string& why){
            if (!ended){
                ended = true;
                reason = why;
                loop.quit();
            }
        };

        QObject::connect(
            &player, &QMediaPlayer::mediaStatusChanged, &loop,
            [&](QMediaPlayer::MediaStatus status){
                if (status == QMediaPlayer::EndOfMedia){
                    //  A beat on the logo, then on with the program.
                    QTimer::singleShot(INTRO_LINGER_MS, &loop, [&]{ finish("it played to the end"); });
                }else if (status == QMediaPlayer::InvalidMedia){
                    finish("the film cannot be decoded");
                }
            }
        );
        QObject::connect(
            &player, &QMediaPlayer::errorOccurred, &loop,
            [&](QMediaPlayer::Error, const QString& message){
                logger.log("Intro: the player reports: " + message.toStdString(), COLOR_RED);
                finish("the player failed");
            }
        );
        QObject::connect(
            window.video()->videoSink(), &QVideoSink::videoFrameChanged, &loop,
            [&](const QVideoFrame& frame){
                if (frame.isValid()){
                    picture = true;
                }
            }
        );

        //  If no picture comes, the program does not wait for it.
        QTimer::singleShot(INTRO_FIRST_PICTURE_MS, &loop, [&]{
            if (!picture){
                finish("no picture came within " + std::to_string(INTRO_FIRST_PICTURE_MS / 1000) + " seconds");
            }
        });
        QTimer::singleShot(INTRO_MAX_MS, &loop, [&]{ finish("it took too long"); });

        SkipFilter skip([&]{ finish("it was skipped"); });
        qApp->installEventFilter(&skip);

        //  On the screen the program was started on.
        QScreen* screen = QGuiApplication::screenAt(QCursor::pos());
        if (screen == nullptr){
            screen = QGuiApplication::primaryScreen();
        }
        if (screen != nullptr){
            window.setScreen(screen);
            window.setGeometry(screen->geometry());
        }

        player.setSource(QUrl::fromLocalFile(path));
        window.showFullScreen();
        window.raise();
        window.activateWindow();
        player.play();

        loop.exec();

        qApp->removeEventFilter(&skip);
        player.stop();
        window.hide();
        logger.log("Intro: done, " + reason + ".");
    }catch (...){
        logger.log("Intro: stopped by an error. The program goes on.", COLOR_RED);
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
