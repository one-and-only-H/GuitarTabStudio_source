#include "mainwindow.h"
#include "ui_mainwindow.h"

#include "csv_parser.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QFont>
#include <QInputDialog>
#include <QMessageBox>
#include <QProcess>
#include <QScrollBar>
#include <algorithm>

namespace {

QString encontrarRaizProyecto()
{
    QDir dir(QCoreApplication::applicationDirPath());
    for (int i = 0; i < 6; ++i) {
        if (dir.exists("scripts/midi2csv.py")) {
            return dir.absolutePath();
        }
        if (!dir.cdUp()) {
            break;
        }
    }
    return "C:/Users/SANDRA/Documents/guitarTabCreator/GuitarTabCreator";
}

float mapearRango(int valor, int inMin, int inMax, float outMin, float outMax)
{
    if (inMax <= inMin) return outMin;
    const float t = static_cast<float>(valor - inMin) / static_cast<float>(inMax - inMin);
    return outMin + std::clamp(t, 0.0f, 1.0f) * (outMax - outMin);
}

} // namespace

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    setWindowTitle("GuitarTabStudio 1.0");
    configurarVisorYScroll();
    configurarControlesDSP();

    // --- Bloque 1: Archivos ---
    connect(ui->Abrir_button, &QPushButton::clicked,
            this, &MainWindow::onAbrirCancion);
    connect(ui->Exportar_button, &QPushButton::clicked,
            this, &MainWindow::onExportarTablatura);

    ui->Exportar_button->setEnabled(false);

    // --- Bloque 2: Transporte de Audio ---
    connect(ui->Play_button, &QPushButton::clicked,
            this, &MainWindow::onPlayClicked);
    connect(ui->Pause_button, &QPushButton::clicked,
            this, &MainWindow::onPauseClicked);
    connect(ui->Stop_button, &QPushButton::clicked,
            this, &MainWindow::onStopClicked);

    ui->Play_button->setEnabled(false);
    ui->Pause_button->setEnabled(false);
    ui->Stop_button->setEnabled(false);

    timerProgreso_.setInterval(50); // 20 FPS para el cursor visual
    connect(&timerProgreso_, &QTimer::timeout,
            this, &MainWindow::onActualizarProgresoAudio);
}

MainWindow::~MainWindow()
{
    timerProgreso_.stop();
    player_.stop();
    delete ui;
}

void MainWindow::configurarVisorYScroll()
{
    QFont fuenteMono("Consolas", 11);
    fuenteMono.setStyleHint(QFont::Monospace);

    ui->TabWindow->setFont(fuenteMono);
    ui->TabWindow->setReadOnly(true);
    ui->TabWindow->setLineWrapMode(QPlainTextEdit::NoWrap);
    ui->TabWindow->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    connect(ui->TabWindow->horizontalScrollBar(), &QScrollBar::rangeChanged,
            ui->Tab_scrollbar, &QScrollBar::setRange);
    connect(ui->TabWindow->horizontalScrollBar(), &QScrollBar::valueChanged,
            ui->Tab_scrollbar, &QScrollBar::setValue);
    connect(ui->Tab_scrollbar, &QScrollBar::valueChanged,
            ui->TabWindow->horizontalScrollBar(), &QScrollBar::setValue);

    // Seek cuando el usuario arrastra manualmente la barra
    connect(ui->Tab_scrollbar, &QScrollBar::sliderMoved,
            this, &MainWindow::onScrollUsuarioSeek);
}

void MainWindow::configurarControlesDSP()
{
    // Pastillas: Single Coil por defecto
    ui->Single_Coil_select->setChecked(true);

    // Dials sin vuelta completa y con muescas visibles
    for (QDial *dial : {ui->Gain_dial, ui->Rate_dial, ui->Width_dial}) {
        dial->setRange(0, 100);
        dial->setWrapping(false);
        dial->setNotchesVisible(true);
    }
    ui->Gain_dial->setValue(45);
    ui->Rate_dial->setValue(20);
    ui->Width_dial->setValue(50);

    // EQ vertical: 0..100 (50 = neutro)
    for (QSlider *slider : {ui->Bass_slider, ui->Mid_slider, ui->Treble_slider}) {
        slider->setRange(0, 100);
        slider->setValue(50);
    }

    // Estados iniciales de efectos
    ui->checkBox->setChecked(false);    // OD apagado
    ui->Chorus_check->setChecked(true); // Chorus encendido (coincide con AudioPlayer default)
    ui->Gain_dial->setEnabled(false);
    ui->Rate_dial->setEnabled(true);
    ui->Width_dial->setEnabled(true);

    // Habilitar/deshabilitar dials según sus switches
    connect(ui->checkBox, &QCheckBox::toggled,
            ui->Gain_dial, &QDial::setEnabled);
    connect(ui->Chorus_check, &QCheckBox::toggled,
            ui->Rate_dial, &QDial::setEnabled);
    connect(ui->Chorus_check, &QCheckBox::toggled,
            ui->Width_dial, &QDial::setEnabled);

    // Conectar todos los controles al actualizador DSP en tiempo real
    connect(ui->Single_Coil_select, &QRadioButton::toggled,
            this, &MainWindow::onActualizarParametrosDSP);
    connect(ui->Humbucker_select, &QRadioButton::toggled,
            this, &MainWindow::onActualizarParametrosDSP);
    connect(ui->checkBox, &QCheckBox::toggled,
            this, &MainWindow::onActualizarParametrosDSP);
    connect(ui->Chorus_check, &QCheckBox::toggled,
            this, &MainWindow::onActualizarParametrosDSP);

    connect(ui->Gain_dial, &QDial::valueChanged,
            this, &MainWindow::onActualizarParametrosDSP);
    connect(ui->Rate_dial, &QDial::valueChanged,
            this, &MainWindow::onActualizarParametrosDSP);
    connect(ui->Width_dial, &QDial::valueChanged,
            this, &MainWindow::onActualizarParametrosDSP);

    connect(ui->Bass_slider, &QSlider::valueChanged,
            this, &MainWindow::onActualizarParametrosDSP);
    connect(ui->Mid_slider, &QSlider::valueChanged,
            this, &MainWindow::onActualizarParametrosDSP);
    connect(ui->Treble_slider, &QSlider::valueChanged,
            this, &MainWindow::onActualizarParametrosDSP);

    // Sincronizar valores iniciales con AudioPlayer
    player_.setGain(0.85f);
    onActualizarParametrosDSP();
}

void MainWindow::onActualizarParametrosDSP()
{
    // 1. Pastilla
    player_.setTipoPastilla(
        ui->Humbucker_select->isChecked()
            ? TipoPastilla::HUMBUCKER
            : TipoPastilla::SINGLE_COIL
    );

    // 2. Overdrive y Drive
    player_.setODActive(ui->checkBox->isChecked());
    player_.setODDrive(mapearRango(ui->Gain_dial->value(), 0, 100, OD_DIAL_MIN, OD_DIAL_MAX));

    // 3. EQ de 3 bandas (50 -> 1.0 neutro exacto)
    auto mapearEQ = [](int val) -> float {
        if (val <= 50) {
            return mapearRango(val, 0, 50, EQ_GAIN_MIN, EQ_GAIN_NEUTRO);
        }
        return mapearRango(val, 50, 100, EQ_GAIN_NEUTRO, EQ_GAIN_MAX);
    };
    player_.setBass(mapearEQ(ui->Bass_slider->value()));
    player_.setMid(mapearEQ(ui->Mid_slider->value()));
    player_.setTreble(mapearEQ(ui->Treble_slider->value()));

    // 4. Chorus
    player_.setChorusActive(ui->Chorus_check->isChecked());
    player_.setCRate(mapearRango(ui->Rate_dial->value(), 0, 100, CHORUS_RATE_MIN, CHORUS_RATE_MAX));
    player_.setCDepth(mapearRango(ui->Width_dial->value(), 0, 100, 0.0f, 1.0f));
}

void MainWindow::onAbrirCancion()
{
    const QString raiz = encontrarRaizProyecto();
    const QString carpetaSongs = raiz + "/songs";
    QDir().mkpath(carpetaSongs);

    const QString rutaSeleccionada = QFileDialog::getOpenFileName(
        this,
        "Abrir canción (MIDI o CSV)",
        carpetaSongs,
        "Canciones (*.mid *.midi *.csv);;Archivos MIDI (*.mid *.midi);;Archivos CSV (*.csv)"
    );

    if (rutaSeleccionada.isEmpty()) {
        return;
    }

    // Detener reproducción anterior si estaba activa
    timerProgreso_.stop();
    player_.stop();
    estabaPausado_ = false;

    const QFileInfo info(rutaSeleccionada);
    const QString ext = info.suffix().toLower();

    if (ext == "mid" || ext == "midi") {
        const QString scriptPython = raiz + "/scripts/midi2csv.py";
        if (!QFile::exists(scriptPython)) {
            QMessageBox::critical(
                this,
                "Script no encontrado",
                "No se encontró scripts/midi2csv.py en:\n" + scriptPython
            );
            return;
        }

        ui->statusbar->showMessage("Convirtiendo MIDI a CSV...");

        QProcess proceso;
        proceso.setWorkingDirectory(raiz);
        proceso.start("python", QStringList() << scriptPython << rutaSeleccionada);

        if (!proceso.waitForFinished(15000) || proceso.exitCode() != 0) {
            const QString err = QString::fromLocal8Bit(proceso.readAllStandardError());
            ui->statusbar->clearMessage();
            QMessageBox::critical(
                this,
                "Error al convertir MIDI",
                "No se pudo ejecutar midi2csv.py:\n" + err
            );
            return;
        }

        QDir dirSongs(carpetaSongs);
        const QStringList filtros = { info.completeBaseName() + "_*.csv" };
        const QStringList csvGenerados = dirSongs.entryList(filtros, QDir::Files, QDir::Name);

        if (csvGenerados.isEmpty()) {
            ui->statusbar->clearMessage();
            QMessageBox::warning(
                this,
                "Sin pistas",
                "El MIDI se procesó pero no se encontraron archivos CSV en songs/."
            );
            return;
        }

        QString csvElegido = csvGenerados.first();
        if (csvGenerados.size() > 1) {
            bool ok = false;
            csvElegido = QInputDialog::getItem(
                this,
                "Seleccionar pista",
                "El MIDI tiene varias pistas. Elige la de guitarra:",
                csvGenerados,
                0,
                false,
                &ok
            );
            if (!ok || csvElegido.isEmpty()) {
                ui->statusbar->clearMessage();
                return;
            }
        }

        rutaArchivoActual_ = rutaSeleccionada;
        cargarYMapearCSV(dirSongs.absoluteFilePath(csvElegido));
        return;
    }

    rutaArchivoActual_ = rutaSeleccionada;
    cargarYMapearCSV(rutaSeleccionada);
}

void MainWindow::cargarYMapearCSV(const QString &rutaCSV)
{
    try {
        ui->statusbar->showMessage("Mapeando notas y preparando sintetizador...");

        std::vector<Nota> notas = parsearCSV(rutaCSV.toStdString());
        if (notas.empty()) {
            ui->statusbar->clearMessage();
            QMessageBox::warning(this, "CSV vacío", "El archivo no contiene notas válidas.");
            return;
        }

        notasActuales_ = notas;
        duracionTotalCancion_ = 0.0;
        for (const auto &n : notasActuales_) {
            duracionTotalCancion_ = std::max(duracionTotalCancion_, n.timestamp + n.duracion);
        }

        // 1. Mapeo a tablatura
        ultimoResultado_ = Mapper::mapear(notasActuales_);

        TabGenerator::ConfigTab cfg;
        cfg.ancho_maximo_linea = 10000;

        const std::string tab = TabGenerator::generarTablatura(ultimoResultado_, cfg);
        tablaturaActual_ = QString::fromStdString(tab);

        ui->TabWindow->setPlainText(tablaturaActual_);
        ui->Tab_scrollbar->setValue(0);
        ui->Exportar_button->setEnabled(true);

        // 2. Preparar AudioPlayer con las notas cargadas
        if (!player_.prepararPista(notasActuales_, SAMPLE_RATE)) {
            QMessageBox::warning(this, "Audio", "No se pudo preparar la pista de audio.");
        }

        ui->Play_button->setEnabled(true);
        ui->Pause_button->setEnabled(false);
        ui->Stop_button->setEnabled(false);

        const QFileInfo info(rutaCSV);
        setWindowTitle("GuitarTabStudio 1.0 — " + info.fileName());
        ui->statusbar->showMessage(
            QString("Listo: %1 eventos | %2 notas | Duración: %3 s")
                .arg(ultimoResultado_.eventos.size())
                .arg(ultimoResultado_.notas.size())
                .arg(duracionTotalCancion_, 0, 'f', 1),
            5000
        );

    } catch (const std::exception &e) {
        ui->statusbar->clearMessage();
        QMessageBox::critical(
            this,
            "Error de procesamiento",
            QString("Error al procesar la canción:\n%1").arg(e.what())
        );
    }
}

void MainWindow::onPlayClicked()
{
    if (notasActuales_.empty()) {
        return;
    }

    if (estabaPausado_) {
        player_.resume();
        estabaPausado_ = false;
    } else {
        player_.stop();
        player_.prepararPista(notasActuales_, SAMPLE_RATE);
        onActualizarParametrosDSP();
        player_.play();
    }

    timerProgreso_.start();
    ui->Play_button->setEnabled(false);
    ui->Pause_button->setEnabled(true);
    ui->Stop_button->setEnabled(true);
    ui->statusbar->showMessage("Reproduciendo...");
}

void MainWindow::onPauseClicked()
{
    player_.pause();
    estabaPausado_ = true;
    timerProgreso_.stop();

    ui->Play_button->setEnabled(true);
    ui->Pause_button->setEnabled(false);
    ui->Stop_button->setEnabled(true);
    ui->statusbar->showMessage("En pausa.");
}

void MainWindow::onStopClicked()
{
    timerProgreso_.stop();
    player_.stop();
    estabaPausado_ = false;

    ui->Tab_scrollbar->setValue(0);
    ui->Play_button->setEnabled(!notasActuales_.empty());
    ui->Pause_button->setEnabled(false);
    ui->Stop_button->setEnabled(false);
    ui->statusbar->showMessage("Detenido.", 3000);
}

void MainWindow::onActualizarProgresoAudio()
{
    // Si la canción terminó de reproducirse naturalmente en su hilo
    if (!player_.estaReproduciendo() && !estabaPausado_) {
        timerProgreso_.stop();
        ui->Play_button->setEnabled(!notasActuales_.empty());
        ui->Pause_button->setEnabled(false);
        ui->Stop_button->setEnabled(false);
        ui->statusbar->showMessage("Reproducción finalizada.", 3000);
        return;
    }

    const float tActual = player_.obtenerTiempoActual();
    if (duracionTotalCancion_ > 0.0) {
        const double proporcion = std::clamp(
            static_cast<double>(tActual) / duracionTotalCancion_,
            0.0,
            1.0
        );
        const int maxScroll = ui->Tab_scrollbar->maximum();
        actualizandoScrollDesdeAudio_ = true;
        ui->Tab_scrollbar->setValue(static_cast<int>(proporcion * maxScroll));
        actualizandoScrollDesdeAudio_ = false;
    }
}

void MainWindow::onScrollUsuarioSeek(int valorScroll)
{
    if (actualizandoScrollDesdeAudio_ || duracionTotalCancion_ <= 0.0) {
        return;
    }

    const int maxScroll = ui->Tab_scrollbar->maximum();
    if (maxScroll <= 0) {
        return;
    }

    const float segundoDestino = static_cast<float>(
        (static_cast<double>(valorScroll) / static_cast<double>(maxScroll)) * duracionTotalCancion_
    );
    player_.seek(segundoDestino);
}

void MainWindow::onExportarTablatura()
{
    if (tablaturaActual_.isEmpty()) {
        return;
    }

    const QFileInfo info(rutaArchivoActual_);
    const QString nombreSugerido = info.completeBaseName() + "_tab.txt";

    const QString rutaDestino = QFileDialog::getSaveFileName(
        this,
        "Exportar tablatura como .txt",
        nombreSugerido,
        "Archivo de texto (*.txt)"
    );

    if (rutaDestino.isEmpty()) {
        return;
    }

    TabGenerator::ConfigTab cfg;
    cfg.ancho_maximo_linea = 100;

    if (TabGenerator::exportarTablaturaTxt(ultimoResultado_, rutaDestino.toStdString(), cfg)) {
        ui->statusbar->showMessage("Exportado en: " + rutaDestino, 5000);
    } else {
        QMessageBox::critical(
            this,
            "Error de escritura",
            "No se pudo guardar el archivo en:\n" + rutaDestino
        );
    }
}