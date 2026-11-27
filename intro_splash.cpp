#include "intro_splash.h"

#include <QApplication>
#include <QFile>
#include <QLabel>
#include <QPixmap>
#include <QPropertyAnimation>
#include <QEasingCurve>
#include <QSequentialAnimationGroup>
#include <QPauseAnimation>
#include <QTimer>
#include <QFont>
#include <QColor>
#include <QResource>

#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <mmsystem.h>
#endif

IntroSplash::IntroSplash(QWidget *parent)
    : QDialog(parent)
{
    setWindowFlags(Qt::SplashScreen | Qt::FramelessWindowHint);
    setFixedSize(760, 440);
    setStyleSheet("background-color: white;");

    // Logo del proyecto
    logoLabel_ = new QLabel(this);
    logoLabel_->setAlignment(Qt::AlignCenter);
    logoPixmap_ = QPixmap(":/icons/GuitarTabStudio_logo.png");
    logoLabel_->setPixmap(logoPixmap_.scaled(150, 150, Qt::KeepAspectRatio,
                                             Qt::SmoothTransformation));
    logoLabel_->setGeometry(55, 205, 150, 150);

    titleLabel_ = new QLabel("GuitarTabStudio", this);
    titleLabel_->setAlignment(Qt::AlignCenter);
    titleLabel_->setStyleSheet("color: #202020; background: transparent;");
    // La tipografia intenta seguir el lenguaje visual de los controles del programa (creo que salio bien, idk)
    QFont titleFont("Tw Cen MT", 34, QFont::Bold);
    titleFont.setItalic(true);
    titleFont.setStyleHint(QFont::SansSerif);
    titleLabel_->setFont(titleFont);
    titleLabel_->setGeometry(70, 300, 620, 58);
    titleLabel_->hide();

    subtitleLabel_ = new QLabel("♪ FROM MIDI TO GUITAR TABS ♪", this); //el simbolo de corchea me gusta
    subtitleLabel_->setAlignment(Qt::AlignCenter);
    subtitleLabel_->setStyleSheet(
        "color: #777777; letter-spacing: 4px; background: transparent;");
    QFont subtitleFont("Tw Cen MT", 11, QFont::Bold);
    subtitleFont.setItalic(true);
    subtitleFont.setStyleHint(QFont::SansSerif);
    subtitleLabel_->setFont(subtitleFont);
    subtitleLabel_->setGeometry(100, 365, 560, 26);
    subtitleLabel_->hide();

    animation_ = new QSequentialAnimationGroup(this);

    // Cuatro saltos Em -> C -> G -> D
    auto salto = [this](const QPoint &desde, const QPoint &arriba,
                        const QPoint &aterrizaje, int duracion) {
        auto subir = new QPropertyAnimation(logoLabel_, "pos", animation_);
        subir->setDuration(duracion / 2);
        subir->setStartValue(desde);
        subir->setEndValue(arriba);
        subir->setEasingCurve(QEasingCurve::OutCubic);
        animation_->addAnimation(subir);

        auto bajar = new QPropertyAnimation(logoLabel_, "pos", animation_);
        bajar->setDuration(duracion / 2);
        bajar->setStartValue(arriba);
        bajar->setEndValue(aterrizaje);
        bajar->setEasingCurve(QEasingCurve::InCubic);
        animation_->addAnimation(bajar);
    };

    // Em: primer salto
    salto(QPoint(55, 205), QPoint(95, 105), QPoint(185, 205), 650);
    animation_->addPause(120);
    // C: segundo salto
    salto(QPoint(185, 205), QPoint(275, 85), QPoint(365, 205), 650);
    animation_->addPause(120);
    // G: tercer salto
    salto(QPoint(365, 205), QPoint(455, 95), QPoint(520, 205), 650);
    animation_->addPause(120);
    // D: cuarto salto, que aterriza en el centro
    salto(QPoint(520, 205), QPoint(420, 65), QPoint(305, 145), 700);

    // Al cuarto salto el logo se expande de manera ma ma ma masiva
    auto desplegar = new QPropertyAnimation(logoLabel_, "geometry", animation_);
    desplegar->setDuration(450);
    desplegar->setStartValue(QRect(370, 115, 20, 180));
    desplegar->setEndValue(QRect(250, 20, 260, 260));
    desplegar->setEasingCurve(QEasingCurve::OutBack);
    connect(desplegar, &QPropertyAnimation::valueChanged, this,
            [this](const QVariant &value) {
        const QRect rect = value.toRect();
        logoLabel_->setPixmap(logoPixmap_.scaled(
            rect.size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
    });
    animation_->addAnimation(desplegar);

    connect(animation_, &QSequentialAnimationGroup::finished, this, [this]() {
        titleLabel_->show();
        subtitleLabel_->show();
        QTimer::singleShot(650, this, &QDialog::accept);
    });

    reproducirAcordes();
    animation_->start();
}

IntroSplash::~IntroSplash()
{
#ifdef Q_OS_WIN
    PlaySoundW(nullptr, nullptr, 0);
#endif
}

void IntroSplash::reproducirAcordes()
{
#ifdef Q_OS_WIN
    QFile audioResource(":/intro/intro_chords.wav");

    if (!audioResource.open(QIODevice::ReadOnly)) {
        OutputDebugStringW(L"[GuitarTabStudio] No se pudo abrir :/intro/intro_chords.wav\\n");
        return;
    }

    // La memoria debe seguir viva durante toda la reproducción
    audioData_ = audioResource.readAll();

    if (audioData_.isEmpty()) {
        OutputDebugStringW(L"[GuitarTabStudio] El WAV está vacío.\\n");
        return;
    }

    if (!PlaySoundW(reinterpret_cast<LPCWSTR>(audioData_.constData()),
                    nullptr, SND_MEMORY | SND_ASYNC | SND_NODEFAULT)) {
        OutputDebugStringW(L"[GuitarTabStudio] PlaySoundW no pudo iniciar el audio.\\n");
    }
#endif
}
