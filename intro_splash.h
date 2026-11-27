#ifndef INTRO_SPLASH_H
#define INTRO_SPLASH_H

#include <QDialog>
#include <QByteArray>
#include <QPixmap>

class QLabel;
class QSequentialAnimationGroup;

class IntroSplash : public QDialog
{
    Q_OBJECT

public:
    explicit IntroSplash(QWidget *parent = nullptr);
    ~IntroSplash() override;

private:
    QLabel *logoLabel_ = nullptr;
    QPixmap logoPixmap_;
    QLabel *titleLabel_ = nullptr;
    QLabel *subtitleLabel_ = nullptr;
    QSequentialAnimationGroup *animation_ = nullptr;
    QByteArray audioData_;

    void reproducirAcordes();
};

#endif // INTRO_SPLASH_H
