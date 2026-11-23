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
#include <QRegularExpression>
#include <QTextStream>
#include <algorithm>
#include <cmath>

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

    // Bloque 1: Archivos
    connect(ui->Abrir_button, &QPushButton::clicked,
            this, &MainWindow::onAbrirCancion);
    connect(ui->Exportar_button, &QPushButton::clicked,
            this, &MainWindow::onExportarTablatura);

    ui->Exportar_button->setEnabled(false);

    // Bloque 2: Transporte de Audio
    connect(ui->Play_button, &QPushButton::clicked,
            this, &MainWindow::onPlayClicked);
    connect(ui->Pause_button, &QPushButton::clicked,
            this, &MainWindow::onPauseClicked);
    connect(ui->Stop_button, &QPushButton::clicked,
            this, &MainWindow::onStopClicked);

    ui->Play_button->setEnabled(false);
    ui->Pause_button->setEnabled(false);
    ui->Stop_button->setEnabled(false);

    timerProgreso_.setInterval(50);
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

    connect(ui->Tab_scrollbar, &QScrollBar::sliderMoved,
            this, &MainWindow::onScrollUsuarioSeek);
}

void MainWindow::configurarControlesDSP()
{
    ui->KS_select->setChecked(true);
    ui->Single_Coil_select->setChecked(true);

    for (QDial *dial : {ui->Gain_dial, ui->Rate_dial, ui->Width_dial}) {
        dial->setRange(0, 100);
        dial->setWrapping(false);
        dial->setNotchesVisible(true);
    }
    // Gain_dial inicia en 0 (limpio 1x) y sube exponencialmente hasta 60x
    ui->Gain_dial->setValue(0);
    ui->Rate_dial->setValue(20);
    ui->Width_dial->setValue(45);

    for (QSlider *slider : {ui->Bass_slider, ui->Mid_slider, ui->Treble_slider}) {
        slider->setRange(0, 100);
        slider->setValue(50);
    }

    // Gain_dial siempre habilitado mientras el checkbox OD suma +40x fijos
    ui->checkBox->setChecked(false);
    ui->Chorus_check->setChecked(true);
    ui->Gain_dial->setEnabled(true);
    ui->Rate_dial->setEnabled(true);
    ui->Width_dial->setEnabled(true);

    connect(ui->Chorus_check, &QCheckBox::toggled,
            ui->Rate_dial, &QDial::setEnabled);
    connect(ui->Chorus_check, &QCheckBox::toggled,
            ui->Width_dial, &QDial::setEnabled);

    connect(ui->KS_select, &QRadioButton::toggled,
            this, &MainWindow::onActualizarParametrosDSP);
    connect(ui->Aditiva_select, &QRadioButton::toggled,
            this, &MainWindow::onActualizarParametrosDSP);

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

    player_.setGain(0.90f);
    onActualizarParametrosDSP();
}

void MainWindow::onActualizarParametrosDSP()
{
    player_.setSintetizador(
        ui->Aditiva_select->isChecked()
            ? TipoSintetizador::ADITIVO
            : TipoSintetizador::KARPLUS_STRONG
    );

    player_.setTipoPastilla(
        ui->Humbucker_select->isChecked()
            ? TipoPastilla::HUMBUCKER
            : TipoPastilla::SINGLE_COIL
    );

    // Gain_dial aporta de 1x a 60x y el boton OD suma +40x hasta un tope de 100x
    player_.setODActive(ui->checkBox->isChecked());
    const float normGain = std::clamp(ui->Gain_dial->value() / 100.0f, 0.0f, 1.0f);
    const float gainDial = GAIN_DIAL_MIN * std::pow(GAIN_DIAL_MAX / GAIN_DIAL_MIN, normGain);
    player_.setODDrive(gainDial);

    auto mapearEQ = [](int val) -> float {
        if (val <= 50) {
            return mapearRango(val, 0, 50, EQ_GAIN_MIN, EQ_GAIN_NEUTRO);
        }
        return mapearRango(val, 50, 100, EQ_GAIN_NEUTRO, EQ_GAIN_MAX);
    };
    player_.setBass(mapearEQ(ui->Bass_slider->value()));
    player_.setMid(mapearEQ(ui->Mid_slider->value()));
    player_.setTreble(mapearEQ(ui->Treble_slider->value()));

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
        proceso.start("python", QStringList() << "-u" << scriptPython << rutaSeleccionada);

        if (!proceso.waitForFinished(15000) || proceso.exitCode() != 0) {
            const QString err = QString::fromUtf8(proceso.readAllStandardError());
            const QString out = QString::fromUtf8(proceso.readAllStandardOutput());
            ui->statusbar->clearMessage();
            QMessageBox::critical(
                this,
                "Error al convertir MIDI",
                "No se pudo ejecutar midi2csv.py:\n" + (err.isEmpty() ? out : err)
            );
            return;
        }

        const QString salida = QString::fromUtf8(proceso.readAllStandardOutput());
        const QStringList lineas = salida.split(QRegularExpression("[\r\n]+"), Qt::SkipEmptyParts);

        QStringList rutasCsvGenerados;
        for (const QString &linea : lineas) {
            if (linea.startsWith("CSV_OUT:")) {
                const QString ruta = linea.mid(8).trimmed();
                if (!ruta.isEmpty()) {
                    rutasCsvGenerados.append(QDir::fromNativeSeparators(ruta));
                }
            }
        }

        if (rutasCsvGenerados.isEmpty()) {
            ui->statusbar->clearMessage();
            QMessageBox::warning(
                this,
                "Sin pistas",
                "El MIDI no contiene pistas de guitarra válidas (rango E2-E6)."
            );
            return;
        }

        QString rutaCsvElegida = rutasCsvGenerados.first();
        if (rutasCsvGenerados.size() > 1) {
            QStringList nombresMostrar;
            for (const QString &r : rutasCsvGenerados) {
                nombresMostrar.append(QFileInfo(r).fileName());
            }

            bool ok = false;
            const QString elegido = QInputDialog::getItem(
                this,
                "Seleccionar pista",
                "El MIDI tiene varias pistas. Elige la de guitarra:",
                nombresMostrar,
                0,
                false,
                &ok
            );
            if (!ok || elegido.isEmpty()) {
                ui->statusbar->clearMessage();
                return;
            }
            const int idx = nombresMostrar.indexOf(elegido);
            if (idx >= 0) {
                rutaCsvElegida = rutasCsvGenerados.at(idx);
            }
        }

        rutaArchivoActual_ = rutaSeleccionada;
        cargarYMapearCSV(rutaCsvElegida);
        return;
    }

    rutaArchivoActual_ = rutaSeleccionada;
    cargarYMapearCSV(rutaSeleccionada);
}

void MainWindow::cargarYMapearCSV(const QString &rutaCSV)
{
    try {
        if (!QFileInfo::exists(rutaCSV)) {
            ui->statusbar->clearMessage();
            QMessageBox::critical(this, "Archivo no encontrado", "No existe el archivo:\n" + rutaCSV);
            return;
        }

        ui->statusbar->showMessage("Mapeando notas y preparando sintetizador...");

        // toLocal8Bit() evita que std::ifstream falle en Windows con rutas que tengan tildes o ñ
        const std::string rutaNativa(rutaCSV.toLocal8Bit().constData());
        std::vector<Nota> notas = parsearCSV(rutaNativa);
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

        ultimoResultado_ = Mapper::mapear(notasActuales_);

        TabGenerator::ConfigTab cfg;
        cfg.ancho_maximo_linea = 10000;
        cfg.ancho_compas = 24;

        // Cargar BPM y metrica de compas desde el archivo .meta si existe
        QFile archivoMeta(rutaCSV + ".meta");
        if (archivoMeta.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QTextStream in(&archivoMeta);
            while (!in.atEnd()) {
                const QString linea = in.readLine().trimmed();
                if (linea.startsWith("bpm=")) {
                    bool ok = false;
                    const double val = linea.mid(4).toDouble(&ok);
                    if (ok && val > 0.0) cfg.bpm = val;
                } else if (linea.startsWith("numerador=")) {
                    bool ok = false;
                    const int val = linea.mid(10).toInt(&ok);
                    if (ok && val > 0) cfg.numerador_compas = val;
                } else if (linea.startsWith("denominador=")) {
                    bool ok = false;
                    const int val = linea.mid(12).toInt(&ok);
                    if (ok && val > 0) cfg.denominador_compas = val;
                }
            }
        }

        const std::string tab = TabGenerator::generarTablatura(ultimoResultado_, cfg);
        tablaturaActual_ = QString::fromStdString(tab);

        // Construccion de tabla tiempo <-> caracteres por compases
        mapaTiempoScroll_.clear();
        auto cols = TabGenerator::construirColumnas(ultimoResultado_, cfg);
        double charAcum = 0.0;
        mapaTiempoScroll_.push_back({0.0, 0.0});
        for (const auto &c : cols) {
            mapaTiempoScroll_.push_back({c.timestamp, charAcum});
            charAcum += static_cast<double>(c.ancho_visual);
            mapaTiempoScroll_.push_back({c.timestamp + c.duracion, charAcum});
        }
        totalCharsTab_ = std::max(1.0, charAcum);

        ui->TabWindow->setPlainText(tablaturaActual_);
        ui->Tab_scrollbar->setValue(0);
        ui->Exportar_button->setEnabled(true);

        if (!player_.prepararPista(notasActuales_, SAMPLE_RATE)) {
            QMessageBox::warning(this, "Audio", "No se pudo preparar la pista de audio.");
        }

        ui->Play_button->setEnabled(true);
        ui->Pause_button->setEnabled(false);
        ui->Stop_button->setEnabled(false);

        const QFileInfo info(rutaCSV);
        setWindowTitle("GuitarTabStudio 1.0 — " + info.fileName());
        ui->statusbar->showMessage(
            QString("Listo: %1 eventos | %2 notas | %3/%4 @ %5 BPM | Duración: %6 s")
                .arg(ultimoResultado_.eventos.size())
                .arg(ultimoResultado_.notas.size())
                .arg(cfg.numerador_compas)
                .arg(cfg.denominador_compas)
                .arg(cfg.bpm, 0, 'f', 0)
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
    if (!player_.estaReproduciendo() && !estabaPausado_) {
        timerProgreso_.stop();
        ui->Play_button->setEnabled(!notasActuales_.empty());
        ui->Pause_button->setEnabled(false);
        ui->Stop_button->setEnabled(false);
        ui->statusbar->showMessage("Reproducción finalizada.", 3000);
        return;
    }

    const double tActual = static_cast<double>(player_.obtenerTiempoActual());
    double posCharActual = 0.0;

    if (!mapaTiempoScroll_.empty()) {
        auto it = std::lower_bound(
            mapaTiempoScroll_.begin(), mapaTiempoScroll_.end(), tActual,
            [](const PuntoMapaTiempo &p, double t) { return p.tiempoSeg < t; }
        );
        if (it == mapaTiempoScroll_.begin()) {
            posCharActual = it->posChar;
        } else if (it == mapaTiempoScroll_.end()) {
            posCharActual = mapaTiempoScroll_.back().posChar;
        } else {
            const auto &p1 = *it;
            const auto &p0 = *(it - 1);
            const double dt = std::max(1e-6, p1.tiempoSeg - p0.tiempoSeg);
            const double alpha = std::clamp((tActual - p0.tiempoSeg) / dt, 0.0, 1.0);
            posCharActual = p0.posChar + alpha * (p1.posChar - p0.posChar);
        }
    }

    const double proporcion = std::clamp(posCharActual / totalCharsTab_, 0.0, 1.0);
    const int maxScroll = ui->Tab_scrollbar->maximum();
    actualizandoScrollDesdeAudio_ = true;
    ui->Tab_scrollbar->setValue(static_cast<int>(proporcion * maxScroll));
    actualizandoScrollDesdeAudio_ = false;
}

void MainWindow::onScrollUsuarioSeek(int valorScroll)
{
    if (actualizandoScrollDesdeAudio_ || mapaTiempoScroll_.empty()) {
        return;
    }

    const int maxScroll = ui->Tab_scrollbar->maximum();
    if (maxScroll <= 0) {
        return;
    }

    const double charObjetivo =
        (static_cast<double>(valorScroll) / static_cast<double>(maxScroll)) * totalCharsTab_;

    auto it = std::lower_bound(
        mapaTiempoScroll_.begin(), mapaTiempoScroll_.end(), charObjetivo,
        [](const PuntoMapaTiempo &p, double c) { return p.posChar < c; }
    );

    double segundoDestino = 0.0;
    if (it == mapaTiempoScroll_.begin()) {
        segundoDestino = it->tiempoSeg;
    } else if (it == mapaTiempoScroll_.end()) {
        segundoDestino = mapaTiempoScroll_.back().tiempoSeg;
    } else {
        const auto &p1 = *it;
        const auto &p0 = *(it - 1);
        const double dc = std::max(1e-6, p1.posChar - p0.posChar);
        const double alpha = std::clamp((charObjetivo - p0.posChar) / dc, 0.0, 1.0);
        segundoDestino = p0.tiempoSeg + alpha * (p1.tiempoSeg - p0.tiempoSeg);
    }

    player_.seek(static_cast<float>(segundoDestino));
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