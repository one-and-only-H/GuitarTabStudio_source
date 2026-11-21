#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "audio_player.h"
#include "mapper/mapper.h"
#include "tab_generator.h"

#include <QMainWindow>
#include <QString>
#include <QTimer>
#include <vector>

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    // Bloque 1: Archivos y Tablatura
    void onAbrirCancion();
    void onExportarTablatura();

    // Bloque 2: Transporte y Sincronización
    void onPlayClicked();
    void onPauseClicked();
    void onStopClicked();
    void onActualizarProgresoAudio();
    void onScrollUsuarioSeek(int valorScroll);

    // Bloque 3: Controles DSP en tiempo real
    void onActualizarParametrosDSP();

private:
    Ui::MainWindow *ui;

    AudioPlayer player_;
    QTimer timerProgreso_;

    std::vector<Nota> notasActuales_;
    double duracionTotalCancion_ = 0.0;
    bool estabaPausado_ = false;
    bool actualizandoScrollDesdeAudio_ = false;

    // Mapa tiempo (segundos) <-> posición visual en caracteres (respeta silencios | xN |)
    struct PuntoMapaTiempo {
        double tiempoSeg = 0.0;
        double posChar = 0.0;
    };
    std::vector<PuntoMapaTiempo> mapaTiempoScroll_;
    double totalCharsTab_ = 1.0;

    QString rutaArchivoActual_;
    QString tablaturaActual_;
    Mapper::ResultadoMapeo ultimoResultado_;

    void configurarVisorYScroll();
    void configurarControlesDSP();
    void cargarYMapearCSV(const QString &rutaCSV);
};

#endif // MAINWINDOW_H